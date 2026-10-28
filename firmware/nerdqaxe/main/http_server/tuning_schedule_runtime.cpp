#include "tuning_schedule.h"
#include "profile_store.h"
#include "global_state.h"
#include "tasks/mining_control.h"
#include "tasks/mining_schedule.h"
#include "nvs_config.h"
#include "nvs.h"
#include "sntp.h"
#include <ctime>

extern SNTP sntp;
namespace FiveTratumProfiles {
namespace {
constexpr size_t maxBytes = 2048;
int appliedSlot = -1;
int64_t appliedOccurrence = -1;
const char *lastError = nullptr;

void emptySchedule(JsonDocument &document) {
    document["schemaVersion"] = 1;
    document["enabled"] = false;
    document["utcOffsetMinutes"] = 0;
    document["events"].to<JsonArray>();
}
bool trustedClock(time_t &now) {
    now = time(nullptr);
    return sntp.isTimeSynced() && now >= 1704067200;
}
int selection(JsonObjectConst schedule, time_t now, int64_t &occurrence) {
    const time_t wall = now + schedule["utcOffsetMinutes"].as<int>() * 60;
    tm local{};
    if (!gmtime_r(&wall, &local)) return -1;
    int age = -1;
    const int slot = selectedTuningSlot(schedule, local.tm_wday, local.tm_hour * 60 + local.tm_min, &age);
    occurrence = slot < 0 ? -1 : static_cast<int64_t>(now / 60) - age;
    return slot;
}
// Call under the power owner's hardware mutex. Never resume mining or clear a
// manual/thermal pause. The normal power task owns all actual voltage/PLL writes.
const char *runtimeWait(Board *board) {
    if (!FiveTratumMining::controlSupported() || !board || !board->isInitialized() || !board->isBuckInitialized()) return "waiting-runtime";
    if (POWER_MANAGEMENT_MODULE.isShutdown() || board->isShutdown() || SYSTEM_MODULE.getBoardError() != Board::Error::NONE) return "paused";
    if (FiveTratumMining::requestedPaused()) return "paused";
    if (!FiveTratumMining::miningWritesAllowed()) return "waiting-runtime";
    return nullptr;
}
Limits limitsFor(Board *board) {
    Limits limits;
    limits.maxFrequency = board->getAbsMaxAsicFrequency() > 0 ? board->getAbsMaxAsicFrequency() : board->getDefaultAsicFrequency();
    limits.minVoltage = board->getAbsMinAsicVoltageMillis() > 0 ? board->getAbsMinAsicVoltageMillis() : 1005;
    limits.maxVoltage = board->getAbsMaxAsicVoltageMillis() > 0 ? board->getAbsMaxAsicVoltageMillis() : 1400;
    return limits;
}
}

bool readTuningSchedule(JsonDocument &document) {
    ProfileStoreGuard guard;
    document.clear();
    nvs_handle_t handle;
    esp_err_t err = nvs_open("pf5tfw", NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) { emptySchedule(document); return true; }
    if (err != ESP_OK) return false;
    size_t length = 0;
    err = nvs_get_blob(handle, "tune_sched", nullptr, &length);
    if (err == ESP_ERR_NVS_NOT_FOUND) { nvs_close(handle); emptySchedule(document); return true; }
    if (err != ESP_OK || !length || length > maxBytes) { nvs_close(handle); return false; }
    char buffer[maxBytes];
    err = nvs_get_blob(handle, "tune_sched", buffer, &length);
    nvs_close(handle);
    return err == ESP_OK && !deserializeJson(document, buffer, length, DeserializationOption::NestingLimit(3)) && validTuningSchedule(document.as<JsonObjectConst>());
}
bool saveTuningSchedule(JsonObjectConst schedule) {
    if (!validTuningSchedule(schedule)) return false;
    const size_t length = measureJson(schedule);
    if (!length || length > maxBytes) return false;
    char buffer[maxBytes + 1];
    if (serializeJson(schedule, buffer, sizeof(buffer)) != length) return false;
    ProfileStoreGuard guard;
    // The same lock guards deletion: references cannot disappear between
    // checking them and committing the complete replacement schedule blob.
    for (JsonObjectConst event : schedule["events"].as<JsonArrayConst>()) {
        JsonDocument record;
        if (readSlot(Kind::Tuning, event["slot"].as<unsigned>(), record) != StoreResult::Ok) return false;
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open("pf5tfw", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_blob(handle, "tune_sched", buffer, length);
        if (err == ESP_OK) err = nvs_commit(handle);
        nvs_close(handle);
    }
    if (err != ESP_OK) return false;
    appliedSlot = -1;
    appliedOccurrence = -1;
    lastError = nullptr;
    return true;
}
void updateTuningSchedule() {
    ProfileStoreGuard guard;
    JsonDocument schedule;
    if (!readTuningSchedule(schedule)) { lastError = "Invalid or unreadable stored tuning schedule"; return; }
    if (!schedule["enabled"].as<bool>()) return;
    time_t now;
    if (!trustedClock(now)) return;
    int64_t occurrence = -1;
    const int slot = selection(schedule.as<JsonObjectConst>(), now, occurrence);
    if (slot < 0 || occurrence == appliedOccurrence) return;
    JsonDocument record, patch;
    const auto read = readSlot(Kind::Tuning, static_cast<unsigned>(slot), record);
    if (read != StoreResult::Ok) { lastError = read == StoreResult::Empty ? "Scheduled tuning slot is empty" : "Scheduled tuning slot is unreadable"; return; }
    LockGuard<PowerManagementTask> hardware(POWER_MANAGEMENT_MODULE);
    Board *board = SYSTEM_MODULE.getBoard();
    if (runtimeWait(board)) return;
    if (!POWER_MANAGEMENT_MODULE.refreshProtectionForMining()) { lastError = "Miner protection is not ready for tuning"; return; }
    if (!validTuning(record.as<JsonObjectConst>(), limitsFor(board))) { lastError = "Scheduled tuning slot exceeds this board's limits"; return; }
    buildActivePatch(patch.to<JsonObject>(), Kind::Tuning, record.as<JsonObjectConst>(), -1);
    if (patch.overflowed() || !Config::cfgApplyPatchPersisted(patch.as<JsonObjectConst>())) { lastError = "Unable to persist scheduled tuning; retrying"; return; }
    board->loadSettings();
    appliedSlot = slot;
    appliedOccurrence = occurrence;
    lastError = nullptr;
}
bool writeTuningScheduleStatus(JsonDocument &document, JsonObjectConst schedule) {
    ProfileStoreGuard guard;
    time_t now;
    const bool clockValid = trustedClock(now);
    int64_t occurrence = -1;
    const int slot = clockValid ? selection(schedule, now, occurrence) : -1;
    const bool supported = FiveTratumMining::controlSupported();
    const char *status = "disabled";
    if (schedule["enabled"].as<bool>()) {
        if (!clockValid) status = "waiting-clock";
        else {
            LockGuard<PowerManagementTask> hardware(POWER_MANAGEMENT_MODULE);
            const char *waiting = runtimeWait(SYSTEM_MODULE.getBoard());
            status = waiting ? waiting : lastError ? "error" : occurrence == appliedOccurrence ? "applied" : "pending";
        }
    }
    document["supported"] = supported;
    document["clockValid"] = clockValid;
    document["timezoneMode"] = "fixed-utc-offset";
    if (slot < 0) document["selectedSlot"] = nullptr; else document["selectedSlot"] = slot;
    if (appliedSlot < 0) document["appliedSlot"] = nullptr; else document["appliedSlot"] = appliedSlot;
    document["status"] = status;
    document["error"] = lastError;
    document["maxEvents"] = MAX_TUNING_EVENTS;
    return !document.overflowed();
}
}
