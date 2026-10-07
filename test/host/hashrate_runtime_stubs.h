#ifndef HASHRATE_RUNTIME_STUBS_H_
#define HASHRATE_RUNTIME_STUBS_H_
#define GLOBAL_STATE_H_
#define SYSTEM_H_
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define isnanf isnan
#include "tasks/hashrate_monitor_task.h"

typedef struct {
    float current_hashrate, error_percentage, hashrate_1m, hashrate_10m, hashrate_1h;
    bool mining_paused, hardware_fault, pools_unavailable, mining_runtime_ready;
} SystemModule;
typedef struct {
    SystemModule SYSTEM_MODULE;
    HashrateMonitorModule HASHRATE_MONITOR_MODULE;
    struct { struct { int asic_count; struct { int hash_domains; } asic; } family; } DEVICE_CONFIG;
    struct { bool is_active; } SELF_TEST_MODULE;
    bool ASIC_initalized;
} GlobalState;

typedef unsigned TickType_t;
#define portTICK_PERIOD_MS 1
#define MALLOC_CAP_SPIRAM 0
#define ESP_LOGI(tag, ...) do { (void)(tag); if (0) printf(__VA_ARGS__); } while (0)
#define ESP_LOGD ESP_LOGI
#define ESP_LOGW ESP_LOGI
#define ESP_LOGE ESP_LOGI
static inline void *heap_caps_malloc(size_t bytes, int caps) { (void)caps; return malloc(bytes); }
static inline void vTaskDelay(unsigned ticks) { (void)ticks; }
static inline void vTaskDelayUntil(TickType_t *time, unsigned ticks) { *time += ticks; }
static inline TickType_t xTaskGetTickCount(void) { return 0; }
static inline void ASIC_read_registers(GlobalState *state) { (void)state; }
/* Same conversion formula as the production utils.c; no ESP dependency. */
static inline float hashCounterToGhs(uint64_t duration_us, uint32_t counter)
{
    if (!duration_us) return 0;
    float seconds = duration_us / 1000000.0;
    return counter / seconds * (float)UINT64_C(0x100000000) / 1e9f;
}
#endif
