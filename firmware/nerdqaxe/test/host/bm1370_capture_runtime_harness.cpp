#include "bm1370_capture_runtime.h"
#include "serial.h"
#include "driver/uart.h"
#include "esp_heap_caps.h"

#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <pthread.h>

namespace {
std::atomic<int64_t> fakeTime{1000};
std::atomic<unsigned> allocations{0};
std::atomic<unsigned> ordinaryAllocations{0};
bool failAllocation = false;
std::size_t lastAllocationBytes = 0;
std::size_t lastAllocationAlignment = 0;
uint32_t lastAllocationCaps = 0;
alignas(BM1370Capture::Record) static uint8_t psram[BM1370Capture::MaxStorageBytes + 8];
thread_local bool pauseNextCaptureLock = false;
pthread_mutex_t pauseMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pauseCondition = PTHREAD_COND_INITIALIZER;
bool producerPaused = false;
bool producerReleased = false;

int testCaptureMutexLock(pthread_mutex_t *mutex);
}

int64_t esp_timer_get_time() { return fakeTime.fetch_add(1); }
void *heap_caps_malloc(std::size_t bytes, uint32_t caps) {
    ++ordinaryAllocations;
    assert(bytes == BM1370Capture::MaxStorageBytes);
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    // Model a platform allocator that meets four-byte alignment only. This is
    // a host regression fixture, not an observed address from the live QAxe.
    return psram + 4;
}
void *heap_caps_aligned_alloc(std::size_t alignment, std::size_t bytes, uint32_t caps) {
    ++allocations; lastAllocationBytes = bytes; lastAllocationCaps = caps;
    lastAllocationAlignment = alignment;
    assert(alignment == alignof(BM1370Capture::Record));
    assert(bytes == BM1370Capture::MaxStorageBytes);
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (failAllocation) return nullptr;
    return psram;
}

// The complete production runtime is copied verbatim into a temporary include.
// Replace only mutex acquisition with a deterministic barrier around the real
// pthread mutex. It pauses after the actual runtime's active-session load.
#define pthread_mutex_lock testCaptureMutexLock
#include "capture_runtime_production.cpp"
#undef pthread_mutex_lock

namespace {
int testCaptureMutexLock(pthread_mutex_t *mutex) {
    if (pauseNextCaptureLock && mutex == &FiveTratumCapture::mutex) {
        pauseNextCaptureLock = false;
        pthread_mutex_lock(&pauseMutex);
        producerPaused = true;
        pthread_cond_broadcast(&pauseCondition);
        while (!producerReleased) pthread_cond_wait(&pauseCondition, &pauseMutex);
        pthread_mutex_unlock(&pauseMutex);
    }
    return pthread_mutex_lock(mutex);
}

std::atomic<unsigned> uartWrites{0}, uartReads{0}, uartWaits{0}, uartPolls{0}, uartFlushes{0}, baudChanges{0};
unsigned uartConfigurations = 0, uartPins = 0, uartInstalls = 0;
int nextWriteReturn = -999;
int nextReadReturn = -999;
esp_err_t nextBaudResult = ESP_OK, nextFlushResult = ESP_OK;
std::array<uint8_t, 88> lastWritten{};
int lastWrittenLength = 0;
std::array<uint8_t, 88> incoming{};
std::size_t incomingLength = 0, incomingPosition = 0;
uint32_t lastTimeout = 0;

std::array<uint8_t, 88> jobBytes() {
    std::array<uint8_t, 88> bytes{};
    for (unsigned i = 0; i < 88; ++i) bytes[i] = i * 29 + 17;
    return bytes;
}
BM1370Capture::JobMetadata jobMetadata() {
    BM1370Capture::JobMetadata metadata;
    for (unsigned i = 0; i < 80; ++i) metadata.header[i] = i * 31 + 7;
    metadata.logicalJobCounter = 15;
    metadata.versionMask = 0x1fffe000;
    metadata.asicTicketDifficulty = 2048;
    metadata.poolIndex = 1;
    return metadata;
}
FiveTratumCapture::Status status() {
    FiveTratumCapture::Status result;
    assert(FiveTratumCapture::snapshot(result));
    return result;
}
uint64_t arm(uint32_t duration = BM1370Capture::MaxDurationUs) {
    uint64_t id = 0;
    assert(FiveTratumCapture::arm(duration, id));
    assert(id != 0);
    return id;
}
void freeze(uint64_t id) { assert(FiveTratumCapture::freeze(id)); }
std::size_t page(uint64_t id, std::size_t offset, BM1370Capture::Record *out,
                 std::size_t limit = FiveTratumCapture::PageRecords) {
    std::size_t count = 999;
    FiveTratumCapture::Status snapshot;
    assert(FiveTratumCapture::copyPage(id, offset, out, limit, count, snapshot));
    assert(snapshot.capture.captureId == id && snapshot.capture.state == BM1370Capture::State::Frozen);
    assert(snapshot.storageBytes == 98304 && snapshot.droppedBusy == 0);
    return count;
}

void psramFailureAndReuse() {
    assert(status().capture.state == BM1370Capture::State::Disabled);
    assert(allocations == 0);
    static_assert(alignof(BM1370Capture::Record) == 8, "Regression models the required eight-byte record alignment");
    auto *fourByteStorage = static_cast<BM1370Capture::Record *>(heap_caps_malloc(
        BM1370Capture::MaxStorageBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    assert(reinterpret_cast<std::uintptr_t>(fourByteStorage) % 4 == 0);
    assert(reinterpret_cast<std::uintptr_t>(fourByteStorage) % 8 == 4);
    BM1370Capture::Recorder oldAllocationCandidate;
    assert(!oldAllocationCandidate.arm(fourByteStorage, 384, 1, 100, 1000));
    assert(oldAllocationCandidate.snapshot().state == BM1370Capture::State::Disabled);
    assert(ordinaryAllocations == 1); // Only this deliberate old-allocator reproduction.
    uint64_t id = 42;
    assert(!FiveTratumCapture::arm(0, id) && id == 0 && allocations == 0);
    assert(!FiveTratumCapture::arm(BM1370Capture::MaxDurationUs + 1, id) && id == 0 && allocations == 0);
    failAllocation = true;
    assert(!FiveTratumCapture::arm(1000, id) && id == 0 && allocations == 1);
    auto snapshot = status();
    assert(snapshot.capture.state == BM1370Capture::State::Disabled && snapshot.storageBytes == 0);
    auto bytes = jobBytes();
    FiveTratumCapture::tx(bytes.data(), 7, 7);
    FiveTratumCapture::rx(bytes.data(), 11, 11);
    assert(status().capture.records == 0 && allocations == 1);
    failAllocation = false;
    id = arm();
    assert(allocations == 2 && lastAllocationBytes == 98304);
    assert(lastAllocationAlignment == alignof(BM1370Capture::Record));
    assert(lastAllocationCaps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    assert(FiveTratumCapture::storage == reinterpret_cast<BM1370Capture::Record *>(psram));
    uint64_t rejected = 42;
    assert(!FiveTratumCapture::arm(1000, rejected) && rejected == 0);
    assert(status().capture.captureId == id);
    freeze(id);
    const auto *allocation = FiveTratumCapture::storage;
    for (unsigned i = 0; i < 20; ++i) {
        const auto next = arm();
        FiveTratumCapture::tx(bytes.data(), 7, 7);
        freeze(next);
        assert(FiveTratumCapture::storage == allocation && allocations == 2 && ordinaryAllocations == 1);
    }
}

void serialCompositeAndRawRx() {
    SERIAL_init();
    assert(uartConfigurations == 1 && uartPins == 1 && uartInstalls == 1);
    const auto beforeWrites = uartWrites.load(), beforeReads = uartReads.load();
    const auto beforeWaits = uartWaits.load(), beforePolls = uartPolls.load();
    const uint64_t id = arm();
    auto bytes = jobBytes(); const auto original = bytes;
    auto metadata = jobMetadata(); const auto originalMetadata = metadata;
    assert(SERIAL_send_job(bytes.data(), 88, metadata, 77) == 88);
    bytes.fill(0xff); metadata.header.fill(0xff);
    assert(uartWrites == beforeWrites + 1 && lastWritten == original);
    assert(status().capture.records == 1); // No duplicate ordinary TX event.
    assert(SERIAL_send(bytes.data(), 7) == 7);
    const uint8_t observed[] = {0xaa,0x55,0x4c,0x03,0x52,0x75,0x0c,0xd2,0x05,0xa2,0x9c};
    std::memcpy(incoming.data(), observed, sizeof(observed)); incomingLength = 11; incomingPosition = 0;
    uint8_t rx[11]{};
    nextReadReturn = 5;
    assert(SERIAL_rx(rx, 11, 250) == 5 && std::memcmp(rx, observed, 5) == 0);
    assert(lastTimeout == 250);
    FiveTratumCapture::retirement(BM1370Capture::Retirement::GenerationChange, 77, 78);
    nextReadReturn = 6;
    assert(SERIAL_rx(rx, 11, 250) == 6 && std::memcmp(rx, observed + 5, 6) == 0);
    assert(uartReads == beforeReads + 2);
    assert(uartWaits == beforeWaits && uartPolls == beforePolls);
    freeze(id);
    BM1370Capture::Record records[8];
    assert(page(id, 0, records) == 5);
    assert(records[0].kind == BM1370Capture::Kind::TxJob && records[0].bytes == original);
    assert(records[0].header == originalMetadata.header && records[0].generation == 77);
    assert(records[0].transportReturned == 88 && records[0].requestedLength == 88);
    assert(records[1].kind == BM1370Capture::Kind::TxBytes && records[1].capturedLength == 7 && records[1].generation == 0);
    assert(records[2].kind == BM1370Capture::Kind::RxChunk && records[2].capturedLength == 5 && records[2].generation == 0);
    assert(records[3].kind == BM1370Capture::Kind::Retirement && records[3].generation == 78 && records[3].auxiliaryValue == 77);
    assert(records[3].marker == static_cast<uint8_t>(BM1370Capture::Retirement::GenerationChange));
    assert(records[4].kind == BM1370Capture::Kind::RxChunk && records[4].capturedLength == 6 && records[4].generation == 0);
    uint8_t reassembled[11];
    std::memcpy(reassembled, records[2].bytes.data(), 5);
    std::memcpy(reassembled + 5, records[4].bytes.data(), 6);
    assert(std::memcmp(reassembled, observed, 11) == 0 && status().capture.flags == 0);
    assert(allocations == 2);
}

void signedReturnsAndOnlyExplicitOperations() {
    const uint64_t id = arm();
    auto bytes = jobBytes(); const auto metadata = jobMetadata();
    const auto beforeWrites = uartWrites.load(), beforeReads = uartReads.load();
    const auto beforeWaits = uartWaits.load(), beforeFlushes = uartFlushes.load();
    nextWriteReturn = -7;
    assert(SERIAL_send_job(bytes.data(), 88, metadata, 1) == -7);
    nextWriteReturn = 3;
    assert(SERIAL_send(bytes.data(), 7) == 3);
    nextReadReturn = -17;
    uint8_t read[11];
    assert(SERIAL_rx(read, 11, 250) == -17);
    nextReadReturn = 0;
    assert(SERIAL_rx(read, 11, 250) == 0); // Timeout has no fabricated RX bytes.
    assert(uartWrites == beforeWrites + 2 && uartReads == beforeReads + 2 && uartWaits == beforeWaits);
    SERIAL_set_baud(1000000);
    nextBaudResult = -5;
    assert(!SERIAL_set_baud_checked(115200));
    nextBaudResult = ESP_OK;
    SERIAL_clear_buffer();
    nextFlushResult = -11;
    assert(!SERIAL_clear_buffer_checked());
    nextFlushResult = ESP_OK;
    assert(uartFlushes == beforeFlushes + 2);
    assert(SERIAL_wait_tx_idle(500));
    assert(uartWaits == beforeWaits + 1); // Only the explicit application call waits.
    freeze(id);
    BM1370Capture::Record records[8];
    assert(page(id, 0, records) == 7);
    assert(records[0].transportReturned == -7 && (records[0].flags & BM1370Capture::TransportFailure));
    assert(records[1].transportReturned == 3 && (records[1].flags & BM1370Capture::TransportFailure));
    assert(records[2].kind == BM1370Capture::Kind::Transport && records[2].marker == static_cast<uint8_t>(BM1370Capture::Transport::RxError));
    assert(records[2].transportReturned == -17);
    assert(records[3].marker == static_cast<uint8_t>(BM1370Capture::Transport::BaudChanged) && records[3].auxiliaryValue == 1000000);
    assert(records[4].transportReturned == -5);
    assert(records[5].marker == static_cast<uint8_t>(BM1370Capture::Transport::BufferFlush));
    assert(records[6].transportReturned == -11);
    assert(status().capture.flags & BM1370Capture::Incomplete);
    assert(allocations == 2 && uartPolls == 0);
}

void timeAndFrozenPaging() {
    const uint64_t id = arm(1000);
    auto bytes = jobBytes();
    FiveTratumCapture::tx(bytes.data(), 7, 7);
    const auto active = status();
    std::size_t copied = 999; FiveTratumCapture::Status result; BM1370Capture::Record records[8];
    assert(!FiveTratumCapture::copyPage(id, 0, records, 8, copied, result) && copied == 0);
    assert(!FiveTratumCapture::freeze(id + 1));
    fakeTime.store(active.capture.deadlineUs);
    const auto timed = status();
    assert(timed.capture.state == BM1370Capture::State::Frozen && timed.capture.reason == BM1370Capture::FreezeReason::TimeLimit);
    assert(timed.capture.records == 1 && timed.capture.flags == 0);
    assert(page(id, 0, records) == 1);
    const auto original = records[0];
    FiveTratumCapture::tx(bytes.data(), 7, 7);
    FiveTratumCapture::transport(BM1370Capture::Transport::CallerGap);
    assert(FiveTratumCapture::freeze(id));
    for (unsigned i = 0; i < 16; ++i) {
        const auto immutable = status();
        assert(immutable.capture.records == timed.capture.records && immutable.capture.stoppedUs == timed.capture.stoppedUs
               && immutable.capture.lastObservedUs == timed.capture.lastObservedUs && immutable.capture.flags == timed.capture.flags);
        assert(page(id, 0, records) == 1 && std::memcmp(records, &original, sizeof(original)) == 0);
    }
    assert(!FiveTratumCapture::copyPage(id + 1, 0, records, 8, copied, result) && copied == 0);
    assert(!FiveTratumCapture::copyPage(id, 0, records, 9, copied, result) && copied == 0);
    assert(!FiveTratumCapture::copyPage(id, 0, nullptr, 8, copied, result) && copied == 0);
    assert(!FiveTratumCapture::copyPage(id, 2, records, 8, copied, result) && copied == 0);
    assert(page(id, 1, records) == 0); // Exact EOF is a valid empty final page.
    const auto next = arm();
    assert(next != id && !FiveTratumCapture::copyPage(id, 0, records, 8, copied, result));
    freeze(next);
}

void *delayedProducer(void *) {
    pauseNextCaptureLock = true;
    auto bytes = jobBytes();
    FiveTratumCapture::tx(bytes.data(), 7, 7);
    return nullptr;
}
void delayedOldSessionCannotWriteNewCapture() {
    const auto a = arm();
    pthread_t producer;
    pthread_mutex_lock(&pauseMutex); producerPaused = false; producerReleased = false;
    pthread_mutex_unlock(&pauseMutex);
    assert(pthread_create(&producer, nullptr, delayedProducer, nullptr) == 0);
    pthread_mutex_lock(&pauseMutex);
    while (!producerPaused) pthread_cond_wait(&pauseCondition, &pauseMutex);
    pthread_mutex_unlock(&pauseMutex);
    freeze(a);
    const auto old = status();
    const auto b = arm();
    assert(b != a && status().capture.records == 0);
    pthread_mutex_lock(&pauseMutex); producerReleased = true;
    pthread_cond_broadcast(&pauseCondition); pthread_mutex_unlock(&pauseMutex);
    assert(pthread_join(producer, nullptr) == 0);
    auto fresh = status();
    assert(fresh.capture.captureId == b && fresh.capture.state == BM1370Capture::State::Recording && fresh.capture.records == 0);
    assert(FiveTratumCapture::activeId == b);
    auto bytes = jobBytes();
    FiveTratumCapture::tx(bytes.data(), 7, 7);
    freeze(b);
    BM1370Capture::Record records[8];
    assert(page(b, 0, records) == 1 && records[0].sequence == 1);
    assert(old.capture.captureId == a && old.capture.records == 0);
    assert(allocations == 2);
}

void *producerWork(void *argument) {
    const auto producer = reinterpret_cast<std::uintptr_t>(argument);
    std::array<uint8_t, 11> bytes{};
    bytes[0] = producer;
    for (unsigned i = 0; i < 100; ++i) {
        bytes[1] = i;
        if (producer == 1) FiveTratumCapture::tx(bytes.data(), 7, 7);
        else FiveTratumCapture::rx(bytes.data(), 11, 11);
    }
    return nullptr;
}
void twoProducersOrderedAndBoundedPages() {
    const auto id = arm();
    pthread_t a, b;
    assert(pthread_create(&a, nullptr, producerWork, reinterpret_cast<void *>(1)) == 0);
    assert(pthread_create(&b, nullptr, producerWork, reinterpret_cast<void *>(2)) == 0);
    assert(pthread_join(a, nullptr) == 0 && pthread_join(b, nullptr) == 0);
    freeze(id);
    assert(status().capture.records == 200 && status().capture.flags == 0);
    BM1370Capture::Record records[8];
    uint64_t sequence = 0; int64_t time = 0; unsigned counts[2]{};
    for (unsigned offset = 0; offset < 200; offset += 8) {
        const auto copied = page(id, offset, records);
        assert(copied == 8);
        for (unsigned i = 0; i < 8; ++i) {
            assert(records[i].sequence == ++sequence && records[i].timestampUs > time);
            time = records[i].timestampUs;
            const unsigned producer = records[i].bytes[0];
            assert(producer == 1 || producer == 2); ++counts[producer - 1];
            assert(records[i].generation == 0);
        }
    }
    assert(counts[0] == 100 && counts[1] == 100 && allocations == 2);
}

struct PageRace {
    uint64_t id = 0;
    std::atomic<unsigned> waiting{0};
    std::atomic<bool> start{false};
    unsigned validPages = 0;
    unsigned liveRefusals = 0;
};
void *pageRaceReader(void *argument) {
    auto &race = *static_cast<PageRace *>(argument);
    ++race.waiting;
    while (!race.start.load()) {}
    BM1370Capture::Record records[8];
    for (unsigned i = 0; i < 100; ++i) {
        std::size_t copied = 99;
        FiveTratumCapture::Status snapshot;
        const bool okay = FiveTratumCapture::copyPage(race.id, (i % 2) * 8, records, 8, copied, snapshot);
        if (okay) {
            ++race.validPages;
            assert(snapshot.capture.state == BM1370Capture::State::Frozen && copied == 8);
            for (unsigned j = 0; j < 8; ++j) assert(records[j].sequence == (i % 2) * 8 + j + 1);
        } else {
            ++race.liveRefusals;
            assert(snapshot.capture.state == BM1370Capture::State::Recording && copied == 0);
        }
    }
    return nullptr;
}
void *pageRaceFreezer(void *argument) {
    auto &race = *static_cast<PageRace *>(argument);
    ++race.waiting;
    while (!race.start.load()) {}
    freeze(race.id);
    return nullptr;
}
void concurrentPagesAndFreeze() {
    // Repeat different scheduler orderings with the actual serialized runtime.
    for (unsigned repeat = 0; repeat < 32; ++repeat) {
        PageRace race;
        race.id = arm();
        auto bytes = jobBytes();
        for (unsigned i = 0; i < 16; ++i) FiveTratumCapture::rx(bytes.data(), 11, 11);
        pthread_t reader, freezer;
        assert(pthread_create(&reader, nullptr, pageRaceReader, &race) == 0);
        assert(pthread_create(&freezer, nullptr, pageRaceFreezer, &race) == 0);
        while (race.waiting.load() != 2) {}
        race.start.store(true);
        assert(pthread_join(reader, nullptr) == 0 && pthread_join(freezer, nullptr) == 0);
        assert(race.validPages + race.liveRefusals == 100);
        const auto stopped = status();
        assert(stopped.capture.state == BM1370Capture::State::Frozen && stopped.capture.records == 16);
        BM1370Capture::Record first[8], again[8];
        assert(page(race.id, 0, first) == 8 && page(race.id, 0, again) == 8);
        assert(std::memcmp(first, again, sizeof(first)) == 0);
        assert(allocations == 2);
    }
}

void capacityAndCounterWrap() {
    const auto id = arm(); auto bytes = jobBytes();
    for (unsigned i = 0; i < 384; ++i) FiveTratumCapture::rx(bytes.data(), 11, 11);
    const auto full = status();
    assert(full.capture.state == BM1370Capture::State::Frozen && full.capture.reason == BM1370Capture::FreezeReason::Capacity);
    assert(full.capture.records == 384 && (full.capture.flags & BM1370Capture::StorageFull));
    FiveTratumCapture::rx(bytes.data(), 11, 11);
    assert(status().capture.records == 384);
    BM1370Capture::Record records[8];
    assert(page(id, 376, records) == 8 && records[7].sequence == 384);
    pthread_mutex_lock(&FiveTratumCapture::mutex);
    FiveTratumCapture::nextId = UINT32_MAX - 1;
    pthread_mutex_unlock(&FiveTratumCapture::mutex);
    const auto maximum = arm();
    assert(maximum == UINT32_MAX);
    FiveTratumCapture::rx(bytes.data(), 11, 11);
    freeze(maximum);
    assert(page(maximum, 0, records) == 1);
    uint64_t wrapped = 99;
    assert(!FiveTratumCapture::arm(1000, wrapped) && wrapped == 0);
    const auto preserved = status();
    assert(preserved.capture.captureId == maximum && preserved.capture.records == 1 && allocations == 2);
    assert(FiveTratumCapture::activeId == 0);
}
}

esp_err_t uart_param_config(uart_port_t port, const uart_config_t *config) {
    assert(port == UART_NUM_1 && config->baud_rate == 115200 && config->data_bits == UART_DATA_8_BITS);
    assert(config->parity == UART_PARITY_DISABLE && config->stop_bits == UART_STOP_BITS_1
           && config->flow_ctrl == UART_HW_FLOWCTRL_DISABLE); ++uartConfigurations; return ESP_OK;
}
esp_err_t uart_set_pin(uart_port_t port, int tx, int rx, int rts, int cts) {
    assert(port == UART_NUM_1 && tx == 17 && rx == 18 && rts == UART_PIN_NO_CHANGE && cts == UART_PIN_NO_CHANGE);
    ++uartPins; return ESP_OK;
}
esp_err_t uart_driver_install(uart_port_t port, int rx, int tx, int queue, void *handle, int flags) {
    assert(port == UART_NUM_1 && rx == 2048 && tx == 2048 && queue == 0 && !handle && flags == 0);
    ++uartInstalls; return ESP_OK;
}
esp_err_t uart_set_baudrate(uart_port_t port, uint32_t) { assert(port == UART_NUM_1); ++baudChanges; return nextBaudResult; }
int uart_write_bytes(uart_port_t port, const void *bytes, std::size_t length) {
    assert(port == UART_NUM_1 && length <= 88); ++uartWrites;
    std::memcpy(lastWritten.data(), bytes, length); lastWrittenLength = length;
    const int result = nextWriteReturn == -999 ? static_cast<int>(length) : nextWriteReturn;
    nextWriteReturn = -999; return result;
}
int uart_read_bytes(uart_port_t port, void *out, uint32_t requested, uint32_t timeout) {
    assert(port == UART_NUM_1); ++uartReads; lastTimeout = timeout;
    const int desired = nextReadReturn == -999 ? static_cast<int>(incomingLength - incomingPosition) : nextReadReturn;
    nextReadReturn = -999;
    if (desired <= 0) return desired;
    const auto count = static_cast<std::size_t>(desired);
    assert(count <= requested && incomingPosition + count <= incomingLength);
    std::memcpy(out, incoming.data() + incomingPosition, count); incomingPosition += count;
    return desired;
}
esp_err_t uart_get_buffered_data_len(uart_port_t port, std::size_t *length) { assert(port == UART_NUM_1); ++uartPolls; *length = 0; return ESP_OK; }
esp_err_t uart_flush(uart_port_t port) { assert(port == UART_NUM_1); ++uartFlushes; return nextFlushResult; }
esp_err_t uart_wait_tx_done(uart_port_t port, uint32_t timeout) { assert(port == UART_NUM_1 && timeout == 500); ++uartWaits; return ESP_OK; }

int main() {
    psramFailureAndReuse(); serialCompositeAndRawRx(); signedReturnsAndOnlyExplicitOperations();
    timeAndFrozenPaging(); delayedOldSessionCannotWriteNewCapture(); twoProducersOrderedAndBoundedPages();
    concurrentPagesAndFreeze(); capacityAndCounterWrap();
    assert(ordinaryAllocations == 1);
    std::printf("{\"groupsPassed\":8,\"actualRuntimeAndSerialBodies\":true,\"realPthreads\":true,\"delayedOldSessionRejected\":true,\"concurrentProducerRecords\":200,\"concurrentPageFreezeAttempts\":3200,\"pageLimit\":8,\"storageBytes\":98304,\"psramAllocationAttempts\":2,\"psramAlignmentRequested\":8,\"ordinaryFourByteAllocationRejected\":true,\"runtimeOrdinaryMallocCalls\":0,\"perEventAllocation\":false,\"extraUartOperations\":false,\"counterWrapRejected\":true,\"ownQAxeHardwareTested\":false,\"physicalWireComplete\":false,\"physicalChipIdentity\":false,\"independentWorkAssignment\":false}\n");
}
