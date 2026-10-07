#pragma once

#include "ArduinoJson.h"
#include "tasks/chip_hashrate_sample.h"
#include "stratum/mux_status.h"
#include "capabilities_report.h"

namespace FiveTratumStatus {

constexpr int MAX_ASICS = 64;
constexpr size_t MAX_RESPONSE_BYTES = 24576;

struct Snapshot {
    const char *firmwareVersion = nullptr;
    char deviceId[FiveTratumCapabilities::DEVICE_ID_SIZE]{};
    const char *boardModel = nullptr;
    const char *asicModel = nullptr;
    int64_t nowUs = 0;
    uint64_t uptimeSeconds = 0;
    int asicCount = 0;
    FiveTratumTelemetry::ChipHashrateSample rates[MAX_ASICS]{};
    float temperaturesC[MAX_ASICS]{};
    MuxStatusSnapshot pools[2]{};
    bool dual = false;
    int activePool = 0;
};

// Serialization only: no hardware probes, settings writes or work assignment.
bool buildReport(JsonDocument &doc, const Snapshot &snapshot);

} // namespace FiveTratumStatus
