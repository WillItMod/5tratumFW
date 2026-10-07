#pragma once
#include <ArduinoJson.h>
#include "mining_schedule_policy.h"

namespace FiveTratumMining {
struct ScheduleSettings {
    MiningScheduleConfig policy{};
    char timezone[40] = "UTC";
};
const char *timezoneRule(const char *name);
bool parseSchedule(JsonObjectConst input, ScheduleSettings &out);
void writeSchedule(JsonObject out, const ScheduleSettings &settings);
void writeScheduleLimits(JsonObject out);
}
