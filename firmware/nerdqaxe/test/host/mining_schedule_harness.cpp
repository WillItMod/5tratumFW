#include "mining_schedule.h"
#include "nvs.h"
#include "sntp.h"
#include <cassert>
#include <cstring>
#include <string>
#include <iostream>

SNTP sntp;
static time_t wall = 1791374400; // 2026-10-07 12:00:00 UTC, Wednesday.
extern "C" time_t time(time_t *out) { if (out) *out = wall; return wall; }
static std::string flash, pending;
static bool failCommit = false;
static int commits = 0;
int nvs_open(const char *ns, int mode, nvs_handle_t *h) {
    assert(std::string(ns) == "pf5tfw"); *h = 1;
    return mode == NVS_READONLY && flash.empty() ? ESP_ERR_NVS_NOT_FOUND : ESP_OK;
}
int nvs_get_blob(nvs_handle_t, const char *key, void *out, size_t *len) {
    assert(std::string(key) == "power");
    if (flash.empty()) return ESP_ERR_NVS_NOT_FOUND;
    if (out) { assert(*len >= flash.size()); memcpy(out, flash.data(), flash.size()); }
    *len = flash.size(); return ESP_OK;
}
int nvs_set_blob(nvs_handle_t, const char *key, const void *data, size_t len) {
    assert(std::string(key) == "power"); pending.assign(static_cast<const char *>(data), len); return ESP_OK;
}
int nvs_commit(nvs_handle_t) { ++commits; if (failCommit) return -7; flash = pending; return ESP_OK; }
void nvs_close(nvs_handle_t) { pending.clear(); }

static JsonDocument document(const char *text) { JsonDocument doc; assert(!deserializeJson(doc, text)); return doc; }
static JsonDocument report() { JsonDocument out; assert(FiveTratumMining::scheduleJson(out)); return out; }
static void at(const char *utc) {
    tm t{}; assert(strptime(utc, "%Y-%m-%dT%H:%M:%S", &t)); wall = timegm(&t);
    FiveTratumMining::updateSchedule();
}
int main() {
    using namespace FiveTratumMining;
    assert(initializeSchedule()); assert(!requestedPaused() && commits == 0);
    auto schedule = document(R"({"enabled":true,"timezone":"UTC","windows":[{"days":8,"start":"12:00","end":"18:00"}]})");
    assert(saveSchedule(schedule.as<JsonObjectConst>())); assert(requestedPaused()); // No trusted NTP yet.
    const auto persisted = flash;
    sntp.synchronized = true;
    at("2026-10-07T11:59:59"); assert(!requestedPaused());
    at("2026-10-07T12:00:00"); assert(requestedPaused());
    assert(manualOverride(false)); assert(!requestedPaused());
    at("2026-10-07T17:59:59"); assert(!requestedPaused() && report()["status"]["manualOverride"] == "running");
    at("2026-10-07T18:00:00"); assert(!requestedPaused() && report()["status"]["manualOverride"] == "none");
    // Losing trusted clock pauses an enabled policy; re-sync reevaluates it.
    sntp.synchronized = false; updateSchedule(); assert(requestedPaused());
    sntp.synchronized = true; updateSchedule(); assert(!requestedPaused());
    // A failed durable commit must neither replace policy nor timezone.
    failCommit = true;
    auto other = document(R"({"enabled":true,"timezone":"Asia/Tokyo","windows":[{"days":127,"start":"00:01","end":"23:59"}]})");
    assert(!saveSchedule(other.as<JsonObjectConst>())); assert(flash == persisted);
    assert(report()["schedule"]["timezone"] == "UTC"); failCommit = false;
    // Reboot loads persisted policy, clears manual state and waits for NTP.
    assert(manualOverride(true)); sntp.synchronized = false;
    assert(initializeSchedule()); assert(requestedPaused()); assert(report()["status"]["manualOverride"] == "none");
    sntp.synchronized = true;
    at("2026-10-07T18:00:00"); assert(!requestedPaused());
    // Overnight Saturday starts continue into Sunday, including week wrap.
    schedule = document(R"({"enabled":true,"timezone":"UTC","windows":[{"days":64,"start":"22:00","end":"02:00"}]})");
    assert(saveSchedule(schedule.as<JsonObjectConst>()));
    at("2026-10-10T22:00:00"); assert(requestedPaused());
    at("2026-10-11T01:59:00"); assert(requestedPaused());
    at("2026-10-11T02:00:00"); assert(!requestedPaused());
    // Overlapping windows expire manual override only at the effective union edge.
    schedule = document(R"({"enabled":true,"timezone":"UTC","windows":[{"days":8,"start":"12:00","end":"16:00"},{"days":8,"start":"15:00","end":"18:00"}]})");
    assert(saveSchedule(schedule.as<JsonObjectConst>()));
    at("2026-10-07T12:01:00"); assert(manualOverride(false));
    at("2026-10-07T16:00:00"); assert(!requestedPaused() && report()["status"]["manualOverride"] == "running");
    // Jumping across several edges still expires the epoch deadline.
    at("2026-10-14T12:01:00"); assert(requestedPaused() && report()["status"]["manualOverride"] == "none");
    // London repeated hour uses the correct local wall window twice.
    schedule = document(R"({"enabled":true,"timezone":"Europe/London","windows":[{"days":1,"start":"01:30","end":"02:00"}]})");
    assert(saveSchedule(schedule.as<JsonObjectConst>()));
    at("2026-10-25T00:30:00"); assert(requestedPaused()); // 01:30 BST.
    at("2026-10-25T01:00:00"); assert(!requestedPaused()); // 01:00 GMT.
    at("2026-10-25T01:30:00"); assert(requestedPaused()); // 01:30 GMT.
    at("2026-10-25T02:00:00"); assert(!requestedPaused());
    at("2026-03-29T01:00:00"); assert(!requestedPaused()); // Skipped 01:30 local.
    // Disabled schedule keeps the current request rather than energizing the rail.
    assert(manualOverride(true));
    schedule = document(R"({"enabled":false,"timezone":"UTC","windows":[]})");
    assert(saveSchedule(schedule.as<JsonObjectConst>()) && requestedPaused());
    assert(manualOverride(false) && !requestedPaused());
    for (const char *bad : {
        R"({"enabled":true,"timezone":"UTC","windows":[]})",
        R"({"enabled":1,"timezone":"UTC","windows":[]})",
        R"({"enabled":false,"timezone":"Unknown/Zone","windows":[]})",
        R"({"enabled":false,"timezone":"UTC","windows":[],"asicIndex":0})",
        R"({"enabled":true,"timezone":"UTC","windows":[{"days":1.5,"start":"12:00","end":"18:00"}]})",
        R"({"enabled":true,"timezone":"UTC","windows":[{"days":0,"start":"12:00","end":"18:00"}]})",
        R"({"enabled":true,"timezone":"UTC","windows":[{"days":128,"start":"12:00","end":"18:00"}]})",
        R"({"enabled":true,"timezone":"UTC","windows":[{"days":127,"start":"24:00","end":"18:00"}]})",
        R"({"enabled":true,"timezone":"UTC","windows":[{"days":127,"start":"12:00","end":"12:00"}]})"
    }) { auto input = document(bad); const int before = commits; assert(!saveSchedule(input.as<JsonObjectConst>())); assert(commits == before); }
    flash = "{bad"; assert(!initializeSchedule() && requestedPaused() && hasScheduleError());
    assert(!manualOverride(false) && !clearOverride());
    schedule = document(R"({"enabled":false,"timezone":"UTC","windows":[]})");
    assert(saveSchedule(schedule.as<JsonObjectConst>())); assert(!hasScheduleError());
    // Repair does not silently resume a paused device; an explicit request does.
    assert(requestedPaused()); assert(manualOverride(false) && !requestedPaused());
    std::cout << "Production weekly policy/schema/persistence passed boundaries, NTP, reboot, commit failure, override, DST and malformed-storage checks\n";
}
