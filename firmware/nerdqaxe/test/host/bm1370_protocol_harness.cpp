#include "bm1370_protocol.h"
#include "stock_source.h"

#include <cassert>
#include <cstdio>
#include <limits>
#include <new>
#include <type_traits>

using namespace BM1370Protocol;

// Fail if the actual codec or harness allocates. The test uses fixed storage.
void *operator new(std::size_t) { std::abort(); }
void *operator new[](std::size_t) { std::abort(); }
void operator delete(void *) noexcept { std::abort(); }
void operator delete[](void *) noexcept { std::abort(); }

static_assert(std::is_trivially_copyable<StreamParser>::value, "fixed parser storage");
static_assert(sizeof(StreamParser) <= 64, "bounded parser storage");
static_assert(sizeof(TxPacket) <= 104, "bounded packet storage");

static const std::array<std::array<uint8_t, 11>, 5> captures{{
    {{0xaa,0x55,0x4c,0x03,0x52,0x75,0x0c,0xd2,0x05,0xa2,0x9c}},
    {{0xaa,0x55,0x18,0x00,0xa6,0x40,0x02,0x99,0x22,0xf9,0x91}},
    {{0xaa,0x55,0x07,0x35,0xcd,0xcf,0x02,0x5e,0x00,0x2e,0x96}},
    {{0xaa,0x55,0x46,0x03,0x32,0xe7,0x00,0xc3,0x2c,0x83,0x99}},
    {{0xaa,0x55,0x13,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x10}}
}};

static void validCrc(std::array<uint8_t, 11> &frame, uint8_t type) {
    unsigned matches = 0;
    uint8_t trailer = 0;
    for (unsigned crc = 0; crc < 32; ++crc) {
        frame[10] = (type << 5) | crc;
        if (crc5(frame.data() + 2, 9) == 0) { ++matches; trailer = frame[10]; }
    }
    assert(matches == 1);
    frame[10] = trailer;
}

static bool outputClear(const Response &response) {
    for (uint8_t byte : response.raw) if (byte) return false;
    return response.nonce == 0 && response.rolledVersionBits == 0 && response.jobSlotId == 0
        && response.extraDifficulty == 0 && response.resultHeader == 0
        && response.registerValue == 0 && response.registerAddress == 0 && response.registerIndex == 0;
}
static bool outputClear(const TxPacket &packet) {
    for (uint8_t byte : packet.bytes) if (byte) return false;
    return packet.length == 0 && packet.wireJobId == 0;
}

static void capturedFrames() {
    const uint32_t nonces[] = {0x7552034c, 0x40a60018, 0xcfcd3507, 0xe7320346};
    const uint8_t slots[] = {0x68, 0x48, 0x28, 0x60};
    const uint32_t versions[] = {0x00b44000, 0x045f2000, 0x0005c000, 0x05906000};
    for (std::size_t i = 0; i < captures.size(); ++i) {
        Response response;
        assert(crc5(const_cast<uint8_t *>(captures[i].data()) + 2, 9) == 0);
        assert(decodeResponse(captures[i].data(), captures[i].size(), response) == DecodeStatus::Ok);
        assert(response.raw == captures[i]);
        if (i < 4) {
            assert(response.type == ResponseType::Nonce);
            assert(response.nonce == nonces[i]);
            assert(response.jobSlotId == slots[i]);
            assert(response.rolledVersionBits == versions[i]);
            assert(response.resultHeader == captures[i][7]);
            assert(response.extraDifficulty == captures[i][6]);
            assert(response.registerValue == 0);
        } else {
            assert(response.type == ResponseType::Register);
            assert(response.registerValue == 0x13700000);
            assert(response.registerAddress == 0 && response.registerIndex == 0);
            assert(response.nonce == 0 && response.jobSlotId == 0);
        }
    }
}

static void bitMutations() {
    for (const auto &original : captures) {
        for (std::size_t byte = 2; byte < original.size(); ++byte) {
            for (unsigned bit = 0; bit < 8; ++bit) {
                auto corrupted = original;
                corrupted[byte] ^= 1U << bit;
                Response response;
                assert(crc5(corrupted.data() + 2, 9) != 0);
                assert(decodeResponse(corrupted.data(), corrupted.size(), response) == DecodeStatus::BadCrc);
                assert(outputClear(response));
                StreamParser parser;
                for (auto value : corrupted) assert(!parser.push(value, response));
                unsigned returned = 0;
                for (auto value : original) returned += parser.push(value, response);
                assert(returned == 1 && response.raw == original);
                assert(parser.stats().crcFailures >= 1 && parser.stats().validFrames == 1);
            }
        }
    }
}

static void fragmentsAndNoise() {
    Response response;
    for (const auto &frame : captures) {
        for (unsigned split = 0; split <= 11; ++split) {
            StreamParser parser;
            unsigned returned = 0;
            for (unsigned i = 0; i < split; ++i) returned += parser.push(frame[i], response);
            if (split < 11) { assert(returned == 0); assert(parser.bufferedBytes() == split); }
            // A timeout/no-byte poll leaves the parser intact.
            for (unsigned i = split; i < 11; ++i) returned += parser.push(frame[i], response);
            assert(returned == 1 && response.raw == frame);
            assert(parser.bufferedBytes() == 0);
        }
    }
    // Every partition of an eleven-byte frame, including byte-only reads.
    for (unsigned partitions = 0; partitions < (1U << 10); ++partitions) {
        StreamParser parser;
        unsigned returned = 0;
        for (unsigned i = 0; i < 11; ++i) {
            returned += parser.push(captures[0][i], response);
            if (i < 10 && (partitions & (1U << i))) assert(returned == 0);
        }
        assert(returned == 1 && response.raw == captures[0]);
    }
    StreamParser parser;
    const uint8_t noise[] = {0, 0x55, 0xaa, 0xaa, 0x55, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xaa, 0xaa};
    for (auto byte : noise) assert(!parser.push(byte, response));
    unsigned returned = 0;
    for (unsigned repeat = 0; repeat < 8; ++repeat) {
        for (const auto &frame : captures) for (auto byte : frame) returned += parser.push(byte, response);
    }
    assert(returned == 40 && parser.stats().validFrames == 40);
    assert(parser.stats().bytesReceived == sizeof(noise) + 440);
    assert(parser.stats().discardedBytes == sizeof(noise));
    assert(parser.stats().crcFailures > 0);
    assert(parser.bufferedBytes() == 0);
}

static void embeddedPreambles() {
    Response response;
    // A genuine CRC-valid nonce may contain AA55 in its payload. It is one frame.
    auto embedded = captures[0];
    embedded[2] = 0xaa; embedded[3] = 0x55;
    embedded[8] = 0xaa; embedded[9] = 0x55;
    validCrc(embedded, 4);
    StreamParser parser;
    unsigned returned = 0;
    for (auto byte : embedded) returned += parser.push(byte, response);
    assert(returned == 1 && response.raw == embedded && parser.stats().crcFailures == 0);

    // A failed candidate overlaps a complete valid frame at every offset.
    for (unsigned prefix = 1; prefix <= 10; ++prefix) {
        parser.reset(true);
        for (unsigned i = 0; i < prefix; ++i) {
            const uint8_t byte = i == 0 ? 0xaa : (i == 1 ? 0x55 : 0);
            assert(!parser.push(byte, response));
        }
        returned = 0;
        for (auto byte : captures[0]) returned += parser.push(byte, response);
        assert(returned == 1 && response.raw == captures[0]);
        assert(parser.stats().discardedBytes == prefix);
    }
    // Bad frame ending in AA must retain that prefix for the following 55.
    auto trailing = captures[0];
    trailing[10] = 0xaa;
    assert(crc5(trailing.data() + 2, 9) != 0);
    parser.reset(true);
    for (auto byte : trailing) assert(!parser.push(byte, response));
    assert(parser.bufferedBytes() == 1);
    returned = 0;
    for (unsigned i = 1; i < 11; ++i) returned += parser.push(captures[0][i], response);
    assert(returned == 1 && response.raw == captures[0]);
}

static void malformedAndUnsupported() {
    Response response;
    for (std::size_t length = 0; length < 14; ++length) {
        if (length == 11) continue;
        assert(decodeResponse(captures[0].data(), length, response) == DecodeStatus::InvalidLength);
        assert(outputClear(response));
    }
    assert(decodeResponse(nullptr, 11, response) == DecodeStatus::InvalidLength);
    auto badPreamble = captures[0]; badPreamble[0] = 0;
    assert(decodeResponse(badPreamble.data(), 11, response) == DecodeStatus::InvalidPreamble);
    for (uint8_t type = 0; type < 8; ++type) {
        if (type == 0 || type == 4) continue;
        auto frame = captures[0];
        frame[6] = 0xaa; frame[7] = 0x55;
        validCrc(frame, type);
        assert(decodeResponse(frame.data(), 11, response) == DecodeStatus::UnsupportedType);
        assert(outputClear(response));
        StreamParser parser;
        for (auto byte : frame) assert(!parser.push(byte, response));
        assert(parser.bufferedBytes() == 0 && parser.stats().unsupportedFrames == 1);
        unsigned returned = 0;
        for (auto byte : captures[0]) returned += parser.push(byte, response);
        assert(returned == 1 && response.raw == captures[0]);
    }
    StreamParser parser;
    for (unsigned i = 0; i < 6; ++i) assert(!parser.push(captures[0][i], response));
    parser.reset();
    assert(parser.bufferedBytes() == 0 && parser.stats().discardedBytes == 6);
    for (unsigned i = 6; i < 11; ++i) assert(!parser.push(captures[0][i], response));
    unsigned returned = 0;
    for (auto byte : captures[0]) returned += parser.push(byte, response);
    assert(returned == 1);
    parser.reset(true);
    assert(parser.stats().bytesReceived == 0 && parser.stats().validFrames == 0);
}

static uint32_t randomState = 0x42fe951c;
static uint32_t randomWord() {
    randomState ^= randomState << 13; randomState ^= randomState >> 17; randomState ^= randomState << 5;
    return randomState;
}
static void jobParityAndRegisterRead() {
    StockAsic stock;
    Job job;
    TxPacket packet;
    const WorkTarget chain{};
    const uint8_t expectedIds[] = {0x00,0x18,0x30,0x48,0x60,0x78,0x10,0x28,0x40,0x58,0x70,0x08,0x20,0x38,0x50,0x68};
    for (unsigned count = 0; count < 1024; ++count) {
        job.logicalJobCounter = count < 16 ? count : randomWord();
        job.startNonce = randomWord(); job.nBits = randomWord(); job.nTime = randomWord(); job.version = randomWord();
        bm_job source{};
        source.starting_nonce = job.startNonce; source.target = job.nBits;
        source.ntime = job.nTime; source.version = job.version;
        for (unsigned i = 0; i < 32; ++i) {
            source.merkle_root_be[i] = job.merkleRootWire[i] = randomWord();
            source.prev_block_hash_be[i] = job.previousBlockHashWire[i] = randomWord();
        }
        const uint8_t sourceId = stock.sendWork(job.logicalJobCounter, &source);
        assert(encodeJob(chain, job, packet) == EncodeStatus::Ok);
        assert(packet.length == 88 && stockLength == 88);
        assert(std::memcmp(packet.bytes.data(), stockBytes.data(), 88) == 0);
        assert(packet.wireJobId == sourceId);
        if (count < 16) assert(sourceId == expectedIds[count]);
        const auto reference = packet;
        assert(encodeRawJob(chain, stockBytes.data() + 4, 82, packet) == EncodeStatus::Ok);
        assert(packet.bytes == reference.bytes);
        assert(packet.bytes[0] == 0x55 && packet.bytes[1] == 0xaa && packet.bytes[2] == 0x21 && packet.bytes[3] == 0x56);
        const uint16_t crc = crc16_false(packet.bytes.data() + 2, 84);
        assert(packet.bytes[86] == (crc >> 8) && packet.bytes[87] == (crc & 255));
    }
    assert(wireJobId(std::numeric_limits<uint32_t>::max()) == 0x68);
    for (unsigned address = 0; address < 256; ++address) {
        for (unsigned reg = 0; reg < 256; ++reg) {
            stock.read(address, reg);
            assert(encodeRegisterRead(address, reg, packet) == EncodeStatus::Ok);
            assert(packet.length == 7 && stockLength == 7);
            assert(std::memcmp(packet.bytes.data(), stockBytes.data(), 7) == 0);
        }
    }
}

static void unsupportedNeverEmits() {
    Job job;
    TxPacket packet;
    std::array<uint8_t, 82> payload{};
    for (unsigned address = 0; address < 256; ++address) {
        packet.bytes.fill(0xff); packet.length = 88;
        const WorkTarget chip{TargetKind::Chip, static_cast<uint8_t>(address)};
        assert(encodeJob(chip, job, packet) == EncodeStatus::Unsupported && outputClear(packet));
        assert(encodeRawJob(chip, payload.data(), 82, packet) == EncodeStatus::Unsupported && outputClear(packet));
        // Unsupported dominates even malformed chip payloads: still no bytes.
        assert(encodeRawJob(chip, nullptr, 0, packet) == EncodeStatus::Unsupported && outputClear(packet));
    }
    const WorkTarget invalid{static_cast<TargetKind>(0xff), 0};
    assert(encodeJob(invalid, job, packet) == EncodeStatus::Unsupported && outputClear(packet));
    assert(encodeRawJob({}, nullptr, 82, packet) == EncodeStatus::InvalidArgument && outputClear(packet));
    for (unsigned length = 0; length < 90; ++length) {
        if (length == 82) continue;
        assert(encodeRawJob({}, payload.data(), length, packet) == EncodeStatus::InvalidArgument && outputClear(packet));
    }
}

static void streamStress() {
    StreamParser parser;
    Response response;
    uint64_t returned = 0;
    for (unsigned i = 0; i < 1000000; ++i) {
        if (parser.push(randomWord(), response)) {
            ++returned;
            Response validation;
            assert(decodeResponse(response.raw.data(), 11, validation) == DecodeStatus::Ok);
        }
        assert(parser.bufferedBytes() < 11);
    }
    const uint64_t randomReturned = returned;
    for (const auto &frame : captures) for (auto byte : frame) returned += parser.push(byte, response);
    assert(returned == randomReturned + 5);
    assert(parser.stats().bytesReceived == 1000055 && parser.stats().validFrames == returned);
    assert(parser.stats().discardedBytes + parser.stats().validFrames * 11 + parser.bufferedBytes() == parser.stats().bytesReceived);
}

int main() {
    const uint32_t endian = 1;
    assert(*reinterpret_cast<const uint8_t *>(&endian) == 1); // Stock sender's supported ESP host order.
    capturedFrames(); bitMutations(); fragmentsAndNoise(); embeddedPreambles();
    malformedAndUnsupported(); jobParityAndRegisterRead(); unsupportedNeverEmits(); streamStress();
    std::printf("{\"groupsPassed\":8,\"publishedCapturedFrames\":5,\"rejectedSingleBitMutations\":360,\"stockJobComparisons\":1024,\"stockRegisterReads\":65536,\"streamPartitions\":1024,\"randomStreamBytes\":1000000,\"heapAllocationAllowed\":false,\"ownQAxeHardwareTested\":false,\"independentWorkAssignment\":false}\n");
}
