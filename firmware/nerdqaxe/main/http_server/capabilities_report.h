#pragma once

#include <cstddef>
#include <cstdint>
#include "ArduinoJson.h"

namespace FiveTratumCapabilities {

constexpr int MAX_ASICS = 64;
constexpr size_t DEVICE_ID_SIZE = 38;
constexpr size_t MAX_JSON_BYTES = 32768;
constexpr const char *UPSTREAM_TAG = "v1.1.0-rc1-test1";
constexpr const char *UPSTREAM_REVISION = "8b45522a6695c6bbccc3d032370fe08a29856d80";
constexpr const char *EXTENSION_BUILD = "5tratumfw-nerd-capabilities-a1";

// Read-only snapshot: no board, UART, regulator, NVS, or network operations here.
struct Snapshot {
    char deviceId[DEVICE_ID_SIZE] = {};
    const char *firmwareVersion = nullptr;
    const char *boardModel = nullptr;
    const char *boardProfile = nullptr;
    const char *asicModel = nullptr;
    int asicCount = 0;
    bool savedFrequencyPresent = false;
    bool savedVoltagePresent = false;
    uint16_t savedFrequencyMHz = 0;
    uint16_t savedVoltageMv = 0;
    int defaultFrequencyMHz = 0;
    int defaultVoltageMv = 0;
    int sourceMaxFrequencyMHz = 0;
    int sourceMaxVoltageMv = 0;
    bool initialized = false;
    float chipTemperatures[MAX_ASICS] = {};
};

// SHA-256 namespace digest is supplied by the platform adapter. Only the first
// 128 bits are exposed. No MAC, IP, hostname, pool username, or secrets are sent.
bool formatDeviceId(const uint8_t digest[32], char output[DEVICE_ID_SIZE]);
bool buildReport(JsonDocument &doc, const Snapshot &snapshot);

} // namespace FiveTratumCapabilities
