#pragma once

#include <cmath>
#include <cstdint>

namespace FiveTratumTelemetry {

// The BM1370 counter-derived estimate already produced by the stock monitor.
// Availability is separate from the value: a measured zero is valid, whereas
// an uninitialized zero must never masquerade as a measurement.
struct ChipHashrateSample {
    float ghPerSecond = 0.0f;
    int64_t capturedAtUs = 0;
    bool available = false;

    static constexpr int64_t MAX_AGE_US = 15000000; // Three stock 5s polls.

    bool isFresh(int64_t nowUs) const {
        return available && std::isfinite(ghPerSecond) && ghPerSecond >= 0.0f &&
               capturedAtUs > 0 && nowUs >= capturedAtUs &&
               nowUs - capturedAtUs <= MAX_AGE_US;
    }
};

// A board total/efficiency must not remain numeric when one of the counters
// that contributes to it has stopped responding.
template<typename ReadSample>
inline bool allCounterSamplesFresh(int count, int64_t nowUs, ReadSample readSample) {
    if (count < 1 || count > 64) return false;
    for (int index = 0; index < count; ++index) {
        if (!readSample(index).isFresh(nowUs)) return false;
    }
    return true;
}

} // namespace FiveTratumTelemetry
