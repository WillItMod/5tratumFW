/* Minimal public-API doubles for the original adapter's host tests. GPL-3.0-or-later. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

typedef int esp_err_t;
enum { ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_NO_MEM = 0x101, ESP_ERR_INVALID_ARG,
       ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_SIZE, ESP_ERR_NOT_FOUND, ESP_ERR_TIMEOUT };
typedef struct { uint32_t addr; } esp_ip4_addr_t;
typedef struct { esp_ip4_addr_t ip, netmask, gw; } esp_netif_ip_info_t;
typedef struct test_netif esp_netif_t;
typedef void *esp_netif_iodriver_handle;
typedef struct { esp_err_t (*post_attach)(esp_netif_t *, esp_netif_iodriver_handle); esp_netif_t *netif; } esp_netif_driver_base_t;
typedef struct {
    void *handle;
    esp_err_t (*transmit)(void *, void *, size_t);
    void (*driver_free_rx_buffer)(void *, void *);
} esp_netif_driver_ifconfig_t;
typedef struct { int flags, get_ip_event, lost_ip_event; const char *if_key, *if_desc; int route_prio; } esp_netif_inherent_config_t;
typedef struct { const esp_netif_inherent_config_t *base; const esp_netif_driver_ifconfig_t *driver; const void *stack; } esp_netif_config_t;
#define ESP_NETIF_INHERENT_DEFAULT_ETH() {.flags=1,.get_ip_event=123,.lost_ip_event=124,.if_key="ETH_DEF",.if_desc="eth",.route_prio=50}
#define ESP_NETIF_NETSTACK_DEFAULT_ETH ((const void *)1)
esp_netif_t *esp_netif_new(const esp_netif_config_t *);
void esp_netif_destroy(esp_netif_t *);
esp_err_t esp_netif_set_mac(esp_netif_t *, uint8_t *);
esp_err_t esp_netif_set_hostname(esp_netif_t *, const char *);
esp_err_t esp_netif_attach(esp_netif_t *, esp_netif_iodriver_handle);
esp_err_t esp_netif_set_driver_config(esp_netif_t *, const esp_netif_driver_ifconfig_t *);
void esp_netif_action_start(void *, const void *, int32_t, void *);
void esp_netif_action_stop(void *, const void *, int32_t, void *);
void esp_netif_action_connected(void *, const void *, int32_t, void *);
void esp_netif_action_disconnected(void *, const void *, int32_t, void *);
bool esp_netif_is_netif_up(esp_netif_t *);
esp_err_t esp_netif_get_ip_info(esp_netif_t *, esp_netif_ip_info_t *);
esp_err_t esp_netif_receive(esp_netif_t *, void *, size_t, void *);
enum { ESP_MAC_ETH = 3, MALLOC_CAP_DMA = 1, MALLOC_CAP_INTERNAL = 2, SPI2_HOST = 1, SPI_DMA_CH_AUTO = 3, SPI_DEVICE_NO_DUMMY = 64 };
esp_err_t esp_read_mac(uint8_t *, int);
void *heap_caps_malloc(size_t, unsigned);
int64_t esp_timer_get_time(void);
#define CONFIG_SPI_MASTER_ISR_IN_IRAM 1
#define ESP_INTR_FLAG_IRAM (1 << 10)
typedef struct { int mosi_io_num, miso_io_num, sclk_io_num, quadwp_io_num, quadhd_io_num, max_transfer_sz, intr_flags; } spi_bus_config_t;
typedef struct { int address_bits, mode, clock_speed_hz, spics_io_num, queue_size, flags; } spi_device_interface_config_t;
typedef struct { uint64_t addr; size_t length; const void *tx_buffer; void *rx_buffer; } spi_transaction_t;
typedef void *spi_device_handle_t;
esp_err_t spi_bus_initialize(int, const spi_bus_config_t *, int);
esp_err_t spi_bus_free(int);
esp_err_t spi_bus_add_device(int, const spi_device_interface_config_t *, spi_device_handle_t *);
esp_err_t spi_bus_remove_device(spi_device_handle_t);
esp_err_t spi_device_queue_trans(spi_device_handle_t, spi_transaction_t *, uint32_t);
esp_err_t spi_device_get_trans_result(spi_device_handle_t, spi_transaction_t **, uint32_t);
typedef uint32_t TickType_t;
typedef pthread_mutex_t portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED PTHREAD_MUTEX_INITIALIZER
#define portENTER_CRITICAL(lock) ((void)pthread_mutex_lock(lock))
#define portEXIT_CRITICAL(lock) ((void)pthread_mutex_unlock(lock))
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
typedef struct test_semaphore *SemaphoreHandle_t;
typedef void *TaskHandle_t;
SemaphoreHandle_t xSemaphoreCreateMutex(void);
SemaphoreHandle_t xSemaphoreCreateBinary(void);
int xSemaphoreTake(SemaphoreHandle_t, TickType_t);
int xSemaphoreGive(SemaphoreHandle_t);
int xTaskCreate(void (*)(void *), const char *, unsigned, void *, unsigned, TaskHandle_t *);
void vTaskDelay(TickType_t);
void vTaskDelete(TaskHandle_t);
