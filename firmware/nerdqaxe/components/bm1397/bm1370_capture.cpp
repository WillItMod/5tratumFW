#include "bm1370_capture.h"

#include <cstring>
#include <limits>

namespace BM1370Capture {
namespace {

bool transportValid(Transport marker) noexcept {
    return marker >= Transport::RxError && marker <= Transport::CallerGap;
}
bool retirementValid(Retirement marker) noexcept {
    return marker >= Retirement::GenerationChange && marker <= Retirement::JobSlotRetired;
}
uint16_t transportFlags(Transport marker) noexcept {
    switch (marker) {
    case Transport::RxError:
    case Transport::TxError:
        return Incomplete | TransportFailure;
    case Transport::ObservedUartLoss:
    case Transport::CallerGap:
        return Incomplete | ObservedLoss;
    case Transport::PartialDiscard:
        return Incomplete | DiscardedPartial;
    case Transport::BufferFlush:
        return Incomplete | StreamFlushed;
    case Transport::RxTimeout:
    case Transport::BaudChanged:
        return NoFlags;
    }
    return Incomplete | InvalidInput;
}
bool overlaps(const Record *buffer, std::size_t capacity, const Record *out,
              std::size_t count) noexcept {
    const auto first = reinterpret_cast<std::uintptr_t>(buffer);
    const auto destination = reinterpret_cast<std::uintptr_t>(out);
    const auto maximum = std::numeric_limits<std::uintptr_t>::max();
    const std::size_t destinationBytes = count * RecordBytes;
    if (destination > maximum - destinationBytes) return true;
    const auto last = first + capacity * RecordBytes; // Arm validated this bound.
    return destination < last && destination + destinationBytes > first;
}

} // namespace

bool Recorder::arm(Record *buffer, std::size_t capacity, uint64_t captureId,
                   int64_t startUs, uint32_t durationUs) noexcept {
    if (snapshot_.state == State::Recording || !buffer || capacity == 0 || capacity > MaxRecords
        || captureId == 0 || captureId == snapshot_.captureId || startUs <= 0
        || durationUs == 0 || durationUs > MaxDurationUs
        || startUs > std::numeric_limits<int64_t>::max() - durationUs) return false;
    const auto address = reinterpret_cast<std::uintptr_t>(buffer);
    if (address % alignof(Record) || address > std::numeric_limits<std::uintptr_t>::max() - capacity * RecordBytes)
        return false;
    snapshot_ = {};
    snapshot_.state = State::Recording;
    snapshot_.captureId = captureId;
    snapshot_.startUs = startUs;
    snapshot_.deadlineUs = startUs + durationUs;
    snapshot_.lastObservedUs = startUs;
    snapshot_.capacity = capacity;
    buffer_ = buffer;
    return true;
}

void Recorder::stop(FreezeReason reason, int64_t nowUs, uint32_t flags) noexcept {
    if (snapshot_.state != State::Recording) return;
    snapshot_.state = State::Frozen;
    snapshot_.reason = reason;
    snapshot_.flags |= flags;
    snapshot_.stoppedUs = nowUs;
}

bool Recorder::prepare(int64_t nowUs) noexcept {
    if (snapshot_.state != State::Recording) return false;
    if (nowUs <= 0) {
        stop(FreezeReason::InvalidClock, nowUs, Incomplete | InvalidTimestamp);
        return false;
    }
    if (nowUs < snapshot_.lastObservedUs) {
        stop(FreezeReason::InvalidClock, nowUs, Incomplete | ClockRegression);
        return false;
    }
    snapshot_.lastObservedUs = nowUs;
    if (nowUs >= snapshot_.deadlineUs) {
        stop(FreezeReason::TimeLimit, nowUs, NoFlags);
        return false;
    }
    return true;
}

bool Recorder::observe(int64_t nowUs) noexcept { return prepare(nowUs); }

bool Recorder::freeze(uint64_t captureId, int64_t nowUs) noexcept {
    if (!captureId || captureId != snapshot_.captureId || snapshot_.state == State::Disabled) return false;
    if (snapshot_.state == State::Frozen) return true;
    if (prepare(nowUs)) stop(FreezeReason::Manual, nowUs, NoFlags);
    return snapshot_.state == State::Frozen;
}

void Recorder::reset() noexcept {
    buffer_ = nullptr;
    snapshot_ = {};
}

bool Recorder::invalid(int64_t nowUs) noexcept {
    stop(FreezeReason::InvalidInput, nowUs, Incomplete | InvalidInput);
    return false;
}

bool Recorder::append(Record &record) noexcept {
    // Only prepared records reach this function; capacity is bounded at arm.
    record.sequence = static_cast<uint64_t>(snapshot_.records) + 1;
    snapshot_.flags |= record.flags;
    buffer_[snapshot_.records] = record;
    ++snapshot_.records;
    if (!snapshot_.firstSequence) snapshot_.firstSequence = record.sequence;
    snapshot_.lastSequence = record.sequence;
    if (snapshot_.records == snapshot_.capacity)
        stop(FreezeReason::Capacity, record.timestampUs, Incomplete | StorageFull);
    return true;
}

bool Recorder::recordTxJob(int64_t nowUs, uint64_t generation, const uint8_t *bytes,
                           std::size_t length, int32_t returned, const JobMetadata &job) noexcept {
    if (!prepare(nowUs)) return false;
    if (!bytes || length != MaxRawBytes || (job.poolIndex != 0 && job.poolIndex != 1 && job.poolIndex != UnknownPool))
        return invalid(nowUs);
    Record record{};
    record.kind = Kind::TxJob;
    record.timestampUs = nowUs;
    record.generation = generation;
    record.logicalJobCounter = job.logicalJobCounter;
    record.versionMask = job.versionMask;
    record.asicTicketDifficulty = job.asicTicketDifficulty;
    record.requestedLength = length;
    record.transportReturned = returned;
    record.capturedLength = length;
    record.poolIndex = job.poolIndex;
    record.header = job.header;
    std::memcpy(record.bytes.data(), bytes, length);
    if (!generation) record.flags |= Incomplete | MissingGeneration;
    if (returned != static_cast<int32_t>(length)) record.flags |= Incomplete | TransportFailure;
    return append(record);
}

bool Recorder::recordTxBytes(int64_t nowUs, uint64_t generation, const uint8_t *bytes,
                             std::size_t length, int32_t returned) noexcept {
    if (!prepare(nowUs)) return false;
    if (!bytes || length == 0 || length > MaxRawBytes) return invalid(nowUs);
    Record record{};
    record.kind = Kind::TxBytes;
    record.timestampUs = nowUs;
    record.generation = generation;
    record.requestedLength = length;
    record.transportReturned = returned;
    record.capturedLength = length;
    std::memcpy(record.bytes.data(), bytes, length);
    if (returned != static_cast<int32_t>(length)) record.flags |= Incomplete | TransportFailure;
    return append(record);
}

bool Recorder::recordRxChunk(int64_t nowUs, uint64_t generation, const uint8_t *bytes,
                             std::size_t length, uint32_t requestedLength) noexcept {
    if (!prepare(nowUs)) return false;
    if (!bytes || length == 0 || length > MaxRawBytes || requestedLength < length) return invalid(nowUs);
    Record record{};
    record.kind = Kind::RxChunk;
    record.timestampUs = nowUs;
    record.generation = generation;
    record.requestedLength = requestedLength;
    record.transportReturned = length;
    record.capturedLength = length;
    std::memcpy(record.bytes.data(), bytes, length);
    return append(record);
}

bool Recorder::recordTransport(int64_t nowUs, uint64_t generation, Transport marker,
                               int32_t returned, uint32_t auxiliaryValue) noexcept {
    if (!prepare(nowUs)) return false;
    if (!transportValid(marker)) return invalid(nowUs);
    Record record{};
    record.kind = Kind::Transport;
    record.marker = static_cast<uint8_t>(marker);
    record.timestampUs = nowUs;
    record.generation = generation;
    record.transportReturned = returned;
    record.auxiliaryValue = auxiliaryValue;
    record.flags = transportFlags(marker);
    return append(record);
}

bool Recorder::recordRetirement(int64_t nowUs, uint64_t generation, Retirement marker,
                                uint32_t auxiliaryValue) noexcept {
    if (!prepare(nowUs)) return false;
    if (!retirementValid(marker)) return invalid(nowUs);
    Record record{};
    record.kind = Kind::Retirement;
    record.marker = static_cast<uint8_t>(marker);
    record.timestampUs = nowUs;
    record.generation = generation;
    record.auxiliaryValue = auxiliaryValue;
    return append(record);
}

std::size_t Recorder::copyFrozen(uint64_t captureId, std::size_t start, Record *out,
                                 std::size_t capacity) const noexcept {
    if (!captureId || captureId != snapshot_.captureId || snapshot_.state != State::Frozen
        || !out || !capacity || start >= snapshot_.records) return 0;
    const std::size_t remaining = snapshot_.records - start;
    const std::size_t count = capacity < remaining ? capacity : remaining;
    const auto address = reinterpret_cast<std::uintptr_t>(out);
    if (address % alignof(Record) || overlaps(buffer_, snapshot_.capacity, out, count)) return 0;
    std::memcpy(out, buffer_ + start, count * RecordBytes);
    return count;
}

} // namespace BM1370Capture
