#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace FiveTratumDisplay {

// Fixed storage for real startup events. New events move the oldest visible
// line out; no timer invents progress, successful stages or percentages.
class BootStatusHistory {
  public:
    static constexpr size_t LINE_COUNT = 4;
    static constexpr size_t LINE_BYTES = 64;
  private:
    char m_lines[LINE_COUNT][LINE_BYTES]{};
    size_t m_count = 0;
  public:
    bool push(const char *value) {
        if (!value || !*value) return false;
        char bounded[LINE_BYTES]{};
        size_t used = 0;
        for (size_t i = 0; value[i] && used < LINE_BYTES - 1; ++i) {
            const unsigned char c = value[i];
            bounded[used++] = c >= 32 && c <= 126 ? static_cast<char>(c) : ' ';
        }
        while (used && bounded[used - 1] == ' ') bounded[--used] = 0;
        if (!used || (m_count && !strcmp(m_lines[m_count - 1], bounded))) return false;
        if (m_count == LINE_COUNT) {
            memmove(m_lines[0], m_lines[1], (LINE_COUNT - 1) * LINE_BYTES);
            --m_count;
        }
        memcpy(m_lines[m_count++], bounded, LINE_BYTES);
        return true;
    }
    size_t size() const { return m_count; }
    const char *line(size_t index) const { return index < m_count ? m_lines[index] : ""; }
};

// Splash timing and actual startup completion are independent. Finishing early
// does not shorten the splash; finishing late does not hide initialization.
class StartupDisplayGate {
    bool m_complete = false;
    bool m_miningRequested = false;
  public:
    bool requestMining() { m_miningRequested = true; return m_complete; }
    bool complete() { m_complete = true; return m_miningRequested; }
    bool isComplete() const { return m_complete; }
};

struct BrandViewConditions {
    bool miningOverview = false;
    bool displayOn = false;
    bool bootComplete = false;
    bool overlay = false;
    bool enrollmentActive = false;
    bool shutdown = false;
    bool animation = false;
    bool telemetryFresh = false;
};

class IdleBrandCycle {
    int64_t m_lastShownUs = 0;
  public:
    static constexpr int64_t INTERVAL_US = 120000000;
    static constexpr int64_t IDLE_US = 30000000;
    static constexpr int64_t DURATION_US = 3000000;
    bool due(int64_t nowUs, int64_t lastKeypressUs, const BrandViewConditions &view) const {
        return view.miningOverview && view.displayOn && view.bootComplete && !view.overlay &&
            !view.enrollmentActive && !view.shutdown && !view.animation && view.telemetryFresh &&
            nowUs >= 0 && lastKeypressUs >= 0 && m_lastShownUs >= 0 &&
            nowUs >= m_lastShownUs && nowUs >= lastKeypressUs &&
            nowUs - m_lastShownUs >= INTERVAL_US && nowUs - lastKeypressUs >= IDLE_US;
    }
    void shown(int64_t nowUs) { if (nowUs >= 0) m_lastShownUs = nowUs; }
};

} // namespace FiveTratumDisplay
