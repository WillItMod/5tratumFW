#pragma once

// Cached power-loop temperatures. Reading this snapshot never accesses I2C or
// takes the hardware mutex, so display and HTTP readers can use it safely.
#include <cstdint>
#include <cmath>
#include <pthread.h>

namespace FiveTratumTelemetry {

struct TemperatureSample {
    float asicBoardC = 0;
    float vrExternalC = 0;
    float vrInternalC = 0;
    int64_t capturedAtUs = -1;

    bool hasReadings() const {
        const auto valid = [](float value) { return std::isfinite(value) && value > 0 && value <= 150; };
        return valid(asicBoardC) || valid(vrExternalC) || valid(vrInternalC);
    }

    bool isFresh(int64_t nowUs) const {
        return hasReadings() && capturedAtUs > 0 && nowUs >= capturedAtUs &&
               nowUs - capturedAtUs <= 15000000;
    }
};

class TemperatureSnapshotCache {
    pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
    TemperatureSample m_sample;
public:
    TemperatureSnapshotCache() = default;
    TemperatureSnapshotCache(const TemperatureSnapshotCache &) = delete;
    TemperatureSnapshotCache &operator=(const TemperatureSnapshotCache &) = delete;

    void publish(const TemperatureSample &sample) {
        pthread_mutex_lock(&m_mutex);
        m_sample = sample;
        pthread_mutex_unlock(&m_mutex);
    }

    void publishPowerLoop(const TemperatureSample &sample, bool hardwareReady) {
        publish(hardwareReady && sample.isFresh(sample.capturedAtUs) ? sample : TemperatureSample{});
    }

    TemperatureSample get() {
        pthread_mutex_lock(&m_mutex);
        const auto sample = m_sample;
        pthread_mutex_unlock(&m_mutex);
        return sample;
    }
};

} // namespace FiveTratumTelemetry
