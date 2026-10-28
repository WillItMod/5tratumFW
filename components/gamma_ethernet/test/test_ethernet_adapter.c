/* Host exercises production adapter lifecycle, SPI framing and stack reentry.
 * All peripheral/netif/RTOS functions below are local test doubles. */
#include "gamma_eth_platform.h"
#include "gamma_ethernet.h"
#include <stdlib.h>
#include <stdatomic.h>
#include <time.h>
#include <errno.h>

#define main unused_protocol_selftest
#include "test_w5500_core.c"
#undef main

struct test_semaphore { pthread_mutex_t mutex; pthread_cond_t condition; unsigned count; };
struct test_netif { esp_netif_driver_ifconfig_t driver; bool up; esp_netif_ip_info_t ip; };
static simulated_chip_t hardware;
static struct test_netif *netif;
static SemaphoreHandle_t bus_lock;
static spi_transaction_t *queued;
static pthread_t worker_thread;
static void (*worker_fn)(void *);
static void *worker_context;
static atomic_uint bus_initializes, bus_frees, device_removes, netif_frees, connects, disconnects, inputs, reentries;
static atomic_bool bus_conflict, fail_attach, fail_task, fail_allocation, stall_result, input_failure, stall_on_close;

static struct timespec deadline(unsigned ms)
{
    struct timespec value;
    assert(clock_gettime(CLOCK_REALTIME, &value) == 0);
    value.tv_sec += ms / 1000;
    value.tv_nsec += (long)(ms % 1000) * 1000000;
    if (value.tv_nsec >= 1000000000) { ++value.tv_sec; value.tv_nsec -= 1000000000; }
    return value;
}

SemaphoreHandle_t xSemaphoreCreateBinary(void)
{
    struct test_semaphore *s = calloc(1, sizeof(*s));
    assert(s && pthread_mutex_init(&s->mutex, NULL) == 0 && pthread_cond_init(&s->condition, NULL) == 0);
    return s;
}
SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    SemaphoreHandle_t s = xSemaphoreCreateBinary(); s->count = 1;
    if (!bus_lock) bus_lock = s;
    return s;
}
int xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait)
{
    struct timespec until = deadline(wait);
    pthread_mutex_lock(&s->mutex);
    while (!s->count) {
        if (pthread_cond_timedwait(&s->condition, &s->mutex, &until) == ETIMEDOUT) {
            pthread_mutex_unlock(&s->mutex); return pdFALSE;
        }
    }
    --s->count;
    pthread_mutex_unlock(&s->mutex); return pdTRUE;
}
int xSemaphoreGive(SemaphoreHandle_t s)
{
    pthread_mutex_lock(&s->mutex); ++s->count; pthread_cond_signal(&s->condition); pthread_mutex_unlock(&s->mutex);
    return pdTRUE;
}
void vTaskDelay(TickType_t wait)
{
    struct timespec delay = {(time_t)(wait / 1000), (long)(wait % 1000) * 1000000};
    nanosleep(&delay, NULL);
}
void vTaskDelete(TaskHandle_t task) { assert(task == NULL); pthread_exit(NULL); }
static void *run_worker(void *unused) { (void)unused; worker_fn(worker_context); return NULL; }
int xTaskCreate(void (*fn)(void *), const char *name, unsigned stack, void *context, unsigned priority, TaskHandle_t *task)
{
    assert(name && stack >= 4096 && priority > 0);
    if (fail_task) return 0;
    worker_fn = fn; worker_context = context; *task = (void *)1;
    assert(pthread_create(&worker_thread, NULL, run_worker, NULL) == 0); return pdPASS;
}
void *heap_caps_malloc(size_t size, unsigned caps)
{
    assert(caps == (MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    if (fail_allocation) return NULL;
    return malloc(size);
}
int64_t esp_timer_get_time(void)
{
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000;
}
esp_err_t esp_read_mac(uint8_t *mac, int kind)
{
    const uint8_t own[] = {2, 0x12, 0x34, 0x56, 0x78, 0x9a};
    assert(kind == ESP_MAC_ETH); memcpy(mac, own, 6); return ESP_OK;
}
esp_err_t spi_bus_initialize(int host, const spi_bus_config_t *config, int dma)
{
    assert(host == SPI2_HOST && dma == SPI_DMA_CH_AUTO);
    assert(config->mosi_io_num == 40 && config->miso_io_num == 39 && config->sclk_io_num == 41);
    assert(config->quadwp_io_num == -1 && config->quadhd_io_num == -1 && config->max_transfer_sz >= 1518);
    assert(config->intr_flags == ESP_INTR_FLAG_IRAM);
    ++bus_initializes; return bus_conflict ? ESP_ERR_INVALID_STATE : ESP_OK;
}
esp_err_t spi_bus_free(int host) { assert(host == SPI2_HOST); ++bus_frees; return ESP_OK; }
esp_err_t spi_bus_add_device(int host, const spi_device_interface_config_t *config, spi_device_handle_t *out)
{
    assert(host == SPI2_HOST && config->address_bits == 24 && config->mode == 0);
    assert(config->clock_speed_hz == 20000000 && config->spics_io_num == 42 && config->queue_size == 1);
    *out = (void *)1; return ESP_OK;
}
esp_err_t spi_bus_remove_device(spi_device_handle_t handle)
{
    assert(handle && queued == NULL); ++device_removes; return ESP_OK;
}
esp_err_t spi_device_queue_trans(spi_device_handle_t handle, spi_transaction_t *t, uint32_t wait)
{
    assert(handle && t && !queued && wait > 0 && wait <= 25);
    assert((t->addr & 3) == 0 && t->length % 8 == 0);
    const bool write = (t->addr & 4) != 0;
    assert((write && t->tx_buffer && !t->rx_buffer) || (!write && !t->tx_buffer && t->rx_buffer));
    int result = transfer(&hardware, (uint8_t)((t->addr >> 3) & 31), (uint16_t)(t->addr >> 8), write,
                          write ? (void *)t->tx_buffer : t->rx_buffer, t->length / 8);
    if (result != 0) return ESP_FAIL;
    if (stall_on_close && write && ((t->addr >> 3) & 31) == 1 && (t->addr >> 8) == 1
        && *(const uint8_t *)t->tx_buffer == 0x10) stall_result = true;
    queued = t; return ESP_OK;
}
esp_err_t spi_device_get_trans_result(spi_device_handle_t handle, spi_transaction_t **result, uint32_t wait)
{
    assert(handle && wait > 0 && wait <= 100);
    if (stall_result) return ESP_ERR_TIMEOUT;
    assert(queued); *result = queued; queued = NULL; return ESP_OK;
}
esp_netif_t *esp_netif_new(const esp_netif_config_t *config)
{
    assert(config->base->route_prio == 100 && config->base->get_ip_event == 123 && config->base->lost_ip_event == 124);
    assert(strcmp(config->base->if_key, "GAMMA_W5500") == 0 && config->stack == ESP_NETIF_NETSTACK_DEFAULT_ETH);
    netif = calloc(1, sizeof(*netif)); assert(netif); return netif;
}
void esp_netif_destroy(esp_netif_t *n) { assert(n == netif); ++netif_frees; free(n); netif = NULL; }
esp_err_t esp_netif_set_mac(esp_netif_t *n, uint8_t *mac) { assert(n && mac[0] == 2); return ESP_OK; }
esp_err_t esp_netif_set_hostname(esp_netif_t *n, const char *hostname) { assert(n && hostname[0]); return ESP_OK; }
esp_err_t esp_netif_attach(esp_netif_t *n, esp_netif_iodriver_handle driver_handle)
{
    return fail_attach ? ESP_FAIL : ((esp_netif_driver_base_t *)driver_handle)->post_attach(n, driver_handle);
}
esp_err_t esp_netif_set_driver_config(esp_netif_t *n, const esp_netif_driver_ifconfig_t *config)
{
    assert(n && config->transmit && config->driver_free_rx_buffer); n->driver = *config; return ESP_OK;
}
void esp_netif_action_start(void *n, const void *base, int32_t id, void *data) { (void)base; (void)id; (void)data; assert(n); }
void esp_netif_action_stop(void *n, const void *base, int32_t id, void *data) { (void)base; (void)id; (void)data; assert(n); }
static void reenter_transmit(esp_netif_t *n)
{
    uint8_t arp[42] = {0};
    assert(n->driver.transmit(n->driver.handle, arp, sizeof(arp)) == ESP_OK);
    ++reentries;
}
void esp_netif_action_connected(void *value, const void *base, int32_t id, void *data)
{
    (void)base; (void)id; (void)data; esp_netif_t *n = value; ++connects;
    n->up = true; n->ip.ip.addr = 0x010200c0; reenter_transmit(n); /* Simulated DHCP/ARP output. */
}
void esp_netif_action_disconnected(void *value, const void *base, int32_t id, void *data)
{
    (void)base; (void)id; (void)data; esp_netif_t *n = value; ++disconnects;
    n->up = false; n->ip.ip.addr = 0;
}
bool esp_netif_is_netif_up(esp_netif_t *n) { return n->up; }
esp_err_t esp_netif_get_ip_info(esp_netif_t *n, esp_netif_ip_info_t *ip) { *ip = n->ip; return ESP_OK; }
esp_err_t esp_netif_receive(esp_netif_t *n, void *frame, size_t length, void *eb)
{
    assert(frame && length == 60 && eb == NULL); ++inputs;
    reenter_transmit(n);
    n->driver.driver_free_rx_buffer(n->driver.handle, frame); /* Also on simulated stack-input error. */
    return input_failure ? ESP_FAIL : ESP_OK;
}
static void fresh_hardware(void)
{
    memset(&hardware, 0, sizeof(hardware)); hardware.common[0x39] = 4; hardware.common[0x2e] = 0xb1;
}
static void await_state(bool available, bool link, bool ip)
{
    for (unsigned n = 0; n < 1000; ++n) {
        gamma_ethernet_status_t value; gamma_ethernet_get_status(&value);
        if (value.available == available && value.link_up == link && value.has_ip == ip) return;
        vTaskDelay(1);
    }
    assert(!"Ethernet status did not converge");
}

int main(void)
{
    gamma_ethernet_status_t status;
    gamma_ethernet_get_status(&status);
    assert(!status.started && !status.available && !status.has_ip && bus_initializes == 0 && !gamma_ethernet_netif());
    assert(gamma_ethernet_stop() == ESP_OK && bus_initializes == 0);
    assert(gamma_ethernet_start("") == ESP_ERR_INVALID_ARG && bus_initializes == 0);
    fail_allocation = true;
    assert(gamma_ethernet_start("miner") == ESP_ERR_NO_MEM && bus_initializes == 0);
    fail_allocation = false;
    fresh_hardware(); hardware.missing = true;
    assert(gamma_ethernet_start("miner") == ESP_ERR_NOT_FOUND && bus_frees == 1 && device_removes == 1);
    gamma_ethernet_get_status(&status);
    assert(!status.available && status.last_error == ESP_ERR_NOT_FOUND && !gamma_ethernet_netif());
    fresh_hardware(); bus_conflict = true;
    unsigned freed_before = bus_frees;
    assert(gamma_ethernet_start("miner") == ESP_ERR_INVALID_STATE && bus_frees == freed_before);
    bus_conflict = false; fail_attach = true;
    assert(gamma_ethernet_start("miner") == ESP_FAIL && netif_frees == 1 && !netif);
    fail_attach = false; fail_task = true;
    assert(gamma_ethernet_start("miner") == ESP_ERR_NO_MEM && netif_frees == 2 && !netif);
    fail_task = false;
    assert(gamma_ethernet_start("miner") == ESP_OK);
    await_state(true, true, true);
    assert(gamma_ethernet_netif() == netif && connects == 1 && reentries == 1);
    uint8_t packet[60] = {0};
    assert(xSemaphoreTake(bus_lock, 1000) == pdTRUE);
    add_packet(&hardware, 0x3fff, packet, sizeof(packet));
    xSemaphoreGive(bus_lock);
    for (unsigned n = 0; n < 1000 && inputs < 1; ++n) vTaskDelay(1);
    assert(inputs == 1 && reentries == 2);
    input_failure = true;
    assert(xSemaphoreTake(bus_lock, 1000) == pdTRUE);
    add_packet(&hardware, 1234, packet, sizeof(packet));
    xSemaphoreGive(bus_lock);
    for (unsigned n = 0; n < 1000 && inputs < 2; ++n) vTaskDelay(1);
    assert(inputs == 2); /* Sanitizers catch double-free in either handoff path. */
    input_failure = false;
    assert(xSemaphoreTake(bus_lock, 1000) == pdTRUE); hardware.common[0x2e] &= 0xfe; xSemaphoreGive(bus_lock);
    await_state(true, false, false);
    assert(xSemaphoreTake(bus_lock, 1000) == pdTRUE); hardware.common[0x2e] |= 1; xSemaphoreGive(bus_lock);
    await_state(true, true, true);
    assert(connects == 2);
    assert(gamma_ethernet_stop() == ESP_OK);
    assert(pthread_join(worker_thread, NULL) == 0);
    await_state(false, false, false);
    assert(!gamma_ethernet_netif() && !queued && netif_frees == 3);
    fresh_hardware();
    assert(gamma_ethernet_start("miner") == ESP_OK);
    await_state(true, true, true);
    unsigned close_removed_before = device_removes;
    stall_on_close = true;
    assert(gamma_ethernet_stop() == ESP_ERR_TIMEOUT);
    assert(pthread_join(worker_thread, NULL) == 0);
    assert(queued && device_removes == close_removed_before && !gamma_ethernet_netif());
    stall_on_close = stall_result = false;
    assert(gamma_ethernet_stop() == ESP_OK && !queued && device_removes == close_removed_before + 1);
    /* A timeout may leave DMA pending. Descriptor/storage survive; no device
     * is removed or DMA memory freed until a later bounded drain succeeds. */
    fresh_hardware(); stall_result = true;
    unsigned removed_before = device_removes;
    assert(gamma_ethernet_start("miner") == ESP_FAIL && queued && device_removes == removed_before);
    assert(gamma_ethernet_start("miner") == ESP_ERR_INVALID_STATE);
    stall_result = false;
    assert(gamma_ethernet_stop() == ESP_OK && !queued && device_removes == removed_before + 1);
    puts("Original Ethernet adapter: off/no GPIO, pin/SPI contract, startup cleanup, DMA timeout quarantine, link/IP loss/reconnect, single RX ownership and stack TX reentry passed");
    return 0;
}
