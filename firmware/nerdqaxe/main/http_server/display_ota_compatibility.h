#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

// Compatibility checks only: this does not authenticate an image, replace
// ESP-IDF's full image validation, or qualify a different controller/board.
namespace FiveTratumDisplayOta {
inline constexpr std::size_t ImagePrefixBytes = 24 + 8 + 256;
inline constexpr int64_t PrefixReadTimeoutUs = 15 * 1000 * 1000;

inline bool prefixReadDeadlineExpired(int64_t startedUs, int64_t nowUs) {
    // esp_timer_get_time is monotonic. Treat impossible clock values as a
    // failed read rather than extending an already bounded admission phase.
    return startedUs < 0 || nowUs < startedUs || nowUs - startedUs >= PrefixReadTimeoutUs;
}

inline bool isWideProfile(const char *id) {
    return id && (std::strcmp(id, "480x320-rgb") == 0 || std::strcmp(id, "480x320-bgr") == 0);
}

inline bool requiresProfileSupport(const char *saved, const char *active) {
    // A pending change back to the stock size must not bypass protection for
    // the larger panel that is still active until the next restart.
    return isWideProfile(saved) || isWideProfile(active);
}

inline uint32_t readLittle32(const uint8_t *value) {
    return static_cast<uint32_t>(value[0]) | static_cast<uint32_t>(value[1]) << 8 |
           static_cast<uint32_t>(value[2]) << 16 | static_cast<uint32_t>(value[3]) << 24;
}

inline bool fieldEquals(const uint8_t *field, std::size_t size, const char *expected) {
    const std::size_t length = std::strlen(expected);
    return length < size && std::memcmp(field, expected, length) == 0 && field[length] == 0;
}

inline bool takeNumber(const char *&cursor, const char *end, unsigned &number) {
    if (cursor == end || *cursor < '0' || *cursor > '9') return false;
    const char *start = cursor;
    number = 0;
    while (cursor != end && *cursor >= '0' && *cursor <= '9') {
        if (number > 99999) return false;
        number = number * 10 + static_cast<unsigned>(*cursor++ - '0');
    }
    return cursor - start == 1 || *start != '0';
}

inline bool takeLiteral(const char *&cursor, const char *end, const char *literal) {
    const auto length = std::strlen(literal);
    if (static_cast<std::size_t>(end - cursor) < length || std::memcmp(cursor, literal, length) != 0) return false;
    cursor += length;
    return true;
}

inline bool profileAwareVersion(const uint8_t *field, std::size_t size) {
#if defined(NERDOCTAXEGAMMA) && !defined(NERDQAXEPLUS2)
    constexpr const char *familyPrefix = "5tratumFW-oct-";
    constexpr unsigned firstProfileBeta = 6;
#elif defined(NERDQAXEPLUS2) && !defined(NERDOCTAXEGAMMA)
    constexpr const char *familyPrefix = "5tratumFW-qa-";
    constexpr unsigned firstProfileBeta = 7;
#else
    // An absent, unsupported or mixed build identity cannot admit an image.
    (void)field;
    (void)size;
    return false;
#endif
#if defined(NERDOCTAXEGAMMA) != defined(NERDQAXEPLUS2)
    if (!field) return false;
    const auto *start = reinterpret_cast<const char *>(field);
    const auto *end = static_cast<const char *>(std::memchr(start, 0, size));
    if (!end) return false;
    const char *cursor = start;
    unsigned major, minor, patch, iteration = 0, stage = 3;
    if (!takeLiteral(cursor, end, familyPrefix) || !takeNumber(cursor, end, major) ||
        !takeLiteral(cursor, end, ".") || !takeNumber(cursor, end, minor) ||
        !takeLiteral(cursor, end, ".") || !takeNumber(cursor, end, patch)) return false;
    if (cursor != end) {
        if (takeLiteral(cursor, end, "-alpha.")) stage = 0;
        else if (takeLiteral(cursor, end, "-beta.")) stage = 1;
        else if (takeLiteral(cursor, end, "-rc.")) stage = 2;
        else return false;
        if (!takeNumber(cursor, end, iteration)) return false;
    }
    if (cursor != end) return false;
    // QA beta.7 and Oct beta.6 first expose persisted wide-display profiles.
    // Future releases of the same family must retain this contract. A version is
    // neither cryptographic proof nor permission to interchange board images.
    if (major != 0) return true;
    if (minor != 1) return minor > 1;
    if (patch != 0) return true;
    return stage > 1 || (stage == 1 && iteration >= firstProfileBeta);
#endif
}

inline bool compatibleImagePrefix(const uint8_t *data, std::size_t bytes) {
    if (!data || bytes < ImagePrefixBytes || data[0] != 0xe9 || data[1] == 0 || data[1] > 16 ||
        data[12] != 9 || data[13] != 0 || data[23] > 1 || readLittle32(data + 28) < 256 ||
        readLittle32(data + 32) != 0xabcd5432) return false;
    return fieldEquals(data + 80, 32, "esp-miner") && profileAwareVersion(data + 48, 32);
}
} // namespace FiveTratumDisplayOta
