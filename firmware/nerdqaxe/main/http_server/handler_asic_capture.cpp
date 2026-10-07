#include "handler_asic_capture.h"
#include "bm1370_capture_runtime.h"
#include "bm1370.h"
#include "handler_capabilities.h"
#include "tasks/mining_control.h"
#include "http_utils.h"
#include "http_cors.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "nvs_config.h"
#include <cstring>
#include <cstdio>

namespace {
struct CaptureJsonAllocator : ArduinoJson::Allocator {
    void *allocate(size_t bytes) override { return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
    void deallocate(void *pointer) override { heap_caps_free(pointer); }
    void *reallocate(void *pointer, size_t bytes) override { return heap_caps_realloc(pointer, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
};
esp_err_t error(httpd_req_t *req, const char *status, const char *message) {
    httpd_resp_set_status(req, status); JsonDocument doc;
    doc["ok"] = false; doc["error"] = message;
    return sendJsonResponse(req, doc);
}
bool prepare(httpd_req_t *req) {
    if (is_network_allowed(req) != ESP_OK) { httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized"); return false; }
    httpd_resp_set_type(req, "application/json"); httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (set_cors_headers(req) != ESP_OK) { httpd_resp_send_500(req); return false; }
    // Raw capture exports receive the same OTP/session checks as mutations.
    if (validateOTP(req) != ESP_OK) return false;
    Board *board = SYSTEM_MODULE.getBoard();
    if (!board || strcmp(board->getDeviceModel(), "NerdQAxe++") ||
        strcmp(board->getAsicModel(), "BM1370") || board->getAsicCount() != 4 ||
        Config::isCanEnabled() || !FiveTratumMining::controlSupported()) {
        error(req, "409 Conflict", "unsupported-capture-device"); return false;
    }
    return true;
}
const char *reason(BM1370Capture::FreezeReason value) {
    switch (value) {
        case BM1370Capture::FreezeReason::Manual: return "manual";
        case BM1370Capture::FreezeReason::TimeLimit: return "time-limit";
        case BM1370Capture::FreezeReason::Capacity: return "capacity";
        case BM1370Capture::FreezeReason::InvalidInput: return "invalid-input";
        case BM1370Capture::FreezeReason::InvalidClock: return "invalid-clock";
        default: return "none";
    }
}
const char *kind(BM1370Capture::Kind value) {
    switch (value) {
        case BM1370Capture::Kind::TxJob: return "tx-job";
        case BM1370Capture::Kind::TxBytes: return "tx-bytes";
        case BM1370Capture::Kind::RxChunk: return "rx-chunk";
        case BM1370Capture::Kind::Transport: return "transport";
        case BM1370Capture::Kind::Retirement: return "retirement";
    }
    return "unknown";
}
void hex(const uint8_t *bytes, size_t count, char *out) {
    constexpr char chars[] = "0123456789abcdef";
    for (size_t i = 0; i < count; ++i) { out[i * 2] = chars[bytes[i] >> 4]; out[i * 2 + 1] = chars[bytes[i] & 15]; }
    out[count * 2] = 0;
}
bool decimal(const char *text, uint32_t &out) {
    if (!text || !*text) return false;
    uint32_t value = 0;
    for (const char *p = text; *p; ++p) {
        if (*p < '0' || *p > '9' || value > (UINT32_MAX - (*p - '0')) / 10) return false;
        value = value * 10 + (*p - '0');
    }
    out = value; return true;
}
bool identity(JsonDocument &doc) {
    Board *board = SYSTEM_MODULE.getBoard(); char id[39];
    if (!board || !readFiveTratumPhysicalDeviceId(id)) return false;
    auto binding = doc["deviceBinding"].to<JsonObject>();
    binding["firmwareVersion"] = esp_app_get_description()->version;
    binding["boardModel"] = board->getDeviceModel(); binding["asicModel"] = board->getAsicModel();
    binding["asicCount"] = board->getAsicCount(); binding["deviceId"] = id;
    binding["diagnosticDriver"] = true; binding["captureBuild"] = true;
    return !doc.overflowed();
}
void status(JsonDocument &doc, const FiveTratumCapture::Status &value) {
    const auto &capture = value.capture;
    doc["schemaVersion"] = BM1370Capture::SchemaVersion;
    doc["captureId"] = capture.captureId;
    auto state = doc["status"].to<JsonObject>();
    state["active"] = capture.state == BM1370Capture::State::Recording;
    state["frozen"] = capture.state == BM1370Capture::State::Frozen;
    state["recordCount"] = capture.records; state["capacity"] = capture.capacity;
    state["incompleteFlags"] = capture.flags; state["freezeReason"] = reason(capture.reason);
    state["droppedBusy"] = value.droppedBusy;
    state["startUs"] = capture.startUs; state["deadlineUs"] = capture.deadlineUs; state["stoppedUs"] = capture.stoppedUs;
    state["physicalWireComplete"] = false; state["physicalChipIdentity"] = false; state["independentWorkAssignment"] = false;
    auto heap = doc["heap"].to<JsonObject>();
    heap["captureStorageBytes"] = value.storageBytes;
    heap["freeInternal"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    heap["largestInternal"] = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    heap["minimumInternal"] = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    heap["freePsram"] = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    heap["largestPsram"] = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
void record(JsonArray records, const BM1370Capture::Record &value) {
    auto row = records.add<JsonObject>();
    row["sequence"] = value.sequence; row["timestampUs"] = value.timestampUs; row["generation"] = value.generation;
    row["kind"] = kind(value.kind); row["marker"] = value.marker;
    row["requestedLength"] = value.requestedLength; row["transportReturned"] = value.transportReturned;
    row["capturedLength"] = value.capturedLength; row["flags"] = value.flags; row["auxiliaryValue"] = value.auxiliaryValue;
    char data[BM1370Capture::MaxRawBytes * 2 + 1]; hex(value.bytes.data(), value.capturedLength, data);
    row["packetHex"] = data;
    if (value.kind == BM1370Capture::Kind::TxJob) {
        char header[161]; hex(value.header.data(), value.header.size(), header); row["headerHex"] = header;
        char difficulty[11]; snprintf(difficulty, sizeof(difficulty), "%lu", static_cast<unsigned long>(value.asicTicketDifficulty));
        row["ticketDifficulty"] = difficulty;
        row["logicalCounter"] = value.logicalJobCounter; row["versionMask"] = value.versionMask; row["poolIndex"] = value.poolIndex;
    }
}
esp_err_t response(httpd_req_t *req, bool page, uint64_t id = 0, size_t offset = 0) {
    FiveTratumCapture::Status snapshot;
    BM1370Capture::Record records[FiveTratumCapture::PageRecords]; size_t count = 0;
    if (page) {
        if (!FiveTratumCapture::copyPage(id, offset, records, FiveTratumCapture::PageRecords, count, snapshot))
            return error(req, "409 Conflict", "capture-not-frozen-or-session-mismatch");
    } else if (!FiveTratumCapture::snapshot(snapshot)) return error(req, "503 Service Unavailable", "capture-unavailable");
    // All serialization and socket work happens after the runtime lock releases.
    CaptureJsonAllocator allocator; JsonDocument doc(&allocator);
    status(doc, snapshot);
    if (!identity(doc)) return error(req, "503 Service Unavailable", "capture-identity-unavailable");
    auto output = doc["records"].to<JsonArray>();
    if (page) {
        doc["offset"] = offset; doc["nextOffset"] = offset + count;
        for (size_t i = 0; i < count; ++i) record(output, records[i]);
    }
    if (doc.overflowed()) return error(req, "503 Service Unavailable", "capture-export-memory-unavailable");
    return sendJsonResponse(req, doc);
}
}
esp_err_t GET_asic_capture(httpd_req_t *req) {
    ConGuard guard(http_server, req); if (!prepare(req)) return ESP_FAIL;
    const size_t length = httpd_req_get_url_query_len(req);
    if (!length) return response(req, false);
    char query[80], idText[24], offsetText[24]; uint32_t id, offset;
    if (length >= sizeof(query) || httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "captureId", idText, sizeof(idText)) != ESP_OK ||
        httpd_query_key_value(query, "offset", offsetText, sizeof(offsetText)) != ESP_OK ||
        !decimal(idText, id) || !id || !decimal(offsetText, offset) || offset > BM1370Capture::MaxRecords)
        return error(req, "400 Bad Request", "invalid-capture-page");
    // Accept exactly these two keys once, in either order.
    char expected[80]; snprintf(expected, sizeof(expected), "captureId=%lu&offset=%lu", static_cast<unsigned long>(id), static_cast<unsigned long>(offset));
    if (strcmp(query, expected)) {
        snprintf(expected, sizeof(expected), "offset=%lu&captureId=%lu", static_cast<unsigned long>(offset), static_cast<unsigned long>(id));
        if (strcmp(query, expected)) return error(req, "400 Bad Request", "invalid-capture-page");
    }
    return response(req, true, id, offset);
}
esp_err_t POST_asic_capture(httpd_req_t *req) {
    ConGuard guard(http_server, req); if (!prepare(req)) return ESP_FAIL;
    if (!req->content_len || req->content_len > 256) return error(req, "400 Bad Request", "invalid-capture-request");
    JsonDocument doc;
    if (getJsonData(req, doc) != ESP_OK || !doc.is<JsonObject>() || !doc["action"].is<const char *>())
        return error(req, "400 Bad Request", "invalid-capture-request");
    const char *action = doc["action"];
    if (!strcmp(action, "arm")) {
        if (doc.size() != 2 || !doc["durationMs"].is<uint32_t>()) return error(req, "400 Bad Request", "invalid-capture-request");
        uint32_t duration = doc["durationMs"]; uint64_t id;
        if (!duration || duration > 2000) return error(req, "400 Bad Request", "invalid-capture-duration");
        FiveTratumMining::MiningOperation operation(FiveTratumMining::operationGate());
        Board *board = SYSTEM_MODULE.getBoard();
        if (!operation || !board->getAsicDriver() || strcmp(board->getAsicDriver()->getName(), "BM1370") ||
            static_cast<BM1370 *>(board->getAsicDriver())->observedAsicCount() != 4)
            return error(req, "409 Conflict", "capture-requires-running-enumerated-chain");
        if (!FiveTratumCapture::arm(duration * 1000, id)) return error(req, "409 Conflict", "capture-active-or-psram-unavailable");
    } else if (!strcmp(action, "freeze")) {
        if (doc.size() != 2 || !doc["captureId"].is<uint32_t>() || !doc["captureId"].as<uint32_t>())
            return error(req, "400 Bad Request", "invalid-capture-request");
        if (!FiveTratumCapture::freeze(doc["captureId"].as<uint32_t>())) return error(req, "409 Conflict", "capture-session-mismatch");
    } else return error(req, "400 Bad Request", "invalid-capture-action");
    return response(req, false);
}
