#include "runtime.h"
#include "core.h"
#include "write_gate.h"
#include "storage_context.h"
#include "presentation.h"
#include "events.h"
#include "recovery_guard.h"
#include "global_state.h"
#include "tasks/mining_control.h"
#include "tasks/mining_schedule.h"
#include "tasks/mining_board_policy.h"
#include "http_server/handler_capabilities.h"
#include "nvs_config.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_app_desc.h"
#include "freertos/task.h"
#include "http_server/http_cors.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <algorithm>
#include <string>
#include <utility>
#include "http_server/http_utils.h"
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace FiveTratumAutotune {
namespace {
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
struct Guard { Guard() { pthread_mutex_lock(&mutex); } ~Guard() { pthread_mutex_unlock(&mutex); } };
fwt_controller controller{};
fwt_history history{};
fwt_input observed{};
uint64_t lastOwnerTickUs=0;
TuningSensors sensors{};
bool initialized = false, optionsError = false, backupAvailable = false, backupRestored = false;
enum class BlobStatus { Unloaded, Missing, Present, Invalid, ReadFailure };
bool storagePrepared=false,powerInitialized=false,baselineLoadError=false;
BlobStatus optionsStatus=BlobStatus::Unloaded,baselineStatus=BlobStatus::Unloaded;
JsonDocument bootOptions;
bool restoring = false, restoreSaved = false, restoreCommitFailed = false;
bool recoveryRequested=false,recoveryAutomatic=false;
const char *recoveryPhase="idle",*recoveryReason="No recovery requested";
uint64_t recoveryPolicy=0,recoveryStarted=0,recoveryLastSample=0;
unsigned recoverySamples=0;
std::atomic_uint recoveryStartups{0};
TaskHandle_t restoreWorker = nullptr;
constexpr uint32_t restoreStackBytes=8192;
uint32_t restoreMinFreeBytes=UINT32_MAX;
bool restoreStackLow=false;
void restoreWorkerLoop(void *);
bool ensureRestoreWorker() {
    if (restoreWorker) return true;
    TaskHandle_t created=nullptr;
    if (!storageContextAllowed() || xTaskCreate(restoreWorkerLoop,"tune_restore",restoreStackBytes,nullptr,8,&created)!=pdPASS || !created) return false;
    restoreWorker=created;return true;
}
bool pending = false;
unsigned voltageMatches = 0;
uint64_t transitionStarted = 0, mutationRevision = 0;
uint64_t transitionHealthUntilUs = 0;
fwt_point target{};
JsonDocument backup;
const char *adapterReason = nullptr;
const char *observationReason = nullptr;
std::atomic<const char *> handoff{nullptr};
std::atomic_uint firmwareUpdates{0};
std::atomic_bool permanentFreeze{false};
constexpr size_t maxBlob = 4096;
constexpr int waiverVersion = 1;

bool exactFamily(Board *board) {
    if (!board || strcmp(board->getAsicModel(),"BM1370") || !FiveTratumMining::controlSupported() || board->isCanSlave() || Config::isCanEnabled()) return false;
    const unsigned count = FiveTratumMining::expectedStandaloneChain(board->getDeviceModel(), board->getAsicModel());
    return (count == 4 || count == 8) && board->getAsicCount() == static_cast<int>(count);
}
fwt_limits hardwareLimits(Board *board) {
    fwt_limits limits{};
    if (!exactFamily(board)) return limits;
    limits.minFrequencyMHz = 100;
    limits.maxFrequencyMHz = board->getAbsMaxAsicFrequency();
    limits.minVoltageMv = board->getAbsMinAsicVoltageMillis() > 0 ? board->getAbsMinAsicVoltageMillis() : 1005;
    limits.maxVoltageMv = board->getAbsMaxAsicVoltageMillis();
    const double asicProtection = POWER_MANAGEMENT_MODULE.getFanController().getOverheatTemp(0);
    const double vrmProtection = POWER_MANAGEMENT_MODULE.getFanController().getOverheatTemp(1);
    // A disabled protection channel does not become an unlimited tuner ceiling.
    limits.maxAsicTempC = asicProtection >= 40 ? std::min(asicProtection - 5.0, 80.0) : 0;
    limits.maxVrmTempC = vrmProtection >= 40 ? std::min(vrmProtection - 5.0, 95.0) : 0;
    limits.maxPowerW = board->getMaxPin();
    limits.frequencyStepMHz = 10; // Existing Nerd API targets integer MHz; PLL driver resolves the actual command.
    limits.voltageStepMv = 5;
    return limits;
}
void writeConfig(JsonObject out, const fwt_config &config) {
    out["priority"] = fwt_priority_name(config.priority); out["continuous"] = config.continuous;
    out["minFrequencyMHz"] = config.minFrequencyMHz; out["maxFrequencyMHz"] = config.maxFrequencyMHz;
    out["minVoltageMv"] = config.minVoltageMv; out["maxVoltageMv"] = config.maxVoltageMv;
    out["maxAsicTempC"] = config.maxAsicTempC; out["maxVrmTempC"] = config.maxVrmTempC; out["maxPowerW"] = config.maxPowerW;
}
bool numeric(JsonObjectConst object, const char *key, double &out) {
    const auto value = object[key];
    if (value.is<bool>() || !value.is<double>()) return false;
    out = value.as<double>(); return std::isfinite(out);
}
bool parseConfig(JsonObjectConst object, fwt_config &config) {
    if (object.size() != 9 || !object["priority"].is<const char *>() || !object["continuous"].is<bool>()) return false;
    const char *priority = object["priority"];
    if (!strcmp(priority, "efficiency")) config.priority = FWT_EFFICIENCY;
    else if (!strcmp(priority, "balanced")) config.priority = FWT_BALANCED;
    else if (!strcmp(priority, "performance")) config.priority = FWT_PERFORMANCE;
    else return false;
    config.continuous = object["continuous"];
    if (!numeric(object,"minFrequencyMHz",config.minFrequencyMHz) || !numeric(object,"maxFrequencyMHz",config.maxFrequencyMHz) ||
        !numeric(object,"minVoltageMv",config.minVoltageMv) || !numeric(object,"maxVoltageMv",config.maxVoltageMv) ||
        !numeric(object,"maxAsicTempC",config.maxAsicTempC) || !numeric(object,"maxVrmTempC",config.maxVrmTempC) ||
        !numeric(object,"maxPowerW",config.maxPowerW)) return false;
    return std::floor(config.minFrequencyMHz) == config.minFrequencyMHz && std::floor(config.maxFrequencyMHz) == config.maxFrequencyMHz &&
           std::floor(config.minVoltageMv) == config.minVoltageMv && std::floor(config.maxVoltageMv) == config.maxVoltageMv;
}
BlobStatus readBlobState(const char *key, JsonDocument &document) {
    if (!storageContextAllowed()) return BlobStatus::ReadFailure;
    document.clear();
    nvs_handle_t handle; esp_err_t err = nvs_open("fwtune", NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) return BlobStatus::Missing;
    if (err != ESP_OK) return BlobStatus::ReadFailure;
    size_t length = 0; err = nvs_get_blob(handle, key, nullptr, &length);
    if (err == ESP_ERR_NVS_NOT_FOUND) { nvs_close(handle); return BlobStatus::Missing; }
    if (err != ESP_OK) {nvs_close(handle);return BlobStatus::ReadFailure;}
    if (!length || length > maxBlob) { nvs_close(handle); return BlobStatus::Invalid; }
    std::unique_ptr<char[]> data(new (std::nothrow) char[length]);
    if (!data) { nvs_close(handle); return BlobStatus::ReadFailure; }
    err = nvs_get_blob(handle,key,data.get(),&length); nvs_close(handle);
    if (err!=ESP_OK) return BlobStatus::ReadFailure;
    const auto parsed=deserializeJson(document,static_cast<const char *>(data.get()),length,DeserializationOption::NestingLimit(4));
    if (parsed==DeserializationError::NoMemory) return BlobStatus::ReadFailure;
    return parsed ? BlobStatus::Invalid : BlobStatus::Present;
}
bool readBlob(const char *key,JsonDocument &document,bool &missing) {
    const auto status=readBlobState(key,document);missing=status==BlobStatus::Missing;
    return missing || status==BlobStatus::Present;
}
bool writeVerified(const char *key, JsonDocument &document) {
    if (!storageContextAllowed()) return false;
    const size_t length = measureJson(document);
    if (document.overflowed() || !length || length > maxBlob) return false;
    std::unique_ptr<char[]> data(new (std::nothrow) char[length + 1]);
    if (!data || serializeJson(document,data.get(),length+1) != length) return false;
    nvs_handle_t handle; esp_err_t err = nvs_open("fwtune",NVS_READWRITE,&handle);
    if (err != ESP_OK) return false;
    err = nvs_set_blob(handle,key,data.get(),length);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) return false;
    JsonDocument verified; bool missing = false;
    if (!readBlob(key,verified,missing) || missing || measureJson(verified) != length) return false;
    std::unique_ptr<char[]> compare(new (std::nothrow) char[length+1]);
    return compare && serializeJson(verified,compare.get(),length+1) == length && !memcmp(data.get(),compare.get(),length);
}
bool captureEffective(Board *board, JsonObject out) {
    if (!board || !Config::captureTuningSettings(out)) return false;
    out[NVS_CONFIG_ASIC_FREQ] = board->getAsicFrequency();
    out[NVS_CONFIG_ASIC_VOLTAGE] = board->getAsicVoltageMillis();
    out[NVS_CONFIG_INVERT_FAN_POLARITY] = Config::cfgGetU16(NVS_CONFIG_INVERT_FAN_POLARITY,0);
    out[NVS_CONFIG_AUTO_FAN_POLARITY] = Config::cfgGetU16(NVS_CONFIG_AUTO_FAN_POLARITY,0);
    out[NVS_CONFIG_FAN_PWM_POLARITY] = board->isInvertFanPolarityEnabled() ? 1 : 0;
    out[NVS_CONFIG_AUTO_FAN_SPEED] = board->getFanMode(0);
    out[NVS_CONFIG_FAN1_MODE] = board->getFanMode(1);
    out[NVS_CONFIG_FAN_SPEED] = Config::getFanManualSpeed(0);
    out[NVS_CONFIG_FAN1_SPEED] = Config::getFanManualSpeed(1);
    out[NVS_CONFIG_OVERHEAT_TEMP] = POWER_MANAGEMENT_MODULE.getFanController().getOverheatTemp(0);
    out[NVS_CONFIG_FAN1_OVERHEAT] = POWER_MANAGEMENT_MODULE.getFanController().getOverheatTemp(1);
    const auto *a = board->getPidSettings(0); const auto *b = board->getPidSettings(1);
    if (!a || !b) return false;
    out[NVS_CONFIG_PID_TARGET_TEMP] = a->targetTemp; out[NVS_CONFIG_PID_P]=a->p; out[NVS_CONFIG_PID_I]=a->i; out[NVS_CONFIG_PID_D]=a->d;
    out[NVS_CONFIG_FAN1_PID_TEMP] = b->targetTemp; out[NVS_CONFIG_FAN1_PID_P]=b->p; out[NVS_CONFIG_FAN1_PID_I]=b->i; out[NVS_CONFIG_FAN1_PID_D]=b->d;
    out[NVS_CONFIG_FAN_PID_USE_MAX] = Config::isFanPidUseMax() ? 1 : 0;
    return true;
}
bool validEffectiveSettings(Board *board, JsonObjectConst values) {
    if (!board || !Config::validTuningSettings(values)) return false;
    const double frequency=values[NVS_CONFIG_ASIC_FREQ].as<unsigned>();
    const double voltage=values[NVS_CONFIG_ASIC_VOLTAGE].as<unsigned>();
    if (frequency<100 || frequency>board->getAbsMaxAsicFrequency() || voltage<controller.limits.minVoltageMv || voltage>board->getAbsMaxAsicVoltageMillis()) return false;
    for (const char *key : {NVS_CONFIG_INVERT_FAN_POLARITY,NVS_CONFIG_AUTO_FAN_POLARITY,NVS_CONFIG_FAN_PWM_POLARITY,NVS_CONFIG_FAN_PID_USE_MAX}) if (values[key].as<unsigned>()>1) return false;
    for (const char *key : {NVS_CONFIG_FAN_SPEED,NVS_CONFIG_FAN1_SPEED}) if (values[key].as<unsigned>()>100) return false;
    const unsigned first=values[NVS_CONFIG_AUTO_FAN_SPEED],second=values[NVS_CONFIG_FAN1_MODE];
    if ((first!=0 && first!=2) || (second!=0 && second!=2 && second!=3)) return false;
    for (const char *key : {NVS_CONFIG_OVERHEAT_TEMP,NVS_CONFIG_FAN1_OVERHEAT,NVS_CONFIG_PID_TARGET_TEMP,NVS_CONFIG_FAN1_PID_TEMP}) {
        const unsigned value=values[key]; if (value<20 || value>125) return false;
    }
    return true;
}
bool effectiveMatch(Board *board) {
    JsonDocument current;
    if (!backupAvailable || !captureEffective(board,current.to<JsonObject>())) return false;
    JsonObjectConst original=backup["settings"].as<JsonObjectConst>();
    if (current.size()!=original.size()) return false;
    for (JsonPairConst value : original) if (current[value.key()] != value.value()) return false;
    return true;
}
bool backupMatches(Board *board, JsonDocument &document) {
    char identity[39];
    return document.size()==7 && board && readFiveTratumPhysicalDeviceId(identity) && document["schemaVersion"].is<int>() &&
        document["schemaVersion"].as<int>() == 1 && document["deviceId"].is<const char *>() &&
        !strcmp(document["deviceId"],identity) && document["model"].is<const char *>() &&
        !strcmp(document["model"],board->getDeviceModel()) && document["asicCount"].is<int>() &&
        document["asicCount"].as<int>() == board->getAsicCount() && document["settings"].is<JsonObject>() && validEffectiveSettings(board,document["settings"].as<JsonObjectConst>()) &&
        document["frequencyMHz"].is<uint16_t>() && document["frequencyMHz"].as<uint16_t>() > 0 &&
        document["coreVoltageMv"].is<uint16_t>() && document["coreVoltageMv"].as<uint16_t>() > 0 &&
        document["frequencyMHz"].as<uint16_t>() == document["settings"][NVS_CONFIG_ASIC_FREQ].as<uint16_t>() &&
        document["coreVoltageMv"].as<uint16_t>() == document["settings"][NVS_CONFIG_ASIC_VOLTAGE].as<uint16_t>();
}
void initialize(Board *board) {
    if (initialized || !storagePrepared || !powerInitialized || !exactFamily(board)) return;
    const fwt_limits limits = hardwareLimits(board);
    fwt_config config{};
    config.priority = FWT_BALANCED; config.continuous = false;
    config.maxFrequencyMHz = board->getAsicFrequency();
    config.minFrequencyMHz = std::max(limits.minFrequencyMHz, config.maxFrequencyMHz * 0.7);
    config.minFrequencyMHz = std::ceil(config.minFrequencyMHz);
    config.maxVoltageMv = board->getAsicVoltageMillis();
    config.minVoltageMv = std::max(limits.minVoltageMv, config.maxVoltageMv - 100);
    config.maxAsicTempC = std::min(limits.maxAsicTempC, 60.0);
    config.maxVrmTempC = std::min(limits.maxVrmTempC, 70.0);
    config.maxPowerW = limits.maxPowerW;
    optionsError=optionsStatus==BlobStatus::Invalid || optionsStatus==BlobStatus::ReadFailure;
    if (optionsStatus==BlobStatus::Present) {
        fwt_config proposed{};
        if (!parseConfig(bootOptions.as<JsonObjectConst>(),proposed) || !fwt_config_valid(&proposed,&limits)) optionsError=true;
        else config=proposed;
    }
    fwt_init(&controller,&limits,&config);
    baselineLoadError=baselineStatus==BlobStatus::Invalid || baselineStatus==BlobStatus::ReadFailure;
    if (baselineLoadError) {adapterReason="Original tuning backup is unreadable";}
    else if (baselineStatus==BlobStatus::Present) {
        backupAvailable = backupMatches(board,backup);
        if (!backupAvailable) {baselineLoadError=true;adapterReason = "Original tuning backup belongs to different hardware or is invalid";}
    }
    mutationRevision = Config::tuningMutationRevision();
    initialized = true;
}
bool ensureBackup(Board *board) {
    JsonDocument existing; bool missing=false;
    if (!readBlob("baseline",existing,missing)) return false;
    if (backupAvailable) {
        // Every Start verifies the same immutable original from durable storage.
        if (missing || !backupMatches(board,existing)) return false;
        std::string cached,stored;serializeJson(backup,cached);serializeJson(existing,stored);
        if (cached!=stored) return false;
        const bool verified=writeVerified("baseline",existing);
        if (verified) baselineStatus=BlobStatus::Present;
        return verified;
    }
    if (!missing) {
        if (!backupMatches(board,existing) || !writeVerified("baseline",existing)) return false;
        if (!backup.set(existing)) return false;
        backupAvailable=true;backupRestored=false;baselineStatus=BlobStatus::Present;return true;
    }
    char identity[39]; if (!readFiveTratumPhysicalDeviceId(identity)) return false;
    JsonDocument document;
    document["schemaVersion"] = 1; document["deviceId"] = identity; document["model"] = board->getDeviceModel();
    document["asicCount"] = board->getAsicCount();
    document["frequencyMHz"] = board->getAsicFrequency(); document["coreVoltageMv"] = board->getAsicVoltageMillis();
    if (!captureEffective(board,document["settings"].to<JsonObject>()) || !backupMatches(board,document) || !writeVerified("baseline",document)) return false;
    bool absent = false;
    if (!readBlob("baseline",backup,absent) || absent || !backupMatches(board,backup)) return false;
    backupAvailable = true; backupRestored = false; baselineStatus=BlobStatus::Present; return true;
}
void acquire(Board *board) {
    observed = {}; sensors = {}; observationReason = nullptr;
    const uint64_t acquisitionStarted=esp_timer_get_time();
    observed.current = {static_cast<double>(POWER_MANAGEMENT_MODULE.getAppliedTuningFrequency()),
                        static_cast<double>(POWER_MANAGEMENT_MODULE.getAppliedTuningVoltage())};
    observed.generation = FiveTratumMining::workGeneration();
    observed.fault = !board || POWER_MANAGEMENT_MODULE.isShutdown() || board->isShutdown() || SYSTEM_MODULE.getBoardError() != Board::Error::NONE;
    observed.miningActive = exactFamily(board) && board->isInitialized() && board->isBuckInitialized() &&
        FiveTratumMining::miningWritesAllowed() && !observed.fault && STRATUM_MANAGER &&
        STRATUM_MANAGER->isAnyConnected();
    observed.fanOk = true;
    if (board) for (int i=0;i<board->getNumFans();++i) {
        uint16_t rpm = 0;
        if (!board->readTuningFan(i,rpm) || rpm < 500 || rpm > 16000) observed.fanOk = false;
    }
    if (!observed.miningActive || !board->readTuningSensors(sensors)) return;
    observed.asicTempC = sensors.asicTempC; observed.vrmTempC = sensors.vrmTempC;
    observed.powerW = sensors.powerW; observed.voltageReadbackMv = sensors.coreVoltageMv;
    const uint64_t now = esp_timer_get_time();
    FiveTratumTelemetry::TuningErrorTelemetry counters;
    if (!HASHRATE_MONITOR.copyTuningErrorTelemetry(counters,board->getAsicCount(),now,true)) return;
    if (counters.asicCount != board->getAsicCount() || !counters.errorPercentageAvailable) return;
    uint64_t counterAt = now;
    auto *asics = board->getAsics();
    const double expectedChip = asics ? observed.current.frequencyMHz * asics->getSmallCoreCount() / 1000.0 : 0;
    if (!std::isfinite(expectedChip) || expectedChip <= 0) return;
    for (int i=0;i<counters.asicCount;++i) {
        const auto &chip = counters.chips[i];
        if (!chip.errorSampleFresh || !chip.errorPercentageAvailable || chip.hashRate <= 0) return;
        if (chip.hashRate < expectedChip * 0.20) { observationReason = "An ASIC reports too little work for reliable tuning"; return; }
        observed.hashRateGHs += chip.hashRate;
        observed.worstErrorPercentage = std::max(observed.worstErrorPercentage, static_cast<double>(chip.errorPercentage));
        if (chip.sampleCapturedAtUs <= 0) return;
        counterAt = std::min(counterAt,static_cast<uint64_t>(chip.sampleCapturedAtUs));
    }
    observed.capturedAtUs = std::min(acquisitionStarted,std::min(counterAt,static_cast<uint64_t>(sensors.capturedAtUs)));
    observed.sampleId = counterAt;
    observed.errorPercentage = counters.errorPercentage;
    observed.valid = observed.fanOk && sensors.vin >= board->getMinVin() && sensors.vin <= board->getMaxVin() &&
        sensors.outputCurrentA > 0 && sensors.coreVoltageMv > 0 && observed.capturedAtUs > 0;
}
bool revisionCurrent() { return !handoff.load() && !firmwareUpdates.load() && !permanentFreeze.load() && Config::tuningMutationRevision() == mutationRevision && (!recoveryRequested || FiveTratumMining::policyRevision()==recoveryPolicy); }
void failTransition(const char *reason) {
    pending = false; fwt_applied(&controller,false,esp_timer_get_time()); fwt_history_capture(&history,&controller);
    adapterReason = reason;
    // A failed/unconfirmed rail transition cannot be healed by clearing faults.
    SYSTEM_MODULE.setBoardError(Board::Error::VOUT_FAULT,0);
    FiveTratumMining::manualOverride(true);
    FiveTratumMining::updateControl();
    observed.valid=false; observed.miningActive=false; observed.fault=true;
}
bool safeToRestore() {
    const double backupAsic = backup["settings"][NVS_CONFIG_OVERHEAT_TEMP].as<double>() - 5.0;
    const double backupVrm = backup["settings"][NVS_CONFIG_FAN1_OVERHEAT].as<double>() - 5.0;
    // A disabled current channel is absent, not a zero-Celsius barrier to
    // restoring the original valid protection. Start still denies that state.
    const double asicLimit = std::min(80.0,controller.limits.maxAsicTempC > 0 ? controller.limits.maxAsicTempC : 80.0);
    const double vrmLimit = std::min(95.0,controller.limits.maxVrmTempC > 0 ? controller.limits.maxVrmTempC : 95.0);
    return backupAvailable && observed.valid && observed.miningActive && !observed.fault && observed.fanOk &&
        observed.asicTempC + 3 < std::min(asicLimit,backupAsic) &&
        observed.vrmTempC + 3 < std::min(vrmLimit,backupVrm);
}
void advanceTransition(uint64_t now) {
    if (!revisionCurrent()) return;
    if (!observed.miningActive || observed.fault || !observed.fanOk) { failTransition("Tuning transition interrupted by miner protection"); return; }
    if (now < transitionStarted || now-transitionStarted > 30000000) { failTransition("Tuning voltage readback did not settle; restart required"); return; }
    if (!sensors.capturedAtUs || !std::isfinite(sensors.coreVoltageMv)) return;
    auto &power = POWER_MANAGEMENT_MODULE;
    const uint16_t frequency = static_cast<uint16_t>(target.frequencyMHz);
    const uint16_t voltage = static_cast<uint16_t>(target.coreVoltageMv);
    const bool upward = frequency > power.getAppliedTuningFrequency() || voltage > power.getAppliedTuningVoltage();
    const double asicLimit = restoring ? controller.limits.maxAsicTempC : controller.config.maxAsicTempC;
    const double vrmLimit = restoring ? controller.limits.maxVrmTempC : controller.config.maxVrmTempC;
    const double powerLimit = restoring ? controller.limits.maxPowerW : controller.config.maxPowerW;
    if (upward && (sensors.asicTempC + 2 >= asicLimit || sensors.vrmTempC + 2 >= vrmLimit || sensors.powerW >= powerLimit)) {
        pending = false;
        if (!restoring) {
            fwt_cancel(&controller,"Tuning headroom changed during transition; miner paused");
            controller.state = FWT_SUSPENDED;
        }
        adapterReason = "Temperature or power headroom changed; restore waits for cooling and an explicit resume";
        FiveTratumMining::manualOverride(true);
        FiveTratumMining::updateControl();
        observed.valid=false; observed.miningActive=false;
        return;
    }
    if (frequency < power.getAppliedTuningFrequency()) {
        if (!revisionCurrent() || !power.commandTuningFrequency(frequency)) { failTransition("Tuning downclock failed"); return; }
        voltageMatches = 0;
    }
    if (voltage != power.getAppliedTuningVoltage()) {
        if (!revisionCurrent() || !power.commandTuningVoltage(voltage)) { failTransition("Tuning voltage command failed"); return; }
        voltageMatches = 0; return;
    }
    if (std::fabs(sensors.coreVoltageMv - voltage) > 25.0) { voltageMatches = 0; return; }
    if (++voltageMatches < 2) return;
    if (frequency != power.getAppliedTuningFrequency()) {
        if (!revisionCurrent() || !power.commandTuningFrequency(frequency)) { failTransition("Tuning clock transition failed"); return; }
        voltageMatches = 0; return;
    }
    pending = false; fwt_applied(&controller,true,now); fwt_history_capture(&history,&controller);
    controller.generation = FiveTratumMining::workGeneration();
    if (restoring) {
        restoring = false; backupRestored = restoreSaved;
        fwt_cancel(&controller,"Original settings restored"); adapterReason = nullptr;
    }
}
bool hardRecoveryFault(Board *board) {
    return !board||!exactFamily(board)||board->isShutdown()||POWER_MANAGEMENT_MODULE.isShutdown()||SYSTEM_MODULE.getBoardError()!=Board::Error::NONE;
}
void blockRestore(const char *reason) {
    restoreCommitFailed=true;adapterReason=reason;
    if(recoveryRequested){recoveryRequested=false;recoveryPhase="blocked";recoveryReason=reason;}
}
bool recoverySensorsSafe(Board *board,bool requireOff) {
    if(hardRecoveryFault(board)||!revisionCurrent()||(requireOff&&!board->isRecoveryPowerOff())) return false;
    TuningSensors sample{};if(!board->readRecoverySensors(sample)) return false;
    const uint64_t now=esp_timer_get_time();
    if(recoveryAutomatic && !(sample.asicTempC+5<controller.config.maxAsicTempC && sample.vrmTempC+5<controller.config.maxVrmTempC)) return false;
    const double asic=std::min(std::min(80.0,controller.limits.maxAsicTempC>0?controller.limits.maxAsicTempC:80.0),backup["settings"][NVS_CONFIG_OVERHEAT_TEMP].as<double>()-5);
    const double vrm=std::min(std::min(95.0,controller.limits.maxVrmTempC>0?controller.limits.maxVrmTempC:95.0),backup["settings"][NVS_CONFIG_FAN1_OVERHEAT].as<double>()-5);
    return sample.capturedAtUs>0&&static_cast<uint64_t>(sample.capturedAtUs)<=now&&now-sample.capturedAtUs<=5000000&&
        std::isfinite(sample.vin)&&sample.vin>=board->getMinVin()&&sample.vin<=board->getMaxVin()&&
        std::isfinite(sample.asicTempC)&&sample.asicTempC>0&&sample.asicTempC+3<asic&&
        std::isfinite(sample.vrmTempC)&&sample.vrmTempC>0&&sample.vrmTempC+3<vrm;
}
// NVS writes require an internal-RAM task stack. The ordinary power task uses
// PSRAM; this bounded worker borrows the same hardware owner mutex, revalidates
// current strict inputs, commits once, then returns actuation to the power task.
bool recordWorkerBudget() {
    // ESP-IDF reports this value in bytes, unlike upstream FreeRTOS ports.
    const uint32_t freeBytes=uxTaskGetStackHighWaterMark(nullptr);
    restoreMinFreeBytes=std::min(restoreMinFreeBytes,freeBytes);
    if (freeBytes<1024) {
        restoreStackLow=true;blockRestore("Original settings restore needs a larger verified stack budget");return false;
    }
    return true;
}
void restoreWorkerLoop(void *) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
        LockGuard<PowerManagementTask> owner(POWER_MANAGEMENT_MODULE);
        Guard guard;
        if (!restoring || restoreSaved || restoreCommitFailed || !revisionCurrent()) continue;
        if (!storageContextAllowed()) {blockRestore("Restore storage requires an internal task stack");continue;}
        if (!recordWorkerBudget()) continue;
        Board *board=SYSTEM_MODULE.getBoard();
        if(recoveryRequested) {
            if(FiveTratumMining::policyRevision()!=recoveryPolicy){restoring=false;recoveryRequested=false;recoveryPhase="blocked";recoveryReason="Power policy changed";continue;}
            if(hardRecoveryFault(board)){blockRestore("Hardware protection blocks recovery; resolve it and retry");continue;}
            if(!board->isRecoveryPowerOff()){blockRestore("Recovery requires the ASIC power to remain off");continue;}
            if(!recoverySensorsSafe(board,true)){recoveryPhase="validating-off";recoveryReason="Waiting for fresh, cool sensors and valid input power";continue;}
            if(!ensureBackup(board)){blockRestore("Original backup could not be verified; retry recovery");continue;}
            recoveryPhase="restoring";recoveryReason="Saving verified original settings while ASIC power is off";
        } else {
            acquire(board);
            if (!recordWorkerBudget()) continue;
            if (!safeToRestore()) {adapterReason="Original settings restore waits for a healthy, cool miner";continue;}
        }
        const uint64_t before=Config::tuningMutationRevision();
        if (!Config::restoreTuningSettings(backup["settings"].as<JsonObjectConst>())) {
            blockRestore("Original settings restore could not be saved and read back; retry restore");continue;
        }
        mutationRevision=before+1; // Only our one internal transaction is authorized.
        if (!revisionCurrent()) {
            restoring=false;fwt_cancel(&controller,"Manual settings took ownership during restore");continue;
        }
        if (!recordWorkerBudget()) continue;
        board->loadSettings();
        if (!revisionCurrent()) {restoring=false;fwt_cancel(&controller,"Manual settings took ownership during restore");continue;}
        if (!board->restoreTuningFanPolarity(board->isInvertFanPolarityEnabled(),revisionCurrent)) {
            blockRestore("Original fan polarity restore interrupted; retry restore");continue;
        }
        POWER_MANAGEMENT_MODULE.getFanController().loadSettings();
        if (!recordWorkerBudget()) continue;
        if (!revisionCurrent()) {restoring=false;fwt_cancel(&controller,"Manual settings took ownership during restore");continue;}
        controller.limits=hardwareLimits(board);
        restoreSaved=true;adapterReason="Original settings saved; waiting for protected live restore";
        if(recoveryRequested){recoveryPhase="resuming";recoveryReason="Original settings verified; waiting for guarded normal resume";}
    }
}
void pointJson(JsonObject parent,const char *key,const fwt_point *point) {
    // Converting an absent member proxy to JsonVariant produces an unbound
    // variant. Insert through its parent so the wire schema always has keys.
    if (!point) { parent[key]=nullptr; return; }
    auto out=parent[key].to<JsonObject>();
    out["frequencyMHz"] = point->frequencyMHz; out["coreVoltageMv"] = point->coreVoltageMv;
}
const char *cachedReason() {
    return adapterReason ? adapterReason : !storagePrepared ? "Tuning storage has not been preloaded on an internal boot stack" :
        !powerInitialized ? "Waiting for power and fan initialization" : optionsError ? "Stored tuning options are invalid or unreadable" :
        observationReason ? observationReason : controller.reason ? controller.reason : "Tuning disabled";
}
bool presentationObscured() {
    return restoring || pending || firmwareUpdates.load() || permanentFreeze.load() ||
           handoff.load()!=nullptr || adapterReason!=nullptr;
}
void eventsJson(JsonObject status) {
    fwt_history_capture(&history,&controller);
    status["eventCapacity"]=FWT_EVENT_CAPACITY;
    auto events=status["events"].to<JsonArray>();
    for(unsigned index=0;index<fwt_history_count(&history);++index) {
        const auto *event=fwt_history_at(&history,index);
        if(!event || !event->id) continue;
        auto value=events.add<JsonObject>();
        value["id"]=event->id;value["atSeconds"]=event->atUs/(double)FWT_SECOND;
        value["kind"]=fwt_event_kind_name(event->kind);value["outcome"]=fwt_event_outcome_name(event->outcome);
        value["reason"]=event->reason ? event->reason : "unavailable";
        pointJson(value,"from",fwt_positive(event->from.frequencyMHz)&&fwt_positive(event->from.coreVoltageMv)?&event->from:nullptr);
        pointJson(value,"point",fwt_positive(event->point.frequencyMHz)&&fwt_positive(event->point.coreVoltageMv)?&event->point:nullptr);
        if(event->hasMeasurement && fwt_positive(event->hashRateGHs) && fwt_positive(event->powerW) &&
           std::isfinite(event->errorPercentage) && event->errorPercentage>=0 && event->errorPercentage<=100 &&
           std::isfinite(event->worstErrorPercentage) && event->worstErrorPercentage>=0 && event->worstErrorPercentage<=100) {
            auto measured=value["measurement"].to<JsonObject>();
            measured["hashRateGHs"]=event->hashRateGHs;measured["powerW"]=event->powerW;
            measured["errorPercentage"]=event->errorPercentage;measured["worstErrorPercentage"]=event->worstErrorPercentage;
        } else value["measurement"]=nullptr;
        if(event->hasConditions) {
            auto conditions=value["conditions"].to<JsonObject>();conditions["asicTempC"]=event->asicTempC;conditions["vrmTempC"]=event->vrmTempC;
            conditions["powerW"]=event->powerW;conditions["hashRateGHs"]=event->hashRateGHs;conditions["errorPercentage"]=event->errorPercentage;conditions["worstErrorPercentage"]=event->worstErrorPercentage;
        } else value["conditions"]=nullptr;
        if(event->hasScore && std::isfinite(event->score)) value["score"]=event->score;else value["score"]=nullptr;
        if(event->hasScore && std::isfinite(event->requiredScore)) value["requiredScore"]=event->requiredScore;else value["requiredScore"]=nullptr;
    }
}
void searchJson(JsonObject status,uint64_t now) {
    const auto view=fwt_search_read(&controller,now,presentationObscured());
    auto search=status["search"].to<JsonObject>();
    if(view.timed) {search["phaseElapsedSeconds"]=view.phaseElapsedSeconds;search["minimumSeconds"]=view.minimumSeconds;}
    else {search["phaseElapsedSeconds"]=nullptr;search["minimumSeconds"]=nullptr;}
    search["samples"]=view.samples;search["minimumSamples"]=view.minimumSamples;
    search["roundExperiments"]=view.roundExperiments;search["roundExperimentLimit"]=view.roundExperimentLimit;
    search["rejectedWindows"]=view.rejectedWindows;
    for(const auto &item:{std::pair<const char *,bool>{"reference",view.referenceAvailable},{"best",view.bestAvailable}}) {
        if(!item.second) {search[item.first]=nullptr;continue;}
        auto measured=search[item.first].to<JsonObject>();
        measured["hashRateGHs"]=!strcmp(item.first,"reference")?view.referenceHash:view.bestHash;
        measured["powerW"]=!strcmp(item.first,"reference")?view.referencePower:view.bestPower;
    }
}
}

bool preserveRecoveryFaults(){return recoveryStartups.load()!=0;}
RecoveryStartupGuard::RecoveryStartupGuard(){++recoveryStartups;}
RecoveryStartupGuard::~RecoveryStartupGuard(){--recoveryStartups;}

bool preloadStorage() {
    if (!storageContextAllowed()) return false;
    Guard guard;
    if (!storagePrepared) {
        // Only raw records here. Fan/profile limits are validated later from
        // RAM after the power owner signals FanController initialization.
        optionsStatus=readBlobState("options",bootOptions);
        baselineStatus=readBlobState("baseline",backup);
        storagePrepared=true;
    }
    return (optionsStatus==BlobStatus::Missing || optionsStatus==BlobStatus::Present) &&
           (baselineStatus==BlobStatus::Missing || baselineStatus==BlobStatus::Present);
}
void powerReady() {
    Guard guard;powerInitialized=true;initialize(SYSTEM_MODULE.getBoard());
}
bool transitionAllowed() { return !firmwareUpdates.load() && !permanentFreeze.load() && ((!pending && !restoring) || revisionCurrent()); }
bool transitionHealthy() {
    if (!transitionAllowed()) return false;
    if(preserveRecoveryFaults()) return recoveryRequested && FiveTratumMining::policyRevision()==recoveryPolicy && recoverySensorsSafe(SYSTEM_MODULE.getBoard(),false);
    if (!pending) {transitionHealthUntilUs=0;return true;}
    const uint64_t now=esp_timer_get_time();
    if (transitionHealthUntilUs && now<transitionHealthUntilUs) return true;
    transitionHealthUntilUs=0;
    Board *board=SYSTEM_MODULE.getBoard();TuningSensors fresh{};
    if (!board || !board->refreshTuningTemperatures() || !board->readTuningSensors(fresh)) return false;
    const uint64_t checkedAt=esp_timer_get_time();
    if (!fresh.capturedAtUs || fresh.capturedAtUs>static_cast<int64_t>(checkedAt) ||
        checkedAt-static_cast<uint64_t>(fresh.capturedAtUs)>5000000 || fresh.vin<board->getMinVin() || fresh.vin>board->getMaxVin() ||
        fresh.outputCurrentA<=0 || std::fabs(fresh.coreVoltageMv-POWER_MANAGEMENT_MODULE.getAppliedTuningVoltage())>30 ||
        fresh.asicTempC>=controller.limits.maxAsicTempC || fresh.vrmTempC>=controller.limits.maxVrmTempC) return false;
    for (int i=0;i<board->getNumFans();++i) {
        uint16_t rpm=0;if (!board->readTuningFan(i,rpm) || rpm<500 || rpm>16000) return false;
    }
    const bool upward=target.frequencyMHz>POWER_MANAGEMENT_MODULE.getAppliedTuningFrequency() || target.coreVoltageMv>POWER_MANAGEMENT_MODULE.getAppliedTuningVoltage();
    if (upward && (fresh.asicTempC+2>=(restoring?controller.limits.maxAsicTempC:controller.config.maxAsicTempC) ||
        fresh.vrmTempC+2>=(restoring?controller.limits.maxVrmTempC:controller.config.maxVrmTempC) ||
        fresh.powerW>=(restoring?controller.limits.maxPowerW:controller.config.maxPowerW))) return false;
    if (!transitionAllowed()) return false;
    transitionHealthUntilUs=std::min(checkedAt+800000,static_cast<uint64_t>(fresh.capturedAtUs)+5000000);return true;
}
bool ownsSettings() {
    Guard guard;
    return firmwareUpdates.load() || permanentFreeze.load() || (initialized && (controller.owns || restoring || pending));
}
void externalHandoff(const char *reason) {
    WriteGuard wire;
    if (reason && (!strcmp(reason,"Restart requested") || !strcmp(reason,"Shutdown requested"))) permanentFreeze.store(true);
    handoff.store(reason ? reason : "External control took ownership");
}
FirmwareUpdateGuard::FirmwareUpdateGuard() {WriteGuard wire;++firmwareUpdates;handoff.store("Firmware update took ownership");}
FirmwareUpdateGuard::~FirmwareUpdateGuard() {WriteGuard wire;--firmwareUpdates;}
void tick() {
    Board *board = SYSTEM_MODULE.getBoard();
    Guard guard; lastOwnerTickUs=esp_timer_get_time(); initialize(board); if (!initialized) return;
    const char *external = handoff.exchange(nullptr);
    const uint64_t revision = Config::tuningMutationRevision();
    if (external || revision != mutationRevision) {
        pending = false; restoring = false; backupRestored = false;
        if(recoveryRequested){recoveryRequested=false;recoveryPhase="blocked";recoveryReason=external?external:"Manual settings changed";}
        fwt_cancel(&controller,external ? external : "Manual or scheduled settings took ownership");
        mutationRevision = revision; adapterReason = nullptr;
    }
    controller.limits = hardwareLimits(board);
    acquire(board);
    const uint64_t now = esp_timer_get_time();
    if(recoveryRequested) {
        if(FiveTratumMining::policyRevision()!=recoveryPolicy) {recoveryRequested=false;restoring=false;recoveryPhase="blocked";recoveryReason="Power policy changed";return;}
        if(hardRecoveryFault(board)){blockRestore("Hardware protection blocks recovery; resolve it and retry");return;}
        if(!restoreSaved){if(restoreWorker)xTaskNotifyGive(restoreWorker);return;}
        if(!strcmp(recoveryPhase,"resuming")) {
            if(!recoverySensorsSafe(board,true)){recoveryReason="Waiting for fresh, cool sensors before resume";return;}
            uint64_t next=0;
            if(!FiveTratumMining::resumeForRecovery(recoveryPolicy,next)){blockRestore("Power policy changed; retry recovery");return;}
            recoveryPolicy=next;
            {RecoveryStartupGuard startup;FiveTratumMining::updateControl();}
            if(!revisionCurrent()||FiveTratumMining::policyRevision()!=recoveryPolicy){recoveryRequested=false;restoring=false;recoveryPhase="blocked";recoveryReason="External control interrupted recovery";return;}
            if(hardRecoveryFault(board)||!board->isInitialized()||!FiveTratumMining::miningWritesAllowed()){blockRestore("Normal mining resume failed; resolve protection and retry");return;}
            recoveryPhase="verifying";recoveryReason="Checking original voltage, clock, fans and fresh mining work";
            recoveryStarted=esp_timer_get_time();recoveryLastSample=0;recoverySamples=0;return;
        }
        if(!strcmp(recoveryPhase,"verifying")) {
            const bool healthy=observed.valid&&observed.miningActive&&!observed.fault&&observed.fanOk&&
                observed.current.frequencyMHz==target.frequencyMHz&&observed.current.coreVoltageMv==target.coreVoltageMv&&
                std::fabs(observed.voltageReadbackMv-target.coreVoltageMv)<=25&&safeToRestore()&&effectiveMatch(board)&&
                (!recoveryAutomatic || (observed.asicTempC<controller.config.maxAsicTempC&&observed.vrmTempC<controller.config.maxVrmTempC&&observed.powerW<=controller.config.maxPowerW&&
                    observed.errorPercentage<=FWT_BOARD_ERROR_LIMIT&&observed.worstErrorPercentage<=FWT_CHIP_ERROR_LIMIT));
            if(healthy&&observed.sampleId!=recoveryLastSample){recoveryLastSample=observed.sampleId;++recoverySamples;}
            if(!healthy)recoverySamples=0;
            if(recoverySamples>=2){recoveryRequested=false;restoring=false;backupRestored=true;recoveryPhase="complete";recoveryReason="Original settings restored and normal mining verified";adapterReason=nullptr;fwt_cancel(&controller,"Original settings restored; mining resumed");return;}
            if(now<recoveryStarted||now-recoveryStarted>45000000){FiveTratumMining::manualOverride(true);recoveryPolicy=FiveTratumMining::policyRevision();FiveTratumMining::updateControl();blockRestore("Recovery verification failed; ASIC paused, retry restore");}
            return;
        }
    }
    if (!controller.owns && !restoring && !pending && backupAvailable) {
        backupRestored = observed.valid && observed.miningActive && !observed.fault && observed.fanOk &&
            observed.current.frequencyMHz == backup["frequencyMHz"].as<double>() &&
            observed.current.coreVoltageMv == backup["coreVoltageMv"].as<double>() &&
            std::fabs(observed.voltageReadbackMv-observed.current.coreVoltageMv)<=25 && effectiveMatch(board);
    }
    if(!recoveryRequested&&!strcmp(recoveryPhase,"blocked")&&restoreCommitFailed) return;
    if (restoring && !restoreSaved) {
        if (restoreCommitFailed) {adapterReason="Original settings could not be saved; retry Disable";return;}
        if (safeToRestore() && restoreWorker) xTaskNotifyGive(restoreWorker);
        else adapterReason="Original settings restore waits for healthy cooling and an explicit miner resume";
        return;
    }
    if (restoring && !pending) {
        if (!safeToRestore()) { adapterReason = "Original settings are saved; restore waits for healthy cooling and an explicit miner resume"; return; }
        pending = true; voltageMatches = 0; transitionStarted = now;
    }
    if (pending) { advanceTransition(now); return; }
    if (!controller.owns) return;
    const fwt_request request = fwt_tick(&controller,&observed,now);
    fwt_history_capture(&history,&controller);
    if (request.kind == FWT_REQUEST_APPLY) {
        target = request.point; pending = true; voltageMatches = 0; transitionStarted = now;
        advanceTransition(now);
    } else if (request.kind == FWT_REQUEST_PAUSE) {
        // A soft exhausted search has no admissible learned rollback. The
        // already-authorized run may recover once to its immutable original;
        // hardware, sensor and actuation faults never enter this path.
        const bool automatic=controller.reason && !strcmp(controller.reason,"cannot-reduce-load-within-limits") &&
            observed.valid && observed.miningActive && !observed.fault && observed.fanOk &&
            backupAvailable && restoreWorker && !hardRecoveryFault(board) && revisionCurrent();
        FiveTratumMining::manualOverride(true);
        FiveTratumMining::updateControl();
        observed.valid=false; observed.miningActive=false;
        if(automatic) {
            fwt_cancel(&controller,"Automatic recovery after a rejected tuning point");
            restoring=true;restoreSaved=false;restoreCommitFailed=false;backupRestored=false;
            recoveryRequested=true;recoveryAutomatic=true;recoveryPhase="validating-off";
            recoveryReason="Automatic soft-limit recovery waits for cooling below your tuning limits";
            adapterReason=recoveryReason;recoveryPolicy=FiveTratumMining::policyRevision();
            target={backup["frequencyMHz"].as<double>(),backup["coreVoltageMv"].as<double>()};
            if(target.frequencyMHz>controller.config.maxFrequencyMHz || target.coreVoltageMv>controller.config.maxVoltageMv)
                blockRestore("Original settings exceed your current tuning upper limits; explicit recovery is required");
        }
    }
}

bool tryDisplaySnapshot(DisplaySnapshot &out) {
    out={};
    if(pthread_mutex_trylock(&mutex)!=0) return false;
    if(!initialized || !storagePrepared || !powerInitialized || controller.limits.maxFrequencyMHz<=0 ||
       controller.limits.maxVoltageMv<=0) {pthread_mutex_unlock(&mutex);return false;}
    const uint64_t now=esp_timer_get_time();
    fwt_history_capture(&history,&controller);
    if(const auto *event=fwt_history_latest(&history)) {out.lastKind=event->kind;out.lastOutcome=event->outcome;out.lastReason=event->reason;out.lastLabel=fwt_event_short_label(event);}
    out.available=true;out.valid=observed.valid && observed.miningActive && !observed.fault && observed.fanOk &&
        observed.capturedAtUs>0 && now>=observed.capturedAtUs && now-observed.capturedAtUs<=FWT_FRESH_US;
    out.capturedAtUs=observed.capturedAtUs;out.ownerAtUs=lastOwnerTickUs;
    const bool revoked=handoff.load()!=nullptr || !revisionCurrent();
    out.active=controller.active && !revoked;out.ownsSettings=(controller.owns || restoring || pending) && !revoked;
    out.restoring=restoring;out.continuous=controller.config.continuous;
    out.fault=observed.fault;
    out.updating=firmwareUpdates.load() || permanentFreeze.load();
    out.storageBlocked=optionsError || baselineLoadError;
    out.valid=out.valid && !out.fault && !out.updating && !out.storageBlocked && !revoked;
    out.state=controller.state;out.priority=controller.config.priority;
    if(out.valid) {
        out.current=observed.current;out.hashRateGHs=observed.hashRateGHs;
        out.errorPercentage=observed.errorPercentage;out.powerW=observed.powerW;
        out.asicTempC=observed.asicTempC;out.vrmTempC=observed.vrmTempC;
    }
    out.maxAsicTempC=controller.config.maxAsicTempC;out.maxVrmTempC=controller.config.maxVrmTempC;
    out.maxPowerW=controller.config.maxPowerW;
    out.search=fwt_search_read(&controller,now,presentationObscured());
    pthread_mutex_unlock(&mutex);
    return true;
}

bool activity(JsonDocument &document) {
    document.clear();
    document["schemaVersion"]=1;
    if(pthread_mutex_trylock(&mutex)!=0) {document["available"]=false;return !document.overflowed();}
    // Read only already-bound controller RAM. In particular, do not call
    // initialize/acquire or register a dashboard observer from this endpoint.
    const uint64_t now=esp_timer_get_time();
    fwt_history_capture(&history,&controller);
    const char *external=handoff.load();
    const bool revoked=external || firmwareUpdates.load() || permanentFreeze.load() ||
        (initialized && !revisionCurrent());
    document["available"]=true;
    document["supported"]=initialized && exactFamily(SYSTEM_MODULE.getBoard());
    document["active"]=controller.active && !revoked && !restoring;
    document["ownsSettings"]=(controller.owns || restoring || pending) && !revoked;
    document["state"]=restoring ? "restoring" : fwt_state_name(controller.state);
    document["fault"]=observed.fault;
    document["reason"]=external ? external : revoked ? "External control took ownership" : cachedReason();
    document["priority"]=fwt_priority_name(controller.config.priority);
    document["continuous"]=controller.config.continuous;
    document["runId"]=controller.runId;
    auto recovery=document["recovery"].to<JsonObject>();recovery["requested"]=recoveryRequested;recovery["automatic"]=recoveryAutomatic;recovery["phase"]=recoveryPhase;recovery["reason"]=recoveryReason;
    if(lastOwnerTickUs && now>=lastOwnerTickUs) document["controllerAgeSeconds"]=(now-lastOwnerTickUs)/1000000.0;
    else document["controllerAgeSeconds"]=nullptr;
    pthread_mutex_unlock(&mutex);
    return !document.overflowed();
}

bool report(JsonDocument &document) {
    Guard guard; Board *board = SYSTEM_MODULE.getBoard(); initialize(board);
    document["schemaVersion"] = 1; document["waiverVersion"] = waiverVersion;
    document["supported"] = initialized && exactFamily(board);
    auto hardware = document["hardware"].to<JsonObject>();
    hardware["model"] = board ? board->getDeviceModel() : "Unknown"; hardware["asicCount"] = board ? board->getAsicCount() : 0;
    hardware["asicSensor"] = board ? board->tuningAsicSensorLabel() : "Unavailable ASIC temperature source";
    hardware["vrmSensor"] = board ? board->tuningVrmSensorLabel() : "Unavailable VRM temperature source";
    hardware["powerSource"] = sensors.measuredInputPower ? "Measured regulator input power; not wall power" : "Converter output-derived power estimate; not wall power";
    writeConfig(document["config"].to<JsonObject>(),controller.config);
    auto limits = document["limits"].to<JsonObject>();
    const auto &l=controller.limits;
    limits["minFrequencyMHz"]=l.minFrequencyMHz; limits["maxFrequencyMHz"]=l.maxFrequencyMHz;
    limits["minVoltageMv"]=l.minVoltageMv; limits["maxVoltageMv"]=l.maxVoltageMv;
    limits["maxAsicTempC"]=l.maxAsicTempC; limits["maxVrmTempC"]=l.maxVrmTempC; limits["maxPowerW"]=l.maxPowerW;
    limits["frequencyStepMHz"]=l.frequencyStepMHz; limits["voltageStepMv"]=l.voltageStepMv;
    auto guidance = document["guidance"].to<JsonObject>(); guidance["experimental"]=true;
    guidance["asic"]="Recommended ASIC tuning target: 60 C; readings above 60 C deserve caution and above 65 C a stronger warning. Higher hardware protection limits are not recommended targets. Shared board readings are not a measured hottest-chip junction. The tuner retains at least 5 C below the existing protection setting.";
    guidance["vrm"]="Recommended VRM tuning target: 70 C; readings above 70 C deserve caution and 80 C or higher a stronger warning. Higher hardware protection limits are not recommended targets. External board and internal power-stage sensors differ; the hottest required reading constrains tuning. Existing hardware faults remain authoritative.";
    guidance["power"]="Power is the regulator measurement or output-derived estimate named above; it is not wall power or a PSU rating. Default voltage and clock ceilings retain your saved operating point.";
    auto status = document["status"].to<JsonObject>();
    status["state"] = restoring ? "restoring" : fwt_state_name(controller.state);
    status["reason"] = cachedReason();
    const bool revoked=!revisionCurrent();
    status["active"]=controller.active && !revoked; status["ownsSettings"]=(controller.owns || restoring || pending) && !revoked;
    status["runId"]=controller.runId; status["experiments"]=controller.experiments;
    auto recovery=status["recovery"].to<JsonObject>();recovery["requested"]=recoveryRequested;recovery["automatic"]=recoveryAutomatic;recovery["phase"]=recoveryPhase;recovery["reason"]=recoveryReason;
    pointJson(status,"baseline",controller.baselineAvailable?&controller.baseline:nullptr);
    pointJson(status,"current",observed.current.frequencyMHz>0?&observed.current:nullptr);
    pointJson(status,"best",controller.bestAvailable?&controller.best:nullptr);
    auto telemetry=status["telemetry"].to<JsonObject>();
    const uint64_t now=esp_timer_get_time();
    searchJson(status,now);
    eventsJson(status);
    const bool valid=observed.valid && observed.capturedAtUs>0 && now>=observed.capturedAtUs && now-observed.capturedAtUs<=15000000;
    telemetry["valid"]=valid; telemetry["miningActive"]=observed.miningActive; telemetry["fault"]=observed.fault; telemetry["fanOk"]=observed.fanOk;
    if (valid) {
        telemetry["hashRateGHs"]=observed.hashRateGHs; telemetry["errorPercentage"]=observed.errorPercentage;
        telemetry["worstErrorPercentage"]=observed.worstErrorPercentage; telemetry["powerW"]=observed.powerW;
        telemetry["asicTempC"]=observed.asicTempC; telemetry["vrmTempC"]=observed.vrmTempC;
        telemetry["voltageReadbackMv"]=observed.voltageReadbackMv;
        telemetry["sampleAgeSeconds"]=(now-observed.capturedAtUs)/1000000.0;
    } else for (const char *key:{"hashRateGHs","errorPercentage","worstErrorPercentage","powerW","asicTempC","vrmTempC","voltageReadbackMv","sampleAgeSeconds"}) telemetry[key]=nullptr;
    auto saved=document["backup"].to<JsonObject>(); saved["available"]=backupAvailable; saved["restored"]=backupRestored && backupAvailable && valid && observed.miningActive && !observed.fault && observed.fanOk &&
        !controller.owns && !restoring && !pending && revisionCurrent();
    if (backupAvailable) { saved["frequencyMHz"]=backup["frequencyMHz"]; saved["coreVoltageMv"]=backup["coreVoltageMv"]; }
    else {saved["frequencyMHz"]=nullptr;saved["coreVoltageMv"]=nullptr;}
    auto diagnostic=document["diagnostics"].to<JsonObject>();
    diagnostic["storagePreloaded"]=storagePrepared;
    const char *names[]={"unloaded","missing","present","invalid","read-failure"};
    diagnostic["optionsStorage"]=names[static_cast<unsigned>(optionsStatus)];
    diagnostic["baselineStorage"]=names[static_cast<unsigned>(baselineStatus)];
    diagnostic["restoreWorkerStackBytes"]=restoreStackBytes;
    if (restoreMinFreeBytes==UINT32_MAX) diagnostic["restoreWorkerMinFreeBytes"]=nullptr;
    else diagnostic["restoreWorkerMinFreeBytes"]=restoreMinFreeBytes;
    diagnostic["restoreWorkerStackLow"]=restoreStackLow;
    return !document.overflowed();
}

// The HTTP caller owns the power mutex before entering this bounded transaction.
const char *command(JsonObjectConst input) {
    if (!storageContextAllowed()) return "storage-context-unavailable";
    Guard guard; Board *board=SYSTEM_MODULE.getBoard(); initialize(board);
    if (!initialized || !exactFamily(board)) return "unsupported-hardware";
    if (!input["action"].is<const char *>()) return "invalid-request";
    const char *action=input["action"];
    if (!strcmp(action,"configure")) {
        if (pending || restoring || controller.awaitingApply) return "transition-in-progress";
        fwt_config next{};
        if (input.size()!=2 || !parseConfig(input["config"].as<JsonObjectConst>(),next) || !fwt_config_valid(&next,&controller.limits)) return "invalid-config";
        if (controller.owns && (controller.current.frequencyMHz<next.minFrequencyMHz || controller.current.frequencyMHz>next.maxFrequencyMHz ||
            controller.current.coreVoltageMv<next.minVoltageMv || controller.current.coreVoltageMv>next.maxVoltageMv)) return "current-point-outside-bounds";
        fwt_controller proposed=controller;
        if (!fwt_configure(&proposed,&next)) return "current-point-outside-bounds";
        JsonDocument saved;writeConfig(saved.to<JsonObject>(),next);
        if (!writeVerified("options",saved)) return "storage-unavailable";
        controller=proposed;
        optionsError=false;optionsStatus=BlobStatus::Present;
        if (!baselineLoadError) adapterReason=nullptr;
        return nullptr;
    }
    if (!strcmp(action,"start")) {
        if (input.size()!=3 || !input["waiverAccepted"].is<bool>() || !input["waiverAccepted"].as<bool>() ||
            !input["waiverVersion"].is<int>() || input["waiverVersion"].as<int>()!=waiverVersion) return "waiver-required";
        if (optionsError || baselineLoadError || restoring || pending || controller.owns || firmwareUpdates.load() || permanentFreeze.load()) return "tuner-not-ready";
        controller.limits = hardwareLimits(board);
        if (!fwt_config_valid(&controller.config,&controller.limits)) return "limits-changed-reconfigure";
        const uint64_t before=Config::tuningMutationRevision();
        acquire(board); const uint64_t now=esp_timer_get_time();
        if (!observed.valid || !observed.miningActive || observed.fault || !observed.fanOk ||
            observed.capturedAtUs>now || now-observed.capturedAtUs>15000000) return "telemetry-unavailable";
        if (observed.current.frequencyMHz!=board->getAsicFrequency() || observed.current.coreVoltageMv!=board->getAsicVoltageMillis() ||
            std::fabs(observed.voltageReadbackMv-observed.current.coreVoltageMv)>25) return "saved-settings-not-applied";
        JsonDocument durable;
        if (!Config::captureTuningSettings(durable.to<JsonObject>())) return "saved-settings-unavailable";
        for (const auto &entry : {std::pair<const char *,double>{NVS_CONFIG_ASIC_FREQ,observed.current.frequencyMHz},
                                 std::pair<const char *,double>{NVS_CONFIG_ASIC_VOLTAGE,observed.current.coreVoltageMv}}) {
            const auto value=durable[entry.first];
            if (!value.isNull() && (!value.is<uint16_t>() || value.is<bool>() || value.as<double>()!=entry.second)) return "saved-settings-not-applied";
        }
        if (observed.current.frequencyMHz<controller.config.minFrequencyMHz || observed.current.frequencyMHz>controller.config.maxFrequencyMHz ||
            observed.current.coreVoltageMv<controller.config.minVoltageMv || observed.current.coreVoltageMv>controller.config.maxVoltageMv) return "current-point-outside-bounds";
        if (!ensureBackup(board)) return "original-backup-unavailable";
        if (!ensureRestoreWorker()) return "storage-unavailable";
        {
            WriteGuard activation;
            if (before!=Config::tuningMutationRevision() || handoff.load() || firmwareUpdates.load() || permanentFreeze.load()) return "manual-settings-changed";
            if (!fwt_start(&controller,&observed,esp_timer_get_time())) return "tuner-not-ready";
            fwt_history_capture(&history,&controller);
            mutationRevision=before;backupRestored=false;adapterReason=nullptr;recoveryAutomatic=false;
        }
        return nullptr;
    }
    if (!strcmp(action,"restore-and-resume")) {
        if(input.size()!=1)return "invalid-request";
        if(recoveryRequested||pending||controller.awaitingApply)return "transition-in-progress";
        if(firmwareUpdates.load()||permanentFreeze.load())return "firmware-update-in-progress";
        if(!FiveTratumMining::requestedPaused()||FiveTratumMining::miningWritesAllowed()||board->isInitialized())return "miner-not-paused";
        const uint64_t before=Config::tuningMutationRevision(),policy=FiveTratumMining::policyRevision();
        const char *priorHandoff=handoff.load();
        if(!backupAvailable||!backupMatches(board,backup)||!ensureBackup(board))return "original-backup-unavailable";
        {
            WriteGuard activation;
            if(before!=Config::tuningMutationRevision()||policy!=FiveTratumMining::policyRevision()||handoff.load()!=priorHandoff||firmwareUpdates.load()||permanentFreeze.load())return "manual-settings-changed";
            fwt_cancel(&controller,"Explicit original-settings recovery requested");pending=false;restoring=true;restoreSaved=false;restoreCommitFailed=false;backupRestored=false;
            recoveryRequested=true;recoveryAutomatic=false;recoveryPhase="validating-off";recoveryReason="Waiting for verified off-state sensors";
            recoveryPolicy=policy;mutationRevision=before;handoff.store(nullptr);
            target={backup["frequencyMHz"].as<double>(),backup["coreVoltageMv"].as<double>()};
        }
        if(!ensureRestoreWorker()){blockRestore("Recovery worker unavailable; retry recovery");return "storage-unavailable";}
        return nullptr;
    }
    if (!strcmp(action,"stop")) {
        if (input.size()!=1) return "invalid-request";
        recoveryRequested=false;recoveryAutomatic=false;recoveryPhase="idle";recoveryReason="No recovery requested";
        fwt_cancel(&controller,"Disable requested; restoring original settings");pending=false;restoring=false;
        if (!backupAvailable || !backupMatches(board,backup)) return "original-backup-unavailable";
        restoring=true;restoreSaved=false;restoreCommitFailed=false;backupRestored=false;
        mutationRevision=Config::tuningMutationRevision();handoff.store(nullptr);
        target={backup["frequencyMHz"].as<double>(),backup["coreVoltageMv"].as<double>()};
        adapterReason="Disable accepted; original settings restore waits for a healthy, cool miner";
        if (!ensureRestoreWorker()) {restoreCommitFailed=true;adapterReason="Restore worker unavailable; retry Disable";return "storage-unavailable";}
        return nullptr;
    }
    return "invalid-request";
}
}

namespace {
esp_err_t tuneResult(httpd_req_t *req,const char *error) {
    httpd_resp_set_status(req,error ? (!strcmp(error,"invalid-config") || !strcmp(error,"invalid-request") ? "400 Bad Request" : "409 Conflict") : "200 OK");
    JsonDocument document; document["ok"]=error==nullptr;
    if(error)document["error"]=error;else document["restartRequired"]=false;
    return sendJsonResponse(req,document);
}
bool prepareTune(httpd_req_t *req,bool write) {
    if(is_network_allowed(req)!=ESP_OK){httpd_resp_send_err(req,HTTPD_401_UNAUTHORIZED,"Unauthorized");return false;}
    httpd_resp_set_type(req,"application/json");httpd_resp_set_hdr(req,"Cache-Control","no-store");
    if(set_cors_headers(req)!=ESP_OK){httpd_resp_send_500(req);return false;}
    return !write || validateOTP(req)==ESP_OK;
}
}
esp_err_t GET_five_tratum_autotune_activity(httpd_req_t *req) {
    ConGuard connection(http_server,req);if(!prepareTune(req,false))return ESP_FAIL;
    JsonDocument document;if(!FiveTratumAutotune::activity(document))return tuneResult(req,"telemetry-unavailable");
    return sendJsonResponse(req,document);
}
esp_err_t GET_five_tratum_autotune(httpd_req_t *req) {
    ConGuard connection(http_server,req);if(!prepareTune(req,false))return ESP_FAIL;
    JsonDocument document;if(!FiveTratumAutotune::report(document))return tuneResult(req,"telemetry-unavailable");
    return sendJsonResponse(req,document);
}
esp_err_t POST_five_tratum_autotune(httpd_req_t *req) {
    ConGuard connection(http_server,req);if(!prepareTune(req,true))return ESP_FAIL;
    if(!req->content_len || req->content_len>1536)return tuneResult(req,"invalid-request");
    JsonDocument document;if(getJsonData(req,document)!=ESP_OK)return ESP_FAIL;
    if(!document.is<JsonObject>())return tuneResult(req,"invalid-request");
    LockGuard<PowerManagementTask> hardware(POWER_MANAGEMENT_MODULE);
    return tuneResult(req,FiveTratumAutotune::command(document.as<JsonObjectConst>()));
}
