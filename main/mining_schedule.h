#ifndef MINING_SCHEDULE_H
#define MINING_SCHEDULE_H

#include "global_state.h"
#include "mining_schedule_policy.h"
#include "cJSON.h"

typedef struct {
    MiningScheduleConfig policy;
    char timezone[32];
} MiningScheduleSettings;

esp_err_t mining_schedule_init(GlobalState *state);
void mining_schedule_start_time_sync(void);
bool mining_schedule_owns_clock(void);
void mining_schedule_update(void);
void mining_schedule_manual_override(bool paused);
void mining_schedule_clear_override(void);
bool mining_schedule_applied_paused(void);
void mining_schedule_report_power(bool applied_paused, const char *error);
bool mining_schedule_parse(const cJSON *json, MiningScheduleSettings *settings);
esp_err_t mining_schedule_save(const MiningScheduleSettings *settings);
cJSON *mining_schedule_get_json(void);

#endif
