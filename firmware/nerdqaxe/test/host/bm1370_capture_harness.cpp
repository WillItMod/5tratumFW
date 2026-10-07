#include "bm1370_capture.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <type_traits>

using namespace BM1370Capture;

void *operator new(std::size_t) { std::abort(); }
void *operator new[](std::size_t) { std::abort(); }
void operator delete(void *) noexcept { std::abort(); }
void operator delete[](void *) noexcept { std::abort(); }

static_assert(std::is_trivially_copyable<Record>::value, "fixed records");
static_assert(std::is_standard_layout<Record>::value, "explicit field layout");
static_assert(sizeof(Recorder) <= 128, "small recorder state");
static_assert(MaxStorageBytes == 98304, "96 KiB maximum caller storage");
struct Guarded {
    uint64_t before = 0x123456789abcdef0;
    std::array<Record, MaxRecords> records{};
    uint64_t after = 0x0fedcba987654321;
};
static Guarded storage;
static std::array<Record, MaxRecords> exported{};

static void guards() {
    assert(storage.before == 0x123456789abcdef0 && storage.after == 0x0fedcba987654321);
}
static std::array<uint8_t, 88> payload() {
    std::array<uint8_t, 88> bytes{};
    for (unsigned i = 0; i < bytes.size(); ++i) bytes[i] = i * 29 + 7;
    return bytes;
}
static JobMetadata metadata() {
    JobMetadata job;
    for (unsigned i = 0; i < job.header.size(); ++i) job.header[i] = i * 17 + 23;
    job.logicalJobCounter = 0xffffffff;
    job.versionMask = 0x1fffe000;
    job.asicTicketDifficulty = 2048;
    job.poolIndex = 1;
    return job;
}
static void emptyTail(const Record &record) {
    for (unsigned i = record.capturedLength; i < record.bytes.size(); ++i) assert(record.bytes[i] == 0);
    assert(record.reserved0 == 0);
    for (auto byte : record.reserved1) assert(byte == 0);
}

static void inactiveAndArmBounds() {
    Recorder recorder;
    auto bytes = payload(); auto job = metadata();
    assert(recorder.snapshot().state == State::Disabled);
    assert(!recorder.observe(100));
    assert(!recorder.freeze(1, 100));
    assert(!recorder.recordTxJob(100, 1, bytes.data(), 88, 88, job));
    assert(!recorder.recordTxBytes(100, 0, bytes.data(), 7, 7));
    assert(!recorder.recordRxChunk(100, 0, bytes.data(), 1, 11));
    assert(!recorder.recordTransport(100, 1, Transport::RxTimeout));
    assert(!recorder.recordRetirement(100, 1, Retirement::ParserReset));
    assert(recorder.copyFrozen(1, 0, exported.data(), 384) == 0);
    assert(recorder.snapshot().flags == 0 && recorder.snapshot().captureId == 0);
    assert(!recorder.arm(nullptr, 1, 1, 100, 200));
    assert(!recorder.arm(storage.records.data(), 0, 1, 100, 200));
    assert(!recorder.arm(storage.records.data(), 385, 1, 100, 200));
    assert(!recorder.arm(storage.records.data(), std::numeric_limits<std::size_t>::max(), 1, 100, 200));
    assert(!recorder.arm(storage.records.data(), 1, 0, 100, 200));
    assert(!recorder.arm(storage.records.data(), 1, 1, 0, 200));
    assert(!recorder.arm(storage.records.data(), 1, 1, -1, 200));
    assert(!recorder.arm(storage.records.data(), 1, 1, 100, 0));
    assert(!recorder.arm(storage.records.data(), 1, 1, 100, MaxDurationUs + 1));
    assert(!recorder.arm(storage.records.data(), 1, 1, std::numeric_limits<int64_t>::max(), 1));
    auto *unaligned = reinterpret_cast<Record *>(reinterpret_cast<uint8_t *>(storage.records.data()) + 1);
    assert(!recorder.arm(unaligned, 1, 1, 100, 200));
    assert(recorder.snapshot().state == State::Disabled);
    assert(recorder.arm(storage.records.data(), 2, 1, 100, MaxDurationUs));
    assert(!recorder.arm(storage.records.data(), 2, 2, 100, 1));
    assert(!recorder.snapshot().physicalWireComplete && !recorder.snapshot().physicalChipIdentity
           && !recorder.snapshot().independentWorkAssignment);
    guards();
}

static void immutableMetadataAndGenerationTail() {
    Recorder recorder;
    auto bytes = payload(); const auto originalBytes = bytes;
    auto job = metadata(); const auto originalJob = job;
    const uint8_t frame[] = {0xaa,0x55,0x4c,0x03,0x52,0x75,0x0c,0xd2,0x05,0xa2,0x9c};
    assert(recorder.arm(storage.records.data(), 16, 2, 100, MaxDurationUs));
    assert(recorder.recordTxJob(100, 7, bytes.data(), 88, 88, job));
    bytes.fill(0xff); job.header.fill(0xff); job.logicalJobCounter = 0; job.poolIndex = 0;
    assert(recorder.recordRxChunk(101, 0, frame, 5, 11));
    assert(recorder.recordRetirement(102, 8, Retirement::GenerationChange, 7));
    assert(recorder.recordRxChunk(103, 0, frame + 5, 6, 11));
    assert(recorder.recordTxJob(104, 8, bytes.data(), 88, 88, job));
    assert(recorder.freeze(2, 105));
    assert(recorder.copyFrozen(2, 0, exported.data(), 384) == 5);
    const auto &tx = exported[0];
    assert(tx.kind == Kind::TxJob && tx.sequence == 1 && tx.generation == 7 && tx.timestampUs == 100);
    assert(tx.requestedLength == 88 && tx.transportReturned == 88 && tx.capturedLength == 88);
    assert(tx.bytes == originalBytes && tx.header == originalJob.header);
    assert(tx.logicalJobCounter == 0xffffffff && tx.versionMask == 0x1fffe000 && tx.asicTicketDifficulty == 2048 && tx.poolIndex == 1);
    assert(exported[1].kind == Kind::RxChunk && exported[1].capturedLength == 5 && exported[1].generation == 0);
    assert(exported[2].kind == Kind::Retirement && exported[2].marker == static_cast<uint8_t>(Retirement::GenerationChange));
    assert(exported[2].auxiliaryValue == 7 && exported[2].generation == 8);
    assert(exported[3].kind == Kind::RxChunk && exported[3].capturedLength == 6 && exported[3].generation == 0);
    assert(exported[4].kind == Kind::TxJob && exported[4].generation == 8 && exported[4].bytes == bytes);
    uint8_t reconstructed[11];
    std::memcpy(reconstructed, exported[1].bytes.data(), 5);
    std::memcpy(reconstructed + 5, exported[3].bytes.data(), 6);
    assert(std::memcmp(reconstructed, frame, 11) == 0); // No falsely assigned generation on partial RX.
    for (unsigned i = 0; i < 5; ++i) {
        assert(exported[i].sequence == i + 1);
        emptyTail(exported[i]);
    }
    assert(recorder.snapshot().flags == 0); // Raw RX unknown context is not capture loss.
    guards();
}

static void acceptanceAndExplicitMarkers() {
    Recorder recorder;
    auto bytes = payload(); const auto job = metadata();
    assert(recorder.arm(storage.records.data(), 32, 3, 100, MaxDurationUs));
    const int32_t returns[] = {88, 87, 0, -1, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()};
    for (unsigned i = 0; i < 6; ++i) assert(recorder.recordTxJob(100 + i, i ? 9 : 0, bytes.data(), 88, returns[i], job));
    assert(recorder.recordTxBytes(110, 0, bytes.data(), 7, 7));
    assert(recorder.recordTxBytes(111, 0, bytes.data(), 7, 3));
    for (unsigned i = 1; i <= 8; ++i) assert(recorder.recordTransport(120 + i, 0, static_cast<Transport>(i), -31, 31337));
    for (unsigned i = 1; i <= 4; ++i) assert(recorder.recordRetirement(140 + i, 10, static_cast<Retirement>(i), 42));
    assert(recorder.freeze(3, 150));
    assert(recorder.copyFrozen(3, 0, exported.data(), 384) == 20);
    assert((exported[0].flags & MissingGeneration) && !(exported[0].flags & TransportFailure));
    for (unsigned i = 1; i < 6; ++i) {
        assert(exported[i].transportReturned == returns[i]);
        assert((exported[i].flags & (Incomplete | TransportFailure)) == (Incomplete | TransportFailure));
        assert(exported[i].bytes == bytes); // Requested bytes never masquerade as transmitted bytes.
    }
    assert(exported[6].kind == Kind::TxBytes && exported[6].generation == 0 && exported[6].flags == 0);
    assert((exported[6].header == std::array<uint8_t,80>{}));
    assert(exported[6].poolIndex == UnknownPool);
    assert(exported[7].flags & TransportFailure);
    const uint16_t markerFlags[] = {Incomplete | TransportFailure, Incomplete | TransportFailure, 0,
        Incomplete | StreamFlushed, 0, Incomplete | ObservedLoss, Incomplete | DiscardedPartial, Incomplete | ObservedLoss};
    for (unsigned i = 0; i < 8; ++i) {
        const auto &event = exported[8 + i];
        assert(event.kind == Kind::Transport && event.marker == i + 1 && event.flags == markerFlags[i]);
        assert(event.transportReturned == -31 && event.auxiliaryValue == 31337);
    }
    assert((recorder.snapshot().flags & (Incomplete | TransportFailure | MissingGeneration | ObservedLoss | DiscardedPartial | StreamFlushed))
           == (Incomplete | TransportFailure | MissingGeneration | ObservedLoss | DiscardedPartial | StreamFlushed));
    guards();
}

static void capacityStopAndPagedExport() {
    Recorder recorder;
    auto bytes = payload(); auto job = metadata();
    // Every permitted capacity, not only the maximum.
    for (std::size_t capacity = 1; capacity <= MaxRecords; ++capacity) {
        assert(recorder.arm(storage.records.data(), capacity, 100 + capacity, 100, MaxDurationUs));
        for (std::size_t i = 0; i < capacity; ++i)
            assert(recorder.recordTxJob(100 + i, 7, bytes.data(), 88, 88, job));
        const auto frozen = recorder.snapshot();
        assert(frozen.state == State::Frozen && frozen.reason == FreezeReason::Capacity);
        assert(frozen.records == capacity && frozen.firstSequence == 1 && frozen.lastSequence == capacity);
        assert((frozen.flags & (Incomplete | StorageFull)) == (Incomplete | StorageFull));
        assert(!recorder.recordRxChunk(10000, 0, bytes.data(), 88, 88));
        assert(recorder.copyFrozen(100 + capacity, 0, exported.data(), std::numeric_limits<std::size_t>::max()) == capacity);
        for (std::size_t i = 0; i < capacity; ++i) {
            assert(exported[i].sequence == i + 1 && exported[i].timestampUs == 100 + static_cast<int64_t>(i));
            assert(exported[i].bytes == bytes);
        }
        Record page[3];
        for (std::size_t start = 0; start < capacity; start += 3) {
            const std::size_t count = recorder.copyFrozen(100 + capacity, start, page, 3);
            assert(count == (capacity - start < 3 ? capacity - start : 3));
            assert(std::memcmp(page, exported.data() + start, count * RecordBytes) == 0);
        }
        assert(recorder.snapshot().records == frozen.records && recorder.snapshot().stoppedUs == frozen.stoppedUs);
        guards();
    }
}

static void frozenAndIds() {
    Recorder recorder;
    auto bytes = payload(); auto job = metadata();
    assert(recorder.arm(storage.records.data(), 5, 1000, 100, 1000));
    assert(recorder.recordTxJob(100, 1, bytes.data(), 88, 88, job));
    assert(recorder.copyFrozen(1000, 0, exported.data(), 1) == 0); // No live pointer or copy.
    assert(!recorder.freeze(999, 101));
    assert(recorder.snapshot().state == State::Recording && recorder.snapshot().lastObservedUs == 100);
    assert(recorder.freeze(1000, 102));
    assert(recorder.copyFrozen(1000, 0, exported.data(), 1) == 1);
    Record original = exported[0]; const auto snapshot = recorder.snapshot();
    assert(!recorder.recordTxJob(103, 1, bytes.data(), 88, 88, job));
    assert(!recorder.recordTransport(104, 0, Transport::CallerGap, -1));
    assert(!recorder.observe(-100));
    assert(recorder.freeze(1000, 10000000));
    assert(recorder.snapshot().reason == snapshot.reason && recorder.snapshot().flags == snapshot.flags
           && recorder.snapshot().stoppedUs == snapshot.stoppedUs && recorder.snapshot().lastObservedUs == snapshot.lastObservedUs);
    assert(recorder.copyFrozen(999, 0, exported.data(), 1) == 0);
    assert(recorder.copyFrozen(1000, std::numeric_limits<std::size_t>::max(), exported.data(), 1) == 0);
    assert(recorder.copyFrozen(1000, 1, exported.data(), 1) == 0);
    assert(recorder.copyFrozen(1000, 0, nullptr, 1) == 0);
    assert(recorder.copyFrozen(1000, 0, exported.data(), 0) == 0);
    assert(recorder.copyFrozen(1000, 0, storage.records.data(), 1) == 0); // Immutable store cannot be export destination.
    assert(recorder.copyFrozen(1000, 0, storage.records.data() + 1, 1) == 0);
    auto *unaligned = reinterpret_cast<Record *>(reinterpret_cast<uint8_t *>(exported.data()) + 1);
    assert(recorder.copyFrozen(1000, 0, unaligned, 1) == 0);
    assert(!recorder.arm(storage.records.data(), 5, 1000, 200, 100)); // Avoid old page-ID alias.
    assert(recorder.copyFrozen(1000, 0, exported.data(), 1) == 1);
    assert(std::memcmp(exported.data(), &original, RecordBytes) == 0);
    assert(recorder.arm(storage.records.data(), 5, 1001, 200, 100));
    assert(recorder.copyFrozen(1000, 0, exported.data(), 1) == 0);
    assert(recorder.recordTxJob(200, 2, bytes.data(), 88, 88, job));
    assert(recorder.freeze(1001, 201));
    assert(recorder.copyFrozen(1001, 0, exported.data(), 1) == 1 && exported[0].sequence == 1);
    recorder.reset();
    assert(recorder.snapshot().state == State::Disabled && recorder.snapshot().captureId == 0);
    assert(recorder.copyFrozen(1001, 0, exported.data(), 1) == 0);
    assert(storage.records[0].generation == 2); // Reset never frees/erases caller memory.
    guards();
}

static void deadlineAndClockFailures() {
    Recorder recorder;
    auto bytes = payload();
    assert(recorder.arm(storage.records.data(), 5, 2000, 100, 2));
    assert(recorder.recordRxChunk(100, 0, bytes.data(), 1, 11));
    assert(recorder.recordRxChunk(101, 0, bytes.data(), 1, 11));
    assert(!recorder.recordRxChunk(102, 0, bytes.data(), 1, 11)); // Deadline excluded.
    assert(recorder.snapshot().records == 2 && recorder.snapshot().reason == FreezeReason::TimeLimit);
    assert(recorder.snapshot().flags == 0 && recorder.snapshot().stoppedUs == 102);
    assert(recorder.arm(storage.records.data(), 5, 2001, 100, MaxDurationUs));
    assert(!recorder.observe(10000000));
    assert(recorder.snapshot().reason == FreezeReason::TimeLimit && recorder.snapshot().records == 0);
    assert(recorder.arm(storage.records.data(), 5, 2002, 100, MaxDurationUs));
    assert(!recorder.observe(99));
    assert(recorder.snapshot().reason == FreezeReason::InvalidClock && (recorder.snapshot().flags & ClockRegression));
    assert(recorder.arm(storage.records.data(), 5, 2003, 100, MaxDurationUs));
    assert(recorder.observe(200));
    assert(!recorder.recordRxChunk(199, 0, bytes.data(), 1, 11));
    assert(recorder.snapshot().lastObservedUs == 200 && recorder.snapshot().records == 0);
    assert(recorder.arm(storage.records.data(), 5, 2004, 100, MaxDurationUs));
    assert(!recorder.observe(0));
    assert(recorder.snapshot().reason == FreezeReason::InvalidClock && (recorder.snapshot().flags & InvalidTimestamp));
    assert(recorder.arm(storage.records.data(), 5, 2005, 100, MaxDurationUs));
    assert(!recorder.observe(std::numeric_limits<int64_t>::min()));
    assert(recorder.snapshot().flags & Incomplete);
    assert(recorder.arm(storage.records.data(), 5, 2006, std::numeric_limits<int64_t>::max() - 2, 2));
    assert(recorder.observe(std::numeric_limits<int64_t>::max() - 1));
    assert(!recorder.observe(std::numeric_limits<int64_t>::max()));
    assert(recorder.snapshot().reason == FreezeReason::TimeLimit);
    guards();
}

static void invalidInputsNeverTruncate() {
    Recorder recorder;
    auto bytes = payload(); auto job = metadata();
    uint64_t id = 3000;
    auto arm = [&]() { assert(recorder.arm(storage.records.data(), 5, ++id, 100, MaxDurationUs)); };
    auto invalid = [&]() { assert(recorder.snapshot().state == State::Frozen && recorder.snapshot().reason == FreezeReason::InvalidInput
                                && (recorder.snapshot().flags & (Incomplete | InvalidInput)) == (Incomplete | InvalidInput)
                                && recorder.snapshot().records == 0); guards(); };
    arm(); assert(!recorder.recordTxJob(100, 1, nullptr, 88, 88, job)); invalid();
    arm(); assert(!recorder.recordTxJob(100, 1, bytes.data(), 87, 87, job)); invalid();
    arm(); job.poolIndex = 2; assert(!recorder.recordTxJob(100, 1, bytes.data(), 88, 88, job)); invalid(); job.poolIndex = 1;
    arm(); assert(!recorder.recordTxBytes(100, 0, bytes.data(), 0, 0)); invalid();
    arm(); assert(!recorder.recordTxBytes(100, 0, bytes.data(), 89, 89)); invalid();
    arm(); assert(!recorder.recordRxChunk(100, 0, bytes.data(), 89, 89)); invalid();
    arm(); assert(!recorder.recordRxChunk(100, 0, bytes.data(), std::numeric_limits<std::size_t>::max(), 88)); invalid();
    arm(); assert(!recorder.recordRxChunk(100, 0, nullptr, 1, 11)); invalid();
    arm(); assert(!recorder.recordRxChunk(100, 0, bytes.data(), 3, 2)); invalid();
    arm(); assert(!recorder.recordRxChunk(100, 0, bytes.data(), 0, 11)); invalid();
    arm(); assert(!recorder.recordTransport(100, 0, static_cast<Transport>(0))); invalid();
    arm(); assert(!recorder.recordTransport(100, 0, static_cast<Transport>(255))); invalid();
    arm(); assert(!recorder.recordRetirement(100, 1, static_cast<Retirement>(0))); invalid();
    arm(); assert(!recorder.recordRetirement(100, 1, static_cast<Retirement>(255))); invalid();
}

static void deterministicCopies() {
    Recorder first, second;
    auto bytes = payload(); auto job = metadata();
    static std::array<Record, 4> secondStore{};
    assert(first.arm(storage.records.data(), 4, 4000, 100, 1000));
    assert(second.arm(secondStore.data(), 4, 4000, 100, 1000));
    auto events = [&](Recorder &recorder) {
        assert(recorder.recordTxJob(100, 123, bytes.data(), 88, 88, job));
        assert(recorder.recordTxBytes(101, 0, bytes.data(), 7, 7));
        assert(recorder.recordRxChunk(102, 0, bytes.data(), 3, std::numeric_limits<uint32_t>::max()));
        assert(recorder.recordRetirement(103, 124, Retirement::ParserReset));
    };
    events(first); events(second);
    assert(std::memcmp(storage.records.data(), secondStore.data(), RecordBytes * 4) == 0);
    assert(first.copyFrozen(4000, 0, exported.data(), 4) == 4);
    assert(std::memcmp(exported.data(), secondStore.data(), RecordBytes * 4) == 0);
    for (unsigned i = 0; i < 4; ++i) emptyTail(exported[i]);
    guards();
}

int main() {
    inactiveAndArmBounds(); immutableMetadataAndGenerationTail(); acceptanceAndExplicitMarkers();
    capacityStopAndPagedExport(); frozenAndIds(); deadlineAndClockFailures();
    invalidInputsNeverTruncate(); deterministicCopies();
    std::printf("{\"groupsPassed\":8,\"recordBytes\":256,\"maxRecords\":384,\"storageBytes\":98304,\"maximumDurationUs\":2000000,\"allCapacitiesTested\":384,\"boundedWritesTested\":73920,\"heapAllocationAllowed\":false,\"ownQAxeHardwareTested\":false,\"physicalWireComplete\":false,\"physicalChipIdentity\":false,\"independentWorkAssignment\":false}\n");
}
