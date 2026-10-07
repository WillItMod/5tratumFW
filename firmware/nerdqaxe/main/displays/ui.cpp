#include "global_state.h"
#include "ui_helpers.h"
#include "ui.h"
#include "display_layout.h"

#include "displayDriver.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_app_desc.h"
#include "../macros.h"
#include <algorithm>


#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"

UI::UI() {
    m_last_screen_change_time = 0;
}

///////////////////// FUNCTIONS ////////////////////

void on_screen_loaded(lv_event_t * e)
{
    DisplayDriver* driver = static_cast<DisplayDriver*>(lv_event_get_user_data(e));
    driver->setScreenAnimationRunning(false);
}

// Every physical screen uses the same native 5tratumFW layout. Board themes
// remain available upstream but are not shown by this firmware display.
void UI::splash1ScreenInit() {
    ui_Splash1 = FiveTratumDisplay::screen();
    const auto fields = FiveTratumDisplay::bootStatus(ui_Splash1, m_board->getDeviceModel(), esp_app_get_description()->version);
    for (int i = 0; i < 4; ++i) ui_BootLines[0][i] = fields.lines[i];
}

void UI::splash2ScreenInit() {
    ui_Splash2 = FiveTratumDisplay::screen();
    const auto fields = FiveTratumDisplay::bootStatus(ui_Splash2, m_board->getDeviceModel(), esp_app_get_description()->version);
    for (int i = 0; i < 4; ++i) ui_BootLines[1][i] = fields.lines[i];
    ui_lbConnect = fields.lines[3];
    lv_obj_add_event_cb(ui_Splash2, on_screen_loaded, LV_EVENT_SCREEN_LOADED, m_display);
}

void UI::brandScreenInit() {
    ui_BrandScreen = FiveTratumDisplay::screen();
    const auto fields = FiveTratumDisplay::brandSummary(ui_BrandScreen, m_board->getDeviceModel());
    ui_lbBrandCoin = fields.coin;
    ui_lbBrandRate = fields.hashrate;
    ui_lbBrandPowerTemp = fields.powerTemp;
    ui_lbBrandMux = fields.mux;
    lv_obj_add_event_cb(ui_BrandScreen, on_screen_loaded, LV_EVENT_SCREEN_LOADED, m_display);
}

void UI::portalScreenInit() {
    ui_PortalScreen = FiveTratumDisplay::screen();
    ui_lbSSID = FiveTratumDisplay::portal(ui_PortalScreen);
    lv_obj_add_event_cb(ui_PortalScreen, on_screen_loaded, LV_EVENT_SCREEN_LOADED, m_display);
}

void UI::miningScreenInit() {
    ui_MiningScreen = FiveTratumDisplay::screen();
    const auto fields = FiveTratumDisplay::summary(ui_MiningScreen);
    ui_lbHashrate = fields.hashrate;
    ui_lbHashrateUnit = fields.rateUnit;
    ui_lbPowerEfficiency = fields.powerEfficiency;
    ui_lbSummaryShares = fields.shares;
    ui_lbRoute = fields.route;
    ui_lbMux = fields.mux;
    ui_lbCoinTitle = fields.coinTitle;
    ui_lbIP = fields.ip;
    ui_lbTempFan = fields.tempFan;
    // Compatibility sinks for existing runtime update calls. Detailed readings
    // are shown on the dedicated pages, without duplicating the summary.
    ui_lbVinput = FiveTratumDisplay::hiddenValue(ui_MiningScreen);
    ui_lbVcore = FiveTratumDisplay::hiddenValue(ui_MiningScreen);
    ui_lbIntensidad = FiveTratumDisplay::hiddenValue(ui_MiningScreen);
    ui_lbPower = fields.power;
    ui_lbEficiency = fields.efficiency;
    ui_lbTemp = fields.temperature;
    ui_lbVRExternalTemp = fields.vrExternal;
    ui_lbVRInternalTemp = fields.vrInternal;
    ui_lbTime = FiveTratumDisplay::hiddenValue(ui_MiningScreen);
    ui_lbBestDifficulty = FiveTratumDisplay::hiddenValue(ui_MiningScreen);
    ui_lbRPM = fields.fan;
    ui_lbASIC = FiveTratumDisplay::hiddenValue(ui_MiningScreen);
    ui_imgNet = lv_img_create(ui_MiningScreen);
    lv_img_set_src(ui_imgNet, &ui_img_wifi_png);
    lv_obj_set_pos(ui_imgNet, 294, 6);
    lv_obj_clear_flag(ui_imgNet, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ui_MiningScreen, on_screen_loaded, LV_EVENT_SCREEN_LOADED, m_display);
}

void UI::asicScreenInit() {
    ui_AsicScreen = FiveTratumDisplay::screen();
    const auto fields = FiveTratumDisplay::asics(ui_AsicScreen);
    ui_lbSharedPoint = fields.point;
    ui_lbAsicPage = fields.page;
    ui_lbAsicThermal = fields.thermal;
    ui_lbChipTemperatureTitle = fields.temperatureTitle;
    for (int i = 0; i < 4; ++i) {
        ui_lbChipTemps[i] = fields.temperatures[i];
        ui_lbChipRates[i] = fields.hashrates[i];
        ui_lbChipIds[i] = fields.indices[i];
    }
    lv_obj_add_event_cb(ui_AsicScreen, on_screen_loaded, LV_EVENT_SCREEN_LOADED, m_display);
}

void UI::settingsScreenInit() {
    ui_SettingsScreen = FiveTratumDisplay::screen();
    const auto fields = FiveTratumDisplay::settings(ui_SettingsScreen);
    ui_lbIPSet = fields.ip;
    ui_lbPoolNr = fields.pool;
    ui_lbPoolSet = fields.host;
    ui_lbPortSet = fields.port;
    ui_lbFreqSet = fields.frequency;
    ui_lbVcoreSet = fields.voltage;
    ui_lbFanSet = fields.fan;
    ui_lbShares = fields.shares;
    ui_lbBestDifficultySet = fields.best;
    ui_lbTime = fields.uptime;
    ui_lbElectrical = fields.electrical;
    ui_lbHashrateSet = FiveTratumDisplay::hiddenValue(ui_SettingsScreen);
    lv_obj_add_event_cb(ui_SettingsScreen, on_screen_loaded, LV_EVENT_SCREEN_LOADED, m_display);
}

void UI::logScreenInit() {
    ui_LogScreen = FiveTratumDisplay::screen();
    FiveTratumDisplay::header(ui_LogScreen, "SYSTEM LOG");
    ui_LogLabel = FiveTratumDisplay::text(ui_LogScreen, "", 8, 38, 304);
    lv_label_set_long_mode(ui_LogLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(ui_LogLabel, 124);
}

void UI::bTCScreenInit() {
    ui_BTCScreen = FiveTratumDisplay::screen();
    const auto fields = FiveTratumDisplay::coin(ui_BTCScreen);
    ui_lblCoinIdentity = fields.identity;
    ui_lblCoinName = fields.name;
    ui_lblCoinSource = fields.source;
    ui_lblBTCPrice = fields.price;
    ui_lblHashPrice = fields.hashrate;
    ui_lblTempPrice = fields.temperature;
    lv_obj_add_event_cb(ui_BTCScreen, on_screen_loaded, LV_EVENT_SCREEN_LOADED, m_display);
}

void UI::globalStatsScreenInit() {
    ui_GlobalStats = FiveTratumDisplay::screen();
    const auto fields = FiveTratumDisplay::miningJobs(ui_GlobalStats);
    for (int i = 0; i < 2; ++i) {
        ui_lblJobCoin[i] = fields.coin[i];
        ui_lblJobHeight[i] = fields.height[i];
        ui_lblJobDifficulty[i] = fields.difficulty[i];
        ui_lblJobNBits[i] = fields.nBits[i];
        ui_lblJobAge[i] = fields.age[i];
    }
    ui_lblJobSource = fields.source;
    lv_obj_add_event_cb(ui_GlobalStats, on_screen_loaded, LV_EVENT_SCREEN_LOADED, m_display);
}
bool UI::createQRScreen(uint8_t *buf, int size) {
    // The enrollment encoder is capped at version10 (57 modules). With a
    // four-module quiet zone and >=2px modules, its130px maximum fits below
    // the header. Refuse larger inputs rather than clip a security QR.
    if (!buf || size < 21 || size > 57 || (size - 21) % 4 != 0) return false;
    const int quiet = 4;
    const int scale = std::max(2, 136 / (size + 2 * quiet));
    const int side = (size + 2 * quiet) * scale;
    const size_t bytes = static_cast<size_t>(side) * side * sizeof(lv_color_t);
    lv_color_t *newBuffer = m_qr_canvas_buf;
    if (!newBuffer || side != m_qr_canvas_w) {
        newBuffer = static_cast<lv_color_t*>(MALLOC(bytes));
        if (!newBuffer) return false;
    }
    if (!ui_qrScreen) {
        ui_qrScreen = FiveTratumDisplay::screen();
        m_qr_canvas = FiveTratumDisplay::authenticator(ui_qrScreen, newBuffer, side);
        lv_obj_add_event_cb(ui_qrScreen, on_screen_loaded, LV_EVENT_SCREEN_LOADED, m_display);
    } else if (newBuffer != m_qr_canvas_buf) {
        lv_canvas_set_buffer(m_qr_canvas, newBuffer, side, side, LV_IMG_CF_TRUE_COLOR);
        lv_obj_align(m_qr_canvas, LV_ALIGN_RIGHT_MID, -8, 16);
    }
    if (newBuffer != m_qr_canvas_buf && m_qr_canvas_buf) FREE(m_qr_canvas_buf);
    m_qr_canvas_buf = newBuffer;
    m_qr_canvas_w = side;
    lv_draw_rect_dsc_t background;
    lv_draw_rect_dsc_init(&background);
    background.bg_color = lv_color_white();
    lv_canvas_draw_rect(m_qr_canvas, 0, 0, side, side, &background);
    lv_draw_rect_dsc_t module;
    lv_draw_rect_dsc_init(&module);
    module.bg_color = lv_color_black();
    module.border_opa = LV_OPA_TRANSP;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (qrcodegen_getModule(buf, x, y)) {
                lv_canvas_draw_rect(m_qr_canvas, (quiet + x) * scale, (quiet + y) * scale,
                                    scale, scale, &module);
            }
        }
    }
    return true;
}

void UI::powerOffScreenInit() {
    ui_PowerOffScreen = FiveTratumDisplay::screen();
    FiveTratumDisplay::header(ui_PowerOffScreen, "POWER OFF");
    FiveTratumDisplay::text(ui_PowerOffScreen, "Mining stopped", 8, 58, 304, &ui_font_OpenSansBold24, FiveTratumDisplay::Cyan);
    FiveTratumDisplay::text(ui_PowerOffScreen, "Safe to disconnect power.", 8, 107, 304, &ui_font_OpenSansBold14);
    lv_obj_add_event_cb(ui_PowerOffScreen, on_screen_loaded, LV_EVENT_SCREEN_LOADED, m_display);
}

void UI::destroyQRScreen() {
}


// Function to show the overlay with an error message and custom colors
void UI::showErrorOverlay(const char *error_message, uint32_t error_code) {
    char detail[48];
    snprintf(detail, sizeof(detail), "Fault code #%08X", static_cast<unsigned int>(error_code));
    // The top layer survives screen transitions and always outranks telemetry.
    ui_errOverlayContainer = FiveTratumDisplay::warning(lv_layer_top(), error_message, detail);
}

void UI::hideErrorOverlay()
{
    if (ui_errOverlayContainer != NULL) {
        lv_obj_del(ui_errOverlayContainer); // Delete the overlay object and its children
        ui_errOverlayContainer = NULL;     // Clear the pointer to avoid dangling references
    }
}

// Function to show the overlay with a centered image
void UI::showImageOverlay(const lv_img_dsc_t *image) {
    // The existing event is a locally found candidate, not pool acceptance.
    (void)image;
    ui_imageOverlayContainer = FiveTratumDisplay::warning(lv_scr_act(), "Block candidate", "Pool acceptance is not verified.\nPress a button to dismiss.");
}

void UI::hideImageOverlay()
{
    if (ui_imageOverlayContainer != NULL) {
        lv_obj_del(ui_imageOverlayContainer);
        ui_imageOverlayContainer = NULL;
    }
}

void UI::init(Board* board, DisplayDriver *display)
{
    m_board = board;
    m_display = display;

    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *m_theme =
        lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, m_theme);

    splash1ScreenInit();
    splash2ScreenInit();
    portalScreenInit();
    miningScreenInit();
    asicScreenInit();
    settingsScreenInit();
    bTCScreenInit();
    globalStatsScreenInit();
    brandScreenInit();
    // ui_LogScreen_init();

    lv_disp_load_scr(ui_Splash1);

}
