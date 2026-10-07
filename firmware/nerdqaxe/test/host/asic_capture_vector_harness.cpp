#include "asic_capture_vector_support.h"

static std::vector<uint8_t> sent;
static std::deque<uint8_t> incoming;
void vTaskDelay(uint32_t) {}
int SERIAL_send(uint8_t *bytes, int length) {
    sent.assign(bytes, bytes + length);
    return length;
}
int16_t SERIAL_rx(uint8_t *bytes, uint16_t length, uint16_t) {
    int read = std::min(static_cast<int>(length), static_cast<int>(incoming.size()));
    for (int i = 0; i < read; ++i) { bytes[i] = incoming.front(); incoming.pop_front(); }
    return read;
}
void SERIAL_clear_buffer() { incoming.clear(); }

class Probe : public BM1370 {
public:
    void enumeration() {
        // Declared host fixture, never an observation of physical hardware.
        m_addressInterval = 64;
        m_detectedAsicCount = 4;
        m_chainAddressInterval = 64;
    }
    void address(uint8_t value) { setChipAddress(value); }
    void write(uint8_t addr) { send6(CMD_WRITE_SINGLE, addr, 0xA8, 0x00, 0x07, 0x01, 0xF0); }
};

static std::string hex(const uint8_t *bytes, size_t length) {
    static const char alphabet[] = "0123456789abcdef";
    std::string output;
    for (size_t i = 0; i < length; ++i) { output += alphabet[bytes[i] >> 4]; output += alphabet[bytes[i] & 15]; }
    return output;
}
static void header(const bm_job &job, uint8_t output[80]) {
    // Exact serialization order of production test_nonce_value; the ASIC job
    // and its byte transformations are constructed by the actual source body.
    memcpy(output, &job.version, 4);
    memcpy(output + 4, job.prev_block_hash, 32);
    memcpy(output + 36, job.merkle_root, 32);
    memcpy(output + 68, &job.ntime, 4);
    memcpy(output + 72, &job.target, 4);
    memcpy(output + 76, &job.starting_nonce, 4);
}

static void responseCrc(std::array<uint8_t, 11> &frame, uint8_t typeBits) {
    // Pinned captured BM1370 format: 3 type bits then 5 CRC bits. Use the
    // actual production CRC function to find the unique zero-residue trailer.
    assert((typeBits & 0x1f) == 0);
    int matches = 0;
    uint8_t selected = 0;
    for (uint8_t crc = 0; crc < 32; ++crc) {
        frame[10] = typeBits | crc;
        if (crc5(frame.data() + 2, 9) == 0) { ++matches; selected = frame[10]; }
    }
    assert(matches == 1);
    frame[10] = selected;
}

int main() {
    const uint32_t endianProbe = 1;
    assert(*reinterpret_cast<const uint8_t *>(&endianProbe) == 1);
    Probe asic;
    asic.enumeration();
    // Declared simulated four-chip enumeration, not an observed chain.
    asic.address(128);
    const std::string addressFrame = hex(sent.data(), sent.size());
    asic.write(192);
    const std::string writeFrame = hex(sent.data(), sent.size());
    bm_job jobs[2]{};
    std::string headers[2];
    for (int k = 0; k < 2; ++k) {
        mining_notify notify{};
        notify.version = 0x20000000;
        notify.target = 0x1d00ffff;
        notify.ntime = 0x65000001 + k;
        notify.difficulty = 1;
        uint8_t merkle[32];
        for (int i = 0; i < 32; ++i) {
            notify._prev_block_hash[i] = i + k * 128;
            merkle[i] = i + 64 + k * 128;
        }
        const std::string merkleHex = hex(merkle, 32);
        construct_bm_job(&notify, merkleHex.c_str(), 0x1fffe000, &jobs[k]);
        uint8_t serialized[80];
        header(jobs[k], serialized);
        headers[k] = hex(serialized, sizeof(serialized));
    }
    printf("{\"provenance\":\"simulated\",\"headerA\":\"%s\",\"headerB\":\"%s\",\"addressFrame\":\"%s\",\"writeFrame\":\"%s\",\"crc5Vectors\":[",
           headers[0].c_str(), headers[1].c_str(), addressFrame.c_str(), writeFrame.c_str());
    for (int length = 1; length <= 31; ++length) {
        uint8_t data[31];
        for (int i = 0; i < length; ++i) data[i] = (i * 37 + length * 19) & 255;
        printf("%s{\"bytes\":\"%s\",\"crc\":%u}", length == 1 ? "" : ",", hex(data, length).c_str(), crc5(data, length));
    }
    printf("],\"jobs\":[");
    for (int logical = 0; logical < 16; ++logical) {
        uint8_t wire = asic.sendWork(logical, &jobs[logical % 2]);
        printf("%s{\"logical\":%d,\"wire\":%u,\"work\":\"%s\",\"packetHex\":\"%s\"}",
               logical ? "," : "", logical, wire, logical % 2 ? "B" : "A", hex(sent.data(), sent.size()).c_str());
    }
    printf("],\"responses\":[");
    for (int i = 0; i < 64; ++i) {
        const uint8_t wire = ((i / 4) * 24) & 0x7f;
        const int chip = i % 4;
        uint32_t encoded = 0xA0001234 | (static_cast<uint32_t>(chip * 64) << 17);
        std::array<uint8_t, 11> frame{{0xaa, 0x55, static_cast<uint8_t>(encoded >> 24),
             static_cast<uint8_t>(encoded >> 16), static_cast<uint8_t>(encoded >> 8), static_cast<uint8_t>(encoded),
             0x01, static_cast<uint8_t>((wire << 1) | 0x0f), 0x02, 0x01, 0x00}};
        responseCrc(frame, 0x80);
        incoming.assign(frame.begin(), frame.end());
        task_result result{};
        assert(asic.processWork(&result, 10));
        printf("%s{\"packetHex\":\"%s\",\"nonce\":%u,\"wireJobId\":%u,\"rolledVersionBits\":%u,\"sourceAsicIndex\":%d,\"isRegister\":%u}",
               i ? "," : "", hex(frame.data(), frame.size()).c_str(), result.nonce, result.job_id,
               result.rolled_version, result.asic_nr, result.is_reg_resp);
    }
    std::array<uint8_t, 11> registerFrame{{0xaa, 0x55, 0x12, 0x34, 0x56, 0x78, 0x80, 0x90, 0x00, 0x00, 0x00}};
    responseCrc(registerFrame, 0x00);
    incoming.assign(registerFrame.begin(), registerFrame.end());
    task_result result{};
    assert(asic.processWork(&result, 10));
    printf("],\"registerResponse\":{\"packetHex\":\"%s\",\"data\":%u,\"register\":%u,\"sourceAsicIndex\":%d,\"isRegister\":%u}",
           hex(registerFrame.data(), registerFrame.size()).c_str(), result.data, result.reg, result.asic_nr, result.is_reg_resp);
    std::array<uint8_t, 11> zeroNonce{{0xaa, 0x55, 0, 0, 0, 0, 1, 0, 0, 0, 0}};
    responseCrc(zeroNonce, 0x80);
    printf(",\"zeroNonceResponse\":\"%s\",\"capturedRxEvidenceKind\":\"third-party-captured-fixtures-fed-through-stub-UART\",\"capturedRx\":[", hex(zeroNonce.data(), zeroNonce.size()).c_str());
    // Third-party captured bytes from pinned Mujina 980128574d8bb8b9619b0081b26e18eb4d0c5e31:
    // test_data.rs:208, response.rs:268/350/363/214. These are not our QAxe.
    const std::array<std::array<uint8_t, 11>, 5> captures{{
        {{0xaa,0x55,0x4c,0x03,0x52,0x75,0x0c,0xd2,0x05,0xa2,0x9c}},
        {{0xaa,0x55,0x18,0x00,0xa6,0x40,0x02,0x99,0x22,0xf9,0x91}},
        {{0xaa,0x55,0x07,0x35,0xcd,0xcf,0x02,0x5e,0x00,0x2e,0x96}},
        {{0xaa,0x55,0x46,0x03,0x32,0xe7,0x00,0xc3,0x2c,0x83,0x99}},
        {{0xaa,0x55,0x13,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x10}}
    }};
    for (size_t i = 0; i < captures.size(); ++i) {
        auto frame = captures[i];
        incoming.assign(frame.begin(), frame.end());
        task_result decoded{};
        assert(asic.processWork(&decoded, 10));
        printf("%s{\"packetHex\":\"%s\",\"crcResidue\":%u,\"isRegister\":%u,\"nonce\":%u,\"wireJobId\":%u,\"rolledVersionBits\":%u,\"data\":%u,\"register\":%u}",
               i ? "," : "", hex(frame.data(), frame.size()).c_str(), crc5(frame.data() + 2, 9), decoded.is_reg_resp,
               decoded.nonce, decoded.job_id, decoded.rolled_version, decoded.data, decoded.reg);
    }
    printf("]}\n");
}
