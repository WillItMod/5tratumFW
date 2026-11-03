#pragma once
#include "mining_schedule_schema.h"

namespace FiveTratumMining {
bool initializeSchedule();
void updateSchedule();
bool requestedPaused();
uint64_t policyRevision();
bool resumeForRecovery(uint64_t expectedRevision,uint64_t &appliedRevision);
bool manualOverride(bool paused);
bool clearOverride();
bool saveSchedule(JsonObjectConst input);
bool scheduleJson(JsonDocument &doc);
bool hasScheduleError();
}
