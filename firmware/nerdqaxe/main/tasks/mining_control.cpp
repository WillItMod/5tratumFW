#include "mining_control.h"
#include "mining_board_policy.h"
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
#include "bm1370_capture_runtime.h"
#endif
#include "mining_schedule.h"
#include "boards/board.h"
#include "global_state.h"
#include "nvs_config.h"
#include "serial.h"
#include "asic.h"
#include "create_jobs_task.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>
#include <cstring>
#include <cmath>

namespace FiveTratumMining {
namespace {
OperationGate gate;
MiningPowerState powerState(gate);
Board *controlledBoard = nullptr;
unsigned expectedAsicCount = 0;
std::atomic_bool qualified{false}, runtimeReady{false}, runtimeObserved{false};
std::atomic_bool jobWorkerReady{false}, jobWorkerObserved{false};
pthread_mutex_t reportMutex = PTHREAD_MUTEX_INITIALIZER;
ControlReport published;

class Backend final : public ControlHardware {
    const char *m_startError = "ASIC initialization failed; restart required";
public:
    uint16_t targetFrequency = 0;
    ControlInputs inputs() override {
        return {requestedPaused(), runtimeReady.load() && jobWorkerReady.load(),
            POWER_MANAGEMENT_MODULE.isShutdown() || !controlledBoard || controlledBoard->isShutdown() ||
            SYSTEM_MODULE.getBoardError() != Board::Error::NONE};
    }
    bool waitIdle() override {
        trigger_job_creation();
        const int64_t started = esp_timer_get_time();
        while (!gate.idle()) {
            POWER_MANAGEMENT_MODULE.refreshProtectionForMining();
            if (esp_timer_get_time() - started >= 2000000) return false;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        return true;
    }
    bool waitTxIdle() override { return SERIAL_wait_tx_idle(500); }
    bool powerOff() override { return controlledBoard && controlledBoard->pauseMiningPower(); }
    bool clearTransport() override {
        if (!SERIAL_clear_buffer_checked()) return false;
        if (controlledBoard && controlledBoard->getAsicDriver()) controlledBoard->getAsicDriver()->clearPendingResults();
        return true;
    }
    void retireWork() override {
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
        FiveTratumCapture::retirement(BM1370Capture::Retirement::GenerationChange, 0, gate.generation());
#endif
        // No worker may own a copied result or transmit across this point.
        asicJobs.resetChainGeneration(gate.generation());
        HASHRATE_MONITOR.resetAfterPowerCycle();
    }
    static bool guard(void *context) {
        auto *self = static_cast<Backend *>(context);
        updateSchedule();
        const auto input = self->inputs();
        return !input.requestedPaused && input.runtimeReady && !input.fault &&
               POWER_MANAGEMENT_MODULE.refreshProtectionForMining();
    }
    bool start() override {
        m_startError = "ASIC initialization/protection failed; restart required";
        if (!controlledBoard || !guard(this)) return false;
        Asic *asic = controlledBoard->getAsicDriver();
        if (!asic) return false;
        // The stopped/reset ASIC boots at 115200 with its initial PLL/mask.
        // Reusing cached live values would skip required initialization writes.
        if (!SERIAL_set_baud_checked(115200)) {
            m_startError = "ASIC UART reset baud failed; restart required";
            return false;
        }
        if (!SERIAL_clear_buffer_checked()) return false;
        asic->resetAfterPowerCycle();
        asic->setRecoveryGuard(guard, this);
        const bool initialized = controlledBoard->initAsics();
        asic->setRecoveryGuard(nullptr, nullptr);
        if (!initialized || !guard(this)) return false;
        if (expectedAsicCount == 0 || controlledBoard->getDetectedAsicCount() != static_cast<int>(expectedAsicCount)) {
            m_startError = expectedAsicCount == 8 ? "ASIC chain did not report eight chips; restart required" :
                                                  "ASIC chain did not report four chips; restart required";
            return false;
        }
        if (!asic->transportOkay()) {
            m_startError = "ASIC UART initialization write failed; restart required";
            return false;
        }
        const float measuredVout = controlledBoard->getVout();
        const float savedVolts = controlledBoard->getAsicVoltageMillis() / 1000.0f;
        if (!std::isfinite(measuredVout) || savedVolts <= 0 || std::fabs(measuredVout - savedVolts) > 0.03f) {
            m_startError = "ASIC voltage readback did not match saved voltage; restart required";
            return false;
        }
        // Keep work blocked until the final UART transmission has completed.
        return SERIAL_wait_tx_idle(500);
    }
    const char *startError() override { return m_startError; }
    bool reconfigure() override {
        m_startError = "ASIC frequency update/protection failed; restart required";
        if (!controlledBoard || !guard(this)) return false;
        Asic *asic = controlledBoard->getAsicDriver();
        if (!asic) return false;
        asic->setRecoveryGuard(guard, this);
        const bool updated = controlledBoard->setAsicFrequency(targetFrequency);
        asic->setRecoveryGuard(nullptr, nullptr);
        return updated && asic->transportOkay() && guard(this);
    }
    void publish(const ControlReport &report) override {
        pthread_mutex_lock(&reportMutex);
        published = report;
        pthread_mutex_unlock(&reportMutex);
    }
};
Backend backend;
}

bool initializeControl(Board *board, bool canSlave) {
    controlledBoard = board;
    // The driver profile owns regulator sequencing. Admit only its exact
    // model/ASIC pair, then require that complete chain on every start/resume.
    expectedAsicCount = board ? expectedStandaloneChain(board->getDeviceModel(), board->getAsicModel()) : 0;
    const bool supported = expectedAsicCount != 0 && board->getAsicCount() == expectedAsicCount && board->getAsicDriver() &&
        board->supportsMiningPowerControl() && !canSlave && !Config::isCanEnabled();
    qualified.store(supported);
    if (supported) gate.block();
    return supported;
}
bool controlSupported() { return qualified.load(); }
void setRuntimeReady(bool ready) { runtimeReady.store(ready); runtimeObserved.store(true); }
void notifyJobWorkerReady(bool ready) { jobWorkerReady.store(ready); jobWorkerObserved.store(true); }
void updateControl() {
    if (!controlSupported()) return;
    updateSchedule();
    powerState.reconcile(backend);
}
bool miningWritesAllowed() {
    return !controlSupported() || (gate.isOpen() && !requestedPaused());
}
bool applyFrequency(uint16_t frequency) {
    if (!controlSupported()) return false;
    backend.targetFrequency = frequency;
    return powerState.reconfigure(backend);
}
uint64_t workGeneration() { return gate.generation(); }
OperationGate &operationGate() { return gate; }

bool writeControlReport(JsonDocument &document) {
    const bool supported = controlSupported();
    document["supported"] = supported;
    if (!supported) return !document.overflowed();
    pthread_mutex_lock(&reportMutex);
    const auto report = published;
    pthread_mutex_unlock(&reportMutex);
    const auto input = backend.inputs();
    const bool desired = input.requestedPaused || !input.runtimeReady || input.fault || report.faultLatched;
    auto status = document["status"].as<JsonObject>();
    if (status.isNull()) status = document["status"].to<JsonObject>();
    status["requestedPaused"] = desired;
    status["appliedPaused"] = report.appliedPaused;
    status["transitionPending"] = report.transitionPending || desired != report.appliedPaused;
    const char *error = report.error;
    if (input.fault) error = "Hardware protection latched; restart required";
    else if (hasScheduleError()) error = "Invalid stored mining schedule; correct before resume";
    else if (!input.runtimeReady) {
        if (jobWorkerObserved.load() && !jobWorkerReady.load()) error = "Mining job timer unavailable; restart required";
        else if (runtimeReady.load() && !jobWorkerReady.load()) error = "Mining job timer starting";
        else error = runtimeObserved.load() ? "Pool not configured or mining workers unavailable; restart required" : "Mining runtime starting";
    }
    if (error) status["error"] = error;
    else status["error"] = nullptr;
    return !document.overflowed();
}
} // namespace FiveTratumMining
