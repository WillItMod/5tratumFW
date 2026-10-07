#pragma once
#include "profile_schema.h"
#include <stdint.h>
namespace FiveTratumProfiles {
constexpr unsigned MAX_POOL_EVENTS=16;
// Sunday is bit0; explicit fixed UTC offset, never taken from browser/host locale.
bool validPoolSchedule(JsonObjectConst schedule);
int selectedScheduledSlot(JsonObjectConst schedule, int weekdaySundayZero, int minuteOfDay);
bool scheduleReferences(JsonObjectConst schedule,unsigned slot);
bool readPoolSchedule(JsonDocument &doc);
bool savePoolSchedule(JsonObjectConst schedule);
void startProfileScheduler();
}
