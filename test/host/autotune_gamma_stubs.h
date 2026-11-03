#ifndef AUTOTUNE_GAMMA_STUBS_H
#define AUTOTUNE_GAMMA_STUBS_H
#define GlobalState SettingsOnlyGlobalState
#define SystemModule SettingsOnlySystemModule
#define device_config_init settings_only_device_config_init
#include "sdk_stubs.h"
#undef device_config_init
#undef GlobalState
#undef SystemModule
#include "tasks/power_management_task.h"
typedef struct SystemModule {
    bool mining_paused, mining_runtime_ready, is_connected, hardware_fault, pools_unavailable, tuning_recovery_work_blocked;
    bool overheat_mode, is_firmware_update;
    char hardware_fault_msg[64];
} SystemModule;
struct GlobalState {
    DeviceConfig DEVICE_CONFIG;
    SystemModule SYSTEM_MODULE;
    PowerManagementModule POWER_MANAGEMENT_MODULE;
    bool ASIC_initalized;
    struct {bool is_active;} SELF_TEST_MODULE;
};
esp_err_t device_config_init(GlobalState *);
#include "profiles_runtime_stubs.h"
#define UART_NUM_1 1
#define pdMS_TO_TICKS(ms) (ms)
void vTaskDelay(unsigned);
int64_t esp_timer_get_time(void);
uint32_t SERIAL_tx_failure_count(void);
esp_err_t uart_wait_tx_done(int,unsigned);
#endif
