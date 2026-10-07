#ifndef FIVE_STRATUM_HOST_SDK_STUBS_H
#define FIVE_STRATUM_HOST_SDK_STUBS_H

// Minimal ESP-IDF/FreeRTOS stand-ins used to compile the real settings code.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_NVS_NOT_FOUND 0x1102
#define ESP_ERR_NVS_TYPE_MISMATCH 0x1103
#define ESP_ERR_NVS_INVALID_LENGTH 0x110c
#define ESP_ERR_NVS_NO_FREE_PAGES 0x110d
#define ESP_ERR_NVS_NEW_VERSION_FOUND 0x1110
const char *esp_err_to_name(esp_err_t err);
#define ESP_LOGI(tag, ...) do { (void)(tag); if (0) printf(__VA_ARGS__); } while (0)
#define ESP_LOGW ESP_LOGI
#define ESP_LOGE ESP_LOGI

typedef unsigned nvs_handle_t;
typedef enum { NVS_READONLY, NVS_READWRITE } nvs_open_mode_t;
#define NVS_KEY_NAME_MAX_SIZE 16
typedef struct {
    unsigned long used_entries, free_entries, available_entries, total_entries;
} nvs_stats_t;
esp_err_t nvs_flash_init(void);
esp_err_t nvs_flash_erase(void);
esp_err_t nvs_open(const char *, nvs_open_mode_t, nvs_handle_t *);
void nvs_close(nvs_handle_t);
esp_err_t nvs_get_stats(const char *, nvs_stats_t *);
esp_err_t nvs_find_key(nvs_handle_t, const char *, void *);
esp_err_t nvs_get_str(nvs_handle_t, const char *, char *, size_t *);
esp_err_t nvs_get_u16(nvs_handle_t, const char *, uint16_t *);
esp_err_t nvs_get_i32(nvs_handle_t, const char *, int32_t *);
esp_err_t nvs_get_u64(nvs_handle_t, const char *, uint64_t *);
esp_err_t nvs_set_str(nvs_handle_t, const char *, const char *);
esp_err_t nvs_set_u16(nvs_handle_t, const char *, uint16_t);
esp_err_t nvs_set_i32(nvs_handle_t, const char *, int32_t);
esp_err_t nvs_set_u64(nvs_handle_t, const char *, uint64_t);
esp_err_t nvs_erase_key(nvs_handle_t, const char *);
esp_err_t nvs_commit(nvs_handle_t);

typedef void *QueueHandle_t;
typedef void *SemaphoreHandle_t;
typedef void *TaskHandle_t;
typedef int BaseType_t;
#define pdPASS 1
#define pdTRUE 1
#define portMAX_DELAY 0xffffffffu
QueueHandle_t xQueueCreate(unsigned, unsigned);
BaseType_t xQueueReceive(QueueHandle_t, void *, unsigned);
BaseType_t xQueueSend(QueueHandle_t, const void *, unsigned);
SemaphoreHandle_t xSemaphoreCreateMutex(void);
BaseType_t xSemaphoreTake(SemaphoreHandle_t, unsigned);
BaseType_t xSemaphoreGive(SemaphoreHandle_t);
BaseType_t xTaskCreate(void (*)(void *), const char *, unsigned, void *, unsigned, TaskHandle_t *);
typedef void *httpd_handle_t;

// Avoid the unrelated networking/mining graph while retaining actual board profiles.
#define GLOBAL_STATE_H_
#include "device_config.h"
typedef struct {
    bool mining_paused, mining_runtime_ready, is_connected, hardware_fault, pools_unavailable, overheat_mode;
    char hardware_fault_msg[64];
} SystemModule;
typedef struct { DeviceConfig DEVICE_CONFIG; SystemModule SYSTEM_MODULE; } GlobalState;
#define STRATUM_V1 "SV1"
#define STRATUM_V2 "SV2"
#define SV2_CHANNEL_TYPE_STANDARD "standard"
#define SV2_CHANNEL_TYPE_EXTENDED "extended"

#define CONFIG_ESP_WIFI_SSID "factory-ssid"
#define CONFIG_ESP_WIFI_PASSWORD "factory-password"
#define CONFIG_LWIP_LOCAL_HOSTNAME "factory-host"
#define CONFIG_STRATUM_URL "factory-pool"
#define CONFIG_STRATUM_PORT 3333
#define CONFIG_STRATUM_USER "factory-user"
#define CONFIG_STRATUM_PW "factory-password"
#define CONFIG_STRATUM_DIFFICULTY 256
#define CONFIG_STRATUM_TLS 0
#define CONFIG_STRATUM_CERT ""
#define CONFIG_FALLBACK_STRATUM_URL "factory-fallback"
#define CONFIG_FALLBACK_STRATUM_PORT 3333
#define CONFIG_FALLBACK_STRATUM_USER "factory-fallback-user"
#define CONFIG_FALLBACK_STRATUM_PW "factory-fallback-password"
#define CONFIG_FALLBACK_STRATUM_DIFFICULTY 256
#define CONFIG_FALLBACK_STRATUM_TLS 0
#define CONFIG_FALLBACK_STRATUM_CERT ""
#define CONFIG_ASIC_FREQUENCY 525
#define CONFIG_ASIC_VOLTAGE 1150

#endif
