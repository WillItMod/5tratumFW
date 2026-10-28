#include "tuning_schedule.h"
#include "pool_schedule.h"

namespace FiveTratumProfiles {
bool validTuningSchedule(JsonObjectConst schedule) {
    if (!validPoolSchedule(schedule)) return false;
    bool hasEnabledEvent = false;
    for (JsonObjectConst event : schedule["events"].as<JsonArrayConst>())
        hasEnabledEvent |= event["enabled"].as<bool>();
    return !schedule["enabled"].as<bool>() || hasEnabledEvent;
}
int selectedTuningSlot(JsonObjectConst schedule, int day, int minute, int *ageMinutes) {
    if (ageMinutes) *ageMinutes = -1;
    if (!validTuningSchedule(schedule) || !schedule["enabled"].as<bool>() || day < 0 || day > 6 || minute < 0 || minute > 1439) return -1;
    const int now = day * 1440 + minute;
    int smallestAge = 10081, slot = -1;
    for (JsonObjectConst event : schedule["events"].as<JsonArrayConst>()) {
        if (!event["enabled"].as<bool>()) continue;
        const unsigned mask = event["dayMask"];
        for (int d = 0; d < 7; ++d) if (mask & (1u << d)) {
            const int age = (now - (d * 1440 + event["timeMinutes"].as<int>()) + 10080) % 10080;
            if (age < smallestAge) { smallestAge = age; slot = event["slot"]; }
        }
    }
    if (ageMinutes && slot >= 0) *ageMinutes = smallestAge;
    return slot;
}
bool tuningScheduleReferences(JsonObjectConst schedule, unsigned slot) {
    if (!validTuningSchedule(schedule)) return false;
    for (JsonObjectConst event : schedule["events"].as<JsonArrayConst>())
        if (event["slot"].as<unsigned>() == slot) return true;
    return false;
}
}
