#ifndef FIVE_POOL_SCHEDULE_STUBS_H
#define FIVE_POOL_SCHEDULE_STUBS_H
// Reuse SDK declarations, replacing its intentionally tiny settings-only state.
#define SystemModule SettingsTestSystemModule
#define GlobalState SettingsTestGlobalState
#include "schedule_runtime_stubs.h"
#undef SystemModule
#undef GlobalState
#include <time.h>
typedef enum { STRATUM_PROTOCOL_UNKNOWN, STRATUM_PROTOCOL_V1, STRATUM_PROTOCOL_V2 } stratum_protocol_t;
typedef unsigned tls_mode;
typedef void *esp_transport_handle_t;
typedef unsigned TickType_t;
#define pdMS_TO_TICKS(ms) (ms)
#define ESP_LOGD ESP_LOGI
#define portTICK_PERIOD_MS 1
#define taskENTER_CRITICAL(lock) ((void)(lock))
#define taskEXIT_CRITICAL(lock) ((void)(lock))
typedef struct { unsigned marker; } sv2_conn_t;
typedef struct { unsigned count; char message[64]; } PoolTestRejectedReason;
typedef struct {
    char *pool_url, *pool_user, *pool_pass, *pool_cert;
    char *fallback_pool_url, *fallback_pool_user, *fallback_pool_pass, *fallback_pool_cert;
    uint16_t pool_port, pool_tls, pool_difficulty, fallback_pool_port, fallback_pool_tls, fallback_pool_protocol;
    bool pool_extranonce_subscribe, pool_decode_coinbase_tx;
    bool fallback_pool_decode_coinbase_tx;
    bool mining_paused, mining_runtime_ready, is_connected, hardware_fault, pools_unavailable, overheat_mode;
    bool is_using_fallback, use_fallback_stratum;
    uint64_t shares_accepted, shares_rejected, work_received;
    unsigned rejected_reason_stats_count;
    PoolTestRejectedReason rejected_reason_stats[10];
} SystemModule;
typedef struct {
    DeviceConfig DEVICE_CONFIG;
    SystemModule SYSTEM_MODULE;
    stratum_protocol_t stratum_protocol;
    void *transport;
    void *sv2_noise_ctx;
    sv2_conn_t *sv2_conn;
    int stratum_mux;
    int stratum_queue;
    bool ASIC_initalized;
    unsigned configured_voltage, configured_frequency, configured_fan;
} GlobalState;
#define SYSTEM_H_
void SYSTEM_clean_jobs_queue(GlobalState *);
void vTaskDelay(unsigned);
void vTaskDelete(void *);
void queue_clear(void *);
int64_t esp_timer_get_time(void);
int esp_transport_close(esp_transport_handle_t);
int esp_transport_destroy(esp_transport_handle_t);
int esp_transport_connect(esp_transport_handle_t, const char *, int, int);
int esp_transport_read(esp_transport_handle_t, char *, int, int);
esp_transport_handle_t esp_transport_tcp_init(void);
esp_transport_handle_t STRATUM_V1_transport_init(tls_mode, char *);
int STRATUM_V1_subscribe(esp_transport_handle_t, int, const char *);
int STRATUM_V1_authorize(esp_transport_handle_t, int, const char *, const char *);
void sv2_noise_destroy(void *);
bool wifi_is_connected(void);
esp_err_t httpd_resp_send_500(httpd_req_t *);
esp_err_t httpd_resp_set_status(httpd_req_t *, const char *);
#endif
