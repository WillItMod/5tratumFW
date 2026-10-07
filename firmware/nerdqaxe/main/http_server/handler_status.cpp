#include "handler_status.h"

#include "esp_app_desc.h"
#include "esp_timer.h"
#include "global_state.h"
#include "http_cors.h"
#include "http_utils.h"
#include "psram_allocator.h"
#include "status_report.h"
#include "handler_capabilities.h"

namespace {
esp_err_t unavailable(httpd_req_t *req, const char *reason) {
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, reason, HTTPD_RESP_USE_STRLEN);
}
}

esp_err_t GET_five_tratum_status(httpd_req_t *req) {
    ConGuard guard(http_server, req);
    if (is_network_allowed(req) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (set_cors_headers(req) != ESP_OK) return httpd_resp_send_500(req);

    Board *board = SYSTEM_MODULE.getBoard();
    if (!board || board->getAsicCount() < 1 ||
        board->getAsicCount() > FiveTratumStatus::MAX_ASICS)
        return unavailable(req, "Board status unavailable");

    FiveTratumStatus::Snapshot snapshot;
    snapshot.firmwareVersion = esp_app_get_description()->version;
    if (!readFiveTratumPhysicalDeviceId(snapshot.deviceId))
        return unavailable(req, "Device identity unavailable");
    snapshot.boardModel = board->getDeviceModel();
    snapshot.asicModel = board->getAsicModel();
    snapshot.nowUs = esp_timer_get_time();
    const int64_t startUs = SYSTEM_MODULE.getStartTime();
    snapshot.uptimeSeconds = snapshot.nowUs >= startUs ?
        static_cast<uint64_t>((snapshot.nowUs - startUs) / 1000000) : 0;
    snapshot.asicCount = board->getAsicCount();
    if (board->isInitialized()) {
        for (int i = 0; i < snapshot.asicCount; ++i) {
            if (board->hasHashrateCounter())
                snapshot.rates[i] = HASHRATE_MONITOR.getChipHashrateSample(i);
            snapshot.temperaturesC[i] = board->getChipTemp(i);
        }
    }
    if (STRATUM_MANAGER) {
        const auto view = STRATUM_MANAGER->getMuxDisplayState();
        snapshot.pools[0] = view.pools[0];
        snapshot.pools[1] = view.pools[1];
        snapshot.dual = view.dual;
        snapshot.activePool = view.activePool;
    }
    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);
    if (!FiveTratumStatus::buildReport(doc, snapshot))
        return unavailable(req, "Status report unavailable");
    return sendJsonResponse(req, doc);
}
