#pragma once
#include "bm1370_capture.h"

// Platform boundary for the opt-in, RAM-only software observation recorder.
// Producer calls never allocate, log or poll the UART. The capture lock protects
// only bounded memory copies; it is never held across UART or network operations.
namespace FiveTratumCapture {
constexpr std::size_t PageRecords = 8;
struct Status {
    BM1370Capture::Snapshot capture{};
    uint32_t droppedBusy = 0;
    uint32_t storageBytes = 0;
};
bool arm(uint32_t durationUs, uint64_t &captureId);
bool freeze(uint64_t captureId);
bool snapshot(Status &status);
bool copyPage(uint64_t captureId, std::size_t offset,
              BM1370Capture::Record *out, std::size_t limit,
              std::size_t &copied, Status &status);
void tx(const uint8_t *bytes, std::size_t length, int32_t returned,
        const BM1370Capture::JobMetadata *job = nullptr, uint64_t generation = 0) noexcept;
void rx(const uint8_t *bytes, std::size_t length, uint32_t requested) noexcept;
void transport(BM1370Capture::Transport marker, int32_t returned = 0,
               uint32_t auxiliary = 0) noexcept;
void retirement(BM1370Capture::Retirement marker, uint32_t auxiliary = 0,
                uint64_t generation = 0) noexcept;
}
