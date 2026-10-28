// Actual LVGL rendering and production candidate/priority method bodies only.
// No ESP-IDF, ASIC, networking, physical buttons or upload code is available.
#include "displays/display_candidate.h"
#include "displays/display_candidate_assets.h"
#include "displays/display_layout.h"
#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

static constexpr int MaxWidth = 480, MaxHeight = 320;
static std::array<lv_color_t, MaxWidth * MaxHeight> frame{}, drawBuffer{};
static int width = 320, height = 170;
static unsigned captures = 0;
static uint32_t entropy = 0;
static int64_t fixtureNow = 10000000;
inline uint32_t esp_random() { return entropy++; }
inline int64_t esp_timer_get_time() { return fixtureNow; }
inline int64_t now_us() { return fixtureNow; }
#define ESP_LOGI(...) ((void)0)
#define pdMS_TO_TICKS(ms) (ms)
inline void vTaskDelay(uint32_t) {}
struct PThreadGuard {
    int &state;
    explicit PThreadGuard(int &value) : state(value) { assert(state == 0); state = 1; }
    ~PThreadGuard() { assert(state == 1); state = 0; }
};
inline struct { bool shutdown = false; bool isShutdown() const { return shutdown; } } POWER_MANAGEMENT_MODULE;
inline struct { bool active = false; bool isEnrollmentActive() const { return active; } void disableEnrollment() { active = false; } } otp;
static constexpr uint32_t BTN_EVENT_SHORTPRESS = 1, BTN_EVENT_PRESSED = 2;
struct Button {
    uint32_t event = 0;
    void update() {}
    uint32_t getEvent() const { return event; }
    void clearEvent() { event = 0; }
};

class UI {
public:
    lv_obj_t *ui_errOverlayContainer = nullptr, *ui_imageOverlayContainer = nullptr;
    lv_obj_t *ui_qrScreen = nullptr, *ui_PowerOffScreen = nullptr;
    FiveTratumDisplay::CandidatePresentation m_candidatePresentation;
    FiveTratumDisplay::CandidateVariantCycle m_candidateVariants;
    uint32_t m_candidateStartedTick = 0;
    void showCandidateOverlay(bool animate);
    void updateCandidateOverlayAnimation(bool enabled);
    void hideImageOverlay();
    void showErrorOverlay(const char *error_message, uint32_t error_code);
    void hideErrorOverlay();
    void powerOffScreenInit() { ui_PowerOffScreen = lv_obj_create(nullptr); }
};
class DisplayDriver {
public:
    enum class UiState { NOP, Splash1, Splash2, Wait, Portal, Mining, AsicScreen,
                         SettingsScreen, BTCScreen, GlobalStats, ShowQR, Identify, PowerOff, Brand };
    UI *m_ui = nullptr;
    int m_lvglMutex = 0;
    bool m_showFoundBlockEnabled = true, m_shutdownCountdownActive = false;
    bool m_animationsEnabled = false, m_isActiveOverlay = false, displayIsOn = false;
    bool m_powerShutdownRequested = false;
    bool m_foundBlockOverlay = false;
    uint32_t m_identifyDuration_ms = 60000;
    int64_t m_shutdownStartTime = 0;
    int64_t m_lastKeypressTime = 0, m_buttonIgnoreUntil_us = 0;
    lv_obj_t *m_shutdownLabel = nullptr;
    UiState m_state = UiState::Mining;
    void showFoundBlockOverlay();
    void hideFoundBlockOverlay();
    void showError(const char *error_message, uint32_t error_code);
    void hideError();
    void startShutdownCountdown();
    void hideShutdownCountdown();
    uint32_t handleLvglTick(int32_t &elapsed_Ani_cycles);
    void fixtureEnterPriorityState(UiState state, int64_t now);
    void fixtureProcessOverlayButtons(Button &, Button &, int64_t, bool &, bool &, bool &);
    void safe_screen_change(lv_obj_t *screen, lv_scr_load_anim_t, uint32_t, uint32_t) { lv_scr_load(screen); }
    void increaseLvglTick() { lv_tick_inc(30); }
    void enableLvglAnimations(bool enable) { m_animationsEnabled = enable; }
    void enterState(UiState state, int64_t) { m_state = state; }
    void refreshScreen() {}
    void displayTurnOn() { displayIsOn = true; }
    void updateOverlayPriority();
};
#include "candidate_methods.inc"

static unsigned descendants(lv_obj_t *obj) {
    unsigned count = lv_obj_get_child_cnt(obj);
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); ++i)
        count += descendants(lv_obj_get_child(obj, i));
    return count;
}
static unsigned timers() {
    unsigned count = 0;
    for (lv_timer_t *timer = lv_timer_get_next(nullptr); timer; timer = lv_timer_get_next(timer)) ++count;
    return count;
}
static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels) {
    size_t index = 0;
    for (int y = area->y1; y <= area->y2; ++y)
        for (int x = area->x1; x <= area->x2; ++x)
            frame[static_cast<size_t>(y) * width + x] = pixels[index++];
    lv_disp_flush_ready(driver);
}
static void validate(lv_obj_t *obj) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    lv_area_t bounds;
    lv_obj_get_coords(obj, &bounds);
    assert(bounds.x1 >= 0 && bounds.y1 >= 0 && bounds.x2 < width && bounds.y2 < height);
    if (lv_obj_check_type(obj, &lv_img_class)) {
        const int zoom = lv_img_get_zoom(obj);
        assert(bounds.x1 + lv_obj_get_width(obj) * zoom / LV_IMG_ZOOM_NONE <= width);
        assert(bounds.y1 + lv_obj_get_height(obj) * zoom / LV_IMG_ZOOM_NONE <= height);
    }
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); ++i) validate(lv_obj_get_child(obj, i));
}
static void capture(const std::string &path) {
    lv_obj_update_layout(lv_layer_top());
    validate(lv_layer_top());
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(nullptr);
    std::ofstream file(path, std::ios::binary);
    file << "P6\n" << width << ' ' << height << "\n255\n";
    for (int i = 0; i < width * height; ++i) {
        lv_color32_t rgb;
        rgb.full = lv_color_to32(frame[static_cast<size_t>(i)]);
        const char bytes[] = {static_cast<char>(rgb.ch.red), static_cast<char>(rgb.ch.green), static_cast<char>(rgb.ch.blue)};
        file.write(bytes, 3);
    }
    assert(file.good());
    ++captures;
}
int main(int argc, char **argv) {
    assert(argc == 4);
    width = std::stoi(argv[2]); height = std::stoi(argv[3]);
    assert((width == 320 && height == 170) || (width == 480 && height == 320));
    const std::string output(argv[1]);
    using namespace FiveTratumDisplay;
    static_assert(DonutCandidateCount == CandidateVariantCount);
    static_assert(LVGL_VERSION_MAJOR == 8 && LVGL_VERSION_MINOR == 3 && LVGL_VERSION_PATCH == 11);
    CandidateVariantCycle selections;
    unsigned previous = CandidateVariantCount;
    bool seen[CandidateVariantCount]{};
    uint32_t policyEntropy = 0x13579BDF;
    for (unsigned i = 0; i < 4096; ++i) {
        policyEntropy ^= policyEntropy << 13;
        policyEntropy ^= policyEntropy >> 17;
        policyEntropy ^= policyEntropy << 5;
        const unsigned selected = selections.next(policyEntropy);
        assert(selected < CandidateVariantCount && selected != previous);
        seen[selected] = true; previous = selected;
    }
    for (bool value : seen) assert(value);
    lv_init();
    lv_disp_draw_buf_t draw;
    lv_disp_draw_buf_init(&draw, drawBuffer.data(), nullptr, width * height);
    lv_disp_drv_t display;
    lv_disp_drv_init(&display);
    display.hor_res = width; display.ver_res = height;
    display.flush_cb = flush; display.draw_buf = &draw;
    lv_disp_t *registered = lv_disp_drv_register(&display);
    auto *page = lv_obj_create(nullptr);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, width, height);
    lv_obj_set_style_bg_color(page, lv_color_hex(0x070E1B), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_scr_load(page);
    const unsigned baselineWidgets = descendants(lv_layer_top());
    const unsigned baselineTimers = timers();
    for (unsigned variant = 0; variant < CandidateVariantCount; ++variant) {
        assert(donutCandidateImages[variant].header.w == 80 && donutCandidateImages[variant].header.h == 80);
        assert(donutCandidateImages[variant].header.cf == LV_IMG_CF_TRUE_COLOR_ALPHA);
        assert(donutCandidateImages[variant].data_size == 80 * 80 * 3);
        auto view = candidatePresentation(lv_layer_top(), &donutCandidateImages[variant], variant);
        const unsigned widgetCount = descendants(view.container);
        for (uint32_t elapsed = 0; elapsed < CandidateBurstMs; elapsed += 30) {
            animateCandidate(view, elapsed, true);
            assert(view.variant == variant && descendants(view.container) == widgetCount);
            lv_obj_update_layout(lv_layer_top()); validate(lv_layer_top());
        }
        animateCandidate(view, 330, true);
        capture(output + "/candidate-" + std::to_string(width) + "x" + std::to_string(height) + "-" + std::to_string(variant) + ".ppm");
        animateCandidate(view, CandidateBurstMs, true);
        lv_obj_update_layout(lv_layer_top());
        assert(view.motionFinished && lv_obj_get_y(view.image) == view.imageY);
        for (auto *spark : view.sprinkles) assert(lv_obj_has_flag(spark, LV_OBJ_FLAG_HIDDEN));
        animateCandidate(view, 100, true);
        lv_obj_update_layout(lv_layer_top());
        assert(lv_obj_get_y(view.image) == view.imageY); // An event never restarts itself.
        dismissCandidate(view);
        assert(!view.container && descendants(lv_layer_top()) == baselineWidgets && timers() == baselineTimers);
    }
    auto reduced = candidatePresentation(lv_layer_top(), &donutCandidateImages[0], 0);
    animateCandidate(reduced, 0, false);
    assert(reduced.motionFinished && lv_obj_has_flag(reduced.shine, LV_OBJ_FLAG_HIDDEN));
    capture(output + "/candidate-" + std::to_string(width) + "x" + std::to_string(height) + "-static.ppm");
    dismissCandidate(reduced);

    UI ui;
    DisplayDriver driver;
    driver.m_ui = &ui;
    driver.showFoundBlockOverlay();
    assert(ui.ui_imageOverlayContainer && driver.displayIsOn && driver.m_isActiveOverlay && driver.m_foundBlockOverlay);
    const unsigned stableVariant = ui.m_candidatePresentation.variant;
    int32_t cycles = 0;
    for (unsigned i = 0; i < 100; ++i) driver.handleLvglTick(cycles);
    assert(ui.m_candidatePresentation.variant == stableVariant && ui.m_candidatePresentation.motionFinished);
    assert(!driver.m_animationsEnabled && descendants(lv_layer_top()) > baselineWidgets);
    Button button1, button2;
    bool press1 = false, press2 = false, both = false;
    button1.event = BTN_EVENT_SHORTPRESS;
    { PThreadGuard timerLock(driver.m_lvglMutex); driver.fixtureProcessOverlayButtons(button1, button2, fixtureNow, press1, press2, both); }
    assert(!driver.m_foundBlockOverlay && !ui.ui_imageOverlayContainer && !button1.event);
    assert(!driver.m_isActiveOverlay && descendants(lv_layer_top()) == baselineWidgets);
    previous = stableVariant;
    for (unsigned i = 0; i < 512; ++i) {
        driver.showFoundBlockOverlay();
        assert(ui.m_candidatePresentation.variant != previous);
        previous = ui.m_candidatePresentation.variant;
        driver.showFoundBlockOverlay(); // Replaces the previous overlay, without stacking.
        assert(lv_obj_get_child_cnt(lv_layer_top()) == baselineWidgets + 1);
        previous = ui.m_candidatePresentation.variant;
        driver.hideFoundBlockOverlay();
        assert(descendants(lv_layer_top()) == baselineWidgets && timers() == baselineTimers);
    }
    driver.showFoundBlockOverlay();
    driver.showError("MINER OVERHEATED", 1);
    assert(!ui.ui_imageOverlayContainer && !ui.m_candidatePresentation.container && ui.ui_errOverlayContainer && !driver.m_foundBlockOverlay);
    button2.event = BTN_EVENT_SHORTPRESS;
    { PThreadGuard timerLock(driver.m_lvglMutex); driver.fixtureProcessOverlayButtons(button1, button2, fixtureNow, press1, press2, both); }
    assert(ui.ui_errOverlayContainer && !button2.event); // Safety cannot be dismissed as a candidate.
    driver.showFoundBlockOverlay();
    assert(!ui.ui_imageOverlayContainer); // Thermal/error warning outranks a new event.
    capture(output + "/candidate-" + std::to_string(width) + "x" + std::to_string(height) + "-safety.ppm");
    driver.hideError();
    for (auto state : {DisplayDriver::UiState::ShowQR, DisplayDriver::UiState::PowerOff,
                       DisplayDriver::UiState::Identify, DisplayDriver::UiState::Splash1,
                       DisplayDriver::UiState::Splash2, DisplayDriver::UiState::Wait,
                       DisplayDriver::UiState::Portal}) {
        driver.m_state = state; driver.showFoundBlockOverlay(); assert(!ui.ui_imageOverlayContainer);
    }
    driver.m_state = DisplayDriver::UiState::Mining;
    for (bool *guard : {&driver.m_shutdownCountdownActive, &POWER_MANAGEMENT_MODULE.shutdown, &otp.active}) {
        *guard = true; driver.showFoundBlockOverlay(); assert(!ui.ui_imageOverlayContainer); *guard = false;
    }
    driver.m_showFoundBlockEnabled = false;
    driver.showFoundBlockOverlay(); assert(!ui.ui_imageOverlayContainer);
    driver.m_showFoundBlockEnabled = true;
    driver.showFoundBlockOverlay();
    driver.startShutdownCountdown();
    assert(!ui.ui_imageOverlayContainer && driver.m_shutdownCountdownActive && driver.m_shutdownLabel);
    driver.showFoundBlockOverlay(); assert(!ui.ui_imageOverlayContainer);
    driver.hideShutdownCountdown();
    assert(descendants(lv_layer_top()) == baselineWidgets && timers() == baselineTimers);
    ui.ui_qrScreen = lv_obj_create(nullptr);
    for (auto state : {DisplayDriver::UiState::ShowQR, DisplayDriver::UiState::Identify,
                       DisplayDriver::UiState::PowerOff}) {
        driver.m_state = DisplayDriver::UiState::Mining;
        driver.showFoundBlockOverlay();
        assert(ui.ui_imageOverlayContainer);
        driver.fixtureEnterPriorityState(state, fixtureNow);
        assert(!ui.ui_imageOverlayContainer && !ui.m_candidatePresentation.container && !driver.m_foundBlockOverlay);
        assert(driver.m_powerShutdownRequested == (state == DisplayDriver::UiState::PowerOff));
    }
    assert(descendants(lv_layer_top()) == baselineWidgets && timers() == baselineTimers);
    lv_disp_remove(registered);
    // LVGL 8.3 removes the display/screens/timer, but leaves the driver-owned
    // draw context allocated. Keep it alive through screen deletion, then
    // deinitialize and release it with the same allocator used at registration.
    display.draw_ctx_deinit(&display, display.draw_ctx);
    lv_mem_free(display.draw_ctx);
    display.draw_ctx = nullptr;
    std::cout << "{\"passed\":true,\"resolution\":[" << width << ',' << height << "],\"images\":"
              << captures << ",\"productionMethods\":13,\"productionOverlayButtonHandling\":true,\"productionPriorityCases\":3,\"replacementEvents\":1024,\"fixtureOnly\":true}\n";
}
