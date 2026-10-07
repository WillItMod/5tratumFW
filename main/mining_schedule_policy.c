#include "mining_schedule_policy.h"

#include <stddef.h>
#include <string.h>

static bool local_time_valid(const struct tm *local_time)
{
    return local_time != NULL && local_time->tm_wday >= 0 && local_time->tm_wday <= 6
        && local_time->tm_hour >= 0 && local_time->tm_hour <= 23
        && local_time->tm_min >= 0 && local_time->tm_min <= 59;
}

bool mining_schedule_config_valid(const MiningScheduleConfig *config)
{
    if (config == NULL || config->window_count > MINING_SCHEDULE_MAX_WINDOWS
        || (config->enabled && config->window_count == 0)) {
        return false;
    }
    for (unsigned i = 0; i < config->window_count; ++i) {
        const MiningScheduleWindow *window = &config->windows[i];
        if (window->days == 0 || (window->days & ~0x7fu) != 0
            || window->start_minutes >= MINING_SCHEDULE_MINUTES_PER_DAY
            || window->end_minutes >= MINING_SCHEDULE_MINUTES_PER_DAY
            || window->start_minutes == window->end_minutes) {
            return false;
        }
    }
    return true;
}

static bool paused_at_week_minute(const MiningScheduleConfig *config, int week_minute)
{
    const int day = week_minute / MINING_SCHEDULE_MINUTES_PER_DAY;
    const int minute = week_minute % MINING_SCHEDULE_MINUTES_PER_DAY;
    const unsigned today = 1u << day;
    const unsigned yesterday = 1u << ((day + 6) % 7);
    for (unsigned i = 0; i < config->window_count; ++i) {
        const MiningScheduleWindow *window = &config->windows[i];
        if (window->start_minutes < window->end_minutes) {
            if ((window->days & today) != 0 && minute >= window->start_minutes
                && minute < window->end_minutes) {
                return true;
            }
        } else if (((window->days & today) != 0 && minute >= window->start_minutes)
                   || ((window->days & yesterday) != 0 && minute < window->end_minutes)) {
            return true;
        }
    }
    return false;
}

static int local_week_minute(const struct tm *local_time)
{
    return local_time->tm_wday * MINING_SCHEDULE_MINUTES_PER_DAY
        + local_time->tm_hour * 60 + local_time->tm_min;
}

bool mining_schedule_is_paused(const MiningScheduleConfig *config, bool clock_valid,
                               const struct tm *local_time)
{
    if (!mining_schedule_config_valid(config)) {
        return true;
    }
    if (!config->enabled) {
        return false;
    }
    if (!clock_valid || !local_time_valid(local_time)) {
        return true;
    }
    return paused_at_week_minute(config, local_week_minute(local_time));
}

int mining_schedule_next_boundary_minutes(const MiningScheduleConfig *config,
                                          const struct tm *local_time)
{
    if (!mining_schedule_config_valid(config) || !config->enabled || !local_time_valid(local_time)) {
        return -1;
    }
    const int current = local_week_minute(local_time);
    int next = -1;
    for (unsigned i = 0; i < config->window_count; ++i) {
        const MiningScheduleWindow *window = &config->windows[i];
        for (int day = 0; day < 7; ++day) {
            if ((window->days & (1u << day)) == 0) {
                continue;
            }
            const int start = day * MINING_SCHEDULE_MINUTES_PER_DAY + window->start_minutes;
            const int end = (day * MINING_SCHEDULE_MINUTES_PER_DAY + window->end_minutes
                + (window->end_minutes < window->start_minutes ? MINING_SCHEDULE_MINUTES_PER_DAY : 0))
                % MINING_SCHEDULE_MINUTES_PER_WEEK;
            const int candidates[] = {start, end};
            for (unsigned j = 0; j < 2; ++j) {
                const int boundary = candidates[j];
                const int before = (boundary + MINING_SCHEDULE_MINUTES_PER_WEEK - 1)
                    % MINING_SCHEDULE_MINUTES_PER_WEEK;
                if (paused_at_week_minute(config, before) == paused_at_week_minute(config, boundary)) {
                    continue;
                }
                int distance = boundary - current;
                if (distance <= 0) {
                    distance += MINING_SCHEDULE_MINUTES_PER_WEEK;
                }
                if (next < 0 || distance < next) {
                    next = distance;
                }
            }
        }
    }
    return next;
}

void mining_schedule_engine_init(MiningScheduleEngine *engine, bool initial_paused)
{
    if (engine != NULL) {
        memset(engine, 0, sizeof(*engine));
        engine->effective_paused = initial_paused;
    }
}

void mining_schedule_engine_set_override(MiningScheduleEngine *engine, bool paused)
{
    if (engine != NULL) {
        engine->manual_override_active = true;
        engine->manual_override_paused = paused;
    }
}

void mining_schedule_engine_clear_override(MiningScheduleEngine *engine)
{
    if (engine != NULL) {
        engine->manual_override_active = false;
    }
}

bool mining_schedule_engine_evaluate(MiningScheduleEngine *engine,
                                     const MiningScheduleConfig *config,
                                     bool clock_valid, const struct tm *local_time)
{
    if (engine == NULL) {
        return mining_schedule_is_paused(config, clock_valid, local_time);
    }
    if (!mining_schedule_config_valid(config)) {
        engine->effective_paused = true;
        return true;
    }
    if (!config->enabled) {
        if (engine->manual_override_active) {
            engine->effective_paused = engine->manual_override_paused;
        }
        engine->scheduled_state_known = false;
        return engine->effective_paused;
    }
    if (!clock_valid || !local_time_valid(local_time)) {
        engine->effective_paused = true;
        return true;
    }
    const bool scheduled_pause = paused_at_week_minute(config, local_week_minute(local_time));
    if (engine->scheduled_state_known && scheduled_pause != engine->previous_scheduled_pause) {
        engine->manual_override_active = false;
    }
    engine->scheduled_state_known = true;
    engine->previous_scheduled_pause = scheduled_pause;
    engine->effective_paused = engine->manual_override_active ? engine->manual_override_paused : scheduled_pause;
    return engine->effective_paused;
}
