#include "power_runtime_stubs.h"
#include "nvs_config.h"
#include <assert.h>
#include <math.h>
#include <setjmp.h>
#include <string.h>

// Include the real implementation to exercise its private transition helpers.
#include "tasks/power_management_task.c"

static GlobalState global;
static uint16_t saved_voltage = 1173;
static float saved_frequency = 625.125f;
static unsigned setting_writes, voltage_calls, reset_calls, frequency_calls, nonce_calls, init_calls, flush_calls;
static float last_voltage;
static esp_err_t restore_result = ESP_OK, off_result = ESP_OK, reset_result = ESP_OK;
static unsigned off_failures_remaining;
static uint8_t detected_chips = 1;
static bool serial_ready, applied, pause_on_cooling, overheat_enabled;
static char reported_error[96];
static unsigned delay_count, exit_delay, cooling_count;
static unsigned pause_at_delay;
static jmp_buf task_exit;

const char *esp_err_to_name(esp_err_t value) { (void)value; return "host error"; }
uint16_t nvs_config_get_u16(NvsConfigKey key) { return key == NVS_CONFIG_ASIC_VOLTAGE ? saved_voltage : 100; }
float nvs_config_get_float(NvsConfigKey key) { assert(key == NVS_CONFIG_ASIC_FREQUENCY); return saved_frequency; }
bool nvs_config_get_bool(NvsConfigKey key) { (void)key; return false; }
void nvs_config_set_u16(NvsConfigKey key, uint16_t value) { setting_writes++; if (key == NVS_CONFIG_ASIC_VOLTAGE) saved_voltage = value; }
void nvs_config_set_float(NvsConfigKey key, float value) { assert(key == NVS_CONFIG_ASIC_FREQUENCY); setting_writes++; saved_frequency = value; }
void nvs_config_set_bool(NvsConfigKey key, bool value) { (void)key; (void)value; setting_writes++; }
esp_err_t VCORE_set_voltage(GlobalState *state, float voltage) {
    (void)state; voltage_calls++; last_voltage = voltage;
    if (voltage == 0 && off_failures_remaining) { off_failures_remaining--; return ESP_FAIL; }
    return voltage == 0 ? off_result : restore_result;
}
int16_t VCORE_get_voltage_mv(GlobalState *state) { (void)state; return 1173; }
esp_err_t VCORE_check_fault(GlobalState *state) { (void)state; return ESP_OK; }
esp_err_t asic_hold_reset_low(void) { reset_calls++; return reset_result; }
bool SERIAL_is_initialized(void) { return serial_ready; }
esp_err_t uart_flush(int uart) { assert(serial_ready && uart == UART_NUM_1); flush_calls++; return ESP_OK; }
void ASIC_set_frequency(GlobalState *state) { assert(state->ASIC_initalized); assert(state->POWER_MANAGEMENT_MODULE.frequency_value == 50); frequency_calls++; }
void ASIC_set_nonce_space(GlobalState *state) { assert(state->ASIC_initalized); nonce_calls++; }
uint8_t asic_initialize(GlobalState *state, asic_init_mode_t mode, uint32_t wait) {
    assert(mode == ASIC_INIT_RECOVERY && wait == 0);
    assert(fabsf(last_voltage - saved_voltage / 1000.0f) < 0.00001f);
    assert(state->POWER_MANAGEMENT_MODULE.frequency_value == saved_frequency);
    init_calls++; state->ASIC_initalized = detected_chips > 0;
    return detected_chips;
}
void suffixString(uint64_t value, char *buffer, size_t size, int digits) { (void)value; (void)digits; snprintf(buffer, size, "host"); }
void Power_get_output(GlobalState *state, float *power, float *current) { (void)state; *power = 15; *current = 3; }
float Power_get_input_voltage(GlobalState *state) { (void)state; return 5000; }
float Power_get_vreg_temp(GlobalState *state) { (void)state; return overheat_enabled && !cooling_count ? 110 : 35; }
float Thermal_get_chip_temp(GlobalState *state) { (void)state; return 35; }
float Thermal_get_chip_temp2(GlobalState *state) { (void)state; return -1; }
void mining_schedule_update(void) {}
bool mining_schedule_applied_paused(void) { return applied; }
void mining_schedule_report_power(bool paused, const char *error) { applied = paused; snprintf(reported_error, sizeof(reported_error), "%s", error ? error : ""); }
void vTaskDelay(unsigned ticks) {
    if (ticks == 5000) { cooling_count++; if (pause_on_cooling) global.SYSTEM_MODULE.mining_paused = true; }
    delay_count++;
    if (pause_at_delay == delay_count) global.SYSTEM_MODULE.mining_paused = true;
    if (exit_delay && delay_count >= exit_delay) longjmp(task_exit, 1);
}
void vTaskDelete(void *task) { (void)task; longjmp(task_exit, 1); }

int main(int argc, char **argv) {
    assert(argc == 2);
    global.SYSTEM_MODULE.mining_runtime_ready = true;
    global.DEVICE_CONFIG.family.asic.small_core_count = 2040;
    global.DEVICE_CONFIG.family.asic_count = 1;
    global.ASIC_initalized = true;
    global.POWER_MANAGEMENT_MODULE.frequency_value = saved_frequency;
    global.POWER_MANAGEMENT_MODULE.expected_hashrate = 1275;
    const char *scenario = argv[1];
    if (strcmp(scenario, "stop") == 0) {
        serial_ready = true;
        assert(mining_stop(&global) == ESP_OK && applied && !global.ASIC_initalized);
        assert(last_voltage == 0 && reset_calls == 1 && frequency_calls == 1 && nonce_calls == 1 && flush_calls == 1);
        assert(global.POWER_MANAGEMENT_MODULE.frequency_value == 50 && global.POWER_MANAGEMENT_MODULE.expected_hashrate == 0);
        assert(saved_voltage == 1173 && saved_frequency == 625.125f && setting_writes == 0);
    } else if (strcmp(scenario, "stop-fail") == 0 || strcmp(scenario, "reset-fail") == 0) {
        if (strcmp(scenario, "stop-fail") == 0) off_result = ESP_FAIL; else reset_result = ESP_FAIL;
        assert(mining_stop(&global) == ESP_FAIL && !applied && !global.ASIC_initalized && reported_error[0]);
        assert(voltage_calls == 1 && reset_calls == 1 && setting_writes == 0);
    } else if (strcmp(scenario, "start") == 0) {
        global.ASIC_initalized = false; applied = true;
        assert(mining_start(&global) == 1 && !applied && global.ASIC_initalized);
        assert(init_calls == 1 && flush_calls == 0 && global.POWER_MANAGEMENT_MODULE.frequency_value == saved_frequency);
        assert(saved_voltage == 1173 && saved_frequency == 625.125f && setting_writes == 0);
    } else if (strcmp(scenario, "restore-fail") == 0 || strcmp(scenario, "init-fail") == 0) {
        global.ASIC_initalized = false;
        if (strcmp(scenario, "restore-fail") == 0) restore_result = ESP_FAIL; else detected_chips = 0;
        assert(mining_start(&global) == 0 && global.SYSTEM_MODULE.hardware_fault && applied && !global.ASIC_initalized);
        assert(last_voltage == 0 && reset_calls == 1 && init_calls == (strcmp(scenario, "init-fail") == 0));
        assert(global.SYSTEM_MODULE.hardware_fault_msg[0] && setting_writes == 0);
    } else if (strcmp(scenario, "startup-not-ready") == 0) {
        global.ASIC_initalized = false; global.SYSTEM_MODULE.mining_runtime_ready = false; exit_delay = 4;
        if (setjmp(task_exit) == 0) POWER_MANAGEMENT_task(&global);
        assert(applied && last_voltage == 0 && reset_calls == 1 && init_calls == 0 && setting_writes == 0);
    } else if (strcmp(scenario, "startup-resume") == 0) {
        global.ASIC_initalized = false; applied = true; exit_delay = 4;
        if (setjmp(task_exit) == 0) POWER_MANAGEMENT_task(&global);
        assert(!applied && global.ASIC_initalized && init_calls == 1 && setting_writes == 0);
    } else if (strcmp(scenario, "pause-before-init") == 0 || strcmp(scenario, "pause-settling") == 0) {
        global.ASIC_initalized = false; applied = true;
        pause_at_delay = strcmp(scenario, "pause-before-init") == 0 ? 1 : 3;
        assert(mining_start(&global) == 0);
        assert(global.SYSTEM_MODULE.mining_paused && applied && !global.ASIC_initalized && !mining_state_work_allowed(&global));
        assert(init_calls == (pause_at_delay == 3) && last_voltage == 0 && setting_writes == 0);
    } else if (strcmp(scenario, "retry-cutoff") == 0) {
        global.ASIC_initalized = false; applied = true;
        restore_result = ESP_FAIL; off_failures_remaining = 1; exit_delay = 7;
        if (setjmp(task_exit) == 0) POWER_MANAGEMENT_task(&global);
        assert(global.SYSTEM_MODULE.hardware_fault && applied && !global.ASIC_initalized);
        assert(voltage_calls == 3 && reset_calls == 2 && init_calls == 0 && setting_writes == 0);
    } else if (strcmp(scenario, "worker-gates") == 0) {
        assert(mining_state_work_allowed(&global));
        global.SYSTEM_MODULE.mining_paused = true; assert(!mining_state_work_allowed(&global)); global.SYSTEM_MODULE.mining_paused = false;
        global.SYSTEM_MODULE.hardware_fault = true; assert(!mining_state_work_allowed(&global)); global.SYSTEM_MODULE.hardware_fault = false;
        global.SYSTEM_MODULE.pools_unavailable = true; assert(!mining_state_work_allowed(&global)); global.SYSTEM_MODULE.pools_unavailable = false;
        global.SYSTEM_MODULE.mining_runtime_ready = false; assert(!mining_state_work_allowed(&global)); global.SYSTEM_MODULE.mining_runtime_ready = true;
        global.ASIC_initalized = false; assert(!mining_state_work_allowed(&global));
    } else if (strcmp(scenario, "cooldown-pause") == 0) {
        overheat_enabled = true; pause_on_cooling = true; exit_delay = 10;
        if (setjmp(task_exit) == 0) POWER_MANAGEMENT_task(&global);
        assert(cooling_count >= 6 && global.SYSTEM_MODULE.mining_paused && applied && !global.ASIC_initalized);
        assert(init_calls == 0 && last_voltage == 0);
        // Existing thermal protection keeps its protective settings changes.
        assert(setting_writes == 5 && saved_voltage == 1073 && saved_frequency == 525.125f);
    } else if (strcmp(scenario, "cooldown-resume-pause") == 0 || strcmp(scenario, "cooldown-init-fail") == 0) {
        overheat_enabled = true; exit_delay = 14;
        if (strcmp(scenario, "cooldown-resume-pause") == 0) pause_at_delay = 10; else detected_chips = 0;
        if (setjmp(task_exit) == 0) POWER_MANAGEMENT_task(&global);
        assert(cooling_count >= 6 && applied && !global.ASIC_initalized && last_voltage == 0);
        assert(voltage_calls == 3 && setting_writes == 5 && saved_voltage == 1073 && saved_frequency == 525.125f);
        if (pause_at_delay) assert(global.SYSTEM_MODULE.mining_paused && init_calls == 0);
        else assert(global.SYSTEM_MODULE.hardware_fault && init_calls == 1);
    } else assert(false);
    puts("PASS"); return 0;
}
