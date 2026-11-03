#pragma once

// Only cached snapshots and LVGL are available to the real display bodies.
// This platform boundary cannot access ASIC, GPIO, I2C or the network.
#include "tasks/chip_hashrate_sample.h"
#include "tasks/temperature_sample.h"
#include "stratum/mux_status.h"
#include "display_mux_snapshot.inc"
#include <algorithm>
#include <array>
#include <cassert>

inline int64_t fixtureNowUs = 20000000;
inline int64_t esp_timer_get_time() { return fixtureNowUs; }

struct Board {
    int count = 8;
    bool initialized = true, shutdown = false;
    std::array<float, 8> temperatures{};
    int getAsicCount() const { return count; }
    const char *getDeviceModel() const { return "NerdOCTAXE-γ"; }
    int getAsicFrequency() const { return 700; }
    int getAsicVoltageMillis() const { return 1180; }
    bool isInitialized() const { return initialized; }
    bool isShutdown() const { return shutdown; }
    bool hasHashrateCounter() const { return true; }
    float getChipTemp(int index) const {
        assert(index >= 0 && index < count && index < 8);
        return temperatures[index];
    }
};
inline Board fixtureBoard;
inline struct {
    Board *board = &fixtureBoard;
    Board *getBoard() const { return board; }
} SYSTEM_MODULE;
inline struct {
    std::array<FiveTratumTelemetry::ChipHashrateSample, 8> samples{};
    FiveTratumTelemetry::ChipHashrateSample getChipHashrateSample(int index) const {
        assert(index >= 0 && index < 8);
        return samples[index];
    }
} HASHRATE_MONITOR;
inline struct {
    FiveTratumTelemetry::TemperatureSample sample{60.0625f, 54.5f, 0, fixtureNowUs};
    bool shutdown = false;
    auto getTemperatureSnapshot() const { return sample; }
    bool isShutdown() const { return shutdown; }
    float getPower() const { return 181.25f; }
    int getFanRPM(int index) const { assert(index == 0); return 2522; }
} POWER_MANAGEMENT_MODULE;
inline struct {
    unsigned enables = 0, disables = 0;
    void enableFetching() { ++enables; }
    void disableFetching() { ++disables; }
} APIs_FETCHER;

struct DisplayFixtureWidgets {
    lv_obj_t *ui_MiningScreen = nullptr, *ui_AsicScreen = nullptr;
    lv_obj_t *ui_TuningScreen = nullptr;
    lv_obj_t *ui_lbTuningState = nullptr, *ui_lbTuningGoal = nullptr, *ui_lbTuningPoint = nullptr;
    lv_obj_t *ui_lbTuningWork = nullptr, *ui_lbTuningThermal = nullptr;
    lv_obj_t *ui_lbTuningLimits = nullptr, *ui_lbTuningProgress = nullptr;
    lv_obj_t *ui_lbAsicPage = nullptr, *ui_lbSharedPoint = nullptr;
    lv_obj_t *ui_lbChipIds[4]{}, *ui_lbChipRates[4]{}, *ui_lbChipTemps[4]{};
    lv_obj_t *ui_lbTemp = nullptr, *ui_lbVRExternalTemp = nullptr, *ui_lbVRInternalTemp = nullptr;
    lv_obj_t *ui_lbAsicThermal = nullptr, *ui_lbChipTemperatureTitle = nullptr;
    lv_obj_t *ui_lblTempPrice = nullptr, *ui_lbBrandPowerTemp = nullptr, *ui_lbTempFan = nullptr;
    lv_obj_t *ui_lbCoinTitle = nullptr, *ui_lbBrandCoin = nullptr;
    lv_obj_t *ui_lblCoinIdentity = nullptr, *ui_lblCoinName = nullptr, *ui_lblCoinSource = nullptr;
    lv_obj_t *ui_lblBTCPrice = nullptr, *ui_lblHashPrice = nullptr;
    lv_obj_t *ui_lblJobCoin[2]{}, *ui_lblJobHeight[2]{}, *ui_lblJobDifficulty[2]{};
    lv_obj_t *ui_lblJobNBits[2]{}, *ui_lblJobAge[2]{}, *ui_lblJobSource = nullptr;
};

class DisplayDriver {
public:
    enum class UiState { AsicScreen, TuningScreen, SettingsScreen, BTCScreen };
    DisplayFixtureWidgets *m_ui = nullptr;
    int m_asicPageStart = 0;
    int64_t m_temperatureValidUntilUs = -1;
    MuxCoinContext m_coinContext{};
    bool m_bitcoinFetchingEnabled = false;
    UiState m_state = UiState::AsicScreen;
    bool displayOff = false;
    FiveTratumAutotune::DisplaySnapshot m_tuningSnapshot{};
    void updateAsicReadings();
    void updateTuningReadings(int64_t nowUs);
    void updateThermalReadings(int64_t nowUs);
    void updateCoinContext(const StratumManager::MuxDisplayState &view);
    void updateGlobalMiningStats(const StratumManager::MuxDisplayState &view);
    void fixtureAsicButton(int64_t now, bool btn1Press, bool btn2Press);
    bool ledControl(bool, bool) { return displayOff; }
    void enterState(UiState state, int64_t) { m_state = state; }
    void setScreenAnimationRunning(bool) {}
};

#include "display_driver_methods.inc"

// Compile the real UI constructor and page builders together, rather than
// assuming that separately rendered pages fit when all are retained at once.
LV_IMG_DECLARE(ui_img_wifi_png);
struct FixtureAppDescription {const char *version="5tratumFW-oct-0.1.0-beta.7";};
inline const FixtureAppDescription *esp_app_get_description(){static FixtureAppDescription d;return &d;}
inline bool qrcodegen_getModule(const uint8_t *,int x,int y){return (x+y)%2==0;} // Synthetic pixels; allocation only.
#include "display_actual_ui.inc"
#define MALLOC display_host_alloc
#define FREE display_host_free
#include "display_actual_ui_methods.inc"
#undef MALLOC
#undef FREE
