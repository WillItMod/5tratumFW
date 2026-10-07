#pragma once
#include "mining_schedule_schema.h"

namespace FiveTratumMining {
bool initializeSchedule();
void updateSchedule();
bool requestedPaused();
bool manualOverride(bool paused);
bool clearOverride();
bool saveSchedule(JsonObjectConst input);
bool scheduleJson(JsonDocument &doc);
bool hasScheduleError();
}
