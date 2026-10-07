#include "displays/display_data.h"
#include "tasks/chip_hashrate_sample.h"
#include "tasks/temperature_sample.h"
#include <cassert>
#include <cstring>
#include <limits>
#include <initializer_list>
#include <atomic>
#include <thread>
#include <array>

using namespace FiveTratumDisplay;
int main() {
    char out[80];
    formatTemperature(out, sizeof(out), 0.0f); assert(std::strcmp(out, "--") == 0);
    formatTemperature(out, sizeof(out), std::numeric_limits<float>::quiet_NaN()); assert(std::strcmp(out, "--") == 0);
    formatTemperature(out, sizeof(out), 151.0f); assert(std::strcmp(out, "--") == 0);
    formatTemperature(out, sizeof(out), 58.25f); assert(std::strcmp(out, "58.2 C") == 0);
    formatLabelledTemperature(out, sizeof(out), "ASIC board", 54, true);
    assert(!std::strcmp(out, "ASIC board 54.0 C"));
    formatLabelledTemperature(out, sizeof(out), "VR internal", 91, false);
    assert(!std::strcmp(out, "VR internal --"));
    formatSharedTemperatures(out, sizeof(out), 54, 50, 91, true);
    assert(!std::strcmp(out, "Board 54.0 C | VR ext 50.0 C | int 91.0 C"));
    formatSharedTemperatures(out, sizeof(out), 54, 50, 91, false);
    assert(!std::strcmp(out, "Thermal readings unavailable / stale"));
    formatSharedTemperatures(out, sizeof(out), 0, -1, std::numeric_limits<float>::quiet_NaN(), true);
    assert(!std::strcmp(out, "Thermal readings unavailable"));
    using FiveTratumTelemetry::TemperatureSample;
    TemperatureSample thermal{54, 50, 91, 100};
    assert(thermal.isFresh(100) && thermal.isFresh(15000100));
    assert(!thermal.isFresh(99) && !thermal.isFresh(15000101));
    thermal.capturedAtUs = 0; assert(!thermal.isFresh(0));
    thermal.capturedAtUs = -1; assert(!thermal.isFresh(100));
    thermal = {0, 0, 0, 100}; assert(!thermal.isFresh(100));
    thermal = {-1, 151, std::numeric_limits<float>::infinity(), 100}; assert(!thermal.isFresh(100));
    thermal = {0, 50, 0, 100}; assert(thermal.isFresh(100));
    FiveTratumTelemetry::TemperatureSnapshotCache thermalCache;
    assert(!thermalCache.get().isFresh(100));
    thermalCache.publish({54, 50, 91, 100});
    auto snapshot = thermalCache.get();
    assert(snapshot.asicBoardC == 54 && snapshot.vrExternalC == 50 && snapshot.vrInternalC == 91);
    thermalCache.publishPowerLoop({54, 50, 91, 100}, false);
    assert(thermalCache.get().capturedAtUs == -1);
    for (const TemperatureSample invalid : {TemperatureSample{0, 0, 0, 100}, TemperatureSample{54, 50, 91, 0},
                                            TemperatureSample{-1, 151, std::numeric_limits<float>::infinity(), 100}}) {
        thermalCache.publishPowerLoop(invalid, true);
        assert(thermalCache.get().capturedAtUs == -1);
    }
    thermalCache.publishPowerLoop({0, 50, 0, 100}, true);
    assert(thermalCache.get().isFresh(100) && thermalCache.get().asicBoardC == 0);
    std::atomic<bool> completed{false};
    thermalCache.publish({});
    std::thread writer([&] {
        for (int64_t i = 1; i <= 30000; ++i)
            thermalCache.publish({static_cast<float>(i), static_cast<float>(i + 1), static_cast<float>(i + 2), i});
        completed.store(true);
    });
    std::array<std::thread, 3> readers;
    for (auto &reader : readers) reader = std::thread([&] {
        do {
            const auto coherent = thermalCache.get();
            if (coherent.capturedAtUs > 0) {
                assert(coherent.asicBoardC == coherent.capturedAtUs);
                assert(coherent.vrExternalC == coherent.asicBoardC + 1);
                assert(coherent.vrInternalC == coherent.asicBoardC + 2);
            }
        } while (!completed.load());
    });
    writer.join();
    for (auto &reader : readers) reader.join();
    formatHashrate(out, sizeof(out), 3820.0f); assert(std::strcmp(out, "3.82 TH/s") == 0);
    formatHashrate(out, sizeof(out), 870.0f); assert(std::strcmp(out, "870 GH/s") == 0);
    formatHashrate(out, sizeof(out), -1.0f); assert(std::strcmp(out, "--") == 0);
    formatPowerEfficiency(out, sizeof(out), 84.2f, 3820.0f); assert(std::strcmp(out, "84.2 W  |  22.0 J/TH") == 0);
    formatPowerEfficiency(out, sizeof(out), 84.2f, 0); assert(std::strstr(out, "-- J/TH"));
    formatPowerEfficiency(out, sizeof(out), std::numeric_limits<float>::infinity(), 1000); assert(std::strstr(out, "Power --"));
    formatCounter(out, sizeof(out), std::numeric_limits<uint64_t>::max()); assert(std::strcmp(out, "18.4E") == 0);
    formatShares(out, sizeof(out), 12345, 2); assert(std::strcmp(out, "Accepted 12.3K  |  Rejected 2") == 0);
    assert(std::strcmp(muxText(MuxState::Unverified), "5tratMUX: unverified") == 0);
    assert(std::strcmp(muxText(MuxState::Connected), "5tratMUX: connected") == 0);
    assert(std::strcmp(muxText(MuxState::Stale), "5tratMUX: stale") == 0);
    assert(muxState(true, false, true, false) == MuxState::Disconnected);
    assert(muxState(true, true, false, false) == MuxState::Unverified);
    assert(muxState(true, true, false, true) == MuxState::Stale);
    assert(muxState(false, true, true, false) == MuxState::Unavailable);
    formatMuxStatus(out, sizeof(out), MuxState::Connected, MuxState::Unverified, true, 0);
    assert(std::strcmp(out, "5tratMUX P1: connected | P2: unverified") == 0);
    formatMuxStatus(out, sizeof(out), MuxState::Connected, MuxState::Stale, false, 1);
    assert(std::strcmp(out, "5tratMUX P2: stale") == 0);
    formatPoolConnectivity(out, sizeof(out), true, 2, true); assert(std::strcmp(out, "Shared: 2/2 pools connected") == 0);
    formatOperatingPoint(out, sizeof(out), 500, 1130); assert(std::strcmp(out, "Configured 500 MHz  /  1130 mV") == 0);
    FiveTratumTelemetry::ChipHashrateSample sample{0, 100, true};
    formatChipHashrate(out, sizeof(out), sample.ghPerSecond, sample.isFresh(15000100));
    assert(std::strcmp(out, "0 GH/s") == 0); // A measured zero is valid.
    formatChipHashrate(out, sizeof(out), 950, sample.isFresh(15000101));
    assert(std::strcmp(out, "--") == 0); // Never reuse a stale positive rate.
    sample.available = false;
    formatChipHashrate(out, sizeof(out), 950, sample.isFresh(100));
    assert(std::strcmp(out, "--") == 0);
    char tiny[4] = {};
    formatShares(tiny, sizeof(tiny), std::numeric_limits<uint64_t>::max(), std::numeric_limits<uint64_t>::max());
    assert(tiny[3] == '\0');
    const MuxCoinContext btc{true, "bitcoin", "BTC", "Bitcoin"};
    const MuxCoinContext dgb{true, "digibyte", "DGB", "DigiByte"};
    formatCoinPrice(out, sizeof(out), btc, 12345); assert(!strcmp(out, "BTC price $12345"));
    formatCoinPrice(out, sizeof(out), btc, 0); assert(!strcmp(out, "Price unavailable"));
    formatCoinPrice(out, sizeof(out), dgb, 12345); assert(!strcmp(out, "Price unavailable"));
    formatCoinPrice(out, sizeof(out), {}, 12345); assert(!strcmp(out, "Price unavailable"));
    MuxCoinContext lookalike = dgb;
    strcpy(lookalike.ticker, "BTC");
    assert(!isBitcoinCoinContext(lookalike));
    formatHashrateCoinTitle(out, sizeof(out), dgb); assert(!strcmp(out, "HASHRATE / DGB"));
    formatCoinRoute(out, sizeof(out), {}, 1); assert(!strcmp(out, "P2 route: unknown"));
    MuxWorkContext work{true, "job", "1b0404cb", true, 20000000U, true, 16307.420938523983, 89};
    formatWorkHeight(out, sizeof(out), work); assert(!strcmp(out, "Block 20000000"));
    formatWorkDifficulty(out, sizeof(out), work); assert(!strcmp(out, "16.3K"));
    formatWorkNBits(out, sizeof(out), work); assert(!strcmp(out, "nBits 1b0404cb"));
    formatWorkAge(out, sizeof(out), work); assert(!strcmp(out, "Job 89s old"));
    work.height = UINT32_MAX;
    formatWorkHeight(out, sizeof(out), work); assert(!strcmp(out, "Block 4294967295"));
    work.networkDifficulty = 0.00123;
    formatWorkDifficulty(out, sizeof(out), work); assert(!strcmp(out, "0.00123"));
    for (const double bad : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                            std::numeric_limits<double>::quiet_NaN()}) {
        work.networkDifficulty = bad;
        formatWorkDifficulty(out, sizeof(out), work); assert(!strcmp(out, "--"));
    }
    work.available = false;
    formatWorkHeight(out, sizeof(out), work); assert(!strcmp(out, "Block --"));
    formatWorkDifficulty(out, sizeof(out), work); assert(!strcmp(out, "--"));
    formatWorkNBits(out, sizeof(out), work); assert(!strcmp(out, "nBits --"));
    formatWorkAge(out, sizeof(out), work); assert(!strcmp(out, "Job unavailable"));
}
