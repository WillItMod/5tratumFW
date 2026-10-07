#ifndef FIVE_POOL_SCHEDULE_H
#define FIVE_POOL_SCHEDULE_H
#include "global_state.h"
#include "cJSON.h"
#include <time.h>

#define POOL_SCHEDULE_MAX_EVENTS 16
typedef struct {
    bool enabled;
    uint8_t day_mask, slot;
    uint16_t time_minutes;
} PoolScheduleEvent;
typedef struct {
    bool enabled;
    int16_t utc_offset_minutes;
    uint8_t event_count;
    PoolScheduleEvent events[POOL_SCHEDULE_MAX_EVENTS];
} PoolSchedule;

bool pool_schedule_parse(const cJSON *json, PoolSchedule *schedule);
// Sunday is zero. Returns -1 for a disabled schedule or invalid local clock.
int pool_schedule_select(const PoolSchedule *schedule, unsigned day, unsigned minute);
bool pool_schedule_clock(time_t epoch, bool synchronized, int offset, struct tm *local);
esp_err_t pool_schedule_init(GlobalState *state);
esp_err_t pool_schedule_start(void);
esp_err_t pool_schedule_save(const PoolSchedule *schedule);
cJSON *pool_schedule_get_json(void);
void pool_schedule_tick(void);
#endif
