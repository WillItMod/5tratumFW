#ifndef FIVE_TRATUM_OLED_SCREEN_RUNTIME_STUBS_H
#define FIVE_TRATUM_OLED_SCREEN_RUNTIME_STUBS_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include "lvgl.h"
#include "5tratumfw.h"
#include "oled_display_data.h"
#include "tasks/hashrate_display.h"
typedef int esp_err_t;
#define ESP_OK 0
typedef struct {
    float fan_perc, voltage, core_voltage, power, chip_temp_avg, chip_temp2_avg, vr_temp;
    uint16_t fan_rpm;
} PowerManagementModule;
typedef struct {
    const char *message, *result, *finished;
    bool is_active, is_finished;
} SelfTestModule;
typedef struct {
    char *ssid, *pool_url, *fallback_pool_url;
    char ap_ssid[12], wifi_status[256], ip_addr_str[16], hardware_fault_msg[64];
    char best_session_diff_string[10], best_diff_string[10];
    char firmware_update_filename[20], firmware_update_status[20];
    const char *asic_status;
    float current_hashrate;
    uint64_t shares_accepted, shares_rejected, work_received, best_session_nonce_diff;
    int64_t start_time;
    int identify_mode_time_ms;
    bool is_connected, ap_enabled, mining_paused, mining_runtime_ready, hardware_fault, overheat_mode;
    bool pools_unavailable, is_screen_active, is_firmware_update, is_using_fallback, show_new_block;
    bool pool_decode_coinbase_tx, fallback_pool_decode_coinbase_tx;
    unsigned pool_tls, fallback_pool_tls;
} SystemModule;
enum {STRATUM_PROTOCOL_V1=1, STRATUM_PROTOCOL_V2=2};
typedef struct {
    SystemModule SYSTEM_MODULE;
    PowerManagementModule POWER_MANAGEMENT_MODULE;
    SelfTestModule SELF_TEST_MODULE;
    struct {struct {const char *name;} family; const char *board_version;} DEVICE_CONFIG;
    bool ASIC_initalized;
    int stratum_protocol, block_height;
    char network_diff_string[10], scriptsig[128];
} GlobalState;
enum {NVS_CONFIG_DISPLAY_TIMEOUT, NVS_CONFIG_ASIC_FREQUENCY, NVS_CONFIG_ASIC_VOLTAGE};
typedef struct {bool connected, acknowledged, expired, fallback;} FiveTratumMuxSnapshot;
extern int64_t host_now;
extern bool host_applied, host_display_on;
extern int host_lock_depth;
extern int host_display_timeout;
extern FiveTratumMuxSnapshot host_mux;
extern float host_rate_gh;
extern uint64_t host_rate_sample_us;
static inline bool lvgl_port_lock(uint32_t wait) { (void)wait; ++host_lock_depth; return true; }
static inline void lvgl_port_unlock(void) { --host_lock_depth; }
static inline esp_err_t display_on(bool on) { host_display_on=on; return ESP_OK; }
static inline int64_t esp_timer_get_time(void) { return host_now; }
static inline int32_t nvs_config_get_i32(int key) { (void)key; return host_display_timeout; }
static inline uint16_t nvs_config_get_u16(int key) { (void)key; return 1130; }
static inline float nvs_config_get_float(int key) { (void)key; return 600; }
static inline void get_wifi_current_rssi(int8_t *rssi) { *rssi=-58; }
static inline bool mining_schedule_applied_paused(void) { return host_applied; }
static inline FiveTratumMuxSnapshot mux_peer_status_snapshot(void) { return host_mux; }
static inline HashrateDisplaySnapshot hashrate_monitor_display_snapshot(void *state, uint64_t now_us)
{
    (void)state;
    return hashrate_display_sample(host_rate_gh, host_rate_sample_us, now_us);
}
#define GLOBAL_STATE_H_
#include "mining_state.h"
static inline void SYSTEM_copy_pool_identity(GlobalState *state, bool fallback, char *host, size_t host_size,
                                            char *user, size_t user_size, uint16_t *port)
{
    if (host && host_size) snprintf(host, host_size, "%s", fallback ? state->SYSTEM_MODULE.fallback_pool_url : state->SYSTEM_MODULE.pool_url);
    if (user && user_size) user[0] = 0;
    if (port) *port = 3333;
}
static inline void SYSTEM_copy_pool_options(GlobalState *state, bool fallback, uint16_t *tls, bool *decode)
{
    if (tls) *tls = fallback ? state->SYSTEM_MODULE.fallback_pool_tls : state->SYSTEM_MODULE.pool_tls;
    if (decode) *decode = fallback ? state->SYSTEM_MODULE.fallback_pool_decode_coinbase_tx : state->SYSTEM_MODULE.pool_decode_coinbase_tx;
}
#endif
