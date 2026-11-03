#include "hashrate_monitor_support.h"
#include "ArduinoJson.h"
#include <iostream>
#include <string>

std::atomic<int64_t> hostNow{1000000};
std::atomic<int> hostTaskResult{pdPASS};
HostSystem SYSTEM_MODULE;
HostPower POWER_MANAGEMENT_MODULE;
int64_t esp_timer_get_time() { return hostNow.load(); }
TickType_t xTaskGetTickCount() { return 0; }
void vTaskDelay(TickType_t) {}
void vTaskDelayUntil(TickType_t *, TickType_t) {}
void vTaskSuspend(TaskHandle_t) {}

// Mock the existing read-only mining-state boundary; its actual operation gate
// exercises applied pause and transition suppression in the production block.
namespace FiveTratumMining {
bool miningWritesAllowed() { return operationGate().isOpen(); }
}
struct DashboardAsics { int getSmallCoreCount() { return 2040; } };
struct DashboardBoard : Board {
    DashboardAsics asics;
    float getAsicFrequency() { return 525; }
    DashboardAsics *getAsics() { return &asics; }
};
struct DashboardHistory {
    uint64_t getCurrentTimestamp() { return 1000000; }
    float getCurrentHashrate1m() { return 1000; }
    float getCurrentHashrate10m() { return 1000; }
    float getCurrentHashrate1h() { return 1000; }
    float getCurrentHashrate1d() { return 1000; }
};
struct DashboardStratum {
    double getBestDiff() { return 1000; }
    double getBestSessionDiff() { return 1000; }
    unsigned getSharesAccepted() { return 10; }
    unsigned getSharesRejected() { return 1; }
};

static JsonDocument performanceJson(HashrateMonitor &HASHRATE_MONITOR,
                                    DashboardBoard *board, bool shutdown = false) {
    DashboardHistory historyValue;
    DashboardHistory *history = &historyValue;
    DashboardStratum stratum;
    DashboardStratum *STRATUM_MANAGER = &stratum;
    JsonDocument doc;
    // The runner copies the production performance block byte for byte. Real
    // ArduinoJson serializes it; hardware and unrelated API sections are mocks.
#include "dashboard_performance.inc"
    std::string wire;
    serializeJson(doc, wire);
    JsonDocument decoded;
    assert(deserializeJson(decoded, wire) == DeserializationError::Ok);
    return decoded;
}

static void assertUnavailable(JsonDocument &doc, int count, bool shutdown = false) {
    auto perf = doc["performance"];
    assert(perf["errorPercentage"].isNull());
    assert(perf["chips"].size() == static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        auto chip = perf["chips"][i];
        assert(chip["errorPercentage"].isNull());
        assert(chip["errorSampleFresh"].is<bool>() && !chip["errorSampleFresh"].as<bool>());
        assert(chip["hashRate"].is<float>() && chip["errorHashRate"].is<float>());
        if (shutdown) assert(chip["hashRate"].as<float>() == 0 && chip["errorHashRate"].as<float>() == 0);
    }
}

int main() {
    for (int count : {4, 8}) {
        DashboardBoard board;
        board.count = count;
        Asic asic;
        HashrateMonitor monitor;
        SYSTEM_MODULE.board = &board;
        hostNow = 1000000;
        assert(monitor.start(&board, &asic));
        auto initial = performanceJson(monitor, &board);
        assertUnavailable(initial, count);
        for (int i = 0; i < count; ++i) {
            monitor.onRegisterReply(i, 100);
            monitor.onErrorReply(i, 50);
        }
        auto baseline = performanceJson(monitor, &board);
        assertUnavailable(baseline, count);
        hostNow = 6000000;
        double validDelta = 0, errorDelta = 0;
        for (int i = 0; i < count; ++i) {
            const uint32_t valid = static_cast<uint32_t>((i + 1) * 100);
            const uint32_t error = static_cast<uint32_t>(i * 5);
            validDelta += valid;
            errorDelta += error;
            monitor.onRegisterReply(i, 100 + valid);
            monitor.onErrorReply(i, 50 + error);
        }
        auto measured = performanceJson(monitor, &board);
        auto perf = measured["performance"];
        assert(perf["errorPercentage"].is<float>());
        assert(std::abs(perf["errorPercentage"].as<double>() -
                        100.0 * errorDelta / (validDelta + errorDelta)) < 0.001);
        assert(perf["chips"][0]["errorPercentage"].as<double>() == 0);
        for (int i = 0; i < count; ++i) {
            assert(perf["chips"][i]["errorSampleFresh"].as<bool>());
            assert(perf["chips"][i]["sampleAgeSeconds"].as<double>() == 0);
            assert(!perf["chips"][i]["errorPercentage"].isNull());
        }
        // The raw hardware keys retain their legacy shutdown behavior, while
        // new keys distinguish a stop from a real zero-error measurement.
        auto shutdown = performanceJson(monitor, &board, true);
        assertUnavailable(shutdown, count, true);
        assert(shutdown["performance"]["chips"][0]["sampleAgeSeconds"].isNull());
        FiveTratumMining::operationGate().block();
        auto paused = performanceJson(monitor, &board);
        assertUnavailable(paused, count);
        assert(paused["performance"]["chips"][0]["sampleAgeSeconds"].isNull());
        assert(FiveTratumMining::operationGate().open());
        hostNow = 22000000;
        auto stale = performanceJson(monitor, &board);
        assertUnavailable(stale, count);
        assert(stale["performance"]["chips"][0]["sampleAgeSeconds"].as<double>() == 16);
        monitor.resetAfterPowerCycle();
        auto reset = performanceJson(monitor, &board);
        assertUnavailable(reset, count);
        assert(reset["performance"]["chips"][0]["sampleAgeSeconds"].isNull());
        releaseHostSamples(monitor);
    }
    std::cout << "Actual dashboard performance block: 4/8-chip JSON percentages, "
                 "legacy keys, null/zero, startup/reset, pause/shutdown and stale age passed\n";
}
