#pragma once

#include <cstddef>
#include <cstdio>
#include <cstring>

namespace FiveTratumNativePool {

constexpr std::size_t MaxAgentBytes = 192;
constexpr std::size_t AgentBufferBytes = MaxAgentBytes + 1;
constexpr std::size_t MaxComponentBytes = 64;

inline bool safeComponent(const char *value) noexcept {
    if (!value || !value[0]) return false;
    for (std::size_t i = 0; i <= MaxComponentBytes; ++i) {
        const unsigned char byte = static_cast<unsigned char>(value[i]);
        if (!byte) return i <= MaxComponentBytes;
        if (i == MaxComponentBytes || byte < 0x20 || byte > 0x7e ||
            byte == '"' || byte == '\\' || byte == '/' || byte == ':') return false;
    }
    return false;
}

inline bool validDeviceId(const char *value) noexcept {
    if (!value || std::strncmp(value, "5tfw:", 5)) return false;
    bool nonzero = false;
    for (std::size_t i = 5; i < 37; ++i) {
        const char byte = value[i];
        if (!((byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f'))) return false;
        nonzero |= byte != '0';
    }
    return value[37] == '\0' && nonzero;
}

// A peer-reported ordinary pool stream, never ASIC ownership or attestation.
// Preserve the terminal qualifier so existing work-context-v2 peers opt in.
inline bool buildAgent(char *out, std::size_t capacity, const char *device,
                       const char *asic, const char *version,
                       const char *deviceId, int poolIndex) noexcept {
    if (!out || !capacity) return false;
    out[0] = '\0';
    if (!safeComponent(device) || !safeComponent(asic) || !safeComponent(version)) return false;
    const bool hint = (poolIndex == 0 || poolIndex == 1) && validDeviceId(deviceId);
    const int written = hint
        ? std::snprintf(out, capacity,
            "5tratumFW/%s/%s/%s/native-pool-v1:%s:%d/work-context-v2",
            device, asic, version, deviceId + 5, poolIndex)
        : std::snprintf(out, capacity, "5tratumFW/%s/%s/%s/work-context-v2",
            device, asic, version);
    if (written < 0 || static_cast<std::size_t>(written) >= capacity ||
        static_cast<std::size_t>(written) > MaxAgentBytes) {
        out[0] = '\0';
        return false;
    }
    return true;
}

} // namespace FiveTratumNativePool
