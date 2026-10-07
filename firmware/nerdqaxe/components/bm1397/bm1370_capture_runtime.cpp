#include "bm1370_capture_runtime.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <atomic>
#include <new>
#include <pthread.h>

namespace FiveTratumCapture {
namespace {
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
BM1370Capture::Recorder recorder;
BM1370Capture::Record *storage = nullptr;
uint32_t nextId = 0; // Reject wrap; atomic32 is supported on the ESP32-S3.
std::atomic<uint32_t> activeId{0};

void settleLocked() {
    const auto now = esp_timer_get_time();
    recorder.observe(now);
    activeId.store(recorder.snapshot().state == BM1370Capture::State::Recording
        ? static_cast<uint32_t>(recorder.snapshot().captureId) : 0);
}
void statusLocked(Status &status) {
    status.capture = recorder.snapshot();
    status.droppedBusy = 0; // Producers serialize; there is no lossy try-lock.
    status.storageBytes = storage ? BM1370Capture::MaxStorageBytes : 0;
}
struct Producer {
    bool locked = false;
    bool sameSession = false;
    Producer() noexcept {
        const uint32_t session = activeId.load();
        if (!session) return;
        pthread_mutex_lock(&mutex);
        locked = true;
        sameSession = recorder.snapshot().captureId == session;
        if (!sameSession) return; // A delayed producer must never enter a new capture.
        settleLocked();
    }
    ~Producer() {
        if (locked) {
            activeId.store(recorder.snapshot().state == BM1370Capture::State::Recording
                ? static_cast<uint32_t>(recorder.snapshot().captureId) : 0);
            pthread_mutex_unlock(&mutex);
        }
    }
    explicit operator bool() const noexcept {
        return locked && sameSession && recorder.snapshot().state == BM1370Capture::State::Recording;
    }
};
}
bool arm(uint32_t durationUs, uint64_t &captureId) {
    captureId = 0;
    if (!durationUs || durationUs > BM1370Capture::MaxDurationUs) return false;
    pthread_mutex_lock(&mutex);
    settleLocked();
    bool okay = recorder.snapshot().state != BM1370Capture::State::Recording;
    if (okay && !storage) {
        storage = static_cast<BM1370Capture::Record *>(heap_caps_aligned_alloc(
            alignof(BM1370Capture::Record), BM1370Capture::MaxStorageBytes,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (storage) {
            for (std::size_t i = 0; i < BM1370Capture::MaxRecords; ++i)
                new (storage + i) BM1370Capture::Record{};
        }
    }
    if (!storage || nextId == UINT32_MAX) okay = false;
    if (okay) {
        const uint64_t id = ++nextId;
        okay = recorder.arm(storage, BM1370Capture::MaxRecords, id,
                            esp_timer_get_time(), durationUs);
        if (okay) captureId = id;
    }
    activeId.store(recorder.snapshot().state == BM1370Capture::State::Recording
        ? static_cast<uint32_t>(recorder.snapshot().captureId) : 0);
    pthread_mutex_unlock(&mutex);
    return okay;
}
bool freeze(uint64_t captureId) {
    pthread_mutex_lock(&mutex);
    settleLocked();
    const bool okay = recorder.freeze(captureId, esp_timer_get_time());
    activeId.store(recorder.snapshot().state == BM1370Capture::State::Recording
        ? static_cast<uint32_t>(recorder.snapshot().captureId) : 0);
    pthread_mutex_unlock(&mutex);
    return okay;
}
bool snapshot(Status &status) {
    pthread_mutex_lock(&mutex);
    settleLocked(); statusLocked(status);
    pthread_mutex_unlock(&mutex);
    return true;
}
bool copyPage(uint64_t captureId, std::size_t offset,
              BM1370Capture::Record *out, std::size_t limit,
              std::size_t &copied, Status &status) {
    copied = 0;
    if (!out || !limit || limit > PageRecords) return false;
    pthread_mutex_lock(&mutex);
    settleLocked(); statusLocked(status);
    const bool okay = status.capture.state == BM1370Capture::State::Frozen &&
        status.capture.captureId == captureId && offset <= status.capture.records;
    if (okay) copied = recorder.copyFrozen(captureId, offset, out, limit);
    pthread_mutex_unlock(&mutex);
    return okay;
}
void tx(const uint8_t *bytes, std::size_t length, int32_t returned,
        const BM1370Capture::JobMetadata *job, uint64_t generation) noexcept {
    Producer producer; if (!producer) return;
    const auto now = esp_timer_get_time();
    if (job) recorder.recordTxJob(now, generation, bytes, length, returned, *job);
    else recorder.recordTxBytes(now, 0, bytes, length, returned);
}
void rx(const uint8_t *bytes, std::size_t length, uint32_t requested) noexcept {
    Producer producer; if (!producer) return;
    // Raw RX generation deliberately remains unknown: read time does not
    // identify the generation in which the ASIC produced a queued response.
    recorder.recordRxChunk(esp_timer_get_time(), 0, bytes, length, requested);
}
void transport(BM1370Capture::Transport marker, int32_t returned, uint32_t auxiliary) noexcept {
    Producer producer; if (!producer) return;
    recorder.recordTransport(esp_timer_get_time(), 0, marker, returned, auxiliary);
}
void retirement(BM1370Capture::Retirement marker, uint32_t auxiliary, uint64_t generation) noexcept {
    Producer producer; if (!producer) return;
    recorder.recordRetirement(esp_timer_get_time(), generation, marker, auxiliary);
}
}
