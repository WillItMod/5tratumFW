#pragma once

// Display-only formatting. Never infer a coin, per-ASIC hashrate or freshness
// from cached chain-wide readings.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <inttypes.h>
#include "../stratum/coin_context.h"
#include "../stratum/work_context.h"

namespace FiveTratumDisplay {

enum class MuxState : uint8_t { Unavailable, Unverified, Connected, Disconnected, Stale };

inline const char *muxText(MuxState state) {
    switch (state) {
        case MuxState::Connected: return "5tratMUX: connected";
        case MuxState::Disconnected: return "5tratMUX: disconnected";
        case MuxState::Unavailable: return "5tratMUX: unavailable";
        case MuxState::Unverified: return "5tratMUX: unverified";
        case MuxState::Stale: return "5tratMUX: stale";
    }
    return "5tratMUX: unverified";
}

inline MuxState muxState(bool statusAvailable, bool connected, bool acknowledged, bool expired) {
    if (!statusAvailable) return MuxState::Unavailable;
    if (!connected) return MuxState::Disconnected;
    if (acknowledged) return MuxState::Connected;
    if (expired) return MuxState::Stale;
    return MuxState::Unverified;
}

inline const char *muxStateWord(MuxState state) {
    switch (state) {
        case MuxState::Unavailable: return "unavailable";
        case MuxState::Unverified: return "unverified";
        case MuxState::Connected: return "connected";
        case MuxState::Disconnected: return "disconnected";
        case MuxState::Stale: return "stale";
    }
    return "unverified";
}

inline void formatMuxStatus(char *out, size_t size, MuxState primary, MuxState secondary,
                            bool dual, int active) {
    if (dual) {
        std::snprintf(out, size, "5tratMUX P1: %s | P2: %s", muxStateWord(primary), muxStateWord(secondary));
    } else {
        std::snprintf(out, size, "5tratMUX P%d: %s", active == 1 ? 2 : 1,
                      muxStateWord(active == 1 ? secondary : primary));
    }
}

inline void formatCoinRoute(char *out, size_t size, const MuxCoinContext &coin, int pool) {
    // Keep unusually long declarations bounded on the physical display.
    std::snprintf(out, size, "P%d route: %.12s", pool == 1 ? 2 : 1,
                  coin.available ? coin.ticker : "unknown");
}

inline void formatHashrateCoinTitle(char *out, size_t size, const MuxCoinContext &coin) {
    if (coin.available) std::snprintf(out, size, "HASHRATE / %.8s", coin.ticker);
    else std::snprintf(out, size, "TOTAL HASHRATE");
}

inline void formatCoinPrice(char *out, size_t size, const MuxCoinContext &coin, uint32_t btcUsd) {
    if (isBitcoinCoinContext(coin) && btcUsd) std::snprintf(out, size, "BTC price $%" PRIu32, btcUsd);
    else std::snprintf(out, size, "Price unavailable");
}

inline void formatWorkHeight(char *out, size_t size, const MuxWorkContext &work) {
    if (work.available && work.heightAvailable)
        std::snprintf(out, size, "Block %" PRIu32, work.height);
    else std::snprintf(out, size, "Block --");
}

inline void formatWorkDifficulty(char *out, size_t size, const MuxWorkContext &work) {
    if (!work.available || !work.difficultyAvailable ||
        !std::isfinite(work.networkDifficulty) || work.networkDifficulty <= 0) {
        std::snprintf(out, size, "--");
        return;
    }
    double value = work.networkDifficulty;
    constexpr const char *units[] = {"", "K", "M", "G", "T", "P", "E"};
    int unit = 0;
    while (value >= 1000 && unit < 6) { value /= 1000; ++unit; }
    // Three significant figures also bound extreme compact-target values.
    if (value >= 1000) std::snprintf(out, size, "%.3g", work.networkDifficulty);
    else std::snprintf(out, size, "%.3g%s", value, units[unit]);
}

inline void formatWorkNBits(char *out, size_t size, const MuxWorkContext &work) {
    std::snprintf(out, size, "nBits %s", work.available ? work.nBits : "--");
}

inline void formatWorkAge(char *out, size_t size, const MuxWorkContext &work) {
    if (work.available) std::snprintf(out, size, "Job %" PRIu32 "s old", work.ageSeconds);
    else std::snprintf(out, size, "Job unavailable");
}

inline bool temperatureAvailable(float temperature) {
    // Zero is the driver's uninitialized/cleared cache sentinel.
    return std::isfinite(temperature) && temperature > 0.0f && temperature <= 150.0f;
}

inline void formatTemperature(char *out, size_t size, float temperature) {
    if (!temperatureAvailable(temperature)) {
        std::snprintf(out, size, "--");
        return;
    }
    std::snprintf(out, size, "%.1f C", static_cast<double>(temperature));
}

inline void formatLabelledTemperature(char *out, size_t size, const char *label,
                                     float temperature, bool fresh) {
    char value[16];
    formatTemperature(value, sizeof(value), fresh ? temperature : 0.0f);
    std::snprintf(out, size, "%s %s", label, value);
}

inline void formatSharedTemperatures(char *out, size_t size, float boardC,
                                     float vrExternalC, float vrInternalC, bool fresh) {
    if (!fresh) {
        std::snprintf(out, size, "Thermal readings unavailable / stale");
        return;
    }
    if (!temperatureAvailable(boardC) && !temperatureAvailable(vrExternalC) &&
        !temperatureAvailable(vrInternalC)) {
        std::snprintf(out, size, "Thermal readings unavailable");
        return;
    }
    char board[16], external[16], internal[16];
    formatTemperature(board, sizeof(board), boardC);
    formatTemperature(external, sizeof(external), vrExternalC);
    formatTemperature(internal, sizeof(internal), vrInternalC);
    std::snprintf(out, size, "Board %s | VR ext %s | int %s", board, external, internal);
}

inline void formatHashrate(char *out, size_t size, float gh) {
    if (!std::isfinite(gh) || gh < 0.0f) {
        std::snprintf(out, size, "--");
    } else if (gh >= 1000.0f) {
        std::snprintf(out, size, "%.2f TH/s", static_cast<double>(gh) / 1000.0);
    } else {
        std::snprintf(out, size, "%.0f GH/s", static_cast<double>(gh));
    }
}

inline void formatChipHashrate(char *out, size_t size, float gh, bool freshMeasuredSample) {
    if (!freshMeasuredSample || !std::isfinite(gh) || gh < 0.0f) {
        std::snprintf(out, size, "--");
    } else {
        std::snprintf(out, size, "%.0f GH/s", static_cast<double>(gh));
    }
}

inline void formatPowerEfficiency(char *out, size_t size, float watts, float gh) {
    if (!std::isfinite(watts) || watts <= 0.0f) {
        std::snprintf(out, size, "Power --  |  -- J/TH");
    } else if (!std::isfinite(gh) || gh <= 0.0f) {
        std::snprintf(out, size, "%.1f W  |  -- J/TH", static_cast<double>(watts));
    } else {
        std::snprintf(out, size, "%.1f W  |  %.1f J/TH", static_cast<double>(watts),
                      static_cast<double>(watts) * 1000.0 / gh);
    }
}

inline void formatCounter(char *out, size_t size, uint64_t value) {
    if (value < 10000) {
        std::snprintf(out, size, "%" PRIu64, value);
        return;
    }
    const char units[] = {'K', 'M', 'G', 'T', 'P', 'E'};
    double scaled = static_cast<double>(value) / 1000.0;
    size_t index = 0;
    while (scaled >= 1000.0 && index < sizeof(units) - 1) {
        scaled /= 1000.0;
        ++index;
    }
    std::snprintf(out, size, "%.1f%c", scaled, units[index]);
}

inline void formatShares(char *out, size_t size, uint64_t accepted, uint64_t rejected) {
    char a[20], r[20];
    formatCounter(a, sizeof(a), accepted);
    formatCounter(r, sizeof(r), rejected);
    std::snprintf(out, size, "Accepted %s  |  Rejected %s", a, r);
}

inline void formatOperatingPoint(char *out, size_t size, int mhz, int mv) {
    if (mhz <= 0 || mv <= 0) {
        std::snprintf(out, size, "Configured clock / voltage: --");
    } else {
        std::snprintf(out, size, "Configured %d MHz  /  %d mV", mhz, mv);
    }
}

inline void formatPoolConnectivity(char *out, size_t size, bool available, int connected,
                                   bool dual) {
    if (!available) {
        std::snprintf(out, size, "Shared route: unavailable");
    } else if (dual) {
        std::snprintf(out, size, "Shared: %d/2 pools connected", connected);
    } else {
        std::snprintf(out, size, "Shared route: %s", connected > 0 ? "connected" : "disconnected");
    }
}

} // namespace FiveTratumDisplay
