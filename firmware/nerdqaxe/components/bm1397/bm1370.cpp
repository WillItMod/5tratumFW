#include <endian.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "asic.h"
#include "bm1370.h"
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
#include "bm1370_capture_runtime.h"
#endif

#include "crc.h"
#include "serial.h"
#include "mining_utils.h"


static const char *TAG = "bm1370Module";

static const uint8_t chip_id[6] = {0xaa, 0x55, 0x13, 0x70, 0x00, 0x00};

static const uint64_t BM1370_CORE_COUNT = 128;
static const uint64_t BM1370_SMALL_CORE_COUNT = 2040;

#define REG_NONCE_TOTAL_CNT 0x8c

BM1370::BM1370() : Asic() {
    // NOP
}

const uint8_t* BM1370::getChipId() {
    return (uint8_t*) chip_id;
}

uint32_t BM1370::getDefaultVrFrequency() {
    return vrRegToFreq(0x1eb5);
};

uint8_t BM1370::init(uint64_t frequency, uint16_t asic_count, uint32_t difficulty, uint32_t vrFrequency)
{
    m_detectedAsicCount = 0;
    m_chainAddressInterval = 0;
#if defined(FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER) && FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER
    clearPendingResults();
#endif
    // reset is done externally to not have board dependencies

    // enable and set version rolling mask to 0xFFFF
    send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    // enable and set version rolling mask to 0xFFFF (again)
    send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    // enable and set version rolling mask to 0xFFFF (again)
    send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    // enable and set version rolling mask to 0xFFFF (again)
    send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    int chip_counter = count_asics();
    if (chip_counter > 0 && chip_counter <= 128) {
        m_detectedAsicCount = static_cast<uint16_t>(chip_counter);
        m_chainAddressInterval = 256 / next_power_of_two(chip_counter);
    }
    ESP_LOGIE(chip_counter == asic_count, TAG, "%i chip(s) detected on the chain, expected %i", chip_counter, asic_count);

    // enable and set version rolling mask to 0xFFFF (again)
    send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    // Reg_A8
    send6(CMD_WRITE_ALL, 0x00, 0xA8, 0x00, 0x07, 0x00, 0x00);

    // Misc Control
    //send6(CMD_WRITE_ALL, 0x00, 0x18, 0xFF, 0x0F, 0xC1, 0x00);
    send6(CMD_WRITE_ALL, 0x00, 0x18, 0xF0, 0x00, 0xC1, 0x00);

    // chain inactive
    sendChainInactive();

    // set chip address - distribute evenly across 0-255 range
    m_addressInterval = (chip_counter > 0) ? (256 / next_power_of_two(chip_counter)) : 4;
    for (uint8_t i = 0; i < chip_counter; i++) {
        setChipAddress(i * m_addressInterval);
    }

    // Core Register Control
    send6(CMD_WRITE_ALL, 0x00, 0x3C, 0x80, 0x00, 0x8B, 0x00);

    // Core Register Control
    //send6(CMD_WRITE_ALL, 0x00, 0x3C, 0x80, 0x00, 0x80, 0x18);
    send6(CMD_WRITE_ALL, 0x00, 0x3C, 0x80, 0x00, 0x80, 0x0C);

    setJobDifficultyMask(difficulty);

    // Set the IO Driver Strength on chip 00
    send6(CMD_WRITE_ALL, 0x00, 0x58, 0x00, 0x01, 0x11, 0x11);

    // ?
    send6(CMD_WRITE_ALL, 0x00, 0x68, 0x5A, 0xA5, 0x5A, 0xA5);

    // set baud
    //send6(CMD_WRITE_ALL, 0x00, 0x28, 0x01, 0x30, 0x00, 0x00);

    for (uint8_t i = 0; i < chip_counter; i++) {
        uint8_t addr = i * m_addressInterval;
        // Reg_A8
        send6(CMD_WRITE_SINGLE, addr, 0xA8, 0x00, 0x07, 0x01, 0xF0);
        // Misc Control
        send6(CMD_WRITE_SINGLE, addr, 0x18, 0xF0, 0x00, 0xC1, 0x00);
        // Core Register Control
        send6(CMD_WRITE_SINGLE, addr, 0x3C, 0x80, 0x00, 0x8B, 0x00);
        // Core Register Control
        send6(CMD_WRITE_SINGLE, addr, 0x3C, 0x80, 0x00, 0x80, 0x0C);
        // Core Register Control
        send6(CMD_WRITE_SINGLE, addr, 0x3C, 0x80, 0x00, 0x82, 0xAA);
    }

    // ?
    send6(CMD_WRITE_ALL, 0x00, 0xB9, 0x00, 0x00, 0x44, 0x80);

    // Analog Mux Control
    send6(CMD_WRITE_ALL, 0x00, 0x54, 0x00, 0x00, 0x00, 0x02);

    // ?
    send6(CMD_WRITE_ALL, 0x00, 0xB9, 0x00, 0x00, 0x44, 0x80);

    // Core Register Control
    send6(CMD_WRITE_ALL, 0x00, 0x3C, 0x80, 0x00, 0x8D, 0xEE);

    if (!doFrequencyTransition(frequency)) return 0;

    // set 0x10
    setVrFrequency(vrFrequency);

    send6(CMD_WRITE_ALL, 0x00, 0xA4, 0x90, 0x00, 0xFF, 0xFF);

    return chip_counter;
}

uint8_t BM1370::jobToAsicId(uint8_t job_id) {
    // job-IDs: 00, 18, 30, 48, 60, 78, 10, 28, 40, 58, 70, 08, 20, 38, 50, 68
    return (job_id * 24) & 0x7f;
}

uint8_t BM1370::asicToJobId(uint8_t asic_id) {
    return (asic_id & 0xf0) >> 1;
}

uint16_t BM1370::getSmallCoreCount() {
    return BM1370_SMALL_CORE_COUNT;
}

void BM1370::clearPendingResults()
{
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
    FiveTratumCapture::retirement(BM1370Capture::Retirement::ParserReset,
        m_pendingResponseSize - m_pendingResponsePosition);
#endif
    Asic::clearPendingResults();
#if defined(FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER) && FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER
    m_responseParser.reset();
    m_pendingResponsePosition = 0;
    m_pendingResponseSize = 0;
    memset(m_pendingResponseBytes, 0, sizeof(m_pendingResponseBytes));
#endif
}

void BM1370::resetAfterPowerCycle()
{
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
    FiveTratumCapture::retirement(BM1370Capture::Retirement::PowerReset);
#endif
    Asic::resetAfterPowerCycle();
    clearPendingResults();
    m_detectedAsicCount = 0;
    m_chainAddressInterval = 0;
}

BM1370::DispatchStatus BM1370::dispatchWork(const BM1370Protocol::WorkTarget &target,
                                         uint32_t logicalJobCounter, bm_job *job, uint8_t &wireJobId,
                                         uint64_t generation)
{
#if !defined(FIVETRATUM_BM1370_CAPTURE) || !FIVETRATUM_BM1370_CAPTURE
    (void)generation;
#endif
    wireJobId = 0;
    if (target.kind != BM1370Protocol::TargetKind::Chain) return DispatchStatus::Unsupported;
    if (!job) return DispatchStatus::InvalidArgument;
#if defined(FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER) && FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER
    BM1370Protocol::Job input;
    input.logicalJobCounter = logicalJobCounter;
    input.startNonce = job->starting_nonce;
    input.nBits = job->target;
    input.nTime = job->ntime;
    input.version = job->version;
    memcpy(input.merkleRootWire.data(), job->merkle_root_be, input.merkleRootWire.size());
    memcpy(input.previousBlockHashWire.data(), job->prev_block_hash_be, input.previousBlockHashWire.size());
    BM1370Protocol::TxPacket packet;
    if (BM1370Protocol::encodeJob(target, input, packet) != BM1370Protocol::EncodeStatus::Ok)
        return DispatchStatus::InvalidArgument;
    wireJobId = packet.wireJobId;
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
    BM1370Capture::JobMetadata metadata;
    auto le32 = [](uint8_t *out, uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) out[i] = static_cast<uint8_t>(value >> (8 * i));
    };
    le32(metadata.header.data(), job->version);
    memcpy(metadata.header.data() + 4, job->prev_block_hash, 32);
    memcpy(metadata.header.data() + 36, job->merkle_root, 32);
    le32(metadata.header.data() + 68, job->ntime);
    le32(metadata.header.data() + 72, job->target);
    le32(metadata.header.data() + 76, job->starting_nonce);
    metadata.logicalJobCounter = logicalJobCounter;
    metadata.versionMask = job->version_mask;
    metadata.asicTicketDifficulty = m_asicDifficulty + 1; // Actual shared mask threshold.
    metadata.poolIndex = job->pool_id == 0 || job->pool_id == 1
        ? static_cast<uint8_t>(job->pool_id) : BM1370Capture::UnknownPool;
    const int written = SERIAL_send_job(packet.bytes.data(), packet.length, metadata, generation);
#else
    const int written = SERIAL_send(packet.bytes.data(), packet.length);
#endif
    if (written != static_cast<int>(packet.length))
        m_transportFailed = true;
#else
    wireJobId = Asic::sendWork(logicalJobCounter, job);
#endif
    return transportOkay() ? DispatchStatus::Sent : DispatchStatus::TransportFailed;
}

uint8_t BM1370::sendWork(uint32_t job_id, bm_job *job)
{
#if defined(FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER) && FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER
    uint8_t wireJobId = 0;
    if (dispatchWork({}, job_id, job, wireJobId) == DispatchStatus::InvalidArgument)
        m_transportFailed = true;
    return wireJobId;
#else
    return Asic::sendWork(job_id, job);
#endif
}

void BM1370::sendRawJob(BM1368_job *job)
{
#if defined(FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER) && FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER
    static_assert(sizeof(BM1368_job) == BM1370Protocol::JobPayloadBytes, "Existing BM1370 raw job payload changed");
    BM1370Protocol::TxPacket packet;
    if (BM1370Protocol::encodeRawJob({}, reinterpret_cast<const uint8_t *>(job), sizeof(BM1368_job), packet)
            != BM1370Protocol::EncodeStatus::Ok) {
        m_transportFailed = true;
        return;
    }
    if (SERIAL_send(packet.bytes.data(), packet.length) != static_cast<int>(packet.length))
        m_transportFailed = true;
#else
    Asic::sendRawJob(job);
#endif
}

#if defined(FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER) && FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER
bool BM1370::convertResponse(const BM1370Protocol::Response &response, task_result *result)
{
    if (!m_detectedAsicCount || !m_chainAddressInterval || !result) return false;
    task_result decoded{};
    if (response.type == BM1370Protocol::ResponseType::Register) {
        // An explicit addressed register response must name an enumerated chip.
        if (response.registerAddress % m_chainAddressInterval) return false;
        decoded.asic_nr = response.registerAddress / m_chainAddressInterval;
        if (decoded.asic_nr >= m_detectedAsicCount) return false;
        decoded.data = response.registerValue;
        decoded.reg = response.registerIndex;
        decoded.is_reg_resp = 1;
    } else {
        // Preserve the upstream nonce-space attribution heuristic. A CRC does
        // not establish physical identity or independent chip work ownership.
        const uint8_t source = static_cast<uint8_t>((__bswap32(response.nonce) >> 17) & 0xff);
        decoded.asic_nr = source / m_chainAddressInterval;
        if (decoded.asic_nr >= m_detectedAsicCount) return false;
        decoded.job_id = response.jobSlotId;
        decoded.nonce = response.nonce;
        decoded.rolled_version = response.rolledVersionBits;
        decoded.is_reg_resp = 0;
    }
    *result = decoded;
    return true;
}
#endif

bool BM1370::processWork(task_result *result, uint16_t timeoutMs)
{
#if defined(FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER) && FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER
    if (!result) return false;
    BM1370Protocol::Response response;
    while (m_pendingResponsePosition < m_pendingResponseSize) {
        const uint8_t byte = m_pendingResponseBytes[m_pendingResponsePosition++];
        if (m_responseParser.push(byte, response) && convertResponse(response, result)) return true;
    }
    m_pendingResponsePosition = 0;
    m_pendingResponseSize = 0;
    uint8_t bytes[BM1370Protocol::ResponseBytes];
    // Exactly one UART read per call: timeoutMs bounds the entire lease, never
    // each byte. Retain all already-read bytes after a returned response.
    const int received = SERIAL_rx(bytes, sizeof(bytes), timeoutMs);
    if (received <= 0) return false;
    if (received > static_cast<int>(sizeof(bytes))) {
        clearPendingResults();
        return false;
    }
    for (int i = 0; i < received; ++i) {
        if (m_responseParser.push(bytes[i], response) && convertResponse(response, result)) {
            m_pendingResponseSize = static_cast<uint8_t>(received - i - 1);
            if (m_pendingResponseSize)
                memcpy(m_pendingResponseBytes, bytes + i + 1, m_pendingResponseSize);
            return true;
        }
    }
    return false;
#else
    return Asic::processWork(result, timeoutMs);
#endif
}
