/* Adapted from 5tratumFW Gamma weekly scheduler. GPL-3.0-or-later. */
#ifndef MINING_SCHEDULE_POLICY_H
#define MINING_SCHEDULE_POLICY_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MINING_SCHEDULE_MAX_WINDOWS 8
#define MINING_SCHEDULE_MINUTES_PER_DAY 1440
#define MINING_SCHEDULE_MINUTES_PER_WEEK 10080

typedef struct {
    /* Sunday = 1, Monday = 2, ... Saturday = 64. These are start days. */
    uint8_t days;
    uint16_t start_minutes;
    uint16_t end_minutes;
} MiningScheduleWindow;

typedef struct {
    bool enabled;
    uint8_t window_count;
    MiningScheduleWindow windows[MINING_SCHEDULE_MAX_WINDOWS];
} MiningScheduleConfig;

/* Volatile policy state only. Evaluation never reads or writes NVS. */
typedef struct {
    bool scheduled_state_known;
    bool previous_scheduled_pause;
    bool manual_override_active;
    bool manual_override_paused;
    bool effective_paused;
} MiningScheduleEngine;

/* Enabling requires at least one valid window. Disabled may have none. */
bool mining_schedule_config_valid(const MiningScheduleConfig *config);

/* Start is inclusive, end exclusive; overlaps form a union. An enabled policy
 * with an invalid clock or invalid configuration pauses safely. Disabled is
 * false here; use the engine to retain the current manual state when disabled.
 * Timezone and DST conversion belong to the caller, before supplying local tm.
 */
bool mining_schedule_is_paused(const MiningScheduleConfig *config, bool clock_valid,
                               const struct tm *local_time);

/* Minutes to the next strictly future union pause/resume boundary, or -1 when
 * disabled, invalid, or continuously paused/running for the whole week. Starts
 * and ends hidden by an overlap are not boundaries. Seconds are ignored.
 */
int mining_schedule_next_boundary_minutes(const MiningScheduleConfig *config,
                                          const struct tm *local_time);

void mining_schedule_engine_init(MiningScheduleEngine *engine, bool initial_paused);
void mining_schedule_engine_set_override(MiningScheduleEngine *engine, bool paused);
void mining_schedule_engine_clear_override(MiningScheduleEngine *engine);

/* A manual override expires at the next observed scheduled pause/resume
 * transition. Invalid clock forces pause without consuming the override or
 * changing the previous valid scheduled state. Disabling preserves the latest
 * effective state until another manual request or an enabled valid schedule.
 */
bool mining_schedule_engine_evaluate(MiningScheduleEngine *engine,
                                     const MiningScheduleConfig *config,
                                     bool clock_valid, const struct tm *local_time);

#ifdef __cplusplus
}
#endif

#endif
