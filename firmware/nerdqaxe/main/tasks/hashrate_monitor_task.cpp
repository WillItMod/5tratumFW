#include "global_state.h"
#include "hashrate_monitor_task.h"
#include "boards/board.h"
#include "esp_log.h"
#include "mining.h"
#include "utils.h"
#include "mining_control.h"
#include <memory>
#include <new>

static const char *HR_TAG = "hashrate_monitor";
static constexpr uint8_t REG_NONCE_TOTAL_CNT = 0x90;

HashrateMonitor::HashrateMonitor()
{}

bool HashrateMonitor::start(Board *board, Asic *asic)
{
    if (!board || !asic) {
        ESP_LOGE(HR_TAG, "start(): missing dependencies (board=%p, asic=%p)", (void *) board, (void *) asic);
        return false;
    }
    const int count = board->getAsicCount();
    if (count < 1 || count > 64) {
        ESP_LOGE(HR_TAG, "start(): invalid ASIC count %d", count);
        return false;
    }
    std::unique_ptr<float[]> rates(new (std::nothrow) float[count]());
    std::unique_ptr<int64_t[]> times(new (std::nothrow) int64_t[count]());
    std::unique_ptr<uint32_t[]> counters(new (std::nothrow) uint32_t[count]());
    std::unique_ptr<bool[]> available(new (std::nothrow) bool[count]());
    if (!rates || !times || !counters || !available) {
        ESP_LOGE(HR_TAG, "start(): sample allocation failed");
        return false;
    }
    {
        // RX and display tasks can already be running. Publish count and all
        // storage as one transaction, never an observable partial setup.
        PThreadGuard lock(m_mutex);
        if (m_asicCount != 0) {
            return false;
        }
        m_board = board;
        m_asic = asic;
        m_period_ms = HR_INTERVAL;
        m_chipHashrate = rates.release();
        m_prevResponse = times.release();
        m_prevCounter = counters.release();
        m_chipSampleAvailable = available.release();
        m_asicCount = count;
        if (xTaskCreatePSRAM(&HashrateMonitor::taskWrapper, "hr_monitor", 4096,
                            this, 10, nullptr) != pdPASS) {
            m_asicCount = 0;
            delete[] m_chipHashrate;
            delete[] m_prevResponse;
            delete[] m_prevCounter;
            delete[] m_chipSampleAvailable;
            m_chipHashrate = nullptr;
            m_prevResponse = nullptr;
            m_prevCounter = nullptr;
            m_chipSampleAvailable = nullptr;
            m_board = nullptr;
            m_asic = nullptr;
            ESP_LOGE(HR_TAG, "start(): task creation failed");
            return false;
        }
    }
    ESP_LOGI(HR_TAG, "started (period=%lums)", m_period_ms);
    return true;
}

void HashrateMonitor::setChipHashrate(int nr, float temp) {
    if (nr < 0 || nr >= m_asicCount) {
        return;
    }
    m_chipHashrate[nr] = temp;
}

float HashrateMonitor::getChipHashrate(int nr) {
    PThreadGuard lock(m_mutex);
    if (nr < 0 || nr >= m_asicCount) {
        return 0.0f;
    }
    return m_chipHashrate[nr];
}

float HashrateMonitor::getTotalChipHashrate() {
    PThreadGuard lock(m_mutex);
    float total = 0.0f;
    for (int i=0;i < m_asicCount; i++) {
        total += m_chipHashrate[i];
    }
    return total;
}

FiveTratumTelemetry::ChipHashrateSample HashrateMonitor::getChipHashrateSample(int nr) {
    PThreadGuard lock(m_mutex);
    if (nr < 0 || nr >= m_asicCount || !m_chipSampleAvailable ||
        !m_chipHashrate || !m_prevResponse) {
        return {};
    }
    return {m_chipHashrate[nr], m_prevResponse[nr], m_chipSampleAvailable[nr]};
}

void HashrateMonitor::taskWrapper(void *pv)
{
    auto *self = static_cast<HashrateMonitor *>(pv);
    self->taskLoop();
}

void HashrateMonitor::publishTotalIfComplete()
{
    size_t offset = 0;

    Board* board = SYSTEM_MODULE.getBoard();

    // Iterate through each ASIC and append its count to the log message
    for (int i = 0; i < board->getAsicCount(); i++) {
        offset += snprintf(m_logBuffer + offset, sizeof(m_logBuffer) - offset, "%.2fGH/s / ", getChipHashrate(i));
    }
    if (offset >= 2) {
        m_logBuffer[offset - 2] = 0; // remove trailing slash
    }

    // apply slight 3 tap median filter to remove weird outliers
    m_hashrate = m_median.update(getTotalChipHashrate());

    ESP_LOGI(HR_TAG, "chip hashrates: %s (total: %.3fGH/s)", m_logBuffer, m_hashrate);
}

void HashrateMonitor::taskLoop()
{
    // Small startup delay
    vTaskDelay(pdMS_TO_TICKS(4000));

    // Send broadcast RESET for counter register once
    bool counterNeedsReset = true;
    uint64_t counterGeneration = 0;

    TickType_t lastWake = xTaskGetTickCount();
    while (1) {
        if (POWER_MANAGEMENT_MODULE.isShutdown()) {
            ESP_LOGW(HR_TAG, "suspended");
            vTaskSuspend(NULL);
        }

        if (!m_board || !m_asic) {
            vTaskDelay(pdMS_TO_TICKS(m_period_ms));
            continue;
        }

        {
        FiveTratumMining::MiningOperation operation(FiveTratumMining::operationGate());
        if (!operation) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
        if (counterNeedsReset || counterGeneration != operation.generation()) {
            m_asic->resetCounter(REG_NONCE_TOTAL_CNT);
            counterNeedsReset = false;
            counterGeneration = operation.generation();
        }

        // read the counters
        m_asic->readCounter(REG_NONCE_TOTAL_CNT);

        // responses normally take 20-30ms, so this is safe
        vTaskDelay(pdMS_TO_TICKS(500));

        if (!operation.isCurrent()) continue;

        publishTotalIfComplete();

        // apply a slight smoothing
        if (!m_smoothedHashrate) {
            m_smoothedHashrate = m_hashrate;
        }

        m_smoothedHashrate = 0.5f * m_smoothedHashrate + 0.5f * m_hashrate;

        } // Release UART/result lease before the long periodic wait.

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(m_period_ms));
    }
}

void HashrateMonitor::resetAfterPowerCycle() {
    PThreadGuard lock(m_mutex);
    for (int i = 0; i < m_asicCount; ++i) {
        m_chipHashrate[i] = 0;
        m_prevResponse[i] = 0;
        m_prevCounter[i] = 0;
        m_chipSampleAvailable[i] = false;
    }
    m_hashrate = m_smoothedHashrate = 0;
    m_median = Median<5>{};
}

void HashrateMonitor::onRegisterReply(uint8_t asic_idx, uint32_t counterNow)
{
    PThreadGuard lock(m_mutex);
    if (asic_idx >= m_asicCount) {
        ESP_LOGE(HR_TAG, "respnse for invalid asic %d", (int) asic_idx);
        return;
    }

    int64_t now = esp_timer_get_time();

    // first response
    if (!m_prevResponse[asic_idx]) {
        m_prevResponse[asic_idx] = now;
        m_prevCounter[asic_idx] = counterNow;
        return;
    }

    int64_t timeDelta = now - m_prevResponse[asic_idx];
    if (timeDelta <= 0) {
        return;
    }
    uint32_t counterDelta = counterNow - m_prevCounter[asic_idx];

    double chip_ghs = (double) counterDelta * (double) 0x100000000uLL / (double) timeDelta / 1000.0;
//    ESP_LOGE("XXX", "m_prevResponse[%d]=%lld now=%lld m_prevCounter[%d]=%lu counterNow=%lu timeDelta=%llu counterDelta=%lu chip_ghs=%.3f",
//        (int) asic_idx, m_prevResponse[asic_idx], now, (int) asic_idx, m_prevCounter[asic_idx], counterNow, timeDelta, counterDelta, chip_ghs);

    setChipHashrate(asic_idx, chip_ghs);
    m_chipSampleAvailable[asic_idx] = true;

    m_prevCounter[asic_idx] = counterNow;
    m_prevResponse[asic_idx] = now;
}
