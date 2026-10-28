/* Original optional W5500 SPI/netif adapter for 5tratumFW Gamma.
 * Written against ESP-IDF 6.0.2 public APIs; GPL-3.0-or-later. */
#include "gamma_ethernet.h"
#include "gamma_w5500.h"
#include "driver/spi_master.h"
#include "esp_intr_alloc.h"
#include "esp_heap_caps.h"
#include "esp_mac.h"
#include "esp_netif_defaults.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <stdlib.h>
#include <string.h>

enum { DMA_BYTES = 1536, SPI_WAIT_MS = 25, LOCK_WAIT_MS = 200, POLL_MS = 20 };

typedef struct {
    esp_netif_driver_base_t base; /* esp_netif_attach requires this first. */
    gamma_w5500_t chip;
    spi_device_handle_t spi;
    spi_transaction_t transaction;
    uint8_t *dma_tx;
    uint8_t *dma_rx;
    SemaphoreHandle_t io_lock;
    SemaphoreHandle_t worker_done;
    TaskHandle_t worker;
    bool bus_owned;
    bool transaction_pending;
    bool spi_fault;
    bool worker_created;
    bool stop_requested;
    bool netif_started;
} ethernet_driver_t;

static ethernet_driver_t driver;
static gamma_ethernet_status_t state;
static esp_netif_t *public_netif;
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;

static TickType_t ticks(unsigned milliseconds)
{
    TickType_t value = pdMS_TO_TICKS(milliseconds);
    return value ? value : 1;
}

static void set_error(esp_err_t error)
{
    portENTER_CRITICAL(&state_lock);
    state.last_error = error;
    portEXIT_CRITICAL(&state_lock);
}

static esp_err_t protocol_error(gamma_w5500_result_t result)
{
    switch (result) {
    case GAMMA_W5500_OK: return ESP_OK;
    case GAMMA_W5500_NOT_FOUND: return ESP_ERR_NOT_FOUND;
    case GAMMA_W5500_TIMEOUT: return ESP_ERR_TIMEOUT;
    case GAMMA_W5500_AGAIN: return ESP_ERR_NO_MEM;
    case GAMMA_W5500_INVALID: return ESP_ERR_INVALID_STATE;
    case GAMMA_W5500_LINK_DOWN: return ESP_ERR_INVALID_STATE;
    case GAMMA_W5500_BAD_FRAME: return ESP_ERR_INVALID_SIZE;
    default: return ESP_FAIL;
    }
}

/* Persistent DMA storage and descriptor outlive even an abnormal SPI timeout.
 * An incomplete transaction quarantines this SPI device until safely drained;
 * no stack address is left with DMA, and no uncertain SPI transfer is repeated. */
static int spi_transfer(void *context, uint8_t block, uint16_t address, bool write, void *data, size_t length)
{
    ethernet_driver_t *d = context;
    if (!d->spi || d->spi_fault || d->transaction_pending || !data || !length || length > DMA_BYTES || block > 31)
        return -1;
    if (write) memcpy(d->dma_tx, data, length);
    memset(&d->transaction, 0, sizeof(d->transaction));
    d->transaction.addr = (uint32_t)address << 8 | (uint32_t)block << 3 | (write ? 4u : 0u);
    d->transaction.length = length * 8u;
    d->transaction.tx_buffer = write ? d->dma_tx : NULL;
    d->transaction.rx_buffer = write ? NULL : d->dma_rx;
    esp_err_t error = spi_device_queue_trans(d->spi, &d->transaction, ticks(SPI_WAIT_MS));
    if (error != ESP_OK) { set_error(error); return -1; }
    d->transaction_pending = true;
    spi_transaction_t *completed = NULL;
    error = spi_device_get_trans_result(d->spi, &completed, ticks(SPI_WAIT_MS));
    if (error != ESP_OK || completed != &d->transaction) {
        d->spi_fault = true;
        set_error(error == ESP_OK ? ESP_FAIL : error);
        return -1;
    }
    d->transaction_pending = false;
    if (!write) memcpy(data, d->dma_rx, length);
    return 0;
}

static uint64_t clock_ms(void *context)
{
    (void)context;
    return (uint64_t)esp_timer_get_time() / 1000u;
}

static void sleep_ms(void *context, unsigned milliseconds)
{
    (void)context;
    vTaskDelay(ticks(milliseconds));
}

static esp_err_t transmit(void *context, void *frame, size_t length)
{
    ethernet_driver_t *d = context;
    gamma_ethernet_status_t snapshot;
    gamma_ethernet_get_status(&snapshot);
    if (!snapshot.started || !snapshot.available || !d->io_lock) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(d->io_lock, ticks(LOCK_WAIT_MS)) != pdTRUE) return ESP_ERR_TIMEOUT;
    gamma_ethernet_get_status(&snapshot);
    gamma_w5500_result_t result = snapshot.started && snapshot.available
        ? gamma_w5500_transmit(&d->chip, frame, length) : GAMMA_W5500_INVALID;
    xSemaphoreGive(d->io_lock);
    esp_err_t error = protocol_error(result);
    if (error != ESP_OK && result != GAMMA_W5500_AGAIN && result != GAMMA_W5500_LINK_DOWN) set_error(error);
    return error;
}

static void free_received(void *context, void *buffer)
{
    (void)context;
    free(buffer);
}

static esp_err_t attach(esp_netif_t *netif, esp_netif_iodriver_handle handle)
{
    ethernet_driver_t *d = handle;
    const esp_netif_driver_ifconfig_t config = {
        .handle = d, .transmit = transmit, .driver_free_rx_buffer = free_received
    };
    d->base.netif = netif;
    return esp_netif_set_driver_config(netif, &config);
}

static bool stopping(void)
{
    portENTER_CRITICAL(&state_lock);
    bool value = driver.stop_requested;
    portEXIT_CRITICAL(&state_lock);
    return value;
}

static void link_state(bool available, bool link, esp_err_t error)
{
    portENTER_CRITICAL(&state_lock);
    state.available = available;
    state.link_up = link;
    if (!link) { state.has_ip = false; state.ip.addr = 0; }
    if (error != ESP_OK) state.last_error = error;
    portEXIT_CRITICAL(&state_lock);
}

static void poll_worker(void *context)
{
    ethernet_driver_t *d = context;
    bool connected = false;
    esp_err_t fatal = ESP_OK;
    while (!stopping()) {
        bool link = false;
        if (xSemaphoreTake(d->io_lock, ticks(LOCK_WAIT_MS)) != pdTRUE) { fatal = ESP_ERR_TIMEOUT; break; }
        gamma_w5500_result_t result = gamma_w5500_link(&d->chip, &link);
        xSemaphoreGive(d->io_lock);
        if (result != GAMMA_W5500_OK) { fatal = protocol_error(result); break; }
        link_state(true, link, ESP_OK);
        /* These calls can cause immediate DHCP/ARP output. No SPI lock is held. */
        if (link != connected) {
            if (link) esp_netif_action_connected(d->base.netif, NULL, 0, NULL);
            else esp_netif_action_disconnected(d->base.netif, NULL, 0, NULL);
            connected = link;
        }
        if (link) {
            for (unsigned count = 0; count < 8 && !stopping(); ++count) {
                uint8_t *frame = malloc(GAMMA_W5500_FRAME_BYTES);
                if (!frame) { set_error(ESP_ERR_NO_MEM); break; }
                size_t length = 0;
                if (xSemaphoreTake(d->io_lock, ticks(LOCK_WAIT_MS)) != pdTRUE) {
                    free(frame); fatal = ESP_ERR_TIMEOUT; break;
                }
                result = gamma_w5500_receive(&d->chip, frame, GAMMA_W5500_FRAME_BYTES, &length);
                xSemaphoreGive(d->io_lock);
                if (result == GAMMA_W5500_OK) {
                    /* Ownership passes to the attached stack/free callback.
                     * Stack input may synchronously transmit a reply, so this
                     * handoff must remain outside the SPI lock. */
                    esp_err_t input = esp_netif_receive(d->base.netif, frame, length, NULL);
                    if (input != ESP_OK) set_error(input);
                } else {
                    free(frame);
                    if (result == GAMMA_W5500_AGAIN) break;
                    if (result != GAMMA_W5500_BAD_FRAME) { fatal = protocol_error(result); break; }
                }
            }
            esp_netif_ip_info_t ip = {0};
            const bool has_ip = fatal == ESP_OK && esp_netif_is_netif_up(d->base.netif)
                && esp_netif_get_ip_info(d->base.netif, &ip) == ESP_OK && ip.ip.addr != 0;
            portENTER_CRITICAL(&state_lock);
            state.has_ip = has_ip;
            state.ip.addr = has_ip ? ip.ip.addr : 0;
            portEXIT_CRITICAL(&state_lock);
        }
        if (fatal != ESP_OK) break;
        vTaskDelay(ticks(POLL_MS));
    }
    link_state(fatal == ESP_OK, false, fatal);
    if (connected) esp_netif_action_disconnected(d->base.netif, NULL, 0, NULL);
    portENTER_CRITICAL(&state_lock);
    state.started = false;
    d->worker = NULL;
    portEXIT_CRITICAL(&state_lock);
    xSemaphoreGive(d->worker_done);
    vTaskDelete(NULL);
}

static esp_err_t drain_transaction(ethernet_driver_t *d)
{
    if (!d->transaction_pending) return ESP_OK;
    spi_transaction_t *completed = NULL;
    esp_err_t error = spi_device_get_trans_result(d->spi, &completed, ticks(100));
    if (error != ESP_OK || completed != &d->transaction) return ESP_ERR_TIMEOUT;
    d->transaction_pending = false;
    return ESP_OK;
}

static esp_err_t release_resources(void)
{
    ethernet_driver_t *d = &driver;
    if (d->netif_started) {
        esp_netif_action_disconnected(d->base.netif, NULL, 0, NULL);
        esp_netif_action_stop(d->base.netif, NULL, 0, NULL);
        d->netif_started = false;
    }
    portENTER_CRITICAL(&state_lock);
    public_netif = NULL;
    state.started = false;
    state.available = false;
    state.link_up = false;
    state.has_ip = false;
    state.ip.addr = 0;
    portEXIT_CRITICAL(&state_lock);
    if (d->base.netif) { esp_netif_destroy(d->base.netif); d->base.netif = NULL; }
    if (!d->io_lock || xSemaphoreTake(d->io_lock, ticks(LOCK_WAIT_MS)) != pdTRUE) return ESP_ERR_TIMEOUT;
    esp_err_t result = drain_transaction(d);
    if (result == ESP_OK && d->spi) {
        if (!d->spi_fault && d->chip.ready) (void)gamma_w5500_close(&d->chip);
        /* CLOSE itself can create a new uncertain SPI transaction. Its DMA
         * descriptor/storage must also be drained or quarantined before remove. */
        result = drain_transaction(d);
        if (result == ESP_OK) result = spi_bus_remove_device(d->spi);
        if (result == ESP_OK) d->spi = NULL;
    }
    if (result == ESP_OK && d->bus_owned) {
        result = spi_bus_free(SPI2_HOST);
        if (result == ESP_OK) d->bus_owned = false;
    }
    if (result == ESP_OK) {
        free(d->dma_tx); free(d->dma_rx);
        d->dma_tx = d->dma_rx = NULL;
        d->spi_fault = false;
        memset(&d->chip, 0, sizeof(d->chip));
    }
    /* On an abnormal undrainable hardware transaction, persistent descriptor
     * and DMA storage are intentionally retained until restart, never freed
     * under DMA. The interface is already stopped and Wi-Fi can recover. */
    xSemaphoreGive(d->io_lock);
    return result;
}

esp_err_t gamma_ethernet_start(const char *hostname)
{
    ethernet_driver_t *d = &driver;
    if (!hostname || !hostname[0] || strnlen(hostname, 64) >= 64) return ESP_ERR_INVALID_ARG;
    for (const unsigned char *p = (const unsigned char *)hostname; *p; ++p)
        if (*p < 33 || *p > 126) return ESP_ERR_INVALID_ARG;
    if (d->base.netif || d->spi || d->bus_owned || d->worker_created) return ESP_ERR_INVALID_STATE;
    if (!d->io_lock) d->io_lock = xSemaphoreCreateMutex();
    if (!d->worker_done) d->worker_done = xSemaphoreCreateBinary();
    if (!d->io_lock || !d->worker_done) return ESP_ERR_NO_MEM;
    portENTER_CRITICAL(&state_lock);
    memset(&state, 0, sizeof(state));
    d->stop_requested = false;
    portEXIT_CRITICAL(&state_lock);
    d->base.post_attach = attach;
    d->dma_tx = heap_caps_malloc(DMA_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    d->dma_rx = heap_caps_malloc(DMA_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    esp_err_t error = ESP_ERR_NO_MEM;
    if (!d->dma_tx || !d->dma_rx) goto failed;
    const spi_bus_config_t bus = {
        .mosi_io_num = 40, .miso_io_num = 39, .sclk_io_num = 41,
        .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = DMA_BYTES,
#if CONFIG_SPI_MASTER_ISR_IN_IRAM
        /* Keep queued SPI completion available while OTA disables flash cache.
         * This adapter registers no user ISR callbacks. */
        .intr_flags = ESP_INTR_FLAG_IRAM,
#endif
    };
    error = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (error != ESP_OK) goto failed;
    d->bus_owned = true;
    const spi_device_interface_config_t device = {
        .address_bits = 24, .mode = 0, .clock_speed_hz = 20000000,
        .spics_io_num = 42, .queue_size = 1, .flags = SPI_DEVICE_NO_DUMMY
    };
    error = spi_bus_add_device(SPI2_HOST, &device, &d->spi);
    if (error != ESP_OK) goto failed;
    uint8_t mac[6];
    error = esp_read_mac(mac, ESP_MAC_ETH);
    if (error != ESP_OK) goto failed;
    const gamma_w5500_io_t io = {
        .context = d, .transfer = spi_transfer, .milliseconds = clock_ms, .sleep_ms = sleep_ms
    };
    error = protocol_error(gamma_w5500_initialize(&d->chip, &io, mac));
    if (error != ESP_OK) goto failed;
    esp_netif_inherent_config_t inherent = ESP_NETIF_INHERENT_DEFAULT_ETH();
    inherent.if_key = "GAMMA_W5500";
    inherent.if_desc = "gamma-eth";
    inherent.route_prio = 100;
    const esp_netif_config_t netif_config = {
        .base = &inherent, .driver = NULL, .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
    };
    d->base.netif = esp_netif_new(&netif_config);
    if (!d->base.netif) { error = ESP_ERR_NO_MEM; goto failed; }
    if ((error = esp_netif_set_mac(d->base.netif, mac)) != ESP_OK) goto failed;
    if ((error = esp_netif_set_hostname(d->base.netif, hostname)) != ESP_OK) goto failed;
    if ((error = esp_netif_attach(d->base.netif, d)) != ESP_OK) goto failed;
    esp_netif_action_start(d->base.netif, NULL, 0, NULL);
    d->netif_started = true;
    portENTER_CRITICAL(&state_lock);
    state.started = true;
    state.available = true;
    public_netif = d->base.netif;
    portEXIT_CRITICAL(&state_lock);
    if (xTaskCreate(poll_worker, "gamma-ethernet", 5120, d, 5, &d->worker) != pdPASS) {
        error = ESP_ERR_NO_MEM; goto failed;
    }
    d->worker_created = true;
    return ESP_OK;
failed:
    set_error(error);
    (void)release_resources();
    return error;
}

esp_err_t gamma_ethernet_stop(void)
{
    ethernet_driver_t *d = &driver;
    portENTER_CRITICAL(&state_lock);
    d->stop_requested = true;
    state.started = false;
    portEXIT_CRITICAL(&state_lock);
    if (d->worker_created) {
        /* Polling wakes within 20ms; do not notify a task handle that could be
         * concurrently retiring after a fatal hardware observation. */
        if (xSemaphoreTake(d->worker_done, ticks(1000)) != pdTRUE) { set_error(ESP_ERR_TIMEOUT); return ESP_ERR_TIMEOUT; }
        d->worker_created = false;
    }
    if (!d->spi && !d->bus_owned && !d->base.netif && !d->dma_tx && !d->dma_rx) return ESP_OK;
    esp_err_t error = release_resources();
    if (error != ESP_OK) set_error(error);
    return error;
}

void gamma_ethernet_get_status(gamma_ethernet_status_t *status)
{
    if (!status) return;
    portENTER_CRITICAL(&state_lock);
    *status = state;
    portEXIT_CRITICAL(&state_lock);
}

esp_netif_t *gamma_ethernet_netif(void)
{
    portENTER_CRITICAL(&state_lock);
    esp_netif_t *netif = public_netif;
    portEXIT_CRITICAL(&state_lock);
    return netif;
}
