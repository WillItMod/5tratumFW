#include "capabilities_report.h"
#include "strict_config_read.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

using namespace FiveTratumCapabilities;

struct FailingAllocator : ArduinoJson::Allocator {
    bool fail = false;
    void *allocate(size_t size) override { return fail ? nullptr : malloc(size); }
    void deallocate(void *p) override { free(p); }
    void *reallocate(void *p, size_t size) override { return fail ? nullptr : realloc(p, size); }
};

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    const char *scenario = argv[1];
    if (strncmp(scenario, "strict-", 7) == 0) {
        const char *json = "{}";
        if (strcmp(scenario, "strict-integer") == 0) json = "{\"value\":500}";
        else if (strcmp(scenario, "strict-true") == 0) json = "{\"value\":true}";
        else if (strcmp(scenario, "strict-false") == 0) json = "{\"value\":false}";
        else if (strcmp(scenario, "strict-string") == 0) json = "{\"value\":\"500\"}";
        else if (strcmp(scenario, "strict-bad-string") == 0) json = "{\"value\":\"bad\"}";
        else if (strcmp(scenario, "strict-fraction") == 0) json = "{\"value\":500.5}";
        else if (strcmp(scenario, "strict-float") == 0) json = "{\"value\":500.0}";
        else if (strcmp(scenario, "strict-null") == 0) json = "{\"value\":null}";
        else if (strcmp(scenario, "strict-negative") == 0) json = "{\"value\":-1}";
        else if (strcmp(scenario, "strict-overflow") == 0) json = "{\"value\":65536}";
        else if (strcmp(scenario, "strict-malformed") == 0) json = "{\"value\":";
        JsonDocument cached;
        deserializeJson(cached, json);
        std::string before, after;
        serializeJson(cached, before);
        uint16_t setting = 77;
        const bool accepted = Config::readStrictU16(cached["value"], setting);
        serializeJson(cached, after);
        std::cout << "{\"accepted\":" << (accepted ? "true" : "false") <<
            ",\"value\":" << setting << ",\"cacheUnchanged\":" << (before == after ? "true" : "false") << "}";
        return 0;
    }
    Snapshot s;
    uint8_t digest[32];
    for (int i = 0; i < 32; ++i) digest[i] = static_cast<uint8_t>(i + 1);
    if (!formatDeviceId(digest, s.deviceId)) return 3;
    s.firmwareVersion = UPSTREAM_TAG;
    s.boardModel = "NerdQAxe++";
    s.boardProfile = "NERDQAXEPLUS2";
    s.asicModel = "BM1370";
    s.asicCount = 4;
    s.savedFrequencyPresent = s.savedVoltagePresent = true;
    s.savedFrequencyMHz = 500;
    s.savedVoltageMv = 1130;
    s.defaultFrequencyMHz = 600;
    s.defaultVoltageMv = 1150;
    s.sourceMaxFrequencyMHz = 800;
    s.sourceMaxVoltageMv = 1400;
    s.initialized = true;

    if (strcmp(scenario, "missing-settings") == 0) s.savedFrequencyPresent = s.savedVoltagePresent = false;
    else if (strcmp(scenario, "saved-outside-presets") == 0) { s.savedFrequencyMHz = 725; s.savedVoltageMv = 1199; }
    else if (strcmp(scenario, "invalid-settings") == 0) { s.savedFrequencyMHz = 0; s.savedVoltageMv = 2501; }
    else if (strcmp(scenario, "mixed-temperatures") == 0 || strcmp(scenario, "uninitialized") == 0) {
        s.chipTemperatures[0] = 0;
        s.chipTemperatures[1] = std::numeric_limits<float>::quiet_NaN();
        s.chipTemperatures[2] = -1;
        s.chipTemperatures[3] = 52.5;
        if (strcmp(scenario, "uninitialized") == 0) s.initialized = false;
    } else if (strcmp(scenario, "octaxe") == 0) { s.asicCount = 8; s.boardModel = "NerdOCTAxe-Gamma"; s.boardProfile = "NERDOCTAXEGAMMA"; }
    else if (strcmp(scenario, "unknown-asic") == 0) { s.asicCount = 1; s.asicModel = "BM1397"; s.boardModel = "NerdAxe"; s.boardProfile = "NERDAXE"; }
    else if (strcmp(scenario, "zero-count") == 0) s.asicCount = 0;
    else if (strcmp(scenario, "excessive-count") == 0) s.asicCount = 65;
    else if (strcmp(scenario, "max-count") == 0) s.asicCount = 64;
    else if (strcmp(scenario, "invalid-label") == 0) s.boardModel = "NerdQAxe++\nsecret";
    else if (strcmp(scenario, "invalid-id") == 0) strcpy(s.deviceId, "5tfw:00000000000000000000000000000000");
    else if (strcmp(scenario, "zero-digest") == 0) {
        memset(digest, 0, sizeof(digest));
        if (formatDeviceId(digest, s.deviceId)) return 4;
        std::cout << "{\"built\":false,\"identityRejected\":true}";
        return 0;
    } else if (strcmp(scenario, "stock") != 0 && strcmp(scenario, "oom") != 0) return 5;

    FailingAllocator allocator;
    allocator.fail = strcmp(scenario, "oom") == 0;
    JsonDocument doc(&allocator);
    const bool built = buildReport(doc, s);
    if (!built) {
        if (doc.size() != 0) return 6;
        std::cout << "{\"built\":false}";
        return 0;
    }
    std::string output;
    serializeJson(doc, output);
    std::cout << output;
}
