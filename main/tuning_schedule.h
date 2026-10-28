#ifndef FIVE_TUNING_SCHEDULE_H
#define FIVE_TUNING_SCHEDULE_H
#include "pool_schedule.h"
typedef PoolSchedule TuningSchedule;
#define TUNING_SCHEDULE_MAX_EVENTS POOL_SCHEDULE_MAX_EVENTS
bool tuning_schedule_parse(const cJSON *json, TuningSchedule *schedule);
int tuning_schedule_select(const TuningSchedule *schedule, unsigned day, unsigned minute, unsigned *age_minutes);
esp_err_t tuning_schedule_init(GlobalState *state);
esp_err_t tuning_schedule_start(void);
esp_err_t tuning_schedule_save(const TuningSchedule *schedule);
void tuning_schedule_tick(void);
cJSON *tuning_schedule_get_json(void);
#endif
