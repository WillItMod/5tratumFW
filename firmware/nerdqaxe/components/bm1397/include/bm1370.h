#pragma once

#include "driver/gpio.h"
#include "mining.h"
#include "rom/gpio.h"
#include "bm1368.h"
#include "bm1370_protocol.h"

class BM1370 : public Asic {
protected:
    uint16_t m_detectedAsicCount = 0; // Observed enumeration, never configured count.
    uint16_t m_chainAddressInterval = 0; // Can represent a single-chip interval256.
#if defined(FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER) && FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER
    BM1370Protocol::StreamParser m_responseParser;
    uint8_t m_pendingResponseBytes[BM1370Protocol::ResponseBytes]{};
    uint8_t m_pendingResponsePosition = 0, m_pendingResponseSize = 0;
    bool convertResponse(const BM1370Protocol::Response &response, task_result *result);
#endif
    const uint8_t* getChipId() override;
    uint32_t getDefaultVrFrequency() override;

    uint8_t jobToAsicId(uint8_t job_id) override;
    uint8_t asicToJobId(uint8_t asic_id) override;

public:
    enum class DispatchStatus { Sent, Unsupported, InvalidArgument, TransportFailed };
    BM1370();
    uint8_t sendWork(uint32_t job_id, bm_job *next_bm_job) override;
    void sendRawJob(BM1368_job *job) override;
    bool processWork(task_result *result, uint16_t timeoutMs = 60000) override;
    void resetAfterPowerCycle() override;
    void clearPendingResults() override;
    // No chip-targeted work format is established. Chip requests always return
    // Unsupported before any UART write; register addressing is not job routing.
    DispatchStatus dispatchWork(const BM1370Protocol::WorkTarget &target,
                                uint32_t logicalJobCounter, bm_job *job, uint8_t &wireJobId,
                                uint64_t generation = 0);
    const char* getName() override { return "BM1370"; }
    uint8_t init(uint64_t frequency, uint16_t asic_count, uint32_t difficulty, uint32_t vrFrequency) override;
    uint16_t getSmallCoreCount() override;
    uint16_t observedAsicCount() const { return m_detectedAsicCount; }
};
