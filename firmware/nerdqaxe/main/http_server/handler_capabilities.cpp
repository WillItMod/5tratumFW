#include "handler_capabilities.h"

#include <cstring>
#include "esp_app_desc.h"
#include "esp_mac.h"
#include "mbedtls/sha256.h"
#include "capabilities_report.h"
#include "global_state.h"
#include "nvs_config.h"
#include "psram_allocator.h"
#include "http_cors.h"
#include "http_utils.h"

namespace {

esp_err_t unavailable(httpd_req_t *req, const char *reason) {
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, reason, HTTPD_RESP_USE_STRLEN);
}

const char *compiledBoardProfile() {
#if defined(NERDQAXEPLUS2)
    return "NERDQAXEPLUS2";
#elif defined(NERDQAXEPLUS)
    return "NERDQAXEPLUS";
#elif defined(NERDOCTAXEGAMMA)
    return "NERDOCTAXEGAMMA";
#elif defined(NERDOCTAXEPLUS)
    return "NERDOCTAXEPLUS";
#elif defined(NERDHAXEGAMMA)
    return "NERDHAXEGAMMA";
#elif defined(NERDAXEGAMMA)
    return "NERDAXEGAMMA";
#elif defined(NERDAXE)
    return "NERDAXE";
#elif defined(NERDEKO)
    return "NERDEKO";
#elif defined(NERDQX)
    return "NERDQX";
#elif defined(Q1370)
    return "Q1370";
#elif defined(Q1373)
    return "Q1373";
#else
    return "unknown";
#endif
}

} // namespace

bool readFiveTratumPhysicalDeviceId(char output[FiveTratumCapabilities::DEVICE_ID_SIZE]) {
    uint8_t baseMac[6];
    if (esp_efuse_mac_get_default(baseMac) != ESP_OK) return false;
    bool nonzero = false;
    bool notAllOnes = false;
    for (uint8_t byte : baseMac) { nonzero |= byte != 0; notAllOnes |= byte != 0xff; }
    if (!nonzero || !notAllOnes || (baseMac[0] & 1)) return false;
    static constexpr char NAMESPACE[] = "5tratum/device/v1";
    uint8_t input[sizeof(NAMESPACE) + sizeof(baseMac)];
    memcpy(input, NAMESPACE, sizeof(NAMESPACE)); // include separator NUL
    memcpy(input + sizeof(NAMESPACE), baseMac, sizeof(baseMac));
    uint8_t digest[32];
    if (mbedtls_sha256(input, sizeof(input), digest, 0) != 0) return false;
    return FiveTratumCapabilities::formatDeviceId(digest, output);
}

esp_err_t GET_five_tratum_capabilities(httpd_req_t *req) {
    ConGuard guard(http_server, req);
    if (is_network_allowed(req) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (set_cors_headers(req) != ESP_OK) return httpd_resp_send_500(req);

    Board *board = SYSTEM_MODULE.getBoard();
    if (!board || board->getAsicCount() < 1 || board->getAsicCount() > FiveTratumCapabilities::MAX_ASICS)
        return unavailable(req, "Board profile unavailable");
    FiveTratumCapabilities::Snapshot snapshot;
    if (!readFiveTratumPhysicalDeviceId(snapshot.deviceId))
        return unavailable(req, "Stable identity unavailable");
    snapshot.firmwareVersion = esp_app_get_description()->version;
    snapshot.boardModel = board->getDeviceModel();
    snapshot.boardProfile = compiledBoardProfile();
    snapshot.asicModel = board->getAsicModel();
    snapshot.asicCount = board->getAsicCount();
    snapshot.savedFrequencyPresent = Config::cfgReadU16(NVS_CONFIG_ASIC_FREQ, snapshot.savedFrequencyMHz);
    snapshot.savedVoltagePresent = Config::cfgReadU16(NVS_CONFIG_ASIC_VOLTAGE, snapshot.savedVoltageMv);
    snapshot.defaultFrequencyMHz = board->getDefaultAsicFrequency();
    snapshot.defaultVoltageMv = board->getDefaultAsicVoltageMillis();
    snapshot.sourceMaxFrequencyMHz = board->getAbsMaxAsicFrequency();
    snapshot.sourceMaxVoltageMv = board->getAbsMaxAsicVoltageMillis();
    snapshot.initialized = board->isInitialized();
    // Board::getChipTemp dereferences its allocated array: never read it before
    // normal board/ASIC initialization has completed. No new temperature probe.
    if (snapshot.initialized) for (int i = 0; i < snapshot.asicCount; ++i)
        snapshot.chipTemperatures[i] = board->getChipTemp(i);

    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);
    if (!FiveTratumCapabilities::buildReport(doc, snapshot))
        return unavailable(req, "Capability report unavailable");
    return sendJsonResponse(req, doc);
}
