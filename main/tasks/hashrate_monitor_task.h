#ifndef HASHRATE_MONITOR_TASK_H_
#define HASHRATE_MONITOR_TASK_H_

#include "asic_common.h"
#include "hashrate_display.h"
#include <pthread.h>

typedef struct {
    uint32_t value;
    uint64_t time_us;
    float hashrate;
    /* Timestamp of an accepted total-counter delta, for display freshness. */
    uint64_t rate_sample_us;
} measurement_t;

typedef struct {
    measurement_t* total_measurement;
    measurement_t** domain_measurements;
    measurement_t* error_measurement;

    pthread_mutex_t lock;
    bool is_initialized;
} HashrateMonitorModule;

void hashrate_monitor_task(void *pvParameters);
void hashrate_monitor_register_read(void *pvParameters, register_type_t register_type, uint8_t asic_nr, uint32_t value, uint64_t timestamp_us);
void hashrate_monitor_reset_measurements(void *pvParameters);
HashrateDisplaySnapshot hashrate_monitor_display_snapshot(void *pvParameters, uint64_t now_us);

void update_hashrate(measurement_t * measurement, uint32_t value);
void update_hash_counter(measurement_t * measurement, uint32_t value, uint64_t time_us);
#endif /* HASHRATE_MONITOR_TASK_H_ */
