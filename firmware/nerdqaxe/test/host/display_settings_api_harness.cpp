#include "platform.h"
#include "display_profile_json.h"
#include <algorithm>
#include <cassert>
#include <iostream>

#if defined(NERDQAXEPLUS2) || defined(NERDOCTAXEGAMMA)
static_assert(FiveTratumDisplay::SupportsRuntimeWideProfiles);
#else
static_assert(!FiveTratumDisplay::SupportsRuntimeWideProfiles);
#endif

std::vector<std::string> effects;
std::string selectedProfile;
System SYSTEM_MODULE;
PowerManagement POWER_MANAGEMENT_MODULE;
StratumManager manager;
StratumManager *STRATUM_MANAGER = &manager;
FactoryOTAUpdate FACTORY_OTA_UPDATER;

esp_err_t is_network_allowed(httpd_req_t *req) { return req->authorized ? ESP_OK : ESP_FAIL; }
esp_err_t set_cors_headers(httpd_req_t *req) { return req->cors ? ESP_OK : ESP_FAIL; }
esp_err_t httpd_resp_send_err(httpd_req_t *req, int status, const char *message) {
    req->status = status;
    req->response = message;
    return ESP_FAIL;
}
esp_err_t httpd_resp_send_500(httpd_req_t *req) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal error");
}
esp_err_t validateOTP(httpd_req_t *req) {
    return req->otp ? ESP_OK : httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "OTP required");
}
esp_err_t getJsonData(httpd_req_t *req, JsonDocument &doc) {
    return deserializeJson(doc, req->body)
        ? httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON") : ESP_OK;
}
esp_err_t httpd_resp_send_chunk(httpd_req_t *, const char *, size_t) { return ESP_OK; }
esp_err_t httpd_resp_set_status(httpd_req_t *req, const char *status) {
    req->status = std::stoi(status);
    return ESP_OK;
}
esp_err_t httpd_resp_sendstr(httpd_req_t *req, const char *message) {
    req->response = message;
    return ESP_OK;
}
esp_err_t sendJsonResponse(httpd_req_t *req, JsonDocument &doc) {
    serializeJson(doc, req->response);
    return ESP_OK;
}

namespace {
using Handler = esp_err_t (*)(httpd_req_t *);
bool occurred(const char *name) {
    return std::find(effects.begin(), effects.end(), name) != effects.end();
}
// Both generations receive their own valid display booleans and pool fields.
// Their unchanged production handlers choose the fields they understand.
const char *MUTATIONS = R"({
    "ssid":"fixture-network","wifiPass":"fixture-only","hostname":"fixture-host",
    "frequency":577,"coreVoltage":1137,"vrFrequency":25000,
    "jobInterval":300,"stratumDifficulty":256,
    "flipScreen":true,"flipscreen":true,"invertScreen":true,"invertscreen":true,
    "autoScreenOff":true,"autoscreenoff":true,"overheat_temp":72,
    "invertFanPolarity":true,"invertfanpolarity":true,
    "autofanspeed":0,"manualFanSpeed":71,"pidTargetTemp":57,"pidP":1.2,"pidI":0.4,"pidD":0.1,
    "mempoolCustom":true,"mempoolUrl":"https://fixture.invalid",
    "stratumKeep":true,"stratum_keep":true,"canMaster":true,
    "fans":[{"mode":1,"manualSpeed":71,"overheatTemp":72,
      "pid":{"targetTemp":57,"p":1.2,"i":0.4,"d":0.1}}],
    "poolMode":1,"poolBalance":50,
    "stratumURL":"fixture-pool.invalid","stratumPort":3333,"stratumUser":"fixture.worker",
    "pools":[{"url":"fixture-a.invalid","port":3333,"user":"fixture.a","password":"fixture-a",
      "enonceSubscribe":true,"tls":true,"protocol":1,"sv2AuthorityPubkey":"fixture-key",
      "sv2ChannelType":1,"coinbaseVerifyMode":1,"coinbaseMaxFee":2.5,"coinbaseVerifyForce":true},
      {"url":"fixture-b.invalid","port":3334,"user":"fixture.b","password":"fixture-b",
      "enonceSubscribe":true,"tls":true,"protocol":1,"sv2AuthorityPubkey":"fixture-key",
      "sv2ChannelType":1,"coinbaseVerifyMode":1,"coinbaseMaxFee":2.5,"coinbaseVerifyForce":true}]
})";

std::string payload(const char *rawProfile = nullptr) {
    std::string body = MUTATIONS;
    if (rawProfile) {
        const size_t closing = body.rfind('}');
        assert(closing != std::string::npos);
        body.insert(closing, std::string(",\"displayProfile\":") + rawProfile);
    }
    return body;
}
httpd_req_t call(Handler handler, const std::string &body, bool network = true,
                 bool otp = true, bool cors = true) {
    effects.clear();
    selectedProfile = "unchanged";
    FACTORY_OTA_UPDATER.reads = 0;
    httpd_req_t req;
    req.body = body;
    req.authorized = network;
    req.otp = otp;
    req.cors = cors;
    handler(&req);
    return req;
}
void expectRejected(Handler handler, const char *profile) {
    const auto req = call(handler, payload(profile));
    if (req.status != 400 || !effects.empty() || selectedProfile != "unchanged") {
        std::cerr << "Rejected profile mutated settings: " << profile
                  << " status=" << req.status << " effects=" << effects.size() << '\n';
        std::abort();
    }
    assert(!req.response.empty());
    assert(FACTORY_OTA_UPDATER.reads == 0); // Strict validation precedes the OTA query too.
}
void expectAccepted(Handler handler, const char *profile, const char *expectedId) {
    const auto req = call(handler, payload(profile));
    assert(req.status == 200);
    assert(occurred("Config::setHostname") && occurred("Config::setWifiSSID"));
    assert(occurred("Config::setAsicFrequency") && occurred("Config::setAsicVoltage"));
    assert(occurred("Config::setFanManualSpeed") && occurred("Config::setFlipScreen"));
    assert(occurred("Config::flush"));
    assert(occurred("Board::loadSettings") && occurred("FanController::loadSettings"));
    assert(occurred("System::loadSettings") && occurred("Stratum::loadSettings"));
    if (handler == PATCH_update_settings) assert(occurred("Stratum::saveSettings"));
    else {
        assert(occurred("Config::setStratumURL") && occurred("Config::setStratumFallbackURL"));
        assert(occurred("Stratum::rerunVerification") && occurred("Stratum::clearVerifyBlocked"));
    }
    if (expectedId) {
        assert(selectedProfile == expectedId);
        assert(std::count(effects.begin(), effects.end(), "Config::setDisplayProfile") == 1);
        assert(FACTORY_OTA_UPDATER.reads == 1);
    } else {
        assert(selectedProfile == "unchanged");
        assert(!occurred("Config::setDisplayProfile"));
        assert(FACTORY_OTA_UPDATER.reads == 0);
    }
}
void checkFactoryOtaBusyGuard(Handler handler) {
    for (const auto busy : {FactoryOTAUpdate::OtaStatus{true, false},
                           FactoryOTAUpdate::OtaStatus{false, true},
                           FactoryOTAUpdate::OtaStatus{true, true}}) {
        FACTORY_OTA_UPDATER.status = busy;
        const auto req = call(handler, payload("\"320x170\""));
        assert(req.status == 409 && effects.empty() && selectedProfile == "unchanged");
        assert(req.response.find("Firmware update in progress") != std::string::npos);
        assert(FACTORY_OTA_UPDATER.reads == 1);
        // Requests that do not touch the display profile retain existing behavior.
        expectAccepted(handler, nullptr, nullptr);
    }
    FACTORY_OTA_UPDATER.status = {};
    expectAccepted(handler, "\"320x170\"", "320x170");
}
std::string get(Handler handler, JsonDocument &doc) {
    effects.clear();
    httpd_req_t req;
    assert(handler(&req) == ESP_OK && req.status == 200 && effects.empty());
    assert(!deserializeJson(doc, req.response));
    return req.response;
}
#if defined(NERDQAXEPLUS2) || defined(NERDOCTAXEGAMMA)
void saveOnlyDisplayProfile(Handler handler, const char *id) {
    effects.clear();
    FACTORY_OTA_UPDATER.reads = 0;
    httpd_req_t req;
    req.body = std::string("{\"displayProfile\":\"") + id + "\"}";
    assert(handler(&req) == ESP_OK && req.status == 200);
    assert(selectedProfile == id);
    assert(std::count(effects.begin(), effects.end(), "Config::setDisplayProfile") == 1);
    assert(std::count(effects.begin(), effects.end(), "Config::flush") == 1);
    assert(FACTORY_OTA_UPDATER.reads == 1);
    for (const auto &effect : effects)
        if (effect.compare(0, std::strlen("Config::set"), "Config::set") == 0)
            assert(effect == "Config::setDisplayProfile");
}
#endif
void checkDurableSettingsAndBootLatchedMetadata(Handler handler) {
    using namespace FiveTratumDisplay;
    // Simulated boots reset only the production latch object; GETs and PATCHes
    // never invoke latch. No settings cache or hardware initializer is replaced.
    displayProfileRuntime = DisplayProfileRuntime{};
    selectedProfile = "320x170";
    displayProfileRuntime.latch(selectedProfile.c_str());
    JsonDocument settings, system;
    get(GET_V2_settings, settings);
    assert(settings["displayProfile"] == "320x170");
    for (const char *key : {"displayActiveProfile", "displayRestartRequired", "displayWidth",
             "displayHeight", "displayProfileOptions", "displayProfileCapabilityVersion",
             "displayProfileStorageValid", "displayProfileExperimental"})
        assert(settings[key].isUnbound());
    get(GET_V2_system, system);
    assert(system["displayActiveProfile"] == "320x170");
    assert(system["displayWidth"] == 320 && system["displayHeight"] == 170);
    assert(!system["displayRestartRequired"].as<bool>());
#if defined(NERDQAXEPLUS2) || defined(NERDOCTAXEGAMMA)
    assert(system["displayProfileOptions"].size() == 3);
    assert(system["displayProfileOptions"][0] == "320x170");
    assert(system["displayProfileOptions"][1] == "480x320-rgb");
    assert(system["displayProfileOptions"][2] == "480x320-bgr");
    for (const char *id : {"480x320-rgb", "480x320-bgr"}) {
        // Exercise the actual PATCH path with only a screen choice: it cannot
        // replace clocks, cooling, network, pools or the currently active LCD.
        saveOnlyDisplayProfile(handler, id);
        const auto beforeBoot = get(GET_V2_settings, settings);
        assert(settings["displayProfile"] == id);
        get(GET_V2_system, system);
        assert(system["displayProfile"] == id);
        assert(system["displayActiveProfile"] == "320x170");
        assert(system["displayWidth"] == 320 && system["displayHeight"] == 170);
        assert(system["displayProfileStorageValid"].as<bool>());
        assert(system["displayRestartRequired"].as<bool>());
        assert(!system["displayProfileExperimental"].as<bool>()); // Active panel is stock.
        displayProfileRuntime.latch(selectedProfile.c_str());
        assert(displayProfileRuntime.active().width == 320); // No hot-switch.
        displayProfileRuntime = DisplayProfileRuntime{};
        displayProfileRuntime.latch(selectedProfile.c_str());
        const auto afterBoot = get(GET_V2_settings, settings);
        assert(beforeBoot == afterBoot); // Durable settings hashes stay unchanged.
        get(GET_V2_system, system);
        assert(system["displayActiveProfile"] == id);
        assert(system["displayWidth"] == 480 && system["displayHeight"] == 320);
        assert(!system["displayRestartRequired"].as<bool>());
        assert(system["displayProfileExperimental"].as<bool>());
        assert(displayProfileRuntime.active().bgr == (std::strcmp(id, "480x320-bgr") == 0));

        // Changing colour order is also boot-latched. Reverting a pending
        // choice clears the restart requirement without changing live state.
        const char *other = displayProfileRuntime.active().bgr ? "480x320-rgb" : "480x320-bgr";
        saveOnlyDisplayProfile(handler, other);
        get(GET_V2_system, system);
        assert(system["displayActiveProfile"] == id && system["displayProfile"] == other);
        assert(system["displayRestartRequired"].as<bool>());
        saveOnlyDisplayProfile(handler, id);
        get(GET_V2_system, system);
        assert(system["displayActiveProfile"] == id && !system["displayRestartRequired"].as<bool>());
        saveOnlyDisplayProfile(handler, "320x170");
        get(GET_V2_system, system);
        assert(system["displayActiveProfile"] == id && system["displayRestartRequired"].as<bool>());
        displayProfileRuntime = DisplayProfileRuntime{};
        displayProfileRuntime.latch(selectedProfile.c_str());
        get(GET_V2_system, system);
        assert(system["displayActiveProfile"] == "320x170");
        assert(system["displayWidth"] == 320 && system["displayHeight"] == 170);
        assert(!system["displayRestartRequired"].as<bool>());
        assert(!system["displayProfileExperimental"].as<bool>());
    }
#else
    (void)handler;
    assert(system["displayProfileOptions"].size() == 1);
    for (const char *id : {"480x320-rgb", "480x320-bgr"}) {
        selectedProfile = id; // Simulate an unsupported stored value.
        get(GET_V2_settings, settings);
        assert(settings["displayProfile"] == "320x170");
        get(GET_V2_system, system);
        assert(system["displayActiveProfile"] == "320x170");
        assert(!system["displayProfileStorageValid"].as<bool>());
        assert(!system["displayRestartRequired"].as<bool>());
        assert(selectedProfile == id); // GET does not rewrite storage.
    }
#endif
}
}

int main() {
    unsigned rejected = 0;
    for (Handler handler : {PATCH_update_settings, PATCH_V2_settings}) {
        expectAccepted(handler, nullptr, nullptr); // Legacy requests remain valid.
        expectAccepted(handler, "\"320x170\"", "320x170");
        for (const char *invalid : {
            "null", "0", "320", "480.0", "true", "false", "[]", "{}",
            "[\"320x170\"]", "{\"id\":\"320x170\"}", "\"\"", "\"unknown\"",
            "\"320X170\"", "\"320x170-extra\"", "\" 320x170\"", "\"320x170 \"",
            "\"480x320-RGB\"", "\"480x320\"", "\"480x320-bgr-extra\"",
            "\"320x170\\u0000extra\"", "\"480x320-rgb\\u0000extra\"",
            "\"480x320-bgr\\u0000\"", "\"320x170\\n\""
        }) { expectRejected(handler, invalid); ++rejected; }
#if defined(NERDQAXEPLUS2) || defined(NERDOCTAXEGAMMA)
        expectAccepted(handler, "\"480x320-rgb\"", "480x320-rgb");
        expectAccepted(handler, "\"480x320-bgr\"", "480x320-bgr");
#else
        expectRejected(handler, "\"480x320-rgb\"");
        expectRejected(handler, "\"480x320-bgr\"");
        rejected += 2;
#endif
        auto req = call(handler, payload("\"320x170\""), false);
        assert(req.status == 401 && effects.empty() && selectedProfile == "unchanged");
        req = call(handler, payload("\"320x170\""), true, false);
        assert(req.status == 401 && effects.empty() && selectedProfile == "unchanged");
        req = call(handler, payload("\"320x170\""), true, true, false);
        assert(req.status == 500 && effects.empty() && selectedProfile == "unchanged");
        req = call(handler, "{\"displayProfile\":");
        assert(req.status == 400 && effects.empty() && selectedProfile == "unchanged");
        checkFactoryOtaBusyGuard(handler);
        checkDurableSettingsAndBootLatchedMetadata(handler);
    }
    std::cout << "Actual legacy/v2 PATCH handlers passed " << rejected
              << " mixed-payload profile rejection checks with zero settings/pool/flush/reload effects; "
                 "valid profiles/auth paths, factory-OTA busy guard and durable GET-settings/boot-latched GET-system metadata passed\n";
}
