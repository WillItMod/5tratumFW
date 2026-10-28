#pragma once

// External services only. The PATCH bodies and display validator are production
// source. Every mutating service records an event, even if it changes no value.
#include "ArduinoJson.h"
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using esp_err_t = int;
inline constexpr esp_err_t ESP_OK = 0;
inline constexpr esp_err_t ESP_FAIL = -1;
inline constexpr int HTTPD_400_BAD_REQUEST = 400;
inline constexpr int HTTPD_401_UNAUTHORIZED = 401;
inline constexpr int HTTPD_500_INTERNAL_SERVER_ERROR = 500;
#define ESP_LOGI(...) ((void)0)
#define safe_free(pointer) std::free(pointer)
inline constexpr int MALLOC_CAP_SPIRAM = 0, MALLOC_CAP_INTERNAL = 1;
inline size_t heap_caps_get_free_size(int) { return 1024; }
inline int64_t esp_timer_get_time() { return 1000000; }
struct esp_app_desc_t { const char *version = "fixture-version"; };
inline const esp_app_desc_t *esp_app_get_description() {
    static const esp_app_desc_t description;
    return &description;
}

extern std::vector<std::string> effects;
extern std::string selectedProfile;
inline void record(const char *name) { effects.emplace_back(name); }
inline int profileLockDepth = 0, powerLockDepth = 0;
namespace FiveTratumProfiles {
struct ProfileStoreGuard {
    ProfileStoreGuard() { assert(powerLockDepth == 0); ++profileLockDepth; }
    ~ProfileStoreGuard() { --profileLockDepth; }
};
}
template<class T> struct LockGuard {
    T &object;
    explicit LockGuard(T &value) : object(value) { object.lock(); }
    ~LockGuard() { object.unlock(); }
};

struct httpd_req_t {
    std::string body, response;
    int status = 200;
    bool authorized = true, otp = true, cors = true;
};
inline constexpr int http_server = 0;
struct ConGuard { ConGuard(int, httpd_req_t *) {} };
esp_err_t is_network_allowed(httpd_req_t *req);
esp_err_t set_cors_headers(httpd_req_t *req);
esp_err_t validateOTP(httpd_req_t *req);
esp_err_t getJsonData(httpd_req_t *req, JsonDocument &doc);
esp_err_t httpd_resp_send_err(httpd_req_t *req, int status, const char *message);
esp_err_t httpd_resp_send_500(httpd_req_t *req);
esp_err_t httpd_resp_send_chunk(httpd_req_t *req, const char *, size_t);
esp_err_t httpd_resp_set_status(httpd_req_t *req, const char *status);
esp_err_t httpd_resp_sendstr(httpd_req_t *req, const char *message);
inline esp_err_t httpd_resp_set_type(httpd_req_t *, const char *) { return ESP_OK; }
esp_err_t sendJsonResponse(httpd_req_t *req, JsonDocument &doc);

struct PSRAMAllocator final : ArduinoJson::Allocator {
    void *allocate(size_t size) override { return std::malloc(size); }
    void deallocate(void *pointer) override { std::free(pointer); }
    void *reallocate(void *pointer, size_t size) override { return std::realloc(pointer, size); }
};

namespace Config {
// Variadic mocks preserve the actual handler arguments while counting all
// mutations. Adding a production setter requires explicitly adding its mock.
#define CONFIG_SETTER(name) template<class... Arguments> inline void name(Arguments...) { record("Config::" #name); }
CONFIG_SETTER(setWifiSSID)
CONFIG_SETTER(setWifiPass)
CONFIG_SETTER(setHostname)
CONFIG_SETTER(setAsicVoltage)
CONFIG_SETTER(setAsicFrequency)
CONFIG_SETTER(setAsicJobInterval)
CONFIG_SETTER(setStratumDifficulty)
CONFIG_SETTER(setFlipScreen)
CONFIG_SETTER(setOverheatTemp)
CONFIG_SETTER(setInvertScreen)
CONFIG_SETTER(setFanPolarity)
CONFIG_SETTER(setTempControlMode)
CONFIG_SETTER(setFanSpeed)
CONFIG_SETTER(setAutoScreenOff)
CONFIG_SETTER(setStratumKeepaliveEnabled)
CONFIG_SETTER(setPidTargetTemp)
CONFIG_SETTER(setPidP)
CONFIG_SETTER(setPidI)
CONFIG_SETTER(setPidD)
CONFIG_SETTER(setVrFrequency)
CONFIG_SETTER(setFanMode)
CONFIG_SETTER(setFanManualSpeed)
CONFIG_SETTER(setFanOverheatTemp)
CONFIG_SETTER(setFanPidTargetTemp)
CONFIG_SETTER(setFanPidP)
CONFIG_SETTER(setFanPidI)
CONFIG_SETTER(setFanPidD)
CONFIG_SETTER(setFanPidUseMax)
CONFIG_SETTER(setMempoolCustom)
CONFIG_SETTER(setMempoolUrl)
CONFIG_SETTER(setCanEnabled)
CONFIG_SETTER(setPoolMode)
CONFIG_SETTER(setPoolBalance)
CONFIG_SETTER(setStratumURL)
CONFIG_SETTER(setStratumFallbackURL)
CONFIG_SETTER(setStratumPortNumber)
CONFIG_SETTER(setStratumFallbackPortNumber)
CONFIG_SETTER(setStratumUser)
CONFIG_SETTER(setStratumFallbackUser)
CONFIG_SETTER(setStratumPass)
CONFIG_SETTER(setStratumFallbackPass)
CONFIG_SETTER(setStratumEnonceSubscribe)
CONFIG_SETTER(setStratumFallbackEnonceSubscribe)
CONFIG_SETTER(setStratumTLS)
CONFIG_SETTER(setStratumFallbackTLS)
CONFIG_SETTER(setStratumProtocol)
CONFIG_SETTER(setFallbackStratumProtocol)
CONFIG_SETTER(setSV2AuthorityPubkey)
CONFIG_SETTER(setFallbackSV2AuthorityPubkey)
CONFIG_SETTER(setSV2ChannelType)
CONFIG_SETTER(setFallbackSV2ChannelType)
CONFIG_SETTER(setSV2RequireAuth)
CONFIG_SETTER(setFallbackSV2RequireAuth)
CONFIG_SETTER(setCoinbaseVerifyMode)
CONFIG_SETTER(setCoinbaseMaxFee)
CONFIG_SETTER(setCoinbaseVerifyForce)
#undef CONFIG_SETTER
inline void setDisplayProfile(const char *id) {
    record("Config::setDisplayProfile");
    selectedProfile = id;
}
inline void flush() { record("Config::flush"); }
inline uint16_t getCoinbaseVerifyMode(int) { return 0; }
inline uint16_t getCoinbaseMaxFee(int) { return 0; }
inline bool getCoinbaseVerifyForce(int) { return false; }
inline char *getDisplayProfile() { return strdup(selectedProfile.c_str()); }
#define CONFIG_STRING_GETTER(name) inline char *name() { return strdup("fixture-value"); }
CONFIG_STRING_GETTER(getStratumURL)
CONFIG_STRING_GETTER(getStratumUser)
CONFIG_STRING_GETTER(getStratumFallbackURL)
CONFIG_STRING_GETTER(getStratumFallbackUser)
CONFIG_STRING_GETTER(getSV2AuthorityPubkey)
CONFIG_STRING_GETTER(getFallbackSV2AuthorityPubkey)
CONFIG_STRING_GETTER(getHostname)
CONFIG_STRING_GETTER(getWifiSSID)
CONFIG_STRING_GETTER(getMempoolUrl)
#undef CONFIG_STRING_GETTER
#define CONFIG_GETTER(name) template<class... Arguments> inline uint16_t name(Arguments...) { return 1; }
CONFIG_GETTER(isOTPEnabled)
CONFIG_GETTER(isCanEnabled)
CONFIG_GETTER(getPoolMode)
CONFIG_GETTER(getPoolBalance)
CONFIG_GETTER(isStratumKeepaliveEnabled)
CONFIG_GETTER(getStratumDifficulty)
CONFIG_GETTER(getStratumPortNumber)
CONFIG_GETTER(isStratumEnonceSubscribe)
CONFIG_GETTER(isStratumTLS)
CONFIG_GETTER(getStratumProtocol)
CONFIG_GETTER(getSV2ChannelType)
CONFIG_GETTER(getStratumFallbackPortNumber)
CONFIG_GETTER(isStratumFallbackEnonceSubscribe)
CONFIG_GETTER(isStratumFallbackTLS)
CONFIG_GETTER(getFallbackStratumProtocol)
CONFIG_GETTER(getFallbackSV2ChannelType)
CONFIG_GETTER(isSV2RequireAuth)
CONFIG_GETTER(isFallbackSV2RequireAuth)
CONFIG_GETTER(getFanMode)
CONFIG_GETTER(getFanManualSpeed)
CONFIG_GETTER(getFanOverheatTemp)
CONFIG_GETTER(isMempoolCustom)
CONFIG_GETTER(isInvertScreenEnabled)
CONFIG_GETTER(isAutoScreenOffEnabled)
#undef CONFIG_GETTER
}

struct PidSettings { int targetTemp = 57, p = 120, i = 40, d = 10; };
struct Board {
    enum class Error { NONE, COINBASE_VERIFY_FAULT };
    void loadSettings() { assert(profileLockDepth > 0 && powerLockDepth > 0); record("Board::loadSettings"); }
    const char *getAsicModel() { return "BM1370"; }
    const char *getDeviceModel() {
#if defined(NERDQAXEPLUS2)
        return "NerdQAxe++";
#else
        return "NerdOCTAXE-\xCE\xB3";
#endif
    }
#define BOARD_GETTER(name) uint16_t name() { return 1; }
    BOARD_GETTER(hasCanExtension)
    BOARD_GETTER(getAsicFrequency)
    BOARD_GETTER(getAsicVoltageMillis)
    BOARD_GETTER(getVrFrequency)
    BOARD_GETTER(getDefaultAsicFrequency)
    BOARD_GETTER(getDefaultAsicVoltageMillis)
    BOARD_GETTER(getDefaultVrFrequency)
    BOARD_GETTER(getEcoAsicFrequency)
    BOARD_GETTER(getEcoAsicVoltageMillis)
    BOARD_GETTER(getAbsMinAsicVoltageMillis)
    BOARD_GETTER(getAbsMaxAsicVoltageMillis)
    BOARD_GETTER(getAsicJobIntervalMs)
    BOARD_GETTER(isPIDAvailable)
    BOARD_GETTER(isInvertFanPolarityEnabled)
    BOARD_GETTER(isFlipScreenEnabled)
#undef BOARD_GETTER
    std::vector<uint32_t> getFrequencyOptions() { return {500, 600}; }
    std::vector<uint32_t> getVoltageOptions() { return {1100, 1150}; }
    int getNumFans() { return 2; }
    const char *getFanLabel(int) { return "fixture-fan"; }
    uint16_t getFanMode(int) { return 1; }
    PidSettings *getPidSettings(int) { static PidSettings pid; return &pid; }
};
struct System {
    Board board;
    Board *getBoard() { return &board; }
    Board::Error getBoardError() { return Board::Error::COINBASE_VERIFY_FAULT; }
    void clearBoardError() { record("System::clearBoardError"); }
    void loadSettings() { record("System::loadSettings"); }
    int64_t getStartTime() { return 0; }
    const char *getLastResetReason() { return "fixture-reset"; }
    const char *getMacAddress() { return "fixture-mac"; }
    const char *getIPAddress() { return "fixture-address"; }
    const char *getWifiStatus() { return "fixture-status"; }
    int get_wifi_rssi() { return -50; }
};
struct FanController { void loadSettings() { record("FanController::loadSettings"); } };
struct PowerManagement {
    FanController fans;
    void lock() { assert(profileLockDepth > 0); ++powerLockDepth; }
    void unlock() { --powerLockDepth; }
    FanController &getFanController() { return fans; }
};
using PowerManagementTask = PowerManagement;
struct StratumManager {
    void saveSettings(JsonDocument &) { record("Stratum::saveSettings"); }
    void loadSettings() { record("Stratum::loadSettings"); }
    void clearVerifyBlocked(int) { record("Stratum::clearVerifyBlocked"); }
    void resetVerificationStats(int) { record("Stratum::resetVerificationStats"); }
    void rerunVerification(int) { record("Stratum::rerunVerification"); }
    bool isVerifyBlocked(int) { return false; }
};
extern System SYSTEM_MODULE;
extern PowerManagement POWER_MANAGEMENT_MODULE;
extern StratumManager *STRATUM_MANAGER;
struct Network { bool isApActive() { return false; } };
inline Network NETWORK;
struct FactoryOTAUpdate {
    struct OtaStatus { bool pending = false, running = false; };
    OtaStatus status;
    unsigned reads = 0;
    void getStatus(OtaStatus *out) { ++reads; *out = status; }
};
extern FactoryOTAUpdate FACTORY_OTA_UPDATER;

esp_err_t PATCH_update_settings(httpd_req_t *req);
esp_err_t PATCH_V2_settings(httpd_req_t *req);
esp_err_t GET_V2_settings(httpd_req_t *req);
esp_err_t GET_V2_system(httpd_req_t *req);
