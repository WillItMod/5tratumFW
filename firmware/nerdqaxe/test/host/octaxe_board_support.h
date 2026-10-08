#pragma once

// External SDK, parent-board configuration and hardware boundaries only.
// Oct methods and the base temperature/setting methods are compiled verbatim.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

inline int64_t mockNowUs = 0;
inline int mockStrap = 0;
inline bool mockParentInit = true;
inline int mockParentInitCalls = 0;
inline int mockUartTemperatureRequests = 0;
inline int mockTpsConstructed = 0;
inline int mockTpsDestroyed = 0;
inline float mockExternalVrC = 54.5f;
inline std::array<int, 2> mockMuxInit{{0, 0}};
inline std::array<int, 2> mockMuxProbeCalls{};
inline std::array<int, 2> mockMuxReadCalls{};
inline std::array<std::array<float, 4>, 2> mockMuxValues{};
inline std::vector<std::string> mockOperations;

constexpr int ESP_OK = 0;
constexpr int ESP_FAIL = -1;
using gpio_num_t = int;
constexpr int GPIO_NUM_2 = 2, GPIO_NUM_3 = 3, GPIO_NUM_12 = 12;
constexpr int GPIO_MODE_INPUT = 1, GPIO_PULLDOWN_ONLY = 2;
inline void gpio_reset_pin(int pin) { mockOperations.push_back("reset-" + std::to_string(pin)); }
inline void gpio_set_direction(int pin, int) { mockOperations.push_back("input-" + std::to_string(pin)); }
inline void gpio_set_pull_mode(int pin, int) { mockOperations.push_back("pull-down-" + std::to_string(pin)); }
inline int gpio_get_level(int pin) { mockOperations.push_back("read-" + std::to_string(pin)); return mockStrap; }
inline int64_t esp_timer_get_time() { return mockNowUs; }
inline void vTaskDelay(int milliseconds) { mockNowUs += int64_t(milliseconds) * 1000; }
#define pdMS_TO_TICKS(value) (value)
template <typename... Args> inline void mockLog(const char *, const char *, Args...) {}
#define ESP_LOGI(...) mockLog(__VA_ARGS__)
#define ESP_LOGW(...) mockLog(__VA_ARGS__)

class MockBuck {
public:
    MockBuck() { ++mockTpsConstructed; }
    virtual ~MockBuck() { ++mockTpsDestroyed; }
    virtual const char *kind() const { return "TPS53647"; }
    bool uses_external_vr_temperature() const { return true; }
    float get_temperature() const { return 90.0f; }
};
class TPS53667 : public MockBuck {
public:
    const char *kind() const override { return "TPS53667"; }
};
class Asic {
public:
    void requestChipTemp() { ++mockUartTemperatureRequests; mockOperations.push_back("uart-temperature-request"); }
};
inline Asic mockAsic;
inline float TMP1075_read_temperature(int) { return mockExternalVrC; }

class Tmp451Mux {
    int m_index;
public:
    Tmp451Mux(int a0, int a1, uint8_t address) : m_index(address == 0x4c ? 0 : 1) {
        if (a0 != 2 || a1 != 12 || (address != 0x4c && address != 0x4e)) std::abort();
    }
    int init() { ++mockMuxProbeCalls[m_index]; return mockMuxInit[m_index]; }
    float get_temperature(int channel) {
        if (channel < 0 || channel > 3) std::abort();
        ++mockMuxReadCalls[m_index];
        return mockMuxValues[m_index][channel];
    }
};

struct MockPid { int targetTemp = 55, p = 600, i = 10, d = 1000; };
namespace Config {
inline int savedFrequency = 700, savedVoltage = 1180, savedVr = 25011;
inline int getAsicFrequency(int) { return savedFrequency; }
inline int getAsicVoltage(int) { return savedVoltage; }
inline float getFanSpeed() { return 71.0f; }
inline int getAsicJobInterval(int value) { return value; }
inline bool isFanPolarity(bool value) { return value; }
inline bool isFlipScreenEnabled(bool value) { return value; }
inline int getVrFrequency(int) { return savedVr; }
inline int getFanPidTargetTemp(int ch, int) { return ch ? 65 : 55; }
inline int getFanPidP(int, int value) { return value; }
inline int getFanPidI(int, int value) { return value; }
inline int getFanPidD(int, int value) { return value; }
}

class Board {
protected:
    const char *m_deviceModel = "NerdQAxe++", *m_miningAgent = "NerdQAxe++", *m_asicModel = "BM1370";
    int m_asicCount = 4, m_numPhases = 3, m_imax = 90;
    float m_ifault = 95, m_maxPin = 100, m_minPin = 52;
    float m_minCurrentA = 0, m_maxCurrentA = 8, m_maxVin = 13, m_minVin = 11;
    int m_asicMaxDifficulty = 2048, m_asicMinDifficulty = 512, m_asicMinDifficultyDualPool = 256;
    int m_initVoltageMillis = 1200, m_asicFrequency = 600, m_asicVoltageMillis = 1150;
    int m_defaultAsicFrequency = 600, m_defaultAsicVoltageMillis = 1150;
    int m_absMaxAsicFrequency = 800, m_absMaxAsicVoltageMillis = 1400;
    int m_asicJobIntervalMs = 500, m_vrFrequency = 25011, m_defaultVrFrequency = 25011;
    float m_fanPerc = 100;
    bool m_fanInvertPolarity = false, m_flipScreen = false, m_shutdown = false;
    const char *m_fanLabels[2] = {"M2", "M1"}, *m_swarmColorName = "green";
    std::vector<int> m_asicFrequencies{500, 515, 525, 550, 575, 590, 600};
    std::vector<int> m_asicVoltages{1120, 1130, 1140, 1150, 1160, 1170, 1180, 1190, 1200};
    MockPid m_pidSettings[2];
    std::array<float, 8> m_chipTemps{};
    Asic *m_asics = &mockAsic;
    MockBuck *m_tps = new MockBuck();
public:
    virtual ~Board() { delete m_tps; }
    virtual bool initBoard() { ++mockParentInitCalls; return mockParentInit; }
    virtual void requestChipTemps() {}
    virtual float getVRTemp() { return 0; }
    void setChipTemp(int index, float value) { m_chipTemps.at(index) = value; }
    float getChipTemp(int index) const { return m_chipTemps.at(index); }
    void loadSettings();
};
class NerdQaxePlus : public Board {
public:
    void requestChipTemps() override;
    float getVRTemp() override;
};
class NerdQaxePlus2 : public NerdQaxePlus {};
