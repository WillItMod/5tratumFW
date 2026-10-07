#include "mining_schedule.h"
#include "nvs.h"
#include "sntp.h"
#include <pthread.h>
#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <ctime>

extern SNTP sntp;
namespace FiveTratumMining {
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static ScheduleSettings saved;
static MiningScheduleEngine engine{};
static std::atomic_bool requested{false};
static bool initialized = false, configError = false;
static bool environmentReady = false;
static char timezoneEnvironment[128];
static time_t overrideDeadline = 0;
static bool deadlineComputed = false;
static constexpr size_t maxBytes = 2048;

struct Guard { Guard() { pthread_mutex_lock(&mutex); } ~Guard() { pthread_mutex_unlock(&mutex); } };
static bool clockNow(tm &local, time_t &now) {
    now = time(nullptr);
    return sntp.isTimeSynced() && now >= 1704067200 && localtime_r(&now, &local);
}
static void evaluateLocked() {
    tm local{}; time_t now{}; const bool valid = clockNow(local, now);
    if (!valid) deadlineComputed = false;
    if (valid && overrideDeadline && now >= overrideDeadline) {
        mining_schedule_engine_clear_override(&engine); overrideDeadline = 0;
    }
    requested.store(configError || mining_schedule_engine_evaluate(&engine, &saved.policy, valid, &local));
    if (!engine.manual_override_active) { overrideDeadline = 0; deadlineComputed = false; }
}
static void computeDeadlineLocked() {
    overrideDeadline = 0; deadlineComputed = false;
    if (!engine.manual_override_active || !saved.policy.enabled) return;
    tm local{}; time_t now{};
    if (!clockNow(local, now)) return;
    deadlineComputed = true;
    const bool current = mining_schedule_is_paused(&saved.policy, true, &local);
    const time_t minute = now - now % 60;
    // Actual epoch minutes preserve both occurrences of a repeated DST hour.
    for (int i = 1; i <= 8 * 24 * 60; ++i) {
        time_t candidate = minute + i * 60;
        if (localtime_r(&candidate, &local) && mining_schedule_is_paused(&saved.policy, true, &local) != current) {
            overrideDeadline = candidate; return;
        }
    }
}
bool initializeSchedule() {
    Guard guard;
    saved = ScheduleSettings{}; mining_schedule_engine_init(&engine, false);
    configError = false; overrideDeadline = 0; deadlineComputed = false;
    nvs_handle_t handle; esp_err_t err = nvs_open("pf5tfw", NVS_READONLY, &handle);
    if (err == ESP_OK) {
        size_t length = 0; err = nvs_get_blob(handle, "power", nullptr, &length);
        if (err == ESP_OK) {
            if (!length || length > maxBytes) configError = true;
            else {
                char buffer[maxBytes]; err = nvs_get_blob(handle, "power", buffer, &length);
                JsonDocument doc;
                configError = err != ESP_OK || deserializeJson(doc, buffer, length, DeserializationOption::NestingLimit(4)) || !parseSchedule(doc.as<JsonObjectConst>(), saved);
            }
        } else if (err != ESP_ERR_NVS_NOT_FOUND) configError = true;
        nvs_close(handle);
    } else if (err != ESP_ERR_NVS_NOT_FOUND) configError = true;
    snprintf(timezoneEnvironment, sizeof(timezoneEnvironment), "TZ=%s", timezoneRule(saved.timezone));
    environmentReady = putenv(timezoneEnvironment) == 0;
    if (!environmentReady) configError = true;
    tzset(); initialized = true; evaluateLocked();
    return !configError;
}
void updateSchedule() {
    Guard guard; if (!initialized) return;
    evaluateLocked();
    if (engine.manual_override_active && !deadlineComputed) computeDeadlineLocked();
}
bool requestedPaused() { return requested.load(); }
bool hasScheduleError() { Guard guard; return !initialized || configError; }
bool manualOverride(bool paused) {
    Guard guard; if (!initialized || configError) return false;
    evaluateLocked(); mining_schedule_engine_set_override(&engine, paused);
    evaluateLocked(); computeDeadlineLocked(); return true;
}
bool clearOverride() {
    Guard guard; if (!initialized || configError) return false;
    mining_schedule_engine_clear_override(&engine); overrideDeadline = 0; deadlineComputed = false;
    evaluateLocked(); return true;
}
bool saveSchedule(JsonObjectConst input) {
    ScheduleSettings next; if (!parseSchedule(input, next)) return false;
    JsonDocument document; writeSchedule(document.to<JsonObject>(), next);
    const size_t length = measureJson(document); if (document.overflowed() || !length || length >= maxBytes) return false;
    char buffer[maxBytes]; if (serializeJson(document, buffer, sizeof(buffer)) != length) return false;
    Guard guard; if (!initialized) return false;
    // Register one stable environment buffer before committing. Updating the
    // named rule after a successful commit then cannot fail an allocation.
    if (!environmentReady) {
        environmentReady = putenv(timezoneEnvironment) == 0;
        if (!environmentReady) return false;
    }
    nvs_handle_t handle; esp_err_t err = nvs_open("pf5tfw", NVS_READWRITE, &handle);
    if (err == ESP_OK) { err = nvs_set_blob(handle, "power", buffer, length); if (err == ESP_OK) err = nvs_commit(handle); nvs_close(handle); }
    if (err != ESP_OK) return false;
    if (!next.policy.enabled) engine.effective_paused = requested.load();
    saved = next; configError = false;
    snprintf(timezoneEnvironment, sizeof(timezoneEnvironment), "TZ=%s", timezoneRule(saved.timezone));
    tzset(); evaluateLocked(); computeDeadlineLocked(); return true;
}
bool scheduleJson(JsonDocument &doc) {
    Guard guard; if (!initialized) return false;
    evaluateLocked(); writeSchedule(doc["schedule"].to<JsonObject>(), saved);
    auto status = doc["status"].to<JsonObject>(); tm local{}; time_t now{};
    const bool valid = clockNow(local, now); status["clockValid"] = valid;
    if (valid) { char formatted[40]; strftime(formatted, sizeof(formatted), "%Y-%m-%dT%H:%M:%S%z", &local); status["localTime"] = formatted; }
    else status["localTime"] = nullptr;
    status["scheduledPause"] = mining_schedule_is_paused(&saved.policy, valid, &local);
    status["manualOverride"] = !engine.manual_override_active ? "none" : engine.manual_override_paused ? "paused" : "running";
    status["requestedPaused"] = requested.load();
    status["error"] = configError ? "Invalid stored power schedule; repair schedule before resuming" : nullptr;
    writeScheduleLimits(doc["limits"].to<JsonObject>()); return !doc.overflowed();
}
}
