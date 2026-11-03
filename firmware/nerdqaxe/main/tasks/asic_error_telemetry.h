#pragma once

#include "chip_hashrate_sample.h"

namespace FiveTratumTelemetry {

struct ChipErrorTelemetry {
    float hashRate = 0.0f;
    float errorHashRate = 0.0f;
    float errorPercentage = 0.0f;
    double sampleAgeSeconds = 0.0;
    int64_t sampleCapturedAtUs = 0;
    bool errorPercentageAvailable = false;
    bool sampleAgeAvailable = false;
    bool errorSampleFresh = false;
};

struct AsicErrorTelemetry {
    static constexpr int MAX_ASICS = 64;
    int asicCount = 0;
    ChipErrorTelemetry chips[MAX_ASICS]{};
    float errorPercentage = 0.0f;
    bool errorPercentageAvailable = false;
};

// The qualified tuner owns only4/8-chip chains. Keep its caller frame bounded
// independently of the general64-chip dashboard/report API above.
struct TuningErrorTelemetry {
    static constexpr int MAX_ASICS=8;
    int asicCount=0;
    ChipErrorTelemetry chips[MAX_ASICS]{};
    float errorPercentage=0.0f;
    bool errorPercentageAvailable=false;
};
static_assert(sizeof(TuningErrorTelemetry)<=512,"Tuning snapshot must fit its bounded stack budget");

inline ChipErrorTelemetry chipErrorTelemetry(const ChipHashrateSample &valid,
                                             const ChipHashrateSample &error,
                                             int64_t nowUs, bool miningActive) {
    ChipErrorTelemetry result;
    // Preserve the existing raw API keys. Only the explicit availability flags
    // permit consumers to treat their values as a current error measurement.
    result.hashRate = valid.ghPerSecond;
    result.errorHashRate = error.ghPerSecond;
    if (!miningActive) return result;

    const bool pairMeasured = valid.available && error.available &&
        std::isfinite(valid.ghPerSecond) && valid.ghPerSecond >= 0.0f &&
        std::isfinite(error.ghPerSecond) && error.ghPerSecond >= 0.0f &&
        valid.capturedAtUs > 0 && error.capturedAtUs > 0 &&
        nowUs >= valid.capturedAtUs && nowUs >= error.capturedAtUs;
    if (!pairMeasured) return result;

    const int64_t oldestUs = valid.capturedAtUs < error.capturedAtUs ?
        valid.capturedAtUs : error.capturedAtUs;
    result.sampleAgeSeconds = static_cast<double>(nowUs - oldestUs) / 1000000.0;
    result.sampleCapturedAtUs = oldestUs;
    result.sampleAgeAvailable = true;
    result.errorSampleFresh = valid.isFresh(nowUs) && error.isFresh(nowUs);
    if (!result.errorSampleFresh) return result;

    // Nerd's counters distinguish valid work from work lost to hardware
    // errors. A measured zero error rate is valid; no work has no percentage.
    const double total = static_cast<double>(result.hashRate) + result.errorHashRate;
    if (total > 0.0 && std::isfinite(total)) {
        result.errorPercentage = static_cast<float>(100.0 * result.errorHashRate / total);
        result.errorPercentageAvailable = true;
    }
    return result;
}

} // namespace FiveTratumTelemetry
