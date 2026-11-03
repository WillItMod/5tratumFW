#include "hashrate_monitor_support.h"
#include <iostream>

std::atomic<int64_t> hostNow{1000000};
std::atomic<int> hostTaskResult{pdPASS};
HostSystem SYSTEM_MODULE;
HostPower POWER_MANAGEMENT_MODULE;

int64_t esp_timer_get_time() { return hostNow.load(); }
TickType_t xTaskGetTickCount() { return 0; }
void vTaskDelay(TickType_t) {}
void vTaskDelayUntil(TickType_t *, TickType_t) {}
void vTaskSuspend(TaskHandle_t) {}

static bool near(double left, double right) {
    return std::abs(left - right) < 0.001;
}

static void verifyErrorTelemetryForChain(int count) {
    Board board;
    board.count = count;
    Asic asic;
    SYSTEM_MODULE.board = &board;
    HashrateMonitor monitor;
    hostNow = 1000000;
    assert(monitor.start(&board, &asic));
    auto startup = monitor.getAsicErrorTelemetry(hostNow);
    assert(startup.asicCount == count && !startup.errorPercentageAvailable);
    for (int i = 0; i < count; ++i) {
        assert(!startup.chips[i].errorSampleFresh && !startup.chips[i].sampleAgeAvailable);
        monitor.onRegisterReply(i, 100);
        monitor.onErrorReply(i, 50);
    }
    assert(!monitor.getAsicErrorTelemetry(hostNow).errorPercentageAvailable);

    hostNow = 6000000;
    double validDelta = 0, errorDelta = 0;
    uint32_t validCounters[8]{}, errorCounters[8]{};
    for (int i = 0; i < count; ++i) {
        const uint32_t valid = static_cast<uint32_t>((i + 1) * (i + 1) * 100);
        const uint32_t error = static_cast<uint32_t>((i % 3) * 25);
        validDelta += valid;
        errorDelta += error;
        validCounters[i] = 100 + valid;
        errorCounters[i] = 50 + error;
        monitor.onRegisterReply(i, validCounters[i]);
        // A fresh valid sample alone must not stand in for the error counter.
        auto partial = monitor.getAsicErrorTelemetry(hostNow);
        if (i == 0) assert(!partial.chips[0].errorSampleFresh && !partial.errorPercentageAvailable);
        monitor.onErrorReply(i, errorCounters[i]);
        if (i < count - 1) assert(!monitor.getAsicErrorTelemetry(hostNow).errorPercentageAvailable);
    }
    auto weighted = monitor.getAsicErrorTelemetry(hostNow);
    assert(weighted.errorPercentageAvailable);
    assert(near(weighted.errorPercentage, 100.0 * errorDelta / (validDelta + errorDelta)));
    double unweighted = 0;
    for (int i = 0; i < count; ++i) {
        const auto &chip = weighted.chips[i];
        assert(chip.errorSampleFresh && chip.sampleAgeAvailable && chip.sampleAgeSeconds == 0);
        assert(chip.errorPercentageAvailable && chip.errorPercentage >= 0 && chip.errorPercentage <= 100);
        unweighted += chip.errorPercentage / count;
    }
    assert(std::abs(weighted.errorPercentage - unweighted) > 0.1);
    assert(weighted.chips[0].errorPercentage == 0); // Genuine measured zero.

    // Shutdown, applied pause and transitions pass miningActive=false. Raw
    // legacy values are retained, but no percentage/freshness can escape.
    auto inactive = monitor.getAsicErrorTelemetry(hostNow, false);
    assert(!inactive.errorPercentageAvailable);
    for (int i = 0; i < count; ++i) {
        assert(!inactive.chips[i].errorSampleFresh && !inactive.chips[i].errorPercentageAvailable);
        assert(!inactive.chips[i].sampleAgeAvailable);
    }
    auto exactAge = monitor.getAsicErrorTelemetry(21000000);
    assert(exactAge.errorPercentageAvailable && exactAge.chips[0].sampleAgeSeconds == 15);
    auto stale = monitor.getAsicErrorTelemetry(21000001);
    assert(!stale.errorPercentageAvailable && !stale.chips[0].errorSampleFresh);
    assert(stale.chips[0].sampleAgeAvailable && stale.chips[0].sampleAgeSeconds > 15);
    assert(!monitor.getAsicErrorTelemetry(5999999).errorPercentageAvailable);

    // One missing chip invalidates the board total, while responding chips
    // still have useful individual readings.
    hostNow = 11000000;
    for (int i = 0; i < count - 1; ++i) {
        monitor.onRegisterReply(i, validCounters[i] += 100);
        monitor.onErrorReply(i, errorCounters[i]);
    }
    auto missing = monitor.getAsicErrorTelemetry(22000000);
    assert(!missing.errorPercentageAvailable && missing.chips[0].errorSampleFresh);
    assert(!missing.chips[count - 1].errorSampleFresh);

    // A late reply must not relabel a long average as a fresh measurement.
    hostNow = 22000000;
    monitor.onRegisterReply(count - 1, validCounters[count - 1] += 100);
    monitor.onErrorReply(count - 1, errorCounters[count - 1] += 10);
    auto rebaseline = monitor.getAsicErrorTelemetry(hostNow);
    assert(!rebaseline.chips[count - 1].errorSampleFresh && !rebaseline.errorPercentageAvailable);
    assert(!monitor.getChipHashrateSample(count - 1).available);
    monitor.onRegisterReply(0, validCounters[0] += 100);
    hostNow = 27000000;
    monitor.onRegisterReply(count - 1, validCounters[count - 1] += 100);
    monitor.onErrorReply(count - 1, errorCounters[count - 1] += 10);
    monitor.onRegisterReply(0, validCounters[0] += 100);
    assert(monitor.getAsicErrorTelemetry(hostNow).chips[count - 1].errorSampleFresh);
    // If just the error reply is missing for a long gap, its rebaseline is
    // independent of the still-current valid counter.
    hostNow = 28000000;
    monitor.onRegisterReply(0, validCounters[0] += 100);
    monitor.onErrorReply(0, errorCounters[0] += 10);
    assert(!monitor.getAsicErrorTelemetry(hostNow).chips[0].errorSampleFresh);
    assert(monitor.getChipHashrateSample(0).available);
    hostNow = 33000000;
    monitor.onRegisterReply(0, validCounters[0] += 100);
    monitor.onErrorReply(0, errorCounters[0] += 10);
    assert(monitor.getAsicErrorTelemetry(hostNow).chips[0].errorSampleFresh);

    monitor.resetAfterPowerCycle();
    auto reset = monitor.getAsicErrorTelemetry(hostNow);
    assert(!reset.errorPercentageAvailable);
    for (int i = 0; i < count; ++i) {
        assert(!reset.chips[i].errorSampleFresh && !reset.chips[i].sampleAgeAvailable);
        assert(!monitor.m_chipErrorSampleAvailable[i]);
        monitor.onRegisterReply(i, 0xfffffff0u);
        monitor.onErrorReply(i, 0xfffffff0u);
    }
    hostNow = 38000000;
    for (int i = 0; i < count; ++i) {
        monitor.onRegisterReply(i, 0x10u); // Counter wrapping remains valid.
        monitor.onErrorReply(i, 0x00u);
    }
    auto wrap = monitor.getAsicErrorTelemetry(hostNow);
    assert(wrap.errorPercentageAvailable && near(wrap.errorPercentage, 100.0 / 3.0));

    // Both counters can genuinely measure zero; the ratio is unavailable.
    hostNow = 43000000;
    for (int i = 0; i < count; ++i) {
        monitor.onRegisterReply(i, 0x10u);
        monitor.onErrorReply(i, 0x00u);
    }
    auto idle = monitor.getAsicErrorTelemetry(hostNow);
    assert(!idle.errorPercentageAvailable);
    for (int i = 0; i < count; ++i)
        assert(idle.chips[i].errorSampleFresh && !idle.chips[i].errorPercentageAvailable);

    // Zero work on an individual chip does not invalidate a positive board
    // denominator when every configured chip's paired counters are measured.
    monitor.m_chipHashrate[0] = 100;
    auto oneWorking = monitor.getAsicErrorTelemetry(hostNow);
    assert(oneWorking.errorPercentageAvailable && oneWorking.errorPercentage == 0);
    assert(oneWorking.chips[count - 1].errorSampleFresh && !oneWorking.chips[count - 1].errorPercentageAvailable);
    monitor.m_chipHashrate[0] = 0;

    // Finite, nonnegative paired values and sane timestamps are required.
    monitor.m_chipErrorRate[0] = std::numeric_limits<float>::quiet_NaN();
    assert(!monitor.getAsicErrorTelemetry(hostNow).errorPercentageAvailable);
    monitor.m_chipErrorRate[0] = -1;
    assert(!monitor.getAsicErrorTelemetry(hostNow).chips[0].errorSampleFresh);
    monitor.m_chipErrorRate[0] = std::numeric_limits<float>::infinity();
    assert(!monitor.getAsicErrorTelemetry(hostNow).chips[0].errorSampleFresh);
    monitor.m_chipErrorRate[0] = 0;
    monitor.m_prevErrorResponse[0] = hostNow.load() + 1;
    assert(!monitor.getAsicErrorTelemetry(hostNow).chips[0].sampleAgeAvailable);
    monitor.m_prevErrorResponse[0] = hostNow;

    // Concurrent RX updates and full-chain snapshots must never publish an
    // internally inconsistent board ratio or a partial setup/reset.
    std::atomic<bool> stop{false};
    std::thread reader([&] {
        while (!stop.load()) {
            const auto snapshot = monitor.getAsicErrorTelemetry(hostNow);
            assert(snapshot.asicCount == count);
            double valid = 0, error = 0;
            for (int i = 0; i < count; ++i) {
                valid += snapshot.chips[i].hashRate;
                error += snapshot.chips[i].errorHashRate;
                if (snapshot.errorPercentageAvailable) assert(snapshot.chips[i].errorSampleFresh);
            }
            if (snapshot.errorPercentageAvailable)
                assert(near(snapshot.errorPercentage, 100.0 * error / (valid + error)));
        }
    });
    for (int cycle = 1; cycle <= 1000; ++cycle) {
        hostNow = 43000000 + static_cast<int64_t>(cycle) * 5000000;
        for (int i = 0; i < count; ++i) {
            monitor.onRegisterReply(i, 16 + static_cast<uint32_t>(cycle) * (i + 1) * 100);
            monitor.onErrorReply(i, static_cast<uint32_t>(cycle) * i);
        }
        if (cycle % 100 == 0) monitor.resetAfterPowerCycle();
    }
    stop = true;
    reader.join();
    releaseHostSamples(monitor);
}

int main() {
    Board board;
    Asic asic;
    SYSTEM_MODULE.board = &board;
    HashrateMonitor monitor;
    assert(!monitor.getChipHashrateSample(0).available);
    assert(monitor.getAsicErrorTelemetry(hostNow).asicCount == 0);
    assert(!monitor.getAsicErrorTelemetry(hostNow).errorPercentageAvailable);
    monitor.onRegisterReply(0, 1); // Early RX must be harmless.
    monitor.onErrorReply(0, 1);
    assert(monitor.getChipErrorRate(0) == 0);
    assert(!monitor.start(nullptr, &asic));
    assert(!monitor.start(&board, nullptr));
    board.count = 0;
    assert(!monitor.start(&board, &asic));
    board.count = 65;
    assert(!monitor.start(&board, &asic));
    board.count = 4;
    hostTaskResult = 0;
    assert(!monitor.start(&board, &asic));
    assert(!monitor.getChipHashrateSample(0).available);
    assert(monitor.m_asicCount == 0 && !monitor.m_prevCounter && !monitor.m_chipErrorRate &&
           !monitor.m_prevErrorResponse && !monitor.m_prevErrorCounter && !monitor.m_chipErrorSampleAvailable);
    assert(monitor.getAsicErrorTelemetry(hostNow).asicCount == 0);
    hostTaskResult = pdPASS;

    std::atomic<bool> stop{false};
    std::thread reader([&] {
        while (!stop.load()) {
            auto sample = monitor.getChipHashrateSample(3);
            assert(!sample.available || sample.capturedAtUs > 0);
            assert(std::isfinite(monitor.getChipErrorRate(3)));
        }
    });
    assert(monitor.start(&board, &asic));
    assert(!monitor.start(&board, &asic));
    assert(!monitor.getChipHashrateSample(-1).available);
    assert(!monitor.getChipHashrateSample(4).available);
    monitor.onRegisterReply(250, 1);
    monitor.onErrorReply(250, 1);
    assert(monitor.getChipErrorRate(-1) == 0 && monitor.getChipErrorRate(4) == 0);
    monitor.onRegisterReply(0, 100);
    monitor.onErrorReply(0, 100);
    assert(!monitor.getChipHashrateSample(0).available);
    hostNow = 6000000;
    monitor.onRegisterReply(0, 100); // Measured zero after the second response.
    monitor.onErrorReply(0, 100);
    assert(monitor.getChipErrorRate(0) == 0);
    auto zero = monitor.getChipHashrateSample(0);
    assert(zero.available && zero.ghPerSecond == 0 && zero.isFresh(hostNow));
    assert(!monitor.getChipHashrateSample(1).available);
    hostNow = 11000000;
    monitor.onRegisterReply(0, 1100);
    monitor.onErrorReply(0, 1100);
    auto positive = monitor.getChipHashrateSample(0);
    const double expected = 1000.0 * 4294967296.0 / 5000000.0 / 1000.0;
    assert(std::abs(positive.ghPerSecond - expected) < 0.001);
    assert(std::abs(monitor.getChipErrorRate(0) - expected) < 0.001);
    assert(!positive.isFresh(hostNow.load() + 15000001));
    monitor.onRegisterReply(0, 9999); // Same timestamp must not mutate context.
    monitor.onErrorReply(0, 9999);
    assert(monitor.getChipHashrateSample(0).ghPerSecond == positive.ghPerSecond);
    assert(monitor.m_prevErrorCounter[0] == 1100 && std::abs(monitor.getChipErrorRate(0) - expected) < 0.001);
    monitor.onRegisterReply(1, 0xfffffff0u);
    monitor.onErrorReply(1, 0xfffffff0u);
    hostNow = 16000000;
    monitor.onRegisterReply(1, 0x10u); // Unsigned hardware-counter wrap.
    monitor.onErrorReply(1, 0x10u);
    assert(std::abs(monitor.getChipHashrateSample(1).ghPerSecond -
                    32.0 * 4294967296.0 / 5000000.0 / 1000.0) < 0.001);
    assert(std::abs(monitor.getChipErrorRate(1) - 32.0 * 4294967296.0 / 5000000.0 / 1000.0) < 0.001);
    monitor.onRegisterReply(3, 0);
    monitor.onErrorReply(3, 0);
    for (int i = 1; i <= 1000; ++i) {
        hostNow = 16000000 + static_cast<int64_t>(i) * 5000000;
        monitor.onRegisterReply(3, static_cast<uint32_t>(i) * 1000);
        monitor.onErrorReply(3, static_cast<uint32_t>(i) * 50);
    }
    stop = true;
    reader.join();
    assert(monitor.getChipHashrateSample(3).isFresh(hostNow));
    monitor.resetAfterPowerCycle();
    for (int i = 0; i < 4; ++i) {
        assert(!monitor.getChipHashrateSample(i).available);
        assert(monitor.getChipErrorRate(i) == 0 && monitor.m_prevErrorResponse[i] == 0 && monitor.m_prevErrorCounter[i] == 0);
    }
    hostNow = hostNow.load() + 5000000;
    monitor.onRegisterReply(0, 10); // First post-reset reply establishes a new counter baseline.
    monitor.onErrorReply(0, 10);
    assert(monitor.getChipErrorRate(0) == 0);
    assert(!monitor.getChipHashrateSample(0).available);
    hostNow = hostNow.load() + 5000000;
    monitor.onRegisterReply(0, 20);
    monitor.onErrorReply(0, 20);
    assert(monitor.getChipErrorRate(0) > 0);
    assert(monitor.getChipHashrateSample(0).available && monitor.getChipHashrateSample(0).isFresh(hostNow));
    releaseHostSamples(monitor);
    verifyErrorTelemetryForChain(4);
    verifyErrorTelemetryForChain(8);
    std::cout << "Actual monitor: startup/failure/retry, concurrent publication, "
                 "4/8-chip weighted error telemetry, measured zero, partial/stale/rebaseline, "
                 "pause/reset, counter wrap and concurrent snapshot checks passed\n";
}
