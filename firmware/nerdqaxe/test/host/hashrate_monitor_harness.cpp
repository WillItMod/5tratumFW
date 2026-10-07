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

int main() {
    Board board;
    Asic asic;
    SYSTEM_MODULE.board = &board;
    HashrateMonitor monitor;
    assert(!monitor.getChipHashrateSample(0).available);
    monitor.onRegisterReply(0, 1); // Early RX must be harmless.
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
    assert(monitor.m_asicCount == 0 && !monitor.m_prevCounter);
    hostTaskResult = pdPASS;

    std::atomic<bool> stop{false};
    std::thread reader([&] {
        while (!stop.load()) {
            auto sample = monitor.getChipHashrateSample(3);
            assert(!sample.available || sample.capturedAtUs > 0);
        }
    });
    assert(monitor.start(&board, &asic));
    assert(!monitor.start(&board, &asic));
    assert(!monitor.getChipHashrateSample(-1).available);
    assert(!monitor.getChipHashrateSample(4).available);
    monitor.onRegisterReply(250, 1);
    monitor.onRegisterReply(0, 100);
    assert(!monitor.getChipHashrateSample(0).available);
    hostNow = 6000000;
    monitor.onRegisterReply(0, 100); // Measured zero after the second response.
    auto zero = monitor.getChipHashrateSample(0);
    assert(zero.available && zero.ghPerSecond == 0 && zero.isFresh(hostNow));
    assert(!monitor.getChipHashrateSample(1).available);
    hostNow = 11000000;
    monitor.onRegisterReply(0, 1100);
    auto positive = monitor.getChipHashrateSample(0);
    const double expected = 1000.0 * 4294967296.0 / 5000000.0 / 1000.0;
    assert(std::abs(positive.ghPerSecond - expected) < 0.001);
    assert(!positive.isFresh(hostNow.load() + 15000001));
    monitor.onRegisterReply(0, 9999); // Same timestamp must not mutate context.
    assert(monitor.getChipHashrateSample(0).ghPerSecond == positive.ghPerSecond);
    monitor.onRegisterReply(1, 0xfffffff0u);
    hostNow = 16000000;
    monitor.onRegisterReply(1, 0x10u); // Unsigned hardware-counter wrap.
    assert(std::abs(monitor.getChipHashrateSample(1).ghPerSecond -
                    32.0 * 4294967296.0 / 5000000.0 / 1000.0) < 0.001);
    monitor.onRegisterReply(3, 0);
    for (int i = 1; i <= 1000; ++i) {
        hostNow = 16000000 + static_cast<int64_t>(i) * 5000000;
        monitor.onRegisterReply(3, static_cast<uint32_t>(i) * 1000);
    }
    stop = true;
    reader.join();
    assert(monitor.getChipHashrateSample(3).isFresh(hostNow));
    monitor.resetAfterPowerCycle();
    for (int i = 0; i < 4; ++i) assert(!monitor.getChipHashrateSample(i).available);
    hostNow = hostNow.load() + 5000000;
    monitor.onRegisterReply(0, 10); // First post-reset reply establishes a new counter baseline.
    assert(!monitor.getChipHashrateSample(0).available);
    hostNow = hostNow.load() + 5000000;
    monitor.onRegisterReply(0, 20);
    assert(monitor.getChipHashrateSample(0).available && monitor.getChipHashrateSample(0).isFresh(hostNow));
    releaseHostSamples(monitor);
    std::cout << "Actual monitor: startup/failure/retry, concurrent publication, "
                 "first/second response, measured zero, counter wrap and stale checks passed\n";
}
