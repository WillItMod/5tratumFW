#include <inttypes.h>
#include <stdio.h>
#include <limits>
#include <algorithm>

#include "lv_conf.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ui.h"
#include "ui_ipc.h"
#include "ui_helpers.h"
#include "global_state.h"
#include "system.h"
#include "macros.h"
#include "button.h"

#include "nvs_config.h"
#include "displayDriver.h"
#include "display_data.h"
#include "display_graphics.h"
#include "display_layout.h"

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

static const char *TAG = "TDisplayS3";

#ifdef NERDQX
#define SPLASH1_TIMEOUT_MS 3000
#define SPLASH2_TIMEOUT_MS 5000
#elif defined(Q1370) || defined(Q1373)
#define SPLASH1_TIMEOUT_MS 3000
#define SPLASH2_TIMEOUT_MS 5000
#else
#define SPLASH1_TIMEOUT_MS 3000
#define SPLASH2_TIMEOUT_MS 3000
#endif

// small helpers
static inline int64_t now_us() { return esp_timer_get_time(); }
static inline int32_t elapsed_ms(int64_t start_us, int64_t now) {
    return static_cast<int32_t>((now - start_us) / 1000);
}

DisplayDriver::DisplayDriver() {
    m_animationsEnabled = false;
    m_lastKeypressTime = 0;
    m_displayIsOn = false;
    m_countdownActive = false;
    m_countdownStartTime = 0;
    m_btcPrice = 0;
    m_isActiveOverlay = false;
    m_lvglMutex = PTHREAD_MUTEX_INITIALIZER;
    m_isAutoScreenOffEnabled = false;
    m_tempControlMode = 0;
    m_fanSpeed = 0;
    m_shutdownCountdownActive = false;
    m_shutdownStartTime = 0;
    m_shutdownLabel = nullptr;
    m_buttonIgnoreUntil_us = 0;
}

void DisplayDriver::loadSettings() {
    PThreadGuard lock(m_lvglMutex);
    m_isAutoScreenOffEnabled = Config::isAutoScreenOffEnabled();
    m_tempControlMode = Config::getTempControlMode();
    m_fanSpeed = Config::getFanSpeed();
    m_showFoundBlockEnabled = Config::isShowBlockFoundEnabled();

    // when setting was changed, turn on the display LED
    if (!m_isAutoScreenOffEnabled) {
        displayTurnOn();
    }
}

bool DisplayDriver::notifyLvglFlushReady(esp_lcd_panel_io_handle_t panelIo, esp_lcd_panel_io_event_data_t* edata,
                                   void* userCtx) {
    lv_disp_drv_t* dispDriver = (lv_disp_drv_t*)userCtx;
    lv_disp_flush_ready(dispDriver);
    return false;
}

void DisplayDriver::lvglFlushCallback(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* colorMap) {
    esp_lcd_panel_handle_t panelHandle = (esp_lcd_panel_handle_t)drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // Copy buffer content to the display
    esp_lcd_panel_draw_bitmap(panelHandle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, colorMap);
}

/************ DISPLAY TURN ON/OFF FUNCTIONS *************/
bool DisplayDriver::displayTurnOff(void) {
    if (!m_displayIsOn) {
        return false;
    }
    gpio_set_level(TDISPLAYS3_PIN_NUM_BK_LIGHT, TDISPLAYS3_LCD_BK_LIGHT_OFF_LEVEL);
    gpio_set_level(TDISPLAYS3_PIN_PWR, false);
    ESP_LOGI(TAG, "Screen off");
    m_displayIsOn = false;
    return true;
}

bool DisplayDriver::displayTurnOn(void) {
    if (m_displayIsOn) {
        return false;
    }
    gpio_set_level(TDISPLAYS3_PIN_PWR, true);
    gpio_set_level(TDISPLAYS3_PIN_NUM_BK_LIGHT, TDISPLAYS3_LCD_BK_LIGHT_ON_LEVEL);
    ESP_LOGI(TAG, "Screen on");
    m_displayIsOn = true;
    return true;
}

/************ AUTO TURN OFF DISPLAY FUNCTIONS *************/
void DisplayDriver::startCountdown(void) {
    m_countdownActive = true;
    m_countdownStartTime = esp_timer_get_time();

    if (m_countdownLabel == NULL) {
        lv_obj_t* currentScreen = lv_scr_act();
        lv_obj_t* blackBox = lv_obj_create(currentScreen);
        lv_obj_set_size(blackBox, 200, 100);
        lv_obj_set_style_bg_color(blackBox, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_border_width(blackBox, 0, LV_PART_MAIN);
        lv_obj_align(blackBox, LV_ALIGN_CENTER, 0, 0);

        m_countdownLabel = lv_label_create(blackBox);
        lv_label_set_text(m_countdownLabel, "Turning screen off...");
        lv_obj_set_style_text_color(m_countdownLabel, lv_color_white(), LV_PART_MAIN);
        lv_obj_center(m_countdownLabel);
    }
}

void DisplayDriver::displayHideCountdown(void) {
    if (m_countdownLabel) {
        lv_obj_del(lv_obj_get_parent(m_countdownLabel));
        m_countdownLabel = NULL;
    }
}

void DisplayDriver::checkAutoTurnOffScreen(void) {
    if (!m_displayIsOn)
        return;

    int64_t currentTime = esp_timer_get_time();

    if ((currentTime - m_lastKeypressTime) > 30000000) {  // 30 seconds timeout
        if (!m_countdownActive) {
            startCountdown();
        }

        int64_t elapsedTime = (currentTime - m_countdownStartTime) / 1000000;  // Convert to seconds

        if (elapsedTime > 5) {
            displayHideCountdown();
            displayTurnOff();
            m_countdownActive = false;
        }

    } else {
        if (m_countdownActive) {
            displayHideCountdown();
            m_countdownActive = false;
        }
    }
}

void DisplayDriver::startShutdownCountdown() {
    if (m_shutdownCountdownActive) return;

    m_shutdownCountdownActive = true;
    m_isActiveOverlay = true;
    m_shutdownStartTime = esp_timer_get_time();

    // Explicit hold-to-shutdown is allowed above a safety warning.
    lv_obj_t* box = lv_obj_create(lv_layer_top());
    lv_obj_set_size(box, 200, 100);
    lv_obj_set_style_bg_color(box, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(box, 0, LV_PART_MAIN);
    lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);

    m_shutdownLabel = lv_label_create(box);
    lv_label_set_text(m_shutdownLabel, "Shutdown in 5");
    lv_obj_set_style_text_color(m_shutdownLabel, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(m_shutdownLabel);
}

void DisplayDriver::updateShutdownCountdown() {
    if (!m_shutdownCountdownActive) return;

    int elapsed = (esp_timer_get_time() - m_shutdownStartTime) / 1000000;
    int remaining = 5 - elapsed;
    if (remaining < 0) remaining = 0;

    if (m_shutdownLabel) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Shutdown in %d", remaining);
        lv_label_set_text(m_shutdownLabel, buf);
    }

    // After 5s → trigger shutdown
    if (elapsed >= 5) {
        hideShutdownCountdown();
        enterState(UiState::PowerOff, esp_timer_get_time());
    }
}

void DisplayDriver::hideShutdownCountdown() {
    if (m_shutdownLabel) {
        lv_obj_del(lv_obj_get_parent(m_shutdownLabel));
        m_shutdownLabel = nullptr;
    }
    m_shutdownCountdownActive = false;
    updateOverlayPriority();
}


void DisplayDriver::increaseLvglTick() {
    lv_tick_inc(TDISPLAYS3_LVGL_TICK_PERIOD_MS);
}

// Refresh screen values
void DisplayDriver::refreshScreen(void) {
    // NOP
}

void DisplayDriver::showError(const char *error_message, uint32_t error_code) {
    PThreadGuard lock(m_lvglMutex);
    // hide the overlay and free the memory in case it was open
    m_ui->hideErrorOverlay();

    // now show the (new) error overlay
    m_ui->showErrorOverlay(error_message, error_code);
    if (m_state == UiState::Identify || m_state == UiState::Brand) enterState(UiState::Mining, now_us());
    displayTurnOn();
    m_isActiveOverlay = true;
    refreshScreen();
}

void DisplayDriver::hideError() {
    PThreadGuard lock(m_lvglMutex);
    // hide the overlay and free the memory
    m_ui->hideErrorOverlay();
    updateOverlayPriority();
}

void DisplayDriver::showFoundBlockOverlay() {
    PThreadGuard lock(m_lvglMutex);
    // A candidate is informational and cannot cover a safety fault.
    if (m_ui->ui_errOverlayContainer) return;
    // hide the overlay and free the memory in case it was open
    m_ui->hideImageOverlay();

    // not enabled?
    if (!m_showFoundBlockEnabled) {
        return;
    }
    if (m_state == UiState::Brand) enterState(UiState::Mining, now_us());

    // now show the (new) image overlay
    m_ui->showImageOverlay(&ui_img_found_block_png);
    m_isActiveOverlay = true;
    refreshScreen();
}

void DisplayDriver::hideFoundBlockOverlay() {
    // hide the overlay and free the memory
    m_ui->hideImageOverlay();
    updateOverlayPriority();
}

void DisplayDriver::lvglTimerTaskWrapper(void *param) {
    DisplayDriver *display = (DisplayDriver*) param;
    display->lvglTimerTask(NULL);
}

void DisplayDriver::safe_screen_change(lv_obj_t * new_scr, lv_scr_load_anim_t anim_type, uint32_t speed, uint32_t delay)
{
    m_screenAnimationRunning = true;
    _ui_screen_change(new_scr, anim_type, speed, delay);

}

bool DisplayDriver::enterState(UiState s, int64_t now)
{
    if (m_ui && m_ui->ui_errOverlayContainer && (s == UiState::ShowQR || s == UiState::Identify || s == UiState::Brand)) return false;
    // we already are in this state
    if (m_state == s) {
        return true;
    }
    UiState previousState = m_state;

    m_state = s;
    m_stateStart_us = now;

    switch (m_state) {
    case UiState::NOP:
        // NOP
        break;
    case UiState::Splash1:
        ESP_LOGI(TAG, "enter state splash1");
        enableLvglAnimations(true);
        break;

    case UiState::Splash2:
        ESP_LOGI(TAG, "enter state splash2");
        enableLvglAnimations(true);
        safe_screen_change(m_ui->ui_Splash2, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0);
        if (m_ui->ui_Splash1) { lv_obj_clean(m_ui->ui_Splash1); m_ui->ui_Splash1 = NULL; }
        break;

    case UiState::Wait:
        ESP_LOGI(TAG, "enter state wait");
        // Keep Splash2 visible — system task decides next screen (Portal or Mining).
        // Cleaning Splash2 here would leave an empty (white) screen with no replacement.
        break;

    case UiState::Portal:
        ESP_LOGI(TAG, "enter state portal");
        if (m_ui->ui_Splash2) {
            lv_obj_clean(m_ui->ui_Splash2);
            m_ui->ui_Splash2 = NULL;
            m_ui->ui_lbConnect = nullptr;
        }
        enableLvglAnimations(true);
        safe_screen_change(m_ui->ui_PortalScreen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0);
        break;

    case UiState::Mining:
        ESP_LOGI(TAG, "enter state mining");
        if (m_ui->ui_Splash2) {
            lv_obj_clean(m_ui->ui_Splash2);
            m_ui->ui_Splash2 = NULL;
            m_ui->ui_lbConnect = nullptr;
        }
        enableLvglAnimations(true);
        if (previousState == UiState::GlobalStats) {
            safe_screen_change(m_ui->ui_MiningScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 350, 0);
        } else {
            safe_screen_change(m_ui->ui_MiningScreen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0);
        }
        break;

    case UiState::AsicScreen:
        enableLvglAnimations(true);
        updateAsicReadings();
        safe_screen_change(m_ui->ui_AsicScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 350, 0);
        break;

    case UiState::SettingsScreen:
        ESP_LOGI(TAG, "enter state settings screen");
        enableLvglAnimations(true);
        safe_screen_change(m_ui->ui_SettingsScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 350, 0);
        break;

    case UiState::BTCScreen:
        ESP_LOGI(TAG, "enter state coin route screen");
        enableLvglAnimations(true);
        safe_screen_change(m_ui->ui_BTCScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 350, 0);
        break;

    case UiState::GlobalStats:
        ESP_LOGI(TAG, "enter state global stats");
        enableLvglAnimations(true);
        safe_screen_change(m_ui->ui_GlobalStats, LV_SCR_LOAD_ANIM_MOVE_LEFT, 350, 0);
        break;
    case UiState::Brand:
        enableLvglAnimations(false);
        m_brandCycle.shown(now);
        safe_screen_change(m_ui->ui_BrandScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0);
        break;
    case UiState::ShowQR:
        ESP_LOGI(TAG, "enter qr state");
        enableLvglAnimations(true);
        safe_screen_change(m_ui->ui_qrScreen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0);
        break;
    case UiState::Identify:
        ESP_LOGI(TAG, "enter state identify (blink %lums)", m_identifyDuration_ms);
        break;
    case UiState::PowerOff:
        // An explicit shutdown outranks a previous fault. All physical power
        // operations occur after the timer task releases the display mutex.
        m_ui->hideErrorOverlay();
        m_ui->hideImageOverlay();
        if (!m_ui->ui_PowerOffScreen) {
            m_ui->powerOffScreenInit();
        }
        enableLvglAnimations(false);
        safe_screen_change(m_ui->ui_PowerOffScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0);
        m_powerShutdownRequested = !POWER_MANAGEMENT_MODULE.isShutdown();
        break;
    }
    updateOverlayPriority();
    return true;
}


void DisplayDriver::updateState(int64_t now, bool btn1Press, bool btn2Press, bool btnBothLongPress)
{
    const int ms = elapsed_ms(m_stateStart_us, now);

    if (btnBothLongPress) {
        enterState(UiState::PowerOff, now);
        return;
    }

    switch (m_state) {
    case UiState::NOP:
        // NOP
        break;
    case UiState::Splash1:
        if (ms >= SPLASH1_TIMEOUT_MS) {
            enterState(UiState::Splash2, now);
        }
        break;

    case UiState::Splash2:
        if (ms >= SPLASH2_TIMEOUT_MS) {
            enterState(UiState::Wait, now);
        }
        break;

    case UiState::Wait:
        // NOP
        break;

    case UiState::Portal:
        enterState(UiState::Portal, now);
        break;

    case UiState::Mining:
        if (ledControl(btn1Press, btn2Press)) {
            break;
        }
        if (btn1Press) {
            m_asicPageStart = 0;
            enterState(UiState::AsicScreen, now);
        } else {
            const FiveTratumDisplay::BrandViewConditions view{true, m_displayIsOn,
                m_startupGate.isComplete(), m_isActiveOverlay, otp.isEnrollmentActive(),
                m_shutdownCountdownActive || POWER_MANAGEMENT_MODULE.isShutdown(), m_screenAnimationRunning,
                m_telemetryUpdatedAtUs >= 0 && now >= m_telemetryUpdatedAtUs && now - m_telemetryUpdatedAtUs <= 15000000};
            if (m_brandCycle.due(now, m_lastKeypressTime, view)) enterState(UiState::Brand, now);
        }
        break;
    case UiState::Brand:
        if (m_isActiveOverlay || otp.isEnrollmentActive()) { enterState(UiState::Mining, now); break; }
        if (ledControl(btn1Press, btn2Press)) break;
        if (btn1Press) {
            m_asicPageStart = 0;
            enterState(UiState::AsicScreen, now);
        } else if (now - m_stateStart_us >= FiveTratumDisplay::IdleBrandCycle::DURATION_US) {
            enterState(UiState::Mining, now);
        }
        break;
    case UiState::AsicScreen:
        if (ledControl(btn1Press, btn2Press)) break;
        if (btn1Press) {
            const int count = std::min(64, std::max(0, SYSTEM_MODULE.getBoard()->getAsicCount()));
            if (m_asicPageStart + 4 < count) {
                m_asicPageStart += 4;
                updateAsicReadings();
            } else {
                enterState(UiState::SettingsScreen, now);
            }
        }
        break;
    case UiState::SettingsScreen:
        if (ledControl(btn1Press, btn2Press)) {
            break;
        }
        if (btn1Press) {
            enterState(UiState::BTCScreen, now);
        }
        break;
    case UiState::BTCScreen:
        if (ledControl(btn1Press, btn2Press)) {
            break;
        }
        if (btn1Press) {
            enterState(UiState::GlobalStats, now);
        }
        break;
    case UiState::GlobalStats:
        if (ledControl(btn1Press, btn2Press)) {
            break;
        }
        if (btn1Press) {
            enterState(UiState::Mining, now);
        }
        break;
    case UiState::ShowQR:
        if (btn1Press || btn2Press) {
            // abort enrollment
            otp.disableEnrollment();
            enterState(UiState::Mining, now);
        }
        break;
    case UiState::Identify:
        if (btn1Press || btn2Press || (uint32_t) ms >= m_identifyDuration_ms) {
            displayTurnOn();
            enterState(UiState::Mining, now);
        } else {
            // blink: 500ms on, 500ms off
            if ((ms / 500) % 2 == 0) {
                displayTurnOn();
            } else {
                displayTurnOff();
            }
        }
        break;
    case UiState::PowerOff:
        // NOP
        break;
    }

}



void DisplayDriver::waitForSplashs() {
    // wait until state is not Splash1 or Splash2
    while (static_cast<int>(getState()) < static_cast<int>(UiState::Wait)) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

DisplayDriver::UiState DisplayDriver::getState() {
    PThreadGuard lock(m_lvglMutex);
    return m_state;
}

bool DisplayDriver::ledControl(bool btn1, bool btn2) {
    // btn1 turns it on
    if (btn1) {
        return displayTurnOn();
    }

    // btn2 toggles the LED
    if (btn2) {
        if (!m_displayIsOn) {
            return displayTurnOn();
        }
        return displayTurnOff();
    }
    return false;
}

uint32_t DisplayDriver::handleLvglTick(int32_t &elapsed_Ani_cycles) {
    uint32_t sleepMs;
    {
        PThreadGuard lock(m_lvglMutex);
        increaseLvglTick();
        const uint32_t waitMs = lv_timer_handler();
        if (m_animationsEnabled) {
            if (++elapsed_Ani_cycles > 80) {
                m_animationsEnabled = false;
                elapsed_Ani_cycles = 0;
            }
            sleepMs = std::min(waitMs, static_cast<uint32_t>(5));
        } else {
            sleepMs = (waitMs > 0 && waitMs < 50) ? waitMs : 50;
        }
    }
    vTaskDelay(pdMS_TO_TICKS(sleepMs));
    return sleepMs;
}

void DisplayDriver::processButtons(Button &btn1, Button &btn2, int64_t tnow,
                                   bool &btn1Press, bool &btn2Press, bool &btnBothLongPress)
{
    btn1.update();
    btn2.update();

    uint32_t evt1 = btn1.getEvent();
    uint32_t evt2 = btn2.getEvent();
    bool bothPressed = (evt1 & BTN_EVENT_PRESSED) && (evt2 & BTN_EVENT_PRESSED);
    bool anyPressed = (evt1 & BTN_EVENT_PRESSED) || (evt2 & BTN_EVENT_PRESSED);

    if (anyPressed) {
        m_lastKeypressTime = tnow;
    }

    // Ignore all button events within 200ms of both released
    if (esp_timer_get_time() < m_buttonIgnoreUntil_us) {
        btn1.clearEvent();
        btn2.clearEvent();
        return;
    }

    // A fault remains visible until its source clears. QR presses must reach
    // the state handler to cancel enrollment; only candidates are dismissible.
    if ((evt1 & BTN_EVENT_SHORTPRESS || evt2 & BTN_EVENT_SHORTPRESS) && m_ui->ui_errOverlayContainer) {
        displayTurnOn();
        if (m_state == UiState::ShowQR) {
            otp.disableEnrollment();
            enterState(UiState::Mining, tnow);
        }
        btn1.clearEvent();
        btn2.clearEvent();
        return;
    }
    if ((evt1 & BTN_EVENT_SHORTPRESS || evt2 & BTN_EVENT_SHORTPRESS) && m_ui->ui_imageOverlayContainer) {
        hideFoundBlockOverlay();
        btn1.clearEvent();
        btn2.clearEvent();
        return;
    }

    // --- Shutdown countdown handling ---
    if (bothPressed) {
        if (!m_shutdownCountdownActive) {
            startShutdownCountdown();
        } else {
            updateShutdownCountdown();
        }
        return;
    }

    if (m_shutdownCountdownActive && !bothPressed) {
        hideShutdownCountdown();
        m_buttonIgnoreUntil_us = esp_timer_get_time() + 200 * 1000; // 200ms ignore
        btn1.clearEvent();
        btn2.clearEvent();
        return;
    }

    // Normal button events
    if ((evt1 & BTN_EVENT_LONGPRESS) && (evt2 & BTN_EVENT_LONGPRESS)) {
        btnBothLongPress = true;
        btn1.clearEvent();
        btn2.clearEvent();
    } else {
        if (evt1 & BTN_EVENT_SHORTPRESS) {
            m_lastKeypressTime = tnow;
            btn1Press = true;
            btn1.clearEvent();
        }
        if (evt2 & BTN_EVENT_SHORTPRESS) {
            m_lastKeypressTime = tnow;
            btn2Press = true;
            btn2.clearEvent();
        }
    }
}

void DisplayDriver::handleUiQueueMessages(ui_msg_t &msg, int64_t tnow)
{
    if (xQueueReceive(g_ui_queue, &msg, 0) != pdTRUE) return;

    switch (msg.type) {
        case UI_CMD_SHOW_QR: {
            if (m_ui->ui_errOverlayContainer) break;
            if (!otp.isEnrollmentActive()) {
                ESP_LOGE(TAG, "no otp enrollment active");
                break;
            }
            int size = 0;
            uint8_t* qrBuf = otp.getQrCode(&size);
            if (m_ui->createQRScreen(qrBuf, size)) {
                enterState(UiState::ShowQR, tnow);
                m_isActiveOverlay = true;
            } else {
                ESP_LOGE(TAG, "Cannot render enrollment QR");
                // Do not display a previous enrollment's QR on allocation error.
                if (m_state == UiState::ShowQR) enterState(UiState::Mining, tnow);
            }
            break;
        }
        case UI_CMD_HIDE_QR:
            enterState(UiState::Mining, tnow);
            break;
        case UI_CMD_IDENTIFY:
            if (m_ui->ui_errOverlayContainer) break;
            m_identifyDuration_ms = msg.param;
            m_isActiveOverlay = true;
            enterState(UiState::Identify, tnow);
            break;
    }

    if (msg.payload) {
        free(msg.payload);
        msg.payload = nullptr;
    }
    updateOverlayPriority();
}

void DisplayDriver::updateOverlayPriority() {
    m_isActiveOverlay = m_ui && (m_ui->ui_errOverlayContainer || m_ui->ui_imageOverlayContainer ||
        m_shutdownCountdownActive || m_state == UiState::ShowQR || m_state == UiState::Identify);
}

void DisplayDriver::handleAutoOffAndOverlays()
{
    if (m_isActiveOverlay) {
        displayTurnOn();
    } else if (m_isAutoScreenOffEnabled) {
        checkAutoTurnOffScreen();
    }
}

void DisplayDriver::lvglTimerTask(void *param) {
    (void)param;
    {
        PThreadGuard lock(m_lvglMutex);
        displayTurnOn();
        m_lastKeypressTime = now_us();
        enterState(UiState::Splash1, now_us());
    }
    int32_t elapsed_Ani_cycles = 0;
    Button btn1(PIN_BUTTON_1, 5000);
    Button btn2(PIN_BUTTON_2, 5000);
    ui_msg_t msg;
    while (true) {
        handleLvglTick(elapsed_Ani_cycles); // Holds its own lock; sleeps outside.
        const int64_t tnow = now_us();
        bool shutdownRequested = false;
        {
            // The same mutex as System's telemetry/error updates covers every
            // LVGL object, QR-buffer replacement and display state mutation.
            PThreadGuard lock(m_lvglMutex);
            // A stopped power or System loop must not leave current-looking
            // temperatures on an otherwise responsive physical display.
            if (m_temperatureValidUntilUs >= 0 && tnow > m_temperatureValidUntilUs)
                updateThermalReadings(tnow);
            if (POWER_MANAGEMENT_MODULE.isShutdown()) {
                enterState(UiState::PowerOff, tnow);
            } else {
                bool btn1Press = false, btn2Press = false, btnBothLongPress = false;
                processButtons(btn1, btn2, tnow, btn1Press, btn2Press, btnBothLongPress);
                if (m_screenAnimationRunning) btn1Press = btn2Press = btnBothLongPress = false;
                handleUiQueueMessages(msg, tnow);
                handleAutoOffAndOverlays();
                updateState(tnow, btn1Press, btn2Press, btnBothLongPress);
            }
            shutdownRequested = m_powerShutdownRequested;
            m_powerShutdownRequested = false;
        }
        // Never wait for the board/power hardware mutex while owning LVGL.
        if (shutdownRequested) POWER_MANAGEMENT_MODULE.shutdown();
    }
}


// Función para activar las actualizaciones
void DisplayDriver::enableLvglAnimations(bool enable)
{
    m_animationsEnabled = enable;
}

void DisplayDriver::mainCreatSysteTasks(void)
{
    xTaskCreatePinnedToCore(lvglTimerTaskWrapper, "lvgl Timer", 6000, (void*) this, 4, NULL, 1); // Antes 10000
}

lv_obj_t *DisplayDriver::initTDisplayS3(void)
{
    static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv;      // contains callback functions
    // GPIO configuration
    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {.pin_bit_mask = 1ULL << TDISPLAYS3_PIN_NUM_BK_LIGHT, .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    gpio_pad_select_gpio(TDISPLAYS3_PIN_NUM_BK_LIGHT);
    gpio_pad_select_gpio(TDISPLAYS3_PIN_RD);
    gpio_pad_select_gpio(TDISPLAYS3_PIN_PWR);
    // esp_rom_gpio_pad_select_gpio(TDISPLAYS3_PIN_NUM_BK_LIGHT);
    // esp_rom_gpio_pad_select_gpio(TDISPLAYS3_PIN_RD);
    // esp_rom_gpio_pad_select_gpio(TDISPLAYS3_PIN_PWR);

    gpio_set_direction(TDISPLAYS3_PIN_NUM_BK_LIGHT, GPIO_MODE_OUTPUT);
    gpio_set_direction(TDISPLAYS3_PIN_RD, GPIO_MODE_OUTPUT);
    gpio_set_direction(TDISPLAYS3_PIN_PWR, GPIO_MODE_OUTPUT);

    gpio_set_level(TDISPLAYS3_PIN_RD, true);
    gpio_set_level(TDISPLAYS3_PIN_NUM_BK_LIGHT, TDISPLAYS3_LCD_BK_LIGHT_OFF_LEVEL);

    ESP_LOGI(TAG, "Initialize Intel 8080 bus");
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {.dc_gpio_num = TDISPLAYS3_PIN_NUM_DC,
                                           .wr_gpio_num = TDISPLAYS3_PIN_NUM_PCLK,
                                           .clk_src = LCD_CLK_SRC_DEFAULT,
                                           .data_gpio_nums =
                                               {
                                                   TDISPLAYS3_PIN_NUM_DATA0,
                                                   TDISPLAYS3_PIN_NUM_DATA1,
                                                   TDISPLAYS3_PIN_NUM_DATA2,
                                                   TDISPLAYS3_PIN_NUM_DATA3,
                                                   TDISPLAYS3_PIN_NUM_DATA4,
                                                   TDISPLAYS3_PIN_NUM_DATA5,
                                                   TDISPLAYS3_PIN_NUM_DATA6,
                                                   TDISPLAYS3_PIN_NUM_DATA7,
                                               },
                                           .bus_width = 8,
                                           .max_transfer_bytes = LVGL_LCD_BUF_SIZE * sizeof(uint16_t),
                                           .psram_trans_align = LCD_PSRAM_TRANS_ALIGN,
                                           .sram_trans_align = LCD_SRAM_TRANS_ALIGN};
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = TDISPLAYS3_PIN_NUM_CS,
        .pclk_hz = TDISPLAYS3_LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 20,
        .on_color_trans_done = notifyLvglFlushReady,
        .user_ctx = &disp_drv,
        .lcd_cmd_bits = TDISPLAYS3_LCD_CMD_BITS,
        .lcd_param_bits = TDISPLAYS3_LCD_PARAM_BITS,
        .dc_levels =
            {
                .dc_idle_level = 0,
                .dc_cmd_level = 0,
                .dc_dummy_level = 0,
                .dc_data_level = 1,
            }
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install LCD driver of st7789");
    esp_lcd_panel_handle_t panel_handle = NULL;

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TDISPLAYS3_PIN_NUM_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, true);

    esp_lcd_panel_swap_xy(panel_handle, true);

    Board *board = SYSTEM_MODULE.getBoard();
    if (!board->isFlipScreenEnabled()) {
        esp_lcd_panel_mirror(panel_handle, true, false);
    } else {
        esp_lcd_panel_mirror(panel_handle, false, true);
    }

    // the gap is LCD panel specific, even panels with the same driver IC, can have different gap value
    esp_lcd_panel_set_gap(panel_handle, 0, 35);

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(TDISPLAYS3_PIN_PWR, true);
    gpio_set_level(TDISPLAYS3_PIN_NUM_BK_LIGHT, TDISPLAYS3_LCD_BK_LIGHT_ON_LEVEL);

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    lv_color_t *buf1 = (lv_color_t*) MALLOC_DMA(LVGL_LCD_BUF_SIZE * sizeof(lv_color_t));
    assert(buf1);
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, NULL, LVGL_LCD_BUF_SIZE);

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TDISPLAYS3_LCD_H_RES;
    disp_drv.ver_res = TDISPLAYS3_LCD_V_RES;
    disp_drv.flush_cb = lvglFlushCallback;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    // Configuration is completed.

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    /*const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increaseLvglTick,
        .name = "lvgl_tick"
    };*/
    esp_timer_handle_t lvgl_tick_timer = NULL;
    // ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    // ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, TDISPLAYS3_LVGL_TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "Display LVGL animation");
    lv_obj_t *scr = lv_disp_get_scr_act(disp);

    return scr;
}

void DisplayDriver::updateHashrate(System *module, StratumManager *manager, float power, int pool) {
    (void)manager;
    (void)pool;
    char buffer[80];
    float gh = module->getCurrentHashrate();
    Board *board = module->getBoard();
    if (board && board->hasHashrateCounter() &&
        (!board->isInitialized() || board->isShutdown() ||
         !FiveTratumTelemetry::allCounterSamplesFresh(board->getAsicCount(), esp_timer_get_time(),
             [](int index) { return HASHRATE_MONITOR.getChipHashrateSample(index); }))) {
        gh = std::numeric_limits<float>::quiet_NaN();
    }
    FiveTratumDisplay::formatHashrate(buffer, sizeof(buffer), gh);
    lv_label_set_text(m_ui->ui_lbBrandRate, buffer);
    FiveTratumDisplay::updateSummaryRate(m_ui->ui_lbHashrate, m_ui->ui_lbHashrateUnit, gh);
    lv_label_set_text(m_ui->ui_lbHashrateSet, buffer);
    lv_label_set_text(m_ui->ui_lblHashPrice, buffer);
    FiveTratumDisplay::formatPowerEfficiency(buffer, sizeof(buffer), power, gh);
    lv_label_set_text(m_ui->ui_lbPowerEfficiency, buffer);
    FiveTratumDisplay::updatePowerLabels(m_ui->ui_lbPower, m_ui->ui_lbEficiency, power, gh);
}

void DisplayDriver::updateShares(StratumManager *manager, int pool) {
    if (!manager) {
        lv_label_set_text(m_ui->ui_lbSummaryShares, "Accepted --  |  Rejected --");
        return;
    }
    char buffer[80];
    FiveTratumDisplay::formatShares(buffer, sizeof(buffer), manager->getSharesAccepted(), manager->getSharesRejected());
    lv_label_set_text(m_ui->ui_lbSummaryShares, buffer);
    if (manager->isDualPool()) {
        auto *dual = static_cast<StratumManagerDualPool*>(manager);
        const int index = pool == 1 ? 1 : 0;
        FiveTratumDisplay::formatShares(buffer, sizeof(buffer), dual->getSharesAccepted(index), dual->getSharesRejected(index));
    }
    lv_label_set_text(m_ui->ui_lbShares, buffer);
    lv_label_set_text(m_ui->ui_lbBestDifficulty, manager->getBestDiffString());
    lv_label_set_text(m_ui->ui_lbBestDifficultySet, manager->getBestDiffString());
}

void DisplayDriver::updateTime(System *module)
{
    char strData[20];

    // Calculate the uptime in seconds
    // int64_t currentTimeTest = esp_timer_get_time() + (8 * 3600 * 1000000LL) + (1800 * 1000000LL);//(8 * 60 * 60 * 10000);
    double uptime_in_seconds = (esp_timer_get_time() - module->getStartTime()) / 1000000;
    int uptime_in_days = uptime_in_seconds / (3600 * 24);
    int remaining_seconds = (int) uptime_in_seconds % (3600 * 24);
    int uptime_in_hours = remaining_seconds / 3600;
    remaining_seconds %= 3600;
    int uptime_in_minutes = remaining_seconds / 60;
    int current_seconds = remaining_seconds % 60;

    snprintf(strData, sizeof(strData), "%dd %ih %im %is", uptime_in_days, uptime_in_hours, uptime_in_minutes, current_seconds);
    lv_label_set_text(m_ui->ui_lbTime, strData); // Update label
}

void DisplayDriver::updateCurrentSettings(int pool) {
    PThreadGuard lock(m_lvglMutex);
    if (!m_ui || !m_ui->ui_SettingsScreen) return;
    Board *board = SYSTEM_MODULE.getBoard();
    if (!board) return;
    char buffer[80];
    StratumManager *manager = STRATUM_MANAGER;
    const int index = pool == 1 ? 1 : 0;
    if (manager && manager->isDualPool()) {
        auto *dual = static_cast<StratumManagerDualPool*>(manager);
        lv_label_set_text(m_ui->ui_lbPoolSet, dual->getPoolHost(index));
        snprintf(buffer, sizeof(buffer), "Port %d  |  Shared chain route", dual->getPoolPort(index));
        lv_label_set_text(m_ui->ui_lbPortSet, buffer);
        snprintf(buffer, sizeof(buffer), "Pool %d / dual", index + 1);
        lv_label_set_text(m_ui->ui_lbPoolNr, buffer);
    } else if (manager && manager->isFallback()) {
        auto *fallback = static_cast<StratumManagerFallback*>(manager);
        lv_label_set_text(m_ui->ui_lbPoolSet, fallback->getCurrentPoolHost());
        snprintf(buffer, sizeof(buffer), "Port %d  |  Shared chain route", fallback->getCurrentPoolPort());
        lv_label_set_text(m_ui->ui_lbPortSet, buffer);
        lv_label_set_text(m_ui->ui_lbPoolNr, fallback->isUsingFallback() ? "Fallback pool" : "Primary pool");
    } else {
        lv_label_set_text(m_ui->ui_lbPoolNr, "Pool unavailable");
        lv_label_set_text(m_ui->ui_lbPoolSet, "--");
        lv_label_set_text(m_ui->ui_lbPortSet, "Port --");
    }
    snprintf(buffer, sizeof(buffer), "%d MHz", board->getAsicFrequency());
    lv_label_set_text(m_ui->ui_lbFreqSet, board->getAsicFrequency() > 0 ? buffer : "-- MHz");
    snprintf(buffer, sizeof(buffer), "%d mV", board->getAsicVoltageMillis());
    lv_label_set_text(m_ui->ui_lbVcoreSet, board->getAsicVoltageMillis() > 0 ? buffer : "-- mV");
    FiveTratumDisplay::formatOperatingPoint(buffer, sizeof(buffer), board->getAsicFrequency(), board->getAsicVoltageMillis());
    lv_label_set_text(m_ui->ui_lbSharedPoint, buffer);
    switch (m_tempControlMode) {
        case 1: lv_label_set_text(m_ui->ui_lbFanSet, "Fan auto"); break;
        case 2: lv_label_set_text(m_ui->ui_lbFanSet, "Fan PID"); break;
        default:
            snprintf(buffer, sizeof(buffer), "Fan %u%%", static_cast<unsigned int>(m_fanSpeed));
            lv_label_set_text(m_ui->ui_lbFanSet, buffer);
            break;
    }
}

void DisplayDriver::updateAsicReadings() {
    if (!m_ui || !m_ui->ui_AsicScreen) return;
    Board *board = SYSTEM_MODULE.getBoard();
    const int count = board ? std::min(64, std::max(0, board->getAsicCount())) : 0;
    if (m_asicPageStart >= count) m_asicPageStart = 0;
    char buffer[80];
    snprintf(buffer, sizeof(buffer), "ASIC %d-%d / %d", m_asicPageStart + 1, std::min(count, m_asicPageStart + 4), count);
    lv_label_set_text(m_ui->ui_lbAsicPage, count > 0 ? buffer : "ASICs unavailable");
    if (board) {
        FiveTratumDisplay::formatOperatingPoint(buffer, sizeof(buffer), board->getAsicFrequency(), board->getAsicVoltageMillis());
        lv_label_set_text(m_ui->ui_lbSharedPoint, buffer);
    }
    const int64_t capturedNow = esp_timer_get_time();
    for (int row = 0; row < 4; ++row) {
        const int index = m_asicPageStart + row;
        snprintf(buffer, sizeof(buffer), "ASIC %d", index + 1);
        lv_label_set_text(m_ui->ui_lbChipIds[row], index < count ? buffer : "--");
        const bool readable = board && board->isInitialized() && !board->isShutdown() && index < count;
        FiveTratumTelemetry::ChipHashrateSample rate{};
        if (readable && board->hasHashrateCounter()) rate = HASHRATE_MONITOR.getChipHashrateSample(index);
        FiveTratumDisplay::formatChipHashrate(buffer, sizeof(buffer), rate.ghPerSecond, readable && rate.isFresh(capturedNow));
        lv_label_set_text(m_ui->ui_lbChipRates[row], buffer);
    }
    updateThermalReadings(capturedNow);
}

void DisplayDriver::updateThermalReadings(int64_t nowUs) {
    if (!m_ui || !m_ui->ui_MiningScreen || !m_ui->ui_AsicScreen) return;
    const auto sample = POWER_MANAGEMENT_MODULE.getTemperatureSnapshot();
    Board *board = SYSTEM_MODULE.getBoard();
    const bool readable = board && board->isInitialized() && !board->isShutdown() &&
                          !POWER_MANAGEMENT_MODULE.isShutdown();
    const bool fresh = readable && sample.isFresh(nowUs);
    m_temperatureValidUntilUs = fresh ? sample.capturedAtUs + 15000000 : -1;
    FiveTratumDisplay::updateSummaryTemperatures(m_ui->ui_lbTemp, m_ui->ui_lbVRExternalTemp,
        m_ui->ui_lbVRInternalTemp, sample.asicBoardC, sample.vrExternalC, sample.vrInternalC, fresh);
    float chipC[4]{};
    const int count = board ? std::min(64, std::max(0, board->getAsicCount())) : 0;
    for (int row = 0; row < 4; ++row) {
        const int index = m_asicPageStart + row;
        if (fresh && index < count) chipC[row] = board->getChipTemp(index);
    }
    FiveTratumDisplay::updateAsicTemperatures(m_ui->ui_lbAsicThermal, m_ui->ui_lbChipTemperatureTitle,
        m_ui->ui_lbChipTemps, chipC, sample.asicBoardC, sample.vrExternalC, sample.vrInternalC, fresh);
    char buffer[80], temperature[20];
    FiveTratumDisplay::formatTemperature(temperature, sizeof(temperature), fresh ? sample.asicBoardC : 0.0f);
    lv_label_set_text(m_ui->ui_lblTempPrice, temperature);
    const float brandPower = POWER_MANAGEMENT_MODULE.getPower();
    if (std::isfinite(brandPower) && brandPower > 0)
        snprintf(buffer, sizeof(buffer), "%.1f W / %s", static_cast<double>(brandPower), temperature);
    else snprintf(buffer, sizeof(buffer), "-- W / %s", temperature);
    FiveTratumDisplay::setBoundedCoinName(m_ui->ui_lbBrandPowerTemp, buffer);
    snprintf(buffer, sizeof(buffer), "Board %s  |  Fan %d RPM", temperature, POWER_MANAGEMENT_MODULE.getFanRPM(0));
    lv_label_set_text(m_ui->ui_lbTempFan, buffer);
}

void DisplayDriver::updateMuxStatus(FiveTratumDisplay::MuxState state) {
    PThreadGuard lock(m_lvglMutex);
    m_muxStatus = state;
    if (m_ui && m_ui->ui_lbMux) lv_label_set_text(m_ui->ui_lbMux, FiveTratumDisplay::muxText(state));
}

void DisplayDriver::updateBTCprice(void)
{
    char price_str[32];
    m_btcPrice = isBitcoinCoinContext(m_coinContext) ? APIs_FETCHER.getPrice() : 0;
    FiveTratumDisplay::formatCoinPrice(price_str, sizeof(price_str), m_coinContext, m_btcPrice);
    lv_label_set_text(m_ui->ui_lblBTCPrice, price_str); // Update label
}

void DisplayDriver::updateCoinContext(const StratumManager::MuxDisplayState &view) {
    m_coinContext = selectMuxCoinContext(view.pools[0], view.pools[1], view.dual, view.activePool);
    const bool bitcoin = isBitcoinCoinContext(m_coinContext);
    const bool shouldFetch = bitcoin && m_state == UiState::BTCScreen;
    if (shouldFetch != m_bitcoinFetchingEnabled) {
        if (shouldFetch) APIs_FETCHER.enableFetching();
        else APIs_FETCHER.disableFetching();
        m_bitcoinFetchingEnabled = shouldFetch;
    }
    char buffer[80];
    FiveTratumDisplay::formatHashrateCoinTitle(buffer, sizeof(buffer), m_coinContext);
    lv_label_set_text(m_ui->ui_lbCoinTitle, buffer);
    snprintf(buffer, sizeof(buffer), "Mining%s%.12s", m_coinContext.available ? " / " : "",
             m_coinContext.available ? m_coinContext.ticker : "");
    FiveTratumDisplay::setBoundedCoinName(m_ui->ui_lbBrandCoin, buffer);
    const FiveTratumDisplay::CoinWidgets fields{m_ui->ui_lblCoinIdentity, m_ui->ui_lblCoinName,
        m_ui->ui_lblCoinSource, m_ui->ui_lblBTCPrice, m_ui->ui_lblHashPrice, m_ui->ui_lblTempPrice};
    FiveTratumDisplay::updateCoinIdentity(fields, m_coinContext, view.pools[0].coin,
                                         view.pools[1].coin, view.dual, 0);
    updateGlobalMiningStats(view);
}

void DisplayDriver::updateGlobalMiningStats(const StratumManager::MuxDisplayState &view)
{
    FiveTratumDisplay::MiningJobWidgets fields{};
    MuxCoinContext coins[2]{};
    MuxWorkContext work[2]{};
    for (int i = 0; i < 2; ++i) {
        fields.coin[i] = m_ui->ui_lblJobCoin[i];
        fields.height[i] = m_ui->ui_lblJobHeight[i];
        fields.difficulty[i] = m_ui->ui_lblJobDifficulty[i];
        fields.nBits[i] = m_ui->ui_lblJobNBits[i];
        fields.age[i] = m_ui->ui_lblJobAge[i];
        if (view.pools[i].connected && view.pools[i].acknowledged) {
            coins[i] = view.pools[i].coin;
            work[i] = view.pools[i].workContext;
        }
    }
    fields.source = m_ui->ui_lblJobSource;
    FiveTratumDisplay::updateMiningJobs(fields, coins, work, view.dual, view.activePool);
}

void DisplayDriver::updateGlobalState(int pool) {
    PThreadGuard lock(m_lvglMutex);
    if (!m_ui || !m_ui->ui_MiningScreen || !m_ui->ui_SettingsScreen) return;
    char buffer[80];
    Board *board = SYSTEM_MODULE.getBoard();
    const int rpm = POWER_MANAGEMENT_MODULE.getFanRPM(0);
    snprintf(buffer, sizeof(buffer), "%d RPM", rpm);
    lv_label_set_text(m_ui->ui_lbRPM, board && board->isInitialized() && rpm >= 0 ? buffer : "-- RPM");
    if (board && board->isInitialized() && !board->isShutdown()) {
        const float inputMv = POWER_MANAGEMENT_MODULE.getVoltage();
        const float currentMa = POWER_MANAGEMENT_MODULE.getCurrent();
        const float vout = board->getVout();
        char input[16], current[16], output[16];
        if (std::isfinite(inputMv) && inputMv > 0) snprintf(input, sizeof(input), "%.2f V", static_cast<double>(inputMv) / 1000.0);
        else snprintf(input, sizeof(input), "--");
        if (std::isfinite(currentMa) && currentMa >= 0) snprintf(current, sizeof(current), "%.2f A", static_cast<double>(currentMa) / 1000.0);
        else snprintf(current, sizeof(current), "--");
        if (std::isfinite(vout) && vout > 0) snprintf(output, sizeof(output), "%.3f V", static_cast<double>(vout));
        else snprintf(output, sizeof(output), "--");
        snprintf(buffer, sizeof(buffer), "Last board: %s / %s / Core %s", input, current, output);
    } else {
        snprintf(buffer, sizeof(buffer), "Last board: input -- / current -- / Core --");
    }
    lv_label_set_text(m_ui->ui_lbElectrical, buffer);
    updateTime(&SYSTEM_MODULE);
    updateShares(STRATUM_MANAGER, pool);
    updateHashrate(&SYSTEM_MODULE, STRATUM_MANAGER, POWER_MANAGEMENT_MODULE.getPower(), pool);
    updateAsicReadings();
    if (!STRATUM_MANAGER) snprintf(buffer, sizeof(buffer), "Link --");
    else if (STRATUM_MANAGER->isDualPool()) snprintf(buffer, sizeof(buffer), "Links %d/2", STRATUM_MANAGER->getNumConnectedPools());
    else snprintf(buffer, sizeof(buffer), "Link %s", STRATUM_MANAGER->getNumConnectedPools() > 0 ? "up" : "down");
    lv_label_set_text(m_ui->ui_lbRoute, buffer);
    if (!STRATUM_MANAGER) {
        m_muxStatus = FiveTratumDisplay::MuxState::Unavailable;
        lv_label_set_text(m_ui->ui_lbMux, FiveTratumDisplay::muxText(m_muxStatus));
        updateCoinContext({});
    } else {
        const auto view = STRATUM_MANAGER->getMuxDisplayState();
        const auto &p1 = view.pools[0];
        const auto &p2 = view.pools[1];
        const auto primary = FiveTratumDisplay::muxState(true, p1.connected, p1.acknowledged, p1.expired);
        const auto secondary = FiveTratumDisplay::muxState(true, p2.connected, p2.acknowledged, p2.expired);
        const int index = view.activePool;
        m_muxStatus = index == 1 ? secondary : primary;
        FiveTratumDisplay::formatMuxStatus(buffer, sizeof(buffer), primary, secondary, view.dual, index);
        lv_label_set_text(m_ui->ui_lbMux, buffer);
        updateCoinContext(view);
    }
    updateBTCprice();
    lv_label_set_text(m_ui->ui_lbBrandMux, lv_label_get_text(m_ui->ui_lbMux));
    m_telemetryUpdatedAtUs = now_us();
}

void DisplayDriver::updateIpAddress(const char *ip_address_str)
{
    PThreadGuard lock(m_lvglMutex);
    if (m_ui->ui_MiningScreen == NULL)
        return;
    if (m_ui->ui_SettingsScreen == NULL)
        return;

    lv_label_set_text(m_ui->ui_lbIP, ip_address_str);    // Update label
    lv_label_set_text(m_ui->ui_lbIPSet, ip_address_str); // Update label
}

void DisplayDriver::setNetworkIcon(bool eth_connected)
{
    PThreadGuard lock(m_lvglMutex);
    if (!m_ui || m_ui->ui_imgNet == nullptr)
        return;

    if (eth_connected)
    {
        lv_img_set_src(m_ui->ui_imgNet, &ui_img_eth_png);
    }
    else
    {
        lv_img_set_src(m_ui->ui_imgNet, &ui_img_wifi_png);
    }
}

void DisplayDriver::setCanIcon()
{
    PThreadGuard lock(m_lvglMutex);
    if (!m_ui || m_ui->ui_imgNet == nullptr)
        return;

    lv_img_set_src(m_ui->ui_imgNet, &ui_img_can_png);
}

void DisplayDriver::logMessage(const char *message)
{
    PThreadGuard lock(m_lvglMutex);
    if (m_ui->ui_LogScreen == NULL)
        m_ui->logScreenInit();
    lv_label_set_text(m_ui->ui_LogLabel, message);
    enableLvglAnimations(true);
    _ui_screen_change(m_ui->ui_LogScreen, LV_SCR_LOAD_ANIM_NONE, 500, 0);
}

void DisplayDriver::miningScreen(void)
{
    PThreadGuard lock(m_lvglMutex);
    if (m_startupGate.requestMining()) enterState(UiState::Mining, now_us());
}

void DisplayDriver::renderBootStatus() {
    if (!m_ui) return;
    for (int page = 0; page < 2; ++page) {
        if (!(page ? m_ui->ui_Splash2 : m_ui->ui_Splash1)) continue;
        FiveTratumDisplay::BootWidgets fields{};
        for (int i = 0; i < 4; ++i) fields.lines[i] = m_ui->ui_BootLines[page][i];
        FiveTratumDisplay::updateBootStatus(fields, m_bootHistory);
    }
}

void DisplayDriver::recordBootStage(const char *stage) {
    PThreadGuard lock(m_lvglMutex);
    if (m_bootHistory.push(stage)) renderBootStatus();
}

void DisplayDriver::finishBoot(const char *stage) {
    PThreadGuard lock(m_lvglMutex);
    if (m_bootHistory.push(stage)) renderBootStatus();
    if (m_startupGate.complete() && m_state != UiState::PowerOff &&
        m_state != UiState::ShowQR && m_state != UiState::Identify)
        enterState(UiState::Mining, now_us());
}


void DisplayDriver::portalScreen(const char *message)
{
    PThreadGuard lock(m_lvglMutex);
    strlcpy(m_portalWifiName, message, sizeof(m_portalWifiName));
    lv_label_set_text(m_ui->ui_lbSSID, m_portalWifiName);
    enterState(UiState::Portal, now_us());
}

void DisplayDriver::updateWifiStatus(const char *message)
{
    recordBootStage(message);
}

void DisplayDriver::buttonsInit(void)
{
    gpio_pad_select_gpio(PIN_BUTTON_1);
    gpio_set_direction(PIN_BUTTON_1, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BUTTON_1, GPIO_PULLUP_ONLY);

    gpio_pad_select_gpio(PIN_BUTTON_2);
    gpio_set_direction(PIN_BUTTON_2, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BUTTON_2, GPIO_PULLUP_ONLY);
}

/**
 * @brief Program starts from here
 *
 */
void DisplayDriver::init(Board* board)
{
    ESP_LOGI("INFO", "Setting Up TDisplayS3 Screen");

    // Inicializa el GPIO para el botón
    buttonsInit();

    // init the ipc
    ui_ipc_init();

    lv_obj_t *scr = initTDisplayS3();

    m_ui = new UI();
    m_ui->init(board, this);
    recordBootStage("Display ready");
    // manual_lvgl_update();

    // startUpdateScreenTask(); //Start screen update task
    mainCreatSysteTasks();
}
/**************************  Useful Electronics  ****************END OF FILE***/
