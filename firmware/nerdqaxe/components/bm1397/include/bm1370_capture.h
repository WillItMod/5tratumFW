#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Passive software observations only. No UART, clock, RTOS, allocation or
// locking lives here. Every call (including export) must be externally serialized.
namespace BM1370Capture {

constexpr uint32_t SchemaVersion = 1;
constexpr std::size_t RecordBytes = 256;
constexpr std::size_t MaxRecords = 384;
constexpr std::size_t MaxStorageBytes = RecordBytes * MaxRecords; // 96 KiB.
constexpr std::size_t MaxRawBytes = 88;
constexpr uint32_t MaxDurationUs = 2000000;
constexpr uint8_t UnknownPool = 0xff;

enum class State : uint8_t { Disabled, Recording, Frozen };
enum class Kind : uint8_t { TxJob = 1, TxBytes = 2, RxChunk = 3, Transport = 4, Retirement = 5 };
using RecordKind = Kind;
enum class FreezeReason : uint8_t { None, Manual, TimeLimit, Capacity, InvalidInput, InvalidClock };
enum class Transport : uint8_t {
    RxError = 1, TxError, RxTimeout, BufferFlush, BaudChanged,
    ObservedUartLoss, PartialDiscard, CallerGap
};
enum class Retirement : uint8_t { GenerationChange = 1, PowerReset, ParserReset, JobSlotRetired };

enum Flag : uint32_t {
    NoFlags = 0,
    Incomplete = 1U << 0,
    StorageFull = 1U << 1,
    InvalidInput = 1U << 2,
    InvalidTimestamp = 1U << 3,
    ClockRegression = 1U << 4,
    MissingGeneration = 1U << 5,
    TransportFailure = 1U << 6,
    ObservedLoss = 1U << 7,
    DiscardedPartial = 1U << 8,
    StreamFlushed = 1U << 9
};

struct JobMetadata {
    // Exact canonical eighty-byte template, including its starting nonce;
    // never substitute unrelated node tip/header/coin metadata.
    std::array<uint8_t, 80> header{};
    uint32_t logicalJobCounter = 0;
    uint32_t versionMask = 0;
    uint32_t asicTicketDifficulty = 0;
    uint8_t poolIndex = UnknownPool;
};

// Explicit padding makes every copied byte deterministic. Fields are native
// primitives for bounded runtime serialization, not a portable binary format.
struct Record {
    uint64_t sequence = 0;
    int64_t timestampUs = 0;
    uint64_t generation = 0; // Caller software context, not an on-wire field.
    uint32_t logicalJobCounter = 0;
    uint32_t versionMask = 0;
    uint32_t asicTicketDifficulty = 0;
    uint32_t requestedLength = 0;
    int32_t transportReturned = 0;
    uint32_t auxiliaryValue = 0;
    uint16_t capturedLength = 0;
    Kind kind = Kind::TxJob;
    uint8_t marker = 0;
    uint8_t poolIndex = UnknownPool;
    uint8_t reserved0 = 0;
    uint16_t flags = 0;
    std::array<uint8_t, MaxRawBytes> bytes{};
    std::array<uint8_t, 80> header{};
    std::array<uint8_t, 32> reserved1{};
};
static_assert(sizeof(Record) == RecordBytes, "Capture record layout changed");
static_assert(offsetof(Record, bytes) == 56 && offsetof(Record, header) == 144
              && offsetof(Record, reserved1) == 224, "Capture record padding changed");

struct Snapshot {
    uint32_t schemaVersion = SchemaVersion;
    State state = State::Disabled;
    FreezeReason reason = FreezeReason::None;
    uint32_t flags = NoFlags;
    uint64_t captureId = 0;
    int64_t startUs = 0;
    int64_t deadlineUs = 0;
    int64_t lastObservedUs = 0;
    int64_t stoppedUs = 0;
    std::size_t capacity = 0;
    std::size_t records = 0;
    uint64_t firstSequence = 0;
    uint64_t lastSequence = 0;
    // Driver acceptance and software RX observation never prove wire delivery,
    // absence of UART overruns, physical chip identity or work isolation.
    bool physicalWireComplete = false;
    bool physicalChipIdentity = false;
    bool independentWorkAssignment = false;
};

class Recorder {
public:
    // Disabled until explicitly armed. Buffer ownership/lifetime stays with
    // caller; firmware integration must use PSRAM with no internal fallback.
    bool arm(Record *buffer, std::size_t capacity, uint64_t captureId,
             int64_t startUs, uint32_t durationUs) noexcept;
    bool observe(int64_t nowUs) noexcept;
    bool freeze(uint64_t captureId, int64_t nowUs) noexcept;
    // Drops the capture reference; it does not erase or free caller storage.
    void reset() noexcept;

    bool recordTxJob(int64_t nowUs, uint64_t generation, const uint8_t *bytes,
                     std::size_t length, int32_t returned, const JobMetadata &job) noexcept;
    bool recordTxBytes(int64_t nowUs, uint64_t generation, const uint8_t *bytes,
                       std::size_t length, int32_t returned) noexcept;
    bool recordRxChunk(int64_t nowUs, uint64_t generation, const uint8_t *bytes,
                       std::size_t length, uint32_t requestedLength) noexcept;
    bool recordTransport(int64_t nowUs, uint64_t generation, Transport marker,
                         int32_t returned = 0, uint32_t auxiliaryValue = 0) noexcept;
    bool recordRetirement(int64_t nowUs, uint64_t generation, Retirement marker,
                          uint32_t auxiliaryValue = 0) noexcept;

    const Snapshot &snapshot() const noexcept { return snapshot_; }
    // Only a matching immutable frozen capture can be exported. No pointer to
    // live storage escapes. Zero means absent/mismatched/empty/out-of-range.
    std::size_t copyFrozen(uint64_t captureId, std::size_t start, Record *out,
                           std::size_t capacity) const noexcept;

private:
    bool prepare(int64_t nowUs) noexcept;
    bool invalid(int64_t nowUs) noexcept;
    bool append(Record &record) noexcept;
    void stop(FreezeReason reason, int64_t nowUs, uint32_t flags) noexcept;
    Record *buffer_ = nullptr;
    Snapshot snapshot_{};
};

} // namespace BM1370Capture
