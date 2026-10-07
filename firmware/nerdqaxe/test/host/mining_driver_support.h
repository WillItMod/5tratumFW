#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <vector>
#include <pthread.h>
#include <ArduinoJson.h>
#include "asic.h"
#include "bm1370.h"
#include "crc.h"
#include "mining_utils.h"
#include "serial.h"

#define __bswap32 __builtin_bswap32
#define pdMS_TO_TICKS(ms) (ms)
#define pdTRUE 1
#define pdPASS 1
#define ESP_LOGI(tag, ...) do { (void)(tag); } while (0)
#define ESP_LOGE(tag, ...) do { (void)(tag); } while (0)
#define ESP_LOGW(tag, ...) do { (void)(tag); } while (0)
#define ESP_LOG_BUFFER_HEX(tag, ...) do { (void)(tag); } while (0)
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define TPS53647_EN_PIN 10
#define BM1368_RST_PIN 1
#define LDO_EN_PIN 13

extern uint64_t hostClockMs;
extern std::array<int, 32> gpioLevels;
extern bool serialShortWrite;
extern unsigned simulatedChips;
extern int hostBaud;
extern std::deque<std::vector<uint8_t>> serialFrames;
int64_t esp_timer_get_time();
void vTaskDelay(uint32_t milliseconds);
void gpio_set_level(int pin, int value);
int gpio_get_level(int pin);

struct Buck {
    bool initOkay = true, voltageOkay = true, disableOkay = true;
    std::vector<float> voltages;
    bool init(int, int, float) { return initOkay; }
    bool set_vout(float volts) { voltages.push_back(volts); return voltageOkay; }
    bool disable_vout() { return disableOkay; }
};

// Only the external board/Buck/GPIO boundary is substituted. The tested
// methods below are extracted verbatim from the production board sources.
class Board {
public:
    enum class Error { NONE, TEMP_FAULT };
    virtual ~Board() = default;
    virtual bool initAsics() = 0;
    virtual bool pauseMiningPower() = 0;
    virtual const char *getDeviceModel() const = 0;
    virtual Asic *getAsicDriver() = 0;
    virtual int getDetectedAsicCount() const = 0;
    virtual unsigned getAsicCount() const = 0;
    virtual unsigned getAsicVoltageMillis() const = 0;
    virtual bool isShutdown() const = 0;
    virtual bool isBuckInitialized() const = 0;
    virtual bool supportsMiningPowerControl() const = 0;
    virtual float getVout() = 0;
    virtual bool setAsicFrequency(float frequency) = 0;
    Asic *getAsics() { return getAsicDriver(); }
    unsigned getAsicJobIntervalMs() { return 200; }
};
class NerdQaxePlus : public Board {
public:
    Asic *m_asics = nullptr;
    Buck *m_tps = nullptr;
    unsigned m_numPhases = 3, m_imax = 90;
    float m_ifault = 95;
    unsigned m_initVoltageMillis = 1200, m_asicVoltageMillis = 1130;
    unsigned m_asicFrequency = 500, m_asicCount = 4, m_asicMaxDifficulty = 2048;
    uint32_t m_vrFrequency = 25011;
    int m_chipsDetected = 0;
    bool m_isInitialized = false, m_isBuckInitialized = false, m_shutdown = false;
    float measuredVoutOverride = -1;
    float chipTemps[4]{};
    virtual ~NerdQaxePlus() = default;
    bool initAsics() override;
    virtual bool setVoltage(float volts);
    bool pauseMiningPower() override;
    const char *getDeviceModel() const override { return "NerdQAxe+"; }
    Asic *getAsicDriver() override { return m_asics; }
    int getDetectedAsicCount() const override { return m_chipsDetected; }
    unsigned getAsicCount() const override { return m_asicCount; }
    unsigned getAsicVoltageMillis() const override { return m_asicVoltageMillis; }
    bool isShutdown() const override { return m_shutdown; }
    bool isBuckInitialized() const override { return m_isBuckInitialized; }
    bool supportsMiningPowerControl() const override { return true; }
    float getVout() override {
        return measuredVoutOverride != -1 ? measuredVoutOverride : (m_tps->voltages.empty() ? 0 : m_tps->voltages.back());
    }
    bool setAsicFrequency(float frequency) override { return m_asics->setAsicFrequency(frequency); }
    void setAsicReset(bool state);
    void LDO_enable();
    void LDO_disable();
    void VREG_enable();
    void VREG_disable();
    bool validateVoltage(float volts) { return volts == 0 || (volts >= 1.005f && volts <= 1.4f); }
    void setChipTemp(unsigned index, float temperature) { assert(index < 4); chipTemps[index] = temperature; }
};
class NerdQaxePlus2 final : public NerdQaxePlus {
public:
    bool m_hasRev7TPS546 = false;
    bool initAsics() override;
    bool setVoltage(float volts) override;
    const char *getDeviceModel() const override { return "NerdQAxe++"; }
};

class PThreadGuard {
    pthread_mutex_t &m_mutex;
public:
    explicit PThreadGuard(pthread_mutex_t &mutex) : m_mutex(mutex) { pthread_mutex_lock(&m_mutex); }
    ~PThreadGuard() { pthread_mutex_unlock(&m_mutex); }
};
#define MALLOC(size) std::malloc(size)
void free_bm_job(bm_job *job);
#include "asic_jobs_actual.h"

struct HostSystem {
    Board *board = nullptr;
    Board::Error error = Board::Error::NONE;
    Board::Error getBoardError() { return error; }
    Board *getBoard() { return board; }
    void notifyMiningStarted() {}
};
struct HostFan { bool overheated = false; bool isOverheated(int) { return overheated; } };
class PowerManagementTask {
public:
    Board *m_board = nullptr;
    bool m_shutdown = false, m_lastProtectionBuckReady = false;
    float m_chipTempMax = 40, m_vrTemp = 0;
    int64_t m_lastProtectionAtUs = -1;
    uint64_t maximumPollGapMs = 0;
    unsigned polls = 0, firstBuckPoll = 0;
    HostFan m_fanController;
    bool isShutdown() { return m_shutdown; }
    void pollProtection(); // Sensor/fan boundary; refresh decision is production code.
    bool refreshProtectionForMining();
};
struct HostHashrate { unsigned resets = 0; void resetAfterPowerCycle() { ++resets; } };
extern HostSystem SYSTEM_MODULE;
extern PowerManagementTask POWER_MANAGEMENT_MODULE;
extern HostHashrate HASHRATE_MONITOR;
extern AsicJobs asicJobs;
void trigger_job_creation();
namespace Config { inline bool isCanEnabled() { return false; } }
using TimerHandle_t = void *;
TimerHandle_t xTimerCreate(const char *, unsigned, bool, void *, void (*)(TimerHandle_t));
int xTimerStart(TimerHandle_t, unsigned);
int xTimerDelete(TimerHandle_t, unsigned);
void create_job_timer(TimerHandle_t);
void create_jobs_task(void *);
