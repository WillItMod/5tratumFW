#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Pure, fixed-storage wire codec. It has no UART, RTOS or board dependencies.
// Chain jobs reproduce the existing Nerd BM1370 sender; addressed commands
// do not establish addressed work acceptance. Chip work stays unsupported.
namespace BM1370Protocol {

constexpr std::size_t JobPayloadBytes = 82;
constexpr std::size_t MaxTxBytes = 88;
constexpr std::size_t ResponseBytes = 11;
constexpr std::size_t RegisterReadBytes = 7;

enum class TargetKind : uint8_t { Chain, Chip };
struct WorkTarget {
    TargetKind kind = TargetKind::Chain;
    uint8_t chipAddress = 0;
};

enum class EncodeStatus : uint8_t { Ok, Unsupported, InvalidArgument };
struct TxPacket {
    std::array<uint8_t, MaxTxBytes> bytes{};
    std::size_t length = 0;
    uint8_t wireJobId = 0;
};

struct Job {
    uint32_t logicalJobCounter = 0;
    uint32_t startNonce = 0;
    uint32_t nBits = 0;
    uint32_t nTime = 0;
    uint32_t version = 0;
    // Already in the exact wire order of bm_job.*_be in the existing sender;
    // these arrays are not canonical header-order hashes.
    std::array<uint8_t, 32> merkleRootWire{};
    std::array<uint8_t, 32> previousBlockHashWire{};
};

uint8_t wireJobId(uint32_t logicalJobCounter) noexcept;
EncodeStatus encodeJob(const WorkTarget &target, const Job &job, TxPacket &out) noexcept;
EncodeStatus encodeRawJob(const WorkTarget &target, const uint8_t *payload,
                          std::size_t length, TxPacket &out) noexcept;
// The existing addressed register-read command (0x42), not a work selector.
EncodeStatus encodeRegisterRead(uint8_t address, uint8_t reg, TxPacket &out) noexcept;

enum class ResponseType : uint8_t { Register = 0, Nonce = 4 };
enum class DecodeStatus : uint8_t { Ok, InvalidLength, InvalidPreamble, BadCrc, UnsupportedType };
struct Response {
    std::array<uint8_t, ResponseBytes> raw{};
    ResponseType type = ResponseType::Register;
    uint32_t nonce = 0;                 // Header-order nonce: wire bytes LE32.
    uint32_t rolledVersionBits = 0;     // Wire BE16 shifted left by 13.
    uint8_t jobSlotId = 0;              // (raw result header & 0xf0) >> 1.
    uint8_t extraDifficulty = 0;
    uint8_t resultHeader = 0;
    uint32_t registerValue = 0;         // Wire bytes BE32.
    uint8_t registerAddress = 0;        // Explicit address on register replies.
    uint8_t registerIndex = 0;
    // Nonce replies have no proven physical ASIC index in this codec.
};

// RX format validated against published captured BM1370 frames: zero CRC5
// residue over all nine post-preamble bytes, including type and checksum.
DecodeStatus decodeResponse(const uint8_t *frame, std::size_t length, Response &out) noexcept;

struct StreamStats {
    uint64_t bytesReceived = 0;
    uint64_t discardedBytes = 0;
    uint64_t validFrames = 0;
    uint64_t crcFailures = 0;
    uint64_t unsupportedFrames = 0;
};

class StreamParser {
public:
    // At most one result per byte; consumes no bytes beyond that result.
    // Out is cleared when false. Valid payload preambles are never split.
    bool push(uint8_t byte, Response &out) noexcept;
    // A reset drops an incomplete response. Counters remain unless requested.
    void reset(bool clearStats = false) noexcept;
    std::size_t bufferedBytes() const noexcept { return used_; }
    const StreamStats &stats() const noexcept { return stats_; }

private:
    void resynchronize() noexcept;
    std::array<uint8_t, ResponseBytes> bytes_{};
    std::size_t used_ = 0;
    StreamStats stats_{};
};

} // namespace BM1370Protocol
