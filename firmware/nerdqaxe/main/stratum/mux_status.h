#pragma once

#include <stdint.h>
#include <string.h>
#include <cmath>
#include "ArduinoJson.h"
#include "coin_context.h"
#include "work_context.h"

// A peer advertisement, never cryptographic authentication or ASIC isolation.
struct MuxStatusSnapshot {
    bool connected = false;
    bool acknowledged = false;
    bool expired = false;
    uint32_t ageSeconds = 0;
    uint32_t ttlSeconds = 90;
    MuxCoinContext coin;
    MuxWorkContext workContext;
};

inline MuxCoinContext selectMuxCoinContext(const MuxStatusSnapshot &primary,
                                           const MuxStatusSnapshot &secondary,
                                           bool dual, int active) {
    if (!dual) {
        const auto &selected = active == 1 ? secondary : primary;
        return selected.connected && selected.acknowledged ? selected.coin : MuxCoinContext{};
    }
    // Two configured/connected pools are not proof of one chain. Both must
    // declare the same fresh identity before showing chain-wide market data.
    if (primary.connected && secondary.connected && primary.acknowledged && secondary.acknowledged &&
        primary.coin.available && secondary.coin.available &&
        !strcmp(primary.coin.id, secondary.coin.id) &&
        !strcmp(primary.coin.ticker, secondary.coin.ticker)) return primary.coin;
    return {};
}

// A dual route may have two different jobs, even when both coins match. There
// is deliberately no invented aggregate job/block/difficulty for dual mode.
inline MuxWorkContext selectMuxWorkContext(const MuxStatusSnapshot &primary,
                                           const MuxStatusSnapshot &secondary,
                                           bool dual, int active) {
    if (dual) return {};
    const auto &selected = active == 1 ? secondary : primary;
    return selected.connected && selected.acknowledged ? selected.workContext : MuxWorkContext{};
}

inline bool muxPrintableString(JsonVariantConst value, size_t maximum, char *out) {
    if (!value.is<const char *>()) return false;
    const JsonString text = value.as<JsonString>();
    if (!text.size() || text.size() > maximum) return false;
    for (size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = text.c_str()[i];
        if (c < 32 || c > 126) return false;
    }
    memcpy(out, text.c_str(), text.size());
    out[text.size()] = 0;
    return true;
}

inline bool muxNBitsString(JsonVariantConst value, char *out, bool lowercaseOnly) {
    if (!value.is<const char *>()) return false;
    const JsonString text = value.as<JsonString>();
    if (text.size() != 8) return false;
    for (size_t i = 0; i < 8; ++i) {
        char c = text.c_str()[i];
        if (!lowercaseOnly && c >= 'A' && c <= 'F') c += 'a' - 'A';
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
        out[i] = c;
    }
    out[8] = 0;
    return true;
}

// Caller serializes access. All time values are monotonic microseconds.
class MuxPeerStatus {
    uint64_t m_generation = 0;
    bool m_connected = false;
    bool m_advertised = false;
    bool m_v2 = false;
    int64_t m_receivedUs = 0;
    MuxCoinContext m_coin;
    MuxWorkContext m_work;
    int64_t m_workReceivedUs = 0;
    uint64_t m_workAgeUs = 0;
    char m_jobId[65] = {};
    char m_nBits[9] = {};
    bool m_jobAvailable = false;
    bool m_jobAgeAvailable = false;
    uint64_t m_jobAgeUs = 0;
    int64_t m_jobAgeCapturedUs = 0;

    void clearWork() {
        m_work = {};
        m_workReceivedUs = 0;
        m_workAgeUs = 0;
    }

  public:
    static constexpr uint32_t TTL_SECONDS = 90;
    static constexpr size_t MAX_NOTIFICATION_BYTES = 1024;
    static constexpr const char *METHOD = "mining.5tratum.status";

    uint64_t beginConnection() {
        ++m_generation;
        if (!m_generation) ++m_generation;
        m_connected = true;
        m_advertised = false;
        m_v2 = false;
        m_receivedUs = 0;
        m_coin = {};
        clearWork();
        m_jobAvailable = false;
        m_jobAgeAvailable = false;
        m_jobId[0] = m_nBits[0] = 0;
        return m_generation;
    }

    void disconnect() {
        m_connected = false;
        m_advertised = false;
        m_receivedUs = 0;
        m_coin = {};
        clearWork();
        m_jobAvailable = false;
        m_jobAgeAvailable = false;
        m_jobId[0] = m_nBits[0] = 0;
    }

    uint64_t generation() const { return m_generation; }

    void invalidate(uint64_t generation) {
        if (generation == m_generation) {
            m_advertised = false;
            m_coin = {};
            clearWork();
        }
    }

    // Observe before the normal job dispatcher, without changing mining work.
    // Every new notify removes the preceding v2 job/coin immediately, including
    // malformed metadata, but never intercepts ordinary Stratum notifications.
    void observeMiningNotify(JsonDocument &doc, uint64_t generation) {
        if (!m_connected || generation != m_generation) return;
        const auto method = doc["method"].as<JsonString>();
        if (!method.c_str() || strcmp(method.c_str(), "mining.notify")) return;
        clearWork();
        if (m_v2) m_coin = {};
        m_jobAvailable = false;
        m_jobAgeAvailable = false;
        m_jobId[0] = m_nBits[0] = 0;
        if (method.size() != 13) return; // NUL-suffixed method cannot keep the old job.
        JsonArrayConst params = doc["params"].as<JsonArrayConst>();
        if (params.size() < 9) return;
        m_jobAvailable = muxPrintableString(params[0], 64, m_jobId) &&
                         muxNBitsString(params[6], m_nBits, false);
    }

    bool matchesJob(const MuxWorkContext &work) const {
        return !work.available || (m_jobAvailable && !strcmp(work.jobId, m_jobId) &&
                                   !strcmp(work.nBits, m_nBits));
    }

    bool acknowledge(uint64_t generation, int64_t nowUs, const MuxCoinContext &coin = {},
                     const MuxWorkContext &work = {}, bool v2 = false) {
        if (!m_connected || generation != m_generation || nowUs < 0) return false;
        if (!matchesJob(work)) { invalidate(generation); return false; }
        uint64_t ageUs = static_cast<uint64_t>(work.ageSeconds) * 1000000ULL;
        if (work.available && m_jobAgeAvailable) {
            // Heartbeats must not reset an old job's lifetime, even if a sender
            // repeats or regresses its whole-second advertised age.
            if (nowUs < m_jobAgeCapturedUs) { invalidate(generation); return false; }
            const uint64_t previousAge = m_jobAgeUs + static_cast<uint64_t>(nowUs - m_jobAgeCapturedUs);
            if (ageUs < previousAge) ageUs = previousAge;
        }
        if (work.available) {
            // Keep only a freshness bound for this observed job after null,
            // expiry or invalidation; a new notify resets it. Cleared metadata
            // must not let later heartbeats resurrect the same expired job.
            m_jobAgeAvailable = true;
            m_jobAgeUs = ageUs;
            m_jobAgeCapturedUs = nowUs;
        }
        m_advertised = true;
        m_receivedUs = nowUs;
        m_v2 = v2;
        m_coin = coin;
        clearWork(); // Legacy/null context immediately clears old job metadata.
        if (work.available && ageUs < TTL_SECONDS * 1000000ULL) {
            m_work = work;
            m_workReceivedUs = nowUs;
            m_workAgeUs = ageUs;
        } else if (v2) m_coin = {};
        return true;
    }

    MuxStatusSnapshot snapshot(int64_t nowUs) const {
        MuxStatusSnapshot result;
        result.connected = m_connected;
        if (!m_connected || !m_advertised) return result;
        if (nowUs < m_receivedUs) { result.expired = true; return result; }
        const uint64_t ageUs = static_cast<uint64_t>(nowUs - m_receivedUs);
        const uint64_t ageSeconds = ageUs / 1000000ULL;
        result.ageSeconds = ageSeconds > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(ageSeconds);
        result.acknowledged = ageUs < TTL_SECONDS * 1000000ULL;
        result.expired = !result.acknowledged;
        if (!result.acknowledged) return result;
        result.coin = m_coin;
        if (m_work.available && nowUs >= m_workReceivedUs) {
            const uint64_t workAgeUs = m_workAgeUs + static_cast<uint64_t>(nowUs - m_workReceivedUs);
            if (workAgeUs < TTL_SECONDS * 1000000ULL) {
                result.workContext = m_work;
                result.workContext.ageSeconds = static_cast<uint32_t>(workAgeUs / 1000000ULL);
            } else if (m_v2) result.coin = {};
        }
        return result;
    }
};

inline bool parseMuxCoinContext(JsonVariantConst value, MuxCoinContext &coin) {
    coin = {};
    if (value.isNull()) return true; // Explicit unknown, including legacy peers.
    JsonObjectConst object = value.as<JsonObjectConst>();
    if (object.size() != 3 || !object["id"].is<const char *>() ||
        !object["ticker"].is<const char *>() || !object["name"].is<const char *>()) return false;
    const auto id = object["id"].as<JsonString>();
    const auto ticker = object["ticker"].as<JsonString>();
    const auto name = object["name"].as<JsonString>();
    if (!id.size() || id.size() > 32 || !ticker.size() || ticker.size() > 12 ||
        !name.size() || name.size() > 48) return false;
    for (size_t i = 0; i < id.size(); ++i) {
        const char c = id.c_str()[i];
        // Canonical route IDs include "5trat". Keep this an ASCII slug;
        // accepting its first digit does not relax the observed-job binding.
        if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && !(i && c == '-')) return false;
    }
    for (size_t i = 0; i < ticker.size(); ++i) {
        const char c = ticker.c_str()[i];
        if (!(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9')) return false;
    }
    for (size_t i = 0; i < name.size(); ++i) {
        const unsigned char c = name.c_str()[i];
        if (c < 32 || c > 126) return false; // No controls or embedded NUL.
    }
    memcpy(coin.id, id.c_str(), id.size());
    memcpy(coin.ticker, ticker.c_str(), ticker.size());
    memcpy(coin.name, name.c_str(), name.size());
    coin.available = true;
    return true;
}

inline bool parseMuxWorkContext(JsonVariantConst value, MuxWorkContext &work) {
    work = {};
    if (value.isNull()) return true;
    const JsonObjectConst object = value.as<JsonObjectConst>();
    if (object.size() != 6 || !muxPrintableString(object["jobId"], 64, work.jobId) ||
        !muxNBitsString(object["nBits"], work.nBits, true) ||
        object["height"].isUnbound() || object["networkDifficulty"].isUnbound() ||
        !object["ageSeconds"].is<uint32_t>() || object["ageSeconds"].as<uint32_t>() >= 90 ||
        !object["source"].is<const char *>() || object["source"].as<JsonString>().size() != 21 ||
        strcmp(object["source"].as<const char *>(), "forwarded-stratum-job")) return false;
    if (!object["height"].isNull()) {
        if (!object["height"].is<uint32_t>()) return false;
        work.heightAvailable = true;
        work.height = object["height"].as<uint32_t>();
    }
    if (!object["networkDifficulty"].isNull()) {
        if (!object["networkDifficulty"].is<double>()) return false;
        const double difficulty = object["networkDifficulty"].as<double>();
        if (!std::isfinite(difficulty) || difficulty <= 0) return false;
        work.difficultyAvailable = true;
        work.networkDifficulty = difficulty;
    }
    work.ageSeconds = object["ageSeconds"].as<uint32_t>();
    work.available = true;
    return true;
}

// Consume every message using our method, including invalid messages, so it
// cannot become a share response in the generic Stratum parser.
inline bool consumeMuxStatusNotification(JsonDocument &doc, MuxPeerStatus &state,
                                         uint64_t generation, int64_t nowUs,
                                         size_t notificationBytes = 0) {
    const char *method = doc["method"].as<const char *>();
    if (!method || strcmp(method, MuxPeerStatus::METHOD)) return false;
    JsonObjectConst envelope = doc.as<JsonObjectConst>();
    bool validEnvelope = doc["method"].as<JsonString>().size() == strlen(MuxPeerStatus::METHOD);
    for (JsonPairConst pair : envelope) {
        const char *key = pair.key().c_str();
        if (!strcmp(key, "id") || !strcmp(key, "method") || !strcmp(key, "params")) continue;
        if (!strcmp(key, "jsonrpc") && pair.value().is<const char *>() &&
            pair.value().as<JsonString>().size() == 3 && !strcmp(pair.value().as<const char *>(), "2.0")) continue;
        validEnvelope = false;
    }
    JsonArrayConst params = doc["params"].as<JsonArrayConst>();
    JsonObjectConst status = params.size() == 1 ? params[0].as<JsonObjectConst>() : JsonObjectConst();
    const char *product = status["product"].as<const char *>();
    const char *scope = status["workScope"].as<const char *>();
    const bool hasCoin = !status["coin"].isUnbound();
    const bool hasWork = !status["workContext"].isUnbound();
    const uint32_t protocol = status["protocolVersion"].as<uint32_t>();
    MuxCoinContext coin;
    MuxWorkContext work;
    const bool valid = validEnvelope && !doc.overflowed() &&
        (!notificationBytes || notificationBytes <= MuxPeerStatus::MAX_NOTIFICATION_BYTES) &&
        measureJson(doc) <= MuxPeerStatus::MAX_NOTIFICATION_BYTES && doc["id"].isNull() &&
        status["protocolVersion"].is<uint32_t>() &&
        ((protocol == 1 && !hasWork && status.size() == (hasCoin ? 6 : 5)) ||
         (protocol == 2 && hasCoin && hasWork && status.size() == 7)) &&
        product && status["product"].as<JsonString>().size() == 8 && !strcmp(product, "5tratMUX") &&
        scope && status["workScope"].as<JsonString>().size() == 15 && !strcmp(scope, "chain-broadcast") &&
        status["independentWorkAssignment"].is<bool>() && !status["independentWorkAssignment"].as<bool>() &&
        status["ttlSeconds"].is<uint32_t>() && status["ttlSeconds"].as<uint32_t>() == MuxPeerStatus::TTL_SECONDS &&
        parseMuxCoinContext(status["coin"], coin) && parseMuxWorkContext(status["workContext"], work);
    if (valid) state.acknowledge(generation, nowUs, coin, work, protocol == 2);
    else state.invalidate(generation);
    return true;
}
