/* Weekly mining policy format shared with 5tratumFW Gamma. GPL-3.0-or-later. */
#include "mining_schedule_schema.h"
#include <cstring>
#include <cstdio>

namespace FiveTratumMining {
struct Zone { const char *name; const char *rule; };
static const Zone zones[] = {
    {"UTC", "UTC0"},
    {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Chicago", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Denver", "MST7MDT,M3.2.0,M11.1.0"},
    {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
    {"Asia/Tokyo", "JST-9"}, {"Asia/Shanghai", "CST-8"},
    {"Australia/Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"}
};
const char *timezoneRule(const char *name) {
    if (!name) return nullptr;
    for (const auto &zone : zones) if (strcmp(name, zone.name) == 0) return zone.rule;
    return nullptr;
}
static bool hhmm(JsonVariantConst value, uint16_t &minutes) {
    if (!value.is<const char *>()) return false;
    const char *s = value.as<const char *>();
    if (strlen(s) != 5 || s[2] != ':' || s[0] < '0' || s[0] > '2' ||
        s[1] < '0' || s[1] > '9' || s[3] < '0' || s[3] > '5' || s[4] < '0' || s[4] > '9') return false;
    const unsigned hour = (s[0] - '0') * 10 + s[1] - '0';
    if (hour > 23) return false;
    minutes = hour * 60 + (s[3] - '0') * 10 + s[4] - '0';
    return true;
}
bool parseSchedule(JsonObjectConst input, ScheduleSettings &out) {
    if (input.isNull() || input.size() != 3 || !input["enabled"].is<bool>() ||
        !input["timezone"].is<const char *>() || !timezoneRule(input["timezone"].as<const char *>()) ||
        !input["windows"].is<JsonArrayConst>() || input["windows"].size() > MINING_SCHEDULE_MAX_WINDOWS) return false;
    ScheduleSettings next;
    next.policy.enabled = input["enabled"].as<bool>();
    next.policy.window_count = input["windows"].size();
    snprintf(next.timezone, sizeof(next.timezone), "%s", input["timezone"].as<const char *>());
    unsigned i = 0;
    for (JsonVariantConst value : input["windows"].as<JsonArrayConst>()) {
        if (!value.is<JsonObjectConst>()) return false;
        auto window = value.as<JsonObjectConst>();
        if (window.size() != 3 || !window["days"].is<unsigned>() ||
            window["days"].as<unsigned>() < 1 || window["days"].as<unsigned>() > 127 ||
            !hhmm(window["start"], next.policy.windows[i].start_minutes) ||
            !hhmm(window["end"], next.policy.windows[i].end_minutes)) return false;
        next.policy.windows[i++].days = window["days"].as<unsigned>();
    }
    if (!mining_schedule_config_valid(&next.policy)) return false;
    out = next;
    return true;
}
void writeSchedule(JsonObject out, const ScheduleSettings &settings) {
    out["enabled"] = settings.policy.enabled;
    out["timezone"] = settings.timezone;
    auto windows = out["windows"].to<JsonArray>();
    for (unsigned i = 0; i < settings.policy.window_count; ++i) {
        const auto &window = settings.policy.windows[i];
        char start[8], end[8];
        snprintf(start, sizeof(start), "%02u:%02u", window.start_minutes / 60, window.start_minutes % 60);
        snprintf(end, sizeof(end), "%02u:%02u", window.end_minutes / 60, window.end_minutes % 60);
        auto entry = windows.add<JsonObject>();
        entry["days"] = window.days; entry["start"] = start; entry["end"] = end;
    }
}
void writeScheduleLimits(JsonObject out) {
    out["maxWindows"] = MINING_SCHEDULE_MAX_WINDOWS;
    auto list = out["timezones"].to<JsonArray>();
    for (const auto &zone : zones) list.add(zone.name);
}
}
