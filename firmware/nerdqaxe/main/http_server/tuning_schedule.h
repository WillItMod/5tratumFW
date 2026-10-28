#pragma once
#include "profile_schema.h"

namespace FiveTratumProfiles {
constexpr unsigned MAX_TUNING_EVENTS = 16;
bool validTuningSchedule(JsonObjectConst schedule);
// Returns the latest weekly event, with its age used to distinguish repeated slots.
int selectedTuningSlot(JsonObjectConst schedule, int weekdaySundayZero, int minuteOfDay, int *ageMinutes = nullptr);
bool tuningScheduleReferences(JsonObjectConst schedule, unsigned slot);
bool readTuningSchedule(JsonDocument &document);
bool saveTuningSchedule(JsonObjectConst schedule);
// Status reads are side-effect free; only this scheduler tick can activate a profile.
bool writeTuningScheduleStatus(JsonDocument &document, JsonObjectConst schedule);
void updateTuningSchedule();
}
