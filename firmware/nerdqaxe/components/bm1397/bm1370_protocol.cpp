#include "bm1370_protocol.h"

#include <cstring>
#include <limits>

namespace BM1370Protocol {
namespace {

uint8_t crc5(const uint8_t *data, std::size_t length) noexcept {
    uint8_t reg = 0x1f;
    for (std::size_t i = 0; i < length; ++i) {
        for (int bit = 7; bit >= 0; --bit) {
            const uint8_t feedback = ((reg >> 4) ^ (data[i] >> bit)) & 1;
            reg = (reg << 1) & 0x1f;
            if (feedback) reg ^= 0x05;
        }
    }
    return reg;
}

uint16_t crc16False(const uint8_t *data, std::size_t length) noexcept {
    uint16_t reg = 0xffff;
    for (std::size_t i = 0; i < length; ++i) {
        reg ^= static_cast<uint16_t>(data[i]) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            reg = (reg & 0x8000) ? static_cast<uint16_t>((reg << 1) ^ 0x1021)
                                 : static_cast<uint16_t>(reg << 1);
        }
    }
    return reg;
}

void putLe32(uint8_t *out, uint32_t value) noexcept {
    for (unsigned i = 0; i < 4; ++i) out[i] = static_cast<uint8_t>(value >> (8 * i));
}
uint32_t getLe32(const uint8_t *data) noexcept {
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8)
           | (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}
uint32_t getBe32(const uint8_t *data) noexcept {
    return static_cast<uint32_t>(data[3]) | (static_cast<uint32_t>(data[2]) << 8)
           | (static_cast<uint32_t>(data[1]) << 16) | (static_cast<uint32_t>(data[0]) << 24);
}
void addCounter(uint64_t &counter, std::size_t amount = 1) noexcept {
    const uint64_t maximum = std::numeric_limits<uint64_t>::max();
    counter = amount > maximum - counter ? maximum : counter + amount;
}

} // namespace

uint8_t wireJobId(uint32_t logicalJobCounter) noexcept {
    return static_cast<uint8_t>((logicalJobCounter * 24U) & 0x7fU);
}

EncodeStatus encodeRawJob(const WorkTarget &target, const uint8_t *payload,
                          std::size_t length, TxPacket &out) noexcept {
    out = {};
    if (target.kind != TargetKind::Chain) return EncodeStatus::Unsupported;
    if (!payload || length != JobPayloadBytes) return EncodeStatus::InvalidArgument;
    out.bytes[0] = 0x55;
    out.bytes[1] = 0xaa;
    out.bytes[2] = 0x21;
    out.bytes[3] = 0x56;
    std::memcpy(out.bytes.data() + 4, payload, JobPayloadBytes);
    const uint16_t crc = crc16False(out.bytes.data() + 2, JobPayloadBytes + 2);
    out.bytes[86] = static_cast<uint8_t>(crc >> 8);
    out.bytes[87] = static_cast<uint8_t>(crc);
    out.wireJobId = payload[0];
    out.length = MaxTxBytes;
    return EncodeStatus::Ok;
}

EncodeStatus encodeJob(const WorkTarget &target, const Job &job, TxPacket &out) noexcept {
    out = {};
    if (target.kind != TargetKind::Chain) return EncodeStatus::Unsupported;
    std::array<uint8_t, JobPayloadBytes> payload{};
    payload[0] = wireJobId(job.logicalJobCounter);
    payload[1] = 1;
    putLe32(payload.data() + 2, job.startNonce);
    putLe32(payload.data() + 6, job.nBits);
    putLe32(payload.data() + 10, job.nTime);
    std::memcpy(payload.data() + 14, job.merkleRootWire.data(), 32);
    std::memcpy(payload.data() + 46, job.previousBlockHashWire.data(), 32);
    putLe32(payload.data() + 78, job.version);
    return encodeRawJob(target, payload.data(), payload.size(), out);
}

EncodeStatus encodeRegisterRead(uint8_t address, uint8_t reg, TxPacket &out) noexcept {
    out = {};
    out.bytes[0] = 0x55;
    out.bytes[1] = 0xaa;
    out.bytes[2] = 0x42;
    out.bytes[3] = 0x05;
    out.bytes[4] = address;
    out.bytes[5] = reg;
    out.bytes[6] = crc5(out.bytes.data() + 2, 4);
    out.length = RegisterReadBytes;
    return EncodeStatus::Ok;
}

DecodeStatus decodeResponse(const uint8_t *frame, std::size_t length, Response &out) noexcept {
    out = {};
    if (!frame || length != ResponseBytes) return DecodeStatus::InvalidLength;
    if (frame[0] != 0xaa || frame[1] != 0x55) return DecodeStatus::InvalidPreamble;
    if (crc5(frame + 2, 9) != 0) return DecodeStatus::BadCrc;
    const uint8_t type = frame[10] >> 5;
    if (type != 0 && type != 4) return DecodeStatus::UnsupportedType;
    std::memcpy(out.raw.data(), frame, ResponseBytes);
    out.type = static_cast<ResponseType>(type);
    if (out.type == ResponseType::Register) {
        out.registerValue = getBe32(frame + 2);
        out.registerAddress = frame[6];
        out.registerIndex = frame[7];
    } else {
        out.nonce = getLe32(frame + 2);
        out.extraDifficulty = frame[6];
        out.resultHeader = frame[7];
        out.jobSlotId = (frame[7] & 0xf0) >> 1;
        out.rolledVersionBits = ((static_cast<uint32_t>(frame[8]) << 8) | frame[9]) << 13;
    }
    return DecodeStatus::Ok;
}

void StreamParser::resynchronize() noexcept {
    std::size_t keepFrom = used_;
    for (std::size_t i = 1; i + 1 < used_; ++i) {
        if (bytes_[i] == 0xaa && bytes_[i + 1] == 0x55) { keepFrom = i; break; }
    }
    if (keepFrom == used_ && used_ && bytes_[used_ - 1] == 0xaa) keepFrom = used_ - 1;
    addCounter(stats_.discardedBytes, keepFrom);
    used_ -= keepFrom;
    if (used_) std::memmove(bytes_.data(), bytes_.data() + keepFrom, used_);
}

bool StreamParser::push(uint8_t byte, Response &out) noexcept {
    out = {};
    addCounter(stats_.bytesReceived);
    if (used_ == 0) {
        if (byte == 0xaa) bytes_[used_++] = byte;
        else addCounter(stats_.discardedBytes);
        return false;
    }
    if (used_ == 1 && byte != 0x55) {
        if (byte == 0xaa) { bytes_[0] = byte; addCounter(stats_.discardedBytes); }
        else { used_ = 0; addCounter(stats_.discardedBytes, 2); }
        return false;
    }
    bytes_[used_++] = byte;
    if (used_ != ResponseBytes) return false;
    const DecodeStatus status = decodeResponse(bytes_.data(), used_, out);
    if (status == DecodeStatus::Ok) {
        used_ = 0;
        addCounter(stats_.validFrames);
        return true;
    }
    if (status == DecodeStatus::UnsupportedType) {
        // A CRC-valid unsupported response still has a complete frame boundary.
        // Its payload must not become a nonce/register response of its own.
        addCounter(stats_.unsupportedFrames);
        addCounter(stats_.discardedBytes, used_);
        used_ = 0;
    } else {
        addCounter(stats_.crcFailures);
        resynchronize();
    }
    return false;
}

void StreamParser::reset(bool clearStats) noexcept {
    if (clearStats) stats_ = {};
    else addCounter(stats_.discardedBytes, used_);
    used_ = 0;
    bytes_ = {};
}

} // namespace BM1370Protocol
