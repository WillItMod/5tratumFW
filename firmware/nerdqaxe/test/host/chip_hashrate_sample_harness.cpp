#include "chip_hashrate_sample.h"

#include <cassert>
#include <limits>
#include <iostream>

int main() {
    using FiveTratumTelemetry::ChipHashrateSample;
    constexpr int64_t start = 1000000;
    const ChipHashrateSample unavailable{};
    assert(!unavailable.isFresh(start));
    assert(!(ChipHashrateSample{0.0f, start, false}.isFresh(start)));
    // An actual zero from a completed measurement remains useful.
    assert((ChipHashrateSample{0.0f, start, true}.isFresh(start)));
    const ChipHashrateSample measured{1024.5f, start, true};
    assert(measured.isFresh(start));
    assert(measured.isFresh(start + ChipHashrateSample::MAX_AGE_US));
    assert(!measured.isFresh(start + ChipHashrateSample::MAX_AGE_US + 1));
    assert(!measured.isFresh(start - 1));
    assert(!(ChipHashrateSample{1024.5f, 0, true}.isFresh(start)));
    assert(!(ChipHashrateSample{-1.0f, start, true}.isFresh(start)));
    assert(!(ChipHashrateSample{std::numeric_limits<float>::infinity(), start, true}.isFresh(start)));
    assert(!(ChipHashrateSample{std::numeric_limits<float>::quiet_NaN(), start, true}.isFresh(start)));
    assert(!measured.isFresh(std::numeric_limits<int64_t>::min()));
    assert(!measured.isFresh(std::numeric_limits<int64_t>::max()));
    ChipHashrateSample samples[] = {measured, measured, measured, {0.0f, start, true}};
    auto readSample = [&samples](int index) { return samples[index]; };
    assert(FiveTratumTelemetry::allCounterSamplesFresh(4, start, readSample));
    assert(!FiveTratumTelemetry::allCounterSamplesFresh(0, start, readSample));
    assert(!FiveTratumTelemetry::allCounterSamplesFresh(65, start, readSample));
    samples[2].available = false;
    assert(!FiveTratumTelemetry::allCounterSamplesFresh(4, start, readSample));
    samples[2] = {1000.0f, start - 1, true};
    assert(!FiveTratumTelemetry::allCounterSamplesFresh(4, start + ChipHashrateSample::MAX_AGE_US, readSample));
    std::cout << "Counter-sample and aggregate availability/age checks passed\n";
}
