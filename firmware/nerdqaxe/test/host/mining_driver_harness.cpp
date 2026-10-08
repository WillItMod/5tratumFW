#include "mining_driver_support.h"
#include "asic_jobs_actual.h"
#include "mining_control_state.h"
#include "mining_control.h"
#include <iostream>
#include <limits>
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
#include "bm1370_capture_runtime.h"
#endif

uint64_t hostClockMs = 0;
std::array<int, 32> gpioLevels{};
bool serialShortWrite = false;
bool hostCanEnabled = false;
static bool failNextWrite = false;
static bool failFinalBaud = false;
unsigned simulatedChips = 4;
int hostBaud = 115200;
std::deque<std::vector<uint8_t>> serialFrames;
HostSystem SYSTEM_MODULE;
PowerManagementTask POWER_MANAGEMENT_MODULE;
HostHashrate HASHRATE_MONITOR;
AsicJobs asicJobs;
static bool policyPause = false;
static uint64_t policyPauseAtMs = std::numeric_limits<uint64_t>::max();
static uint64_t thermalFaultAtMs = std::numeric_limits<uint64_t>::max();
static float sensedVr = 55;
static bool timerCreateOkay = true, timerStartOkay = true;
static unsigned timersDeleted = 0;
TimerHandle_t xTimerCreate(const char *, unsigned, bool, void *, void (*)(TimerHandle_t)) { return timerCreateOkay ? reinterpret_cast<void *>(1) : nullptr; }
int xTimerStart(TimerHandle_t, unsigned) { return timerStartOkay; }
int xTimerDelete(TimerHandle_t, unsigned) { ++timersDeleted; return 1; }
void create_job_timer(TimerHandle_t) {}
namespace FiveTratumMining {
bool requestedPaused() { return policyPause; }
void updateSchedule() {}
bool hasScheduleError() { return false; }
}
void trigger_job_creation() {}
int64_t esp_timer_get_time() { return static_cast<int64_t>(hostClockMs * 1000); }
void PowerManagementTask::pollProtection() {
    if (m_lastProtectionAtUs >= 0)
        maximumPollGapMs = std::max(maximumPollGapMs, hostClockMs - static_cast<uint64_t>(m_lastProtectionAtUs / 1000));
    ++polls;
    m_chipTempMax = 40;
    if (m_board->isBuckInitialized()) { m_vrTemp = sensedVr; if (!firstBuckPoll) firstBuckPoll = polls; }
    m_lastProtectionBuckReady = m_board->isBuckInitialized();
    m_lastProtectionAtUs = esp_timer_get_time();
}
static uint64_t lastProtectionMs = 0, longestProtectionGap = 0;
static uint64_t abortAtMs = std::numeric_limits<uint64_t>::max();
static unsigned protectionPolls = 0, transmittedPackets = 0, rxCalls = 0;
static std::vector<std::vector<uint8_t>> transmittedBytes;
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
struct CapturedJobCall {
    std::vector<uint8_t> packet;
    BM1370Capture::JobMetadata metadata;
    uint64_t generation;
    int returned;
};
struct CapturedRetirement {
    BM1370Capture::Retirement marker;
    uint32_t auxiliary;
    uint64_t generation;
};
static std::vector<CapturedJobCall> capturedJobs;
static std::vector<CapturedRetirement> capturedRetirements;
namespace FiveTratumCapture {
void retirement(BM1370Capture::Retirement marker, uint32_t auxiliary, uint64_t generation) noexcept {
    capturedRetirements.push_back({marker, auxiliary, generation});
}
}
#endif

void vTaskDelay(uint32_t milliseconds) {
    hostClockMs += milliseconds;
    if (hostClockMs >= policyPauseAtMs) policyPause = true;
    if (hostClockMs >= thermalFaultAtMs) SYSTEM_MODULE.error = Board::Error::TEMP_FAULT;
}
void gpio_set_level(int pin, int value) { gpioLevels.at(pin) = value; }
int gpio_get_level(int pin) { return gpioLevels.at(pin); }
void SERIAL_set_baud(int baud) { hostBaud = baud; }
bool SERIAL_set_baud_checked(int baud) {
    if (failFinalBaud && baud != 115200) return false;
    SERIAL_set_baud(baud); return true;
}
void SERIAL_clear_buffer() { serialFrames.clear(); }
bool SERIAL_clear_buffer_checked() { SERIAL_clear_buffer(); return true; }
bool SERIAL_wait_tx_idle(uint16_t) { return !serialShortWrite; }
int SERIAL_send(uint8_t *bytes, int length) {
    ++transmittedPackets;
    transmittedBytes.emplace_back(bytes, bytes + length);
    if (failNextWrite) { serialShortWrite = true; failNextWrite = false; }
    if (serialShortWrite) return length - 1;
    if (length >= 6 && bytes[2] == CMD_READ_ALL && bytes[4] == 0 && bytes[5] == 0) {
        for (unsigned i = 0; i < simulatedChips; ++i)
            serialFrames.push_back({0xaa, 0x55, 0x13, 0x70, 0, 0, 0, 0, 0, 0, 0x0f});
    }
    return length;
}
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
int SERIAL_send_job(uint8_t *bytes, int length, const BM1370Capture::JobMetadata &metadata, uint64_t generation) {
    // Replace only the external serial/capture boundary. Copy all arguments
    // while their caller-owned storage is valid and forward the exact UART mock.
    CapturedJobCall call{{bytes, bytes + length}, metadata, generation, 0};
    call.returned = SERIAL_send(bytes, length);
    capturedJobs.push_back(call);
    return call.returned;
}
#endif
int16_t SERIAL_rx(uint8_t *out, uint16_t capacity, uint16_t timeoutMs) {
    ++rxCalls;
    if (serialFrames.empty()) { hostClockMs += timeoutMs; return 0; }
    auto &frame = serialFrames.front();
    const size_t count = std::min<size_t>(frame.size(), capacity);
    std::memcpy(out, frame.data(), count); frame.erase(frame.begin(), frame.begin() + count);
    if (frame.empty()) serialFrames.pop_front();
    return count;
}
void free_bm_job(bm_job *job) { if (job) { free(job->jobid); free(job->extranonce2); free(job); } }

static bool protection(void *) {
    if (hostClockMs - lastProtectionMs >= 800) {
        longestProtectionGap = std::max(longestProtectionGap, hostClockMs - lastProtectionMs);
        lastProtectionMs = hostClockMs; ++protectionPolls;
    }
    return hostClockMs < abortAtMs;
}
static void resetFixture() {
    hostClockMs = lastProtectionMs = longestProtectionGap = 0;
    protectionPolls = transmittedPackets = rxCalls = 0; hostBaud = 115200;
    transmittedBytes.clear();
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
    capturedJobs.clear(); capturedRetirements.clear();
#endif
    serialShortWrite = false; simulatedChips = 4; serialFrames.clear(); gpioLevels.fill(0);
    abortAtMs = std::numeric_limits<uint64_t>::max();
}
static bm_job *job(const char *id) {
    auto *value = static_cast<bm_job *>(std::calloc(1, sizeof(bm_job)));
    value->jobid = strdup(id); value->extranonce2 = strdup("00000001"); return value;
}

// Test-only declaration; production exposes no setter for invented topology.
class DiagnosticProbe : public BM1370 {
public:
    void declaredTopology(unsigned count) {
        m_detectedAsicCount = count;
        m_chainAddressInterval = count ? 256 / next_power_of_two(count) : 0;
    }
};
[[maybe_unused]] static std::vector<uint8_t> crcFrame(std::vector<uint8_t> frame) {
    assert(frame.size() == 11);
    const uint8_t type = frame[10] & 0xe0;
    for (uint8_t checksum = 0; checksum < 32; ++checksum) {
        frame[10] = type | checksum;
        if (crc5(frame.data() + 2, 9) == 0) return frame;
    }
    std::abort();
}
static void checkJobCodecAndUnsupported() {
    resetFixture(); DiagnosticProbe driver;
    bm_job value{};
    value.version = 0x20000000; value.ntime = 0x65abcdef; value.target = 0x1d00ffff;
    value.starting_nonce = 0x12345678;
    for (unsigned i = 0; i < 32; ++i) { value.merkle_root_be[i] = i * 3; value.prev_block_hash_be[i] = 255 - i; }
    Asic *base = &driver;
    for (uint32_t counter = 0; counter < 2048; ++counter) {
        const auto expectedId = driver.Asic::sendWork(counter, &value);
        const auto expected = transmittedBytes.back();
        const auto actualId = base->sendWork(counter, &value);
        assert(actualId == expectedId && transmittedBytes.back() == expected && expected.size() == 88);
    }
    BM1368_job raw{};
    auto *rawBytes = reinterpret_cast<uint8_t *>(&raw);
    for (unsigned i = 0; i < sizeof(raw); ++i) rawBytes[i] = i * 17;
    driver.Asic::sendRawJob(&raw); const auto expected = transmittedBytes.back();
    base->sendRawJob(&raw); assert(transmittedBytes.back() == expected);
    const auto before = transmittedPackets; uint8_t id = 0xff;
    for (unsigned address : {0, 64, 128, 192, 255}) {
        assert(driver.dispatchWork({BM1370Protocol::TargetKind::Chip, static_cast<uint8_t>(address)}, 5, &value, id)
            == BM1370::DispatchStatus::Unsupported && id == 0 && transmittedPackets == before);
    }
    assert(driver.dispatchWork({}, 5, nullptr, id) == BM1370::DispatchStatus::InvalidArgument && transmittedPackets == before);
    serialShortWrite = true;
    assert(driver.dispatchWork({}, 5, &value, id) == BM1370::DispatchStatus::TransportFailed && !driver.transportOkay());
    base->resetAfterPowerCycle(); assert(driver.transportOkay());
    std::cout << "Actual virtual chain/raw sender equals legacy bytes for 2048 jobs; chip targets send zero bytes; short TX latched\n";
}
static void checkDiagnosticReceiver() {
#if defined(FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER) && FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER
    resetFixture(); DiagnosticProbe driver; driver.declaredTopology(4); Asic *base = &driver;
    const std::vector<std::vector<uint8_t>> captured{
        {0xaa,0x55,0x4c,0x03,0x52,0x75,0x0c,0xd2,0x05,0xa2,0x9c},
        {0xaa,0x55,0x18,0x00,0xa6,0x40,0x02,0x99,0x22,0xf9,0x91},
        {0xaa,0x55,0x07,0x35,0xcd,0xcf,0x02,0x5e,0x00,0x2e,0x96},
        {0xaa,0x55,0x46,0x03,0x32,0xe7,0x00,0xc3,0x2c,0x83,0x99},
        {0xaa,0x55,0x13,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x10}
    };
    task_result result{};
    for (const auto &frame : captured) {
        for (unsigned split = 1; split < frame.size(); ++split) {
            base->clearPendingResults(); SERIAL_clear_buffer(); result.nonce = 0xdeadbeef;
            serialFrames.push_back({frame.begin(), frame.begin() + split});
            assert(!base->processWork(&result, 250) && result.nonce == 0xdeadbeef);
            serialFrames.push_back({frame.begin() + split, frame.end()});
            assert(base->processWork(&result, 250));
            if (frame[10] & 0x80) {
                uint32_t nonce = 0; std::memcpy(&nonce, frame.data() + 2, 4);
                assert(!result.is_reg_resp && result.nonce == nonce && result.job_id == ((frame[7] & 0xf0) >> 1));
                assert(result.rolled_version == ((static_cast<uint32_t>(frame[8]) << 8) | frame[9]) << 13);
                assert(result.asic_nr == static_cast<int>(((__builtin_bswap32(nonce) >> 17) & 0xff) / 64));
            } else assert(result.is_reg_resp && result.data == 0x13700000 && result.asic_nr == 0);
        }
        for (unsigned byte = 0; byte < frame.size(); ++byte) for (unsigned bit = 0; bit < 8; ++bit) {
            base->clearPendingResults(); SERIAL_clear_buffer(); result.nonce = 0xdeadbeef;
            auto mutant = frame; mutant[byte] ^= 1U << bit; serialFrames.push_back(mutant);
            assert(!base->processWork(&result, 250) && result.nonce == 0xdeadbeef);
        }
    }
    // An11-byte read completes A and includes the first3bytes of B. B survives.
    base->clearPendingResults(); SERIAL_clear_buffer(); const auto &a = captured[0], &b = captured[1];
    serialFrames.push_back({a.begin(), a.begin() + 3}); assert(!base->processWork(&result, 250));
    std::vector<uint8_t> joined(a.begin() + 3, a.end()); joined.insert(joined.end(), b.begin(), b.end());
    serialFrames.push_back(joined); assert(base->processWork(&result, 250) && result.job_id == ((a[7] & 0xf0) >> 1));
    assert(base->processWork(&result, 250) && result.job_id == ((b[7] & 0xf0) >> 1));
    // Retire while B's prefix is in the driver's pending-tail buffer, rather
    // than in the codec parser. Neither virtual path may resurrect B's suffix.
    for (bool coldReset : {false, true}) {
        base->clearPendingResults(); SERIAL_clear_buffer();
        serialFrames.push_back({a.begin(), a.begin() + 3});
        assert(!base->processWork(&result, 250));
        joined.assign(a.begin() + 3, a.end());
        joined.insert(joined.end(), b.begin(), b.begin() + 3);
        serialFrames.push_back(joined);
        assert(base->processWork(&result, 250));
        if (coldReset) base->resetAfterPowerCycle();
        else base->clearPendingResults();
        driver.declaredTopology(4); // Independent fixture declaration, never production IO.
        serialFrames.push_back({b.begin() + 3, b.end()}); result.nonce = 0xdeadbeef;
        assert(!base->processWork(&result, 250) && result.nonce == 0xdeadbeef);
        serialFrames.push_back(b); assert(base->processWork(&result, 250));
    }
    // Noise + bad CRC then a fresh valid response resynchronizes without UART flush.
    base->clearPendingResults(); SERIAL_clear_buffer(); auto bad = a; bad[3] ^= 1;
    joined = {1,2,0xaa,0x00,3}; joined.insert(joined.end(), bad.begin(), bad.end()); joined.insert(joined.end(), b.begin(), b.end());
    serialFrames.push_back(joined); unsigned calls = 0;
    while (!base->processWork(&result, 250)) assert(++calls < 5);
    assert(result.job_id == ((b[7] & 0xf0) >> 1));
    auto registerFrame = crcFrame({0xaa,0x55,0x12,0x34,0x56,0x78,0x80,0x90,0,0,0});
    serialFrames.push_back(registerFrame); assert(base->processWork(&result, 250) && result.is_reg_resp && result.asic_nr == 2 && result.data == 0x12345678);
    registerFrame[6] = 1; registerFrame = crcFrame(registerFrame); result.nonce = 0xdeadbeef;
    serialFrames.push_back(registerFrame); assert(!base->processWork(&result, 250) && result.nonce == 0xdeadbeef);
    driver.declaredTopology(3); registerFrame[6] = 192; registerFrame = crcFrame(registerFrame);
    serialFrames.push_back(registerFrame); assert(!base->processWork(&result, 250));
    driver.declaredTopology(1); registerFrame[6] = 0; registerFrame = crcFrame(registerFrame);
    serialFrames.push_back(registerFrame); assert(base->processWork(&result, 250) && result.asic_nr == 0);
    registerFrame[6] = 64; registerFrame = crcFrame(registerFrame);
    serialFrames.push_back(registerFrame); assert(!base->processWork(&result, 250));
    // Valid CRC with an unknown type cannot become a nonce.
    driver.declaredTopology(4); auto unsupported = a; unsupported[10] = 0x20; unsupported = crcFrame(unsupported);
    serialFrames.push_back(unsupported); result.nonce = 0xdeadbeef;
    assert(!base->processWork(&result, 250) && result.nonce == 0xdeadbeef);
    // Both virtual retirement paths drop partial frames; reset also drops topology.
    for (bool coldReset : {false,true}) {
        base->clearPendingResults(); serialFrames.push_back({a.begin(), a.begin() + 3}); assert(!base->processWork(&result, 250));
        if (coldReset) base->resetAfterPowerCycle(); else base->clearPendingResults();
        serialFrames.push_back({a.begin() + 3, a.end()}); assert(!base->processWork(&result, 250));
        base->clearPendingResults(); serialFrames.push_back(a);
        if (coldReset) assert(!base->processWork(&result, 250));
        else assert(base->processWork(&result, 250));
        driver.declaredTopology(4);
    }
    const auto beforeClock = hostClockMs; const auto beforeRead = rxCalls;
    assert(!base->processWork(&result, 250) && hostClockMs - beforeClock == 250 && rxCalls == beforeRead + 1);
    std::cout << "Actual diagnostic RX: 5 published frames, 50 splits, 440 single-bit mutants, tail resync, source bounds, retirement and 250 ms budget pass\n";
#endif
}

static void checkCaptureMetadata() {
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
    resetFixture(); DiagnosticProbe driver; Asic *base = &driver;
    // Exercise the actual mask setter: request10000 rounds to a shared8192
    // threshold. Metadata must describe that programmed threshold, not10000.
    driver.setJobDifficultyMask(10000);
    assert(transmittedPackets == 1 && transmittedBytes.back().size() == 11);
    bm_job value{};
    value.version = 0x23012345; value.version_mask = 0x1fffe000;
    value.ntime = 0x65abcdef; value.target = 0x1b0404cb;
    value.starting_nonce = 0x12345678; value.asic_diff = 10000; value.pool_id = 1;
    for (unsigned i = 0; i < 32; ++i) {
        value.prev_block_hash[i] = 255 - i * 3;
        value.merkle_root[i] = i * 7 + 11;
        // Distinct wire-order arrays catch use of the wrong representation.
        value.prev_block_hash_be[i] = i * 5 + 3;
        value.merkle_root_be[i] = 251 - i * 5;
    }
    const std::array<uint8_t, 80> expectedHeader = [&value] {
        std::array<uint8_t, 80> result{};
        const uint8_t version[] = {0x45, 0x23, 0x01, 0x23};
        const uint8_t finalWords[] = {0xef, 0xcd, 0xab, 0x65, 0xcb, 0x04, 0x04, 0x1b, 0x78, 0x56, 0x34, 0x12};
        std::copy(std::begin(version), std::end(version), result.begin());
        std::copy(std::begin(value.prev_block_hash), std::end(value.prev_block_hash), result.begin() + 4);
        std::copy(std::begin(value.merkle_root), std::end(value.merkle_root), result.begin() + 36);
        std::copy(std::begin(finalWords), std::end(finalWords), result.begin() + 68);
        return result;
    }();
    constexpr uint32_t counter = 0xf1234567;
    constexpr uint64_t generation = UINT64_C(0xfedcba9876543210);
    const auto expectedId = driver.Asic::sendWork(counter, &value);
    const auto expectedPacket = transmittedBytes.back();
    uint8_t id = 0xff;
    assert(driver.dispatchWork({}, counter, &value, id, generation) == BM1370::DispatchStatus::Sent);
    assert(id == expectedId && capturedJobs.size() == 1);
    const auto first = capturedJobs.back();
    assert(first.packet == expectedPacket && first.packet.size() == 88 && first.returned == 88);
    assert(first.packet == transmittedBytes.back() && first.metadata.header == expectedHeader);
    assert(first.metadata.logicalJobCounter == counter && first.metadata.versionMask == value.version_mask);
    assert(first.metadata.asicTicketDifficulty == 8192 && first.metadata.asicTicketDifficulty != value.asic_diff);
    assert(first.metadata.poolIndex == 1 && first.generation == generation);
    const auto retiredBeforeDispatch = capturedRetirements.size();
    value.version = value.version_mask = value.ntime = value.target = value.starting_nonce = 0;
    std::memset(value.prev_block_hash, 0, sizeof(value.prev_block_hash));
    std::memset(value.merkle_root, 0, sizeof(value.merkle_root));
    // Retained context is a copy, unaffected by later caller/job reuse.
    assert(capturedJobs.front().metadata.header == expectedHeader && capturedJobs.front().packet == expectedPacket);
    for (int pool : {0, 1, -1, 2}) {
        value.pool_id = pool;
        base->sendWork(counter, &value); // Old virtual API honestly supplies no generation.
        const auto &call = capturedJobs.back();
        assert(call.generation == 0 && call.metadata.logicalJobCounter == counter);
        assert(call.metadata.poolIndex == (pool == 0 || pool == 1 ? pool : BM1370Capture::UnknownPool));
    }
    const auto jobsBeforeUnsupported = capturedJobs.size();
    const auto sendsBeforeUnsupported = transmittedPackets;
    for (unsigned address : {0, 64, 128, 192, 255}) {
        assert(driver.dispatchWork({BM1370Protocol::TargetKind::Chip, static_cast<uint8_t>(address)}, counter,
            &value, id, generation) == BM1370::DispatchStatus::Unsupported && id == 0);
        assert(capturedJobs.size() == jobsBeforeUnsupported && transmittedPackets == sendsBeforeUnsupported);
    }
    assert(driver.dispatchWork({}, counter, nullptr, id, generation) == BM1370::DispatchStatus::InvalidArgument);
    assert(capturedJobs.size() == jobsBeforeUnsupported && transmittedPackets == sendsBeforeUnsupported);
    assert(capturedRetirements.size() == retiredBeforeDispatch);
    serialShortWrite = true;
    assert(driver.dispatchWork({}, counter, &value, id, generation) == BM1370::DispatchStatus::TransportFailed);
    assert(capturedJobs.back().returned == 87 && capturedJobs.back().generation == generation && !driver.transportOkay());
    serialShortWrite = false;
    assert(driver.dispatchWork({}, counter + 1, &value, id, generation) == BM1370::DispatchStatus::TransportFailed);
    assert(capturedJobs.back().returned == 88 && !driver.transportOkay()); // Successful later writes cannot clear failure.
    const auto packetsBeforeRetirement = transmittedPackets;
    const auto gpioBeforeRetirement = gpioLevels;
    base->clearPendingResults();
    assert(capturedRetirements.size() == retiredBeforeDispatch + 1);
    assert(capturedRetirements.back().marker == BM1370Capture::Retirement::ParserReset && capturedRetirements.back().generation == 0);
    base->resetAfterPowerCycle();
    assert(capturedRetirements.size() == retiredBeforeDispatch + 3 && driver.transportOkay());
    assert(capturedRetirements[retiredBeforeDispatch + 1].marker == BM1370Capture::Retirement::PowerReset);
    assert(capturedRetirements.back().marker == BM1370Capture::Retirement::ParserReset);
    assert(transmittedPackets == packetsBeforeRetirement && gpioLevels == gpioBeforeRetirement);
    const FiveTratumCapture::Status unattested{};
    assert(!unattested.capture.physicalWireComplete && !unattested.capture.physicalChipIdentity && !unattested.capture.independentWorkAssignment);
    std::cout << "Actual capture branch: immutable canonical80 and exact88bytes, rounded8192 mask, pool/context, uint64 generation, unknown legacy generation, zero chip IO and sticky short TX pass\n";
#else
    std::abort(); // This scenario is meaningful only in the explicitly captured binary.
#endif
}

int main(int argc, char **argv) {
    if (argc == 2 && std::string(argv[1]) == "codec") { checkJobCodecAndUnsupported(); checkDiagnosticReceiver(); return 0; }
    if (argc == 2 && std::string(argv[1]) == "capture-metadata") { checkCaptureMetadata(); return 0; }
    if (argc == 2 && std::string(argv[1]) == "init-wire") {
        resetFixture(); BM1370 asic; Buck buck; NerdQaxePlus2 board;
        board.m_asics = &asic; board.m_tps = &buck; asic.setRecoveryGuard(protection, nullptr);
        assert(board.pauseMiningPower()); asic.resetAfterPowerCycle(); assert(board.initAsics());
        for (const auto &packet : transmittedBytes) { for (auto byte : packet) std::printf("%02x", byte); std::printf("\n"); }
        return 0;
    }
    if (argc == 2) {
        resetFixture();
        std::string test = argv[1];
        const bool oct47 = test.rfind("oct47-", 0) == 0;
        const bool oct67 = test.rfind("oct67-", 0) == 0;
        const bool oct = oct47 || oct67;
        if (oct) test = test.substr(6);
        BM1370 driver; Buck buck; NerdQaxePlus2 board;
        const unsigned expectedCount = oct ? 8 : 4;
        const unsigned savedFrequency = oct ? 700 : 500, savedVoltage = oct ? 1180 : 1130;
        if (oct) {
            // Constructor/regulator detection is separately exercised by the
            // whole-production-Oct-board fixture. Here the external profile
            // feeds the actual inherited init/power methods and UART driver.
            board.model = "NerdOCTAXE-γ"; board.m_asicCount = 8;
            board.m_asicFrequency = savedFrequency; board.m_asicVoltageMillis = savedVoltage;
            board.m_initVoltageMillis = 0;
            board.m_numPhases = oct47 ? 4 : 6; board.m_imax = oct47 ? 180 : 240;
            board.m_ifault = oct47 ? 160 : 235;
            simulatedChips = 8;
        }
        board.m_hasRev7TPS546 = test == "cold-rev7"; board.m_asics = &driver; board.m_tps = &buck;
        POWER_MANAGEMENT_MODULE.m_board = &board;
        SYSTEM_MODULE.board = &board;
        if (test == "wrong-chip-count") simulatedChips = expectedCount - 1;
        if (test == "extra-chip-count") simulatedChips = expectedCount + 1;
        if (test == "zero-chip-count") simulatedChips = 0;
        if (test == "buck-init-failed") buck.initOkay = false;
        if (test == "buck-voltage-failed") buck.voltageOkay = false;
        if (test == "buck-disable-failed") buck.disableOkay = false;
        if (test == "voltage-mismatch") board.measuredVoutOverride = 1.0f;
        if (test == "voltage-nan") board.measuredVoutOverride = std::numeric_limits<float>::quiet_NaN();
        if (test == "baud-failed") failFinalBaud = true;
        using namespace FiveTratumMining;
        if (test == "wrong-declared-count" || test == "wrong-asic" || test == "wrong-model" || test == "can-slave" || test == "can-enabled") {
            if (test == "wrong-declared-count") board.m_asicCount = expectedCount - 1;
            if (test == "wrong-asic") board.asicModel = "BM1397";
            if (test == "wrong-model") board.model = "NerdOCTAXE+";
            if (test == "can-enabled") hostCanEnabled = true;
            assert(!initializeControl(&board, test == "can-slave"));
            assert(!controlSupported() && buck.voltages.empty() && transmittedPackets == 0);
            JsonDocument unsupported; assert(writeControlReport(unsupported) && !unsupported["supported"].as<bool>());
            return 0;
        }
        if (test == "paused-boot-resume") policyPause = true;
        assert(initializeControl(&board, false));
        if (test == "pause-during-ramp") policyPauseAtMs = 4500;
        if (test == "invalid-vr") sensedVr = 0;
        if (test == "job-timer-create-failed") timerCreateOkay = false;
        if (test == "job-timer-start-failed") timerStartOkay = false;
        if (test != "no-job-ack") create_jobs_task(nullptr);
        setRuntimeReady(test != "no-workers");
        updateControl(); // Exact production backend and actual regulator/ASIC initialization.
        JsonDocument report; assert(writeControlReport(report));
        const bool failed = test == "wrong-chip-count" || test == "extra-chip-count" || test == "zero-chip-count" || test == "buck-init-failed" || test == "buck-voltage-failed" || test == "buck-disable-failed" || test == "voltage-mismatch" || test == "voltage-nan" || test == "invalid-vr" || test == "baud-failed";
        const bool cancelled = test == "pause-during-ramp";
        const bool bootPaused = test == "paused-boot-resume";
        const bool noWorkers = test == "no-workers" || test == "no-job-ack" || test == "job-timer-create-failed" || test == "job-timer-start-failed";
        assert(report["status"]["appliedPaused"].as<bool>() == (failed || cancelled || noWorkers || bootPaused));
        assert(!report["status"]["transitionPending"].as<bool>());
        assert(miningWritesAllowed() == !(failed || cancelled || noWorkers || bootPaused));
        if (!noWorkers && !bootPaused && !failed) assert(POWER_MANAGEMENT_MODULE.firstBuckPoll > 0 && POWER_MANAGEMENT_MODULE.maximumPollGapMs <= 2000);
        assert(board.m_asicFrequency == savedFrequency && board.m_asicVoltageMillis == savedVoltage && board.m_vrFrequency == 25011);
        if (failed) {
            assert(!report["status"]["error"].isNull());
            assert(gpioLevels[BM1368_RST_PIN] == 0 && gpioLevels[TPS53647_EN_PIN] == 0 && gpioLevels[LDO_EN_PIN] == 0);
            simulatedChips = expectedCount; board.measuredVoutOverride = -1;
            updateControl(); // A corrected measurement does not clear the failure latch.
            assert(!miningWritesAllowed());
        } else if (bootPaused) {
            assert(report["status"]["error"].isNull() && !board.m_isInitialized && buck.voltages.empty() && transmittedPackets == 0);
            policyPause = false; updateControl();
            assert(miningWritesAllowed() && board.m_isInitialized && board.m_chipsDetected == static_cast<int>(expectedCount));
        } else if (noWorkers) {
            assert(!report["status"]["error"].isNull() && buck.voltages.empty() && transmittedPackets == 0);
            if (test == "job-timer-start-failed") assert(timersDeleted == 1);
        } else if (cancelled) {
            assert(report["status"]["error"].isNull() && !board.m_isInitialized);
            policyPauseAtMs = std::numeric_limits<uint64_t>::max(); policyPause = false;
            updateControl(); assert(miningWritesAllowed() && board.m_isInitialized);
        } else {
            assert(report["status"]["error"].isNull() && board.m_isInitialized);
            assert(POWER_MANAGEMENT_MODULE.m_vrTemp == 55); // Genuine first buck telemetry, not cold cached zero.
            assert(board.m_chipsDetected == static_cast<int>(expectedCount));
            if (oct) assert(buck.initializedPhases == (oct47 ? 4u : 6u) && buck.initializedCurrent == (oct47 ? 180u : 240u) && buck.initializedFaultCurrent == (oct47 ? 160 : 235));
        }
        if (test == "resume") {
            const uint64_t oldEpoch = workGeneration();
            asicJobs.storeJob(job("previous-running-job"), 5, oldEpoch);
            policyPause = true; updateControl();
            assert(!miningWritesAllowed() && !board.m_shutdown && !board.m_isInitialized);
            assert(!asicJobs.getClone(5, oldEpoch));
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
            const auto marker = std::find_if(capturedRetirements.rbegin(), capturedRetirements.rend(),
                [](const CapturedRetirement &entry) { return entry.marker == BM1370Capture::Retirement::GenerationChange; });
            assert(marker != capturedRetirements.rend() && marker->generation == workGeneration() && marker->generation > oldEpoch);
#endif
            policyPause = false; updateControl();
            assert(miningWritesAllowed() && board.m_isInitialized && !board.m_shutdown && workGeneration() > oldEpoch);
            assert(HASHRATE_MONITOR.resets == 2 && !asicJobs.getClone(5, oldEpoch));
        }
        if (test == "thermal-latch") {
            SYSTEM_MODULE.error = Board::Error::TEMP_FAULT; updateControl();
            assert(!miningWritesAllowed() && !board.m_isInitialized);
            SYSTEM_MODULE.error = Board::Error::NONE; updateControl();
            assert(!miningWritesAllowed() && !board.m_isInitialized);
        }
        if (test == "live-ramp" || test == "live-ramp-pause" || test == "live-ramp-uart-failure" || test == "live-ramp-thermal") {
            const auto oldEpoch = workGeneration();
            asicJobs.storeJob(job("pre-ramp"), 5, oldEpoch);
            const float previousVoltage = buck.voltages.back();
            const auto writesBefore = transmittedPackets;
            const unsigned newFrequency = oct ? 750 : 600;
            board.m_asicFrequency = newFrequency;
            if (test == "live-ramp-pause") policyPauseAtMs = hostClockMs + 500;
            if (test == "live-ramp-thermal") thermalFaultAtMs = hostClockMs + 500;
            if (test == "live-ramp-uart-failure") failNextWrite = true;
            const bool updated = applyFrequency(newFrequency);
            assert(updated == (test == "live-ramp"));
            assert(!asicJobs.getClone(5, oldEpoch) && workGeneration() > oldEpoch && transmittedPackets > writesBefore);
            assert(POWER_MANAGEMENT_MODULE.maximumPollGapMs <= 2000);
            assert(buck.voltages.back() == previousVoltage); // Live PLL ramp never changes the saved core voltage.
            if (updated) assert(miningWritesAllowed() && board.m_isInitialized);
            else assert(!miningWritesAllowed() && !board.m_isInitialized && gpioLevels[BM1368_RST_PIN] == 0);
            if (test == "live-ramp-pause") {
                policyPauseAtMs = std::numeric_limits<uint64_t>::max(); policyPause = false;
                updateControl(); assert(miningWritesAllowed() && board.m_isInitialized);
            }
            if (test == "live-ramp-uart-failure") {
                serialShortWrite = false; updateControl(); assert(!miningWritesAllowed());
            }
            if (test == "live-ramp-thermal") {
                thermalFaultAtMs = std::numeric_limits<uint64_t>::max(); SYSTEM_MODULE.error = Board::Error::NONE;
                updateControl(); assert(!miningWritesAllowed());
            }
        }
        std::cout << "Actual backend + ASIC/board cold-start/readback/generation scenario passed: " << test << '\n';
        return 0;
    }
    for (bool rev7 : {false, true}) {
        resetFixture(); BM1370 asic; Buck buck; NerdQaxePlus2 board;
        board.m_hasRev7TPS546 = rev7; board.m_asics = &asic; board.m_tps = &buck;
        asic.setRecoveryGuard(protection, nullptr);
        assert(board.pauseMiningPower() && !board.m_shutdown && !board.m_isInitialized);
        asic.resetAfterPowerCycle(); SERIAL_set_baud(115200);
        assert(board.initAsics() && board.m_chipsDetected == 4 && board.m_isInitialized);
        assert(board.m_asicFrequency == 500 && board.m_asicVoltageMillis == 1130 && board.m_vrFrequency == 25011);
        assert(std::fabs(buck.voltages.front() - 1.2f) < 0.0001f);
        assert(std::fabs(buck.voltages.back() - 1.130f) < 0.0001f);
        assert(protectionPolls >= 8 && longestProtectionGap <= 2000 && hostClockMs > 7000);
        assert(hostBaud > 115200 && asic.transportOkay());
        assert(board.pauseMiningPower() && !board.m_shutdown && !board.m_isInitialized);
        assert(gpioLevels[BM1368_RST_PIN] == 0 && gpioLevels[TPS53647_EN_PIN] == 0 && gpioLevels[LDO_EN_PIN] == 0);
    }
    for (int failure = 0; failure < 4; ++failure) {
        resetFixture(); BM1370 asic; Buck buck; NerdQaxePlus2 board;
        board.m_asics = &asic; board.m_tps = &buck; asic.setRecoveryGuard(protection, nullptr);
        if (failure == 0) buck.initOkay = false;
        if (failure == 1) buck.voltageOkay = false;
        if (failure == 2) serialShortWrite = true;
        if (failure == 3) abortAtMs = 4500; // New pause/fault during the actual existing PLL ramp.
        assert(!board.initAsics() && !board.m_isInitialized && !board.m_shutdown);
        assert(gpioLevels[BM1368_RST_PIN] == 0 && gpioLevels[TPS53647_EN_PIN] == 0 && gpioLevels[LDO_EN_PIN] == 0);
    }
    resetFixture();
    BM1370 asic; task_result result{}; result.nonce = 0xdeadbeef;
    const std::vector<uint8_t> frame{0xaa, 0x55, 0x12, 0x34, 0x56, 0x78, 0, 0x90, 0, 0, 0};
    serialFrames.push_back(std::vector<uint8_t>(frame.begin(), frame.begin() + 3));
    assert(!asic.processWork(&result, 250) && result.nonce == 0xdeadbeef);
    serialFrames.push_back(std::vector<uint8_t>(frame.begin() + 3, frame.end()));
    assert(asic.processWork(&result, 250) && result.is_reg_resp && result.reg == 0x90 && result.data == 0x12345678);
    serialFrames.push_back({0xaa, 0x55, 1});
    assert(!asic.processWork(&result, 250));
    asic.resetAfterPowerCycle(); // Partial old UART bytes cannot authenticate a future frame.
    serialFrames.push_back(frame);
    assert(asic.processWork(&result, 250) && result.data == 0x12345678);

    AsicJobs jobs;
    jobs.storeJob(job("old"), 5, 1);
    auto *copied = jobs.getClone(5, 1); assert(copied && !strcmp(copied->jobid, "old")); free_bm_job(copied);
    jobs.resetChainGeneration(2);
    assert(!jobs.getClone(5, 1));
    jobs.storeJob(job("new"), 5, 2);
    assert(!jobs.getClone(5, 1));
    copied = jobs.getClone(5, 2); assert(copied && !strcmp(copied->jobid, "new")); free_bm_job(copied);
    jobs.storeJob(job("late"), 5, 1); // Old generation cannot replace the new job.
    copied = jobs.getClone(5, 2); assert(copied && !strcmp(copied->jobid, "new")); free_bm_job(copied);
    assert(!jobs.getClone(250, 2)); jobs.storeJob(job("invalid-index"), 250, 2);
    std::cout << "Actual ASIC and two QAxe regulator init bodies: saved clock/voltage/VR, cooperative polling, "
                 "fault/cancel safe off, full-frame RX and classic job generation retirement pass\n";
}
