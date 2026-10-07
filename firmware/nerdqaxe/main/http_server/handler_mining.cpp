#include "handler_mining.h"
#include "tasks/mining_schedule.h"
#include "tasks/mining_control.h"
#include "handler_capabilities.h"
#include "esp_app_desc.h"
#include "http_utils.h"
#include "http_cors.h"
#include "psram_allocator.h"
#include <cstring>

namespace {
esp_err_t result(httpd_req_t *req, const char *status, const char *error = nullptr) {
    httpd_resp_set_status(req, status); JsonDocument doc; doc["ok"] = error == nullptr;
    if (error) doc["error"] = error;
    else { doc["restartRequired"] = false; doc["message"] = "Request stored; read power status for applied state"; }
    return sendJsonResponse(req, doc);
}
bool prepare(httpd_req_t *req, bool write) {
    if (is_network_allowed(req) != ESP_OK) { httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized"); return false; }
    httpd_resp_set_type(req, "application/json"); httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (set_cors_headers(req) != ESP_OK) { httpd_resp_send_500(req); return false; }
    if (write && validateOTP(req) != ESP_OK) return false;
    if (write && !FiveTratumMining::controlSupported()) { result(req, "409 Conflict", "unsupported-power-control"); return false; }
    return true;
}
bool identity(JsonDocument &doc) {
    Board *board = SYSTEM_MODULE.getBoard(); char id[39];
    if (!board || !readFiveTratumPhysicalDeviceId(id)) return false;
    doc["schemaVersion"] = 1;
    doc["identity"]["deviceId"] = id;
    doc["hardware"]["boardModel"] = board->getDeviceModel();
    doc["hardware"]["asicModel"] = board->getAsicModel();
    doc["hardware"]["asicCount"] = board->getAsicCount();
    doc["firmware"]["product"] = "5tratumFW";
    doc["firmware"]["version"] = esp_app_get_description()->version;
    return !doc.overflowed();
}
esp_err_t snapshotResponse(httpd_req_t *req) {
    PSRAMAllocator allocator; JsonDocument doc(&allocator);
    if (!FiveTratumMining::scheduleJson(doc) || !FiveTratumMining::writeControlReport(doc) || !identity(doc))
        return result(req, "503 Service Unavailable", "power-control-unavailable");
    httpd_resp_set_status(req, "200 OK");
    return sendJsonResponse(req, doc);
}
bool objectRequest(httpd_req_t *req, JsonDocument &doc, bool allowEmpty = false) {
    if (req->content_len == 0 && allowEmpty) { doc.to<JsonObject>(); return true; }
    if (!req->content_len || req->content_len > 2048) { result(req, "400 Bad Request", "invalid-power-request"); return false; }
    if (getJsonData(req, doc) != ESP_OK) return false;
    if (!doc.is<JsonObject>()) { result(req, "400 Bad Request", "invalid-power-request"); return false; }
    return true;
}
esp_err_t manual(httpd_req_t *req, bool paused) {
    ConGuard guard(http_server, req); if (!prepare(req, true)) return ESP_FAIL;
    JsonDocument doc; if (!objectRequest(req, doc, true)) return ESP_FAIL;
    if (doc.size() != 0) return result(req, "400 Bad Request", "invalid-power-request");
    if (!FiveTratumMining::manualOverride(paused)) return result(req, "409 Conflict", "power-schedule-unavailable");
    return result(req, "200 OK");
}
}
esp_err_t GET_mining_schedule(httpd_req_t *req) {
    ConGuard guard(http_server, req); if (!prepare(req, false)) return ESP_FAIL;
    return snapshotResponse(req);
}
esp_err_t PUT_mining_schedule(httpd_req_t *req) {
    ConGuard guard(http_server, req); if (!prepare(req, true)) return ESP_FAIL;
    PSRAMAllocator allocator; JsonDocument doc(&allocator); if (!objectRequest(req, doc)) return ESP_FAIL;
    FiveTratumMining::ScheduleSettings parsed;
    if (!FiveTratumMining::parseSchedule(doc.as<JsonObjectConst>(), parsed)) return result(req, "400 Bad Request", "invalid-power-schedule");
    if (!FiveTratumMining::saveSchedule(doc.as<JsonObjectConst>()))
        return result(req, "507 Insufficient Storage", "power-schedule-not-stored");
    return snapshotResponse(req);
}
esp_err_t POST_mining_pause(httpd_req_t *req) { return manual(req, true); }
esp_err_t POST_mining_resume(httpd_req_t *req) { return manual(req, false); }
esp_err_t POST_mining_schedule_override(httpd_req_t *req) {
    ConGuard guard(http_server, req); if (!prepare(req, true)) return ESP_FAIL;
    JsonDocument doc; if (!objectRequest(req, doc)) return ESP_FAIL;
    if (doc.size() != 1 || !doc["mode"].is<const char *>() || strcmp(doc["mode"].as<const char *>(), "schedule") != 0)
        return result(req, "400 Bad Request", "invalid-power-request");
    return FiveTratumMining::clearOverride() ? result(req, "200 OK") : result(req, "409 Conflict", "power-schedule-unavailable");
}
