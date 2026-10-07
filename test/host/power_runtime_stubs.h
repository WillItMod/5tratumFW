#ifndef POWER_RUNTIME_STUBS_H
#define POWER_RUNTIME_STUBS_H
#define GlobalState SettingsOnlyGlobalState
#include "sdk_stubs.h"
#undef GlobalState
#include "tasks/power_management_task.h"
typedef struct {
    DeviceConfig DEVICE_CONFIG;
    SystemModule SYSTEM_MODULE;
    PowerManagementModule POWER_MANAGEMENT_MODULE;
    struct { bool is_active, is_finished; } SELF_TEST_MODULE;
    bool ASIC_initalized;
} GlobalState;
#define portTICK_PERIOD_MS 1
#define UART_NUM_1 1
typedef enum { ASIC_INIT_COLD_BOOT, ASIC_INIT_RECOVERY } asic_init_mode_t;
void vTaskDelay(unsigned);
void vTaskDelete(void *);
esp_err_t VCORE_set_voltage(GlobalState *, float);
int16_t VCORE_get_voltage_mv(GlobalState *);
esp_err_t VCORE_check_fault(GlobalState *);
esp_err_t asic_hold_reset_low(void);
uint8_t asic_initialize(GlobalState *, asic_init_mode_t, uint32_t);
void ASIC_set_frequency(GlobalState *);
void ASIC_set_nonce_space(GlobalState *);
bool SERIAL_is_initialized(void);
esp_err_t uart_flush(int);
void suffixString(uint64_t, char *, size_t, int);
void Power_get_output(GlobalState *, float *, float *);
float Power_get_input_voltage(GlobalState *);
float Power_get_vreg_temp(GlobalState *);
float Thermal_get_chip_temp(GlobalState *);
float Thermal_get_chip_temp2(GlobalState *);
void mining_schedule_update(void);
bool mining_schedule_applied_paused(void);
void mining_schedule_report_power(bool, const char *);
#endif
