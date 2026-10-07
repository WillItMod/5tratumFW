#include "capabilities_report.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace FiveTratumCapabilities {
namespace {

bool validLabel(const char *value) {
    if (!value) return false;
    const size_t size = strnlen(value, 65);
    if (size == 0 || size > 64) return false;
    for (size_t i = 0; i < size; ++i) {
        const char c = value[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == ' ' || c == '.' || c == '_' ||
              c == '+' || c == '-' || c == '/')) return false;
    }
    return true;
}

bool validDeviceId(const char *value) {
    if (strnlen(value, DEVICE_ID_SIZE) != DEVICE_ID_SIZE - 1 ||
        strncmp(value, "5tfw:", 5) != 0) return false;
    bool nonzero = false;
    for (size_t i = 5; i < DEVICE_ID_SIZE - 1; ++i) {
        const char c = value[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
        nonzero |= c != '0';
    }
    return nonzero;
}

bool validAsicModel(const char *value) {
    if (!value || strnlen(value, 7) != 6 || strncmp(value, "BM", 2) != 0) return false;
    for (int i = 2; i < 6; ++i) if (value[i] < '0' || value[i] > '9') return false;
    return true;
}

bool validSetting(int value) { return value > 0 && value <= 2500; }
bool validTemperature(float value) { return std::isfinite(value) && value > 0 && value <= 150; }

void verification(JsonObject object, const char *status, const char *basis, const char *note) {
    object["status"] = status;
    JsonArray evidence = object["basis"].to<JsonArray>();
    if (basis) evidence.add(basis);
    object["note"] = note;
}

void feature(JsonObject capabilities, const char *name, bool available, const char *scope,
             const char *status, const char *note) {
    JsonObject entry = capabilities[name].to<JsonObject>();
    entry["available"] = available;
    entry["scope"] = available ? scope : "unsupported";
    verification(entry["verification"].to<JsonObject>(), status, "pinned-source", note);
}

bool sourceAddressMappingKnown(const char *model) {
    return strcmp(model, "BM1368") == 0 || strcmp(model, "BM1370") == 0;
}

} // namespace

bool formatDeviceId(const uint8_t digest[32], char output[DEVICE_ID_SIZE]) {
    if (!digest || !output) return false;
    static constexpr char HEX[] = "0123456789abcdef";
    memcpy(output, "5tfw:", 5);
    for (int i = 0; i < 16; ++i) {
        output[5 + i * 2] = HEX[digest[i] >> 4];
        output[6 + i * 2] = HEX[digest[i] & 15];
    }
    output[DEVICE_ID_SIZE - 1] = '\0';
    if (!validDeviceId(output)) { output[0] = '\0'; return false; }
    return true;
}

bool buildReport(JsonDocument &doc, const Snapshot &s) {
    doc.clear();
    if (!validDeviceId(s.deviceId) || !validLabel(s.firmwareVersion) ||
        !validLabel(s.boardModel) || !validLabel(s.boardProfile) || !validAsicModel(s.asicModel) ||
        s.asicCount < 1 || s.asicCount > MAX_ASICS ||
        !validSetting(s.defaultFrequencyMHz) || !validSetting(s.defaultVoltageMv) ||
        !validSetting(s.sourceMaxFrequencyMHz) || !validSetting(s.sourceMaxVoltageMv)) return false;

    JsonObject protocol = doc["protocol"].to<JsonObject>();
    protocol["name"] = "5tratum-device-capabilities";
    protocol["revision"] = 1;
    protocol["maturity"] = "draft";
    JsonObject report = doc["report"].to<JsonObject>();
    report["kind"] = "device-report";
    report["implementedByReportedFirmware"] = true;
    JsonObject identity = doc["identity"].to<JsonObject>();
    identity["deviceId"] = s.deviceId;
    identity["source"] = "efuse-derived";
    identity["verified"] = true;

    JsonObject firmware = doc["firmware"].to<JsonObject>();
    firmware["product"] = "ESP-Miner-NerdQAxePlus";
    firmware["version"] = s.firmwareVersion;
    firmware["sourceTag"] = UPSTREAM_TAG;
    firmware["sourceRevision"] = nullptr;
    verification(firmware["sourceVerification"].to<JsonObject>(), "unverified", "pinned-source",
                 "Prototype uses the pinned upstream baseline plus local extension changes; exact modified-source commit is not embedded.");
    JsonObject integration = doc["integration"].to<JsonObject>();
    integration["proposedProduct"] = "5tratumFW";
    integration["status"] = "capability-prototype";
    JsonObject build = integration["extensionBuild"].to<JsonObject>();
    build["product"] = "5tratumFW capability prototype";
    build["identifier"] = EXTENSION_BUILD;
    build["upstreamBaselineRevision"] = UPSTREAM_REVISION;

    JsonObject hardware = doc["hardware"].to<JsonObject>();
    hardware["boardModel"] = s.boardModel;
    hardware["boardProfile"] = s.boardProfile;
    verification(hardware["profileVerification"].to<JsonObject>(), "observed", "pinned-source",
                 "Compiled target and current board getters only; physical PCB and regulator revision are not identified by this endpoint.");
    hardware["pcbRevision"] = nullptr;
    verification(hardware["revisionVerification"].to<JsonObject>(), "unverified", nullptr,
                 "Physical PCB revision is unknown; no new hardware probes are performed.");
    hardware["asicModel"] = s.asicModel;
    hardware["asicCount"] = s.asicCount;
    verification(hardware["countVerification"].to<JsonObject>(), "observed", "miner-api",
                 "Read from the existing board profile; not a count of internal silicon cores or proof every ASIC is active.");
    hardware["activeAsicCount"] = nullptr;
    verification(hardware["activeCountVerification"].to<JsonObject>(), "unverified", nullptr,
                 "No independent active-chip verification has been performed.");
    hardware["indexMeaning"] = "chain-enumeration-order";
    hardware["physicalPositionVerified"] = false;
    JsonObject hints = hardware["profileHints"].to<JsonObject>();
    hints["role"] = "source-profile-hints-not-safety-authority";
    hints["defaultFrequencyMHz"] = s.defaultFrequencyMHz;
    hints["defaultCoreVoltageMv"] = s.defaultVoltageMv;
    hints["sourceAbsoluteMaxFrequencyMHz"] = s.sourceMaxFrequencyMHz;
    hints["sourceAbsoluteMaxCoreVoltageMv"] = s.sourceMaxVoltageMv;
    verification(hints["verification"].to<JsonObject>(), "source-verified", "pinned-source",
                 "Existing board getter values are informational, not validated safe operating limits or permission to replace saved settings.");

    const bool savedFrequency = s.savedFrequencyPresent && validSetting(s.savedFrequencyMHz);
    const bool savedVoltage = s.savedVoltagePresent && validSetting(s.savedVoltageMv);
    JsonObject point = doc["operatingPoint"].to<JsonObject>();
    point["source"] = savedFrequency || savedVoltage ? "saved-configuration" : "unavailable";
    point["preservationPolicy"] = "retain-existing-values";
    if (savedFrequency) point["configuredFrequencyMHz"] = s.savedFrequencyMHz;
    else point["configuredFrequencyMHz"] = nullptr;
    point["frequencyScope"] = "chain";
    if (savedVoltage) point["configuredCoreVoltageMv"] = s.savedVoltageMv;
    else point["configuredCoreVoltageMv"] = nullptr;
    point["voltageScope"] = "shared-board";
    point["appliedFrequencyMHz"] = nullptr;
    point["appliedCoreVoltageMv"] = nullptr;
    verification(point["verification"].to<JsonObject>(), savedFrequency || savedVoltage ? "observed" : "unavailable",
                 "miner-api", "Read-only cached configuration; absent or invalid values stay null. No settings are saved, clamped, or substituted by this endpoint.");

    bool hasChipTemperature = false;
    if (s.initialized) for (int i = 0; i < s.asicCount; ++i) hasChipTemperature |= validTemperature(s.chipTemperatures[i]);
    const bool knownMapping = sourceAddressMappingKnown(s.asicModel);
    JsonObject capabilities = doc["capabilities"].to<JsonObject>();
    capabilities["algorithms"].to<JsonArray>().add("sha256d");
    capabilities["independentWorkAssignment"] = false;
    capabilities["workTargeting"] = "chain-broadcast";
    verification(capabilities["workVerification"].to<JsonObject>(), "not-implemented", "pinned-source",
                 "No targeted per-ASIC jobs or isolated work contexts are implemented. This prototype never authorizes split routing.");
    feature(capabilities, "registerAddressing", knownMapping, "per-asic", knownMapping ? "source-verified" : "unverified",
            "Selected register addressing is established for BM1368/BM1370 source paths only; no new register operations are performed.");
    feature(capabilities, "resultAttribution", knownMapping, "per-asic", knownMapping ? "source-verified" : "unverified",
            "Source derives ASIC indices for results; attribution does not establish isolated work dispatch.");
    feature(capabilities, "frequencyControl", true, "chain", "source-verified", "The existing stock frequency control applies to the chain; this endpoint cannot change it.");
    feature(capabilities, "independentFrequencyControl", false, "unsupported", "not-implemented", "Independent per-chip clocks are not implemented or verified by this prototype.");
    feature(capabilities, "voltageControl", true, "shared-board", "source-verified", "Core voltage uses the existing shared board regulator path; no control actions are provided here.");
    feature(capabilities, "independentVoltageControl", false, "unsupported", "unavailable", "No independently controlled per-chip voltage rails are established.");
    feature(capabilities, "coolingControl", true, "shared-board", "source-verified", "Existing cooling and safety controls operate at board level.");
    feature(capabilities, "powerTelemetry", true, "shared-board", "source-verified", "Existing board power telemetry is shared physical power and must be counted once.");
    feature(capabilities, "perAsicPowerTelemetry", false, "unsupported", "unavailable", "No measured per-chip power telemetry is established.");
    feature(capabilities, "perAsicTemperatureTelemetry", hasChipTemperature, "per-asic", hasChipTemperature ? "observed" : "unavailable",
            "Only finite positive cached chip readings are reported; zero, invalid, and uninitialized readings remain unavailable.");
    capabilities["difficultyMaskScope"] = "chain";
    capabilities["versionRollingScope"] = "chain";
    capabilities["safetyScope"] = "shared-board";

    int addressDivisor = 1;
    while (addressDivisor < s.asicCount) addressDivisor *= 2;
    JsonArray asics = doc["asics"].to<JsonArray>();
    for (int i = 0; i < s.asicCount; ++i) {
        JsonObject chip = asics.add<JsonObject>();
        chip["asicIndex"] = i;
        char logicalId[48];
        snprintf(logicalId, sizeof(logicalId), "%s/asic/%d", s.deviceId, i);
        chip["logicalMinerId"] = logicalId;
        chip["asicModel"] = s.asicModel;
        if (knownMapping) chip["chainAddress"] = i * (256 / addressDivisor);
        else chip["chainAddress"] = nullptr;
        verification(chip["addressVerification"].to<JsonObject>(), knownMapping ? "inferred" : "unverified",
                     knownMapping ? "pinned-source" : nullptr, "Source-derived address only; no per-chip register readback or physical-position verification.");
        const bool available = s.initialized && validTemperature(s.chipTemperatures[i]);
        JsonObject temperature = chip["temperature"].to<JsonObject>();
        temperature["available"] = available;
        if (available) temperature["valueC"] = s.chipTemperatures[i];
        else temperature["valueC"] = nullptr;
        temperature["reason"] = available ? "measured" :
            (s.initialized && s.chipTemperatures[i] == 0 ? "stock-api-zero-is-not-valid" : "sensor-unavailable");
        JsonObject power = chip["power"].to<JsonObject>();
        power["available"] = false;
        power["valueW"] = nullptr;
        power["reason"] = "shared-board-only";
    }
    if (doc.overflowed() || measureJson(doc) > MAX_JSON_BYTES) { doc.clear(); return false; }
    return true;
}

} // namespace FiveTratumCapabilities
