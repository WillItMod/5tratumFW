// Actual LVGL8.3.11 offscreen rendering. All readings below are test fixtures,
// not live miners. No ESP-IDF, GPIO, ASIC, network or firmware upload code.
#include "displays/display_layout.h"
#include "displays/display_data.h"
#include "tasks/temperature_sample.h"
#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <limits>

static std::array<lv_color_t, 320 * 170> frame{};
static std::array<lv_color_t, 320 * 170> drawBuffer{};
static lv_obj_t *parkingScreen = nullptr;
static unsigned capturedPages = 0;

static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels) {
    size_t index = 0;
    for (int y = area->y1; y <= area->y2; ++y)
        for (int x = area->x1; x <= area->x2; ++x)
            frame[static_cast<size_t>(y) * 320 + x] = pixels[index++];
    lv_disp_flush_ready(driver);
}

static void validate(lv_obj_t *obj) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    lv_area_t bounds;
    lv_obj_get_coords(obj, &bounds);
    assert(bounds.x1 >= 0 && bounds.y1 >= 0 && bounds.x2 < 320 && bounds.y2 < 170);
    if (lv_obj_check_type(obj, &lv_img_class) && lv_img_get_zoom(obj) > LV_IMG_ZOOM_NONE) {
        lv_point_t pivot; lv_img_get_pivot(obj, &pivot);
        assert(pivot.x == 0 && pivot.y == 0); // Established logo layout anchors scaling at top-left.
        const int width = lv_obj_get_width(obj) * lv_img_get_zoom(obj) / LV_IMG_ZOOM_NONE;
        const int height = lv_obj_get_height(obj) * lv_img_get_zoom(obj) / LV_IMG_ZOOM_NONE;
        assert(bounds.x1 + width <= 320 && bounds.y1 + height <= 170);
    }
    // Long mode may truncate bounded hostnames, but core data and navigation
    // text must fit naturally without ellipsis or an unplanned second line.
    if (lv_obj_check_type(obj, &lv_label_class)) {
        const char *value = lv_label_get_text(obj);
        const lv_font_t *font = lv_obj_get_style_text_font(obj, 0);
        lv_point_t size;
        lv_txt_get_size(&size, value, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (std::string(value).find("a-very-long") == std::string::npos &&
            std::string(value).find('\n') == std::string::npos) {
            if (size.x > lv_obj_get_width(obj)) {
                std::cerr << "Clipped label: " << value << " needs " << size.x
                          << " pixels, width " << lv_obj_get_width(obj)
                          << " declared " << lv_obj_get_style_width(obj, 0)
                          << " at " << bounds.x1 << ',' << bounds.y1 << '\n';
                std::abort();
            }
        }
    }
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); ++i) validate(lv_obj_get_child(obj, i));
}

static void capture(lv_obj_t *page, const std::string &path) {
    ++capturedPages;
    lv_scr_load(page);
    lv_obj_update_layout(page);
    lv_obj_update_layout(lv_layer_top());
    validate(page);
    validate(lv_layer_top());
    lv_obj_invalidate(page); // Capture the complete frame, including unchanged glyph regions.
    lv_refr_now(nullptr);
    std::ofstream file(path, std::ios::binary);
    file << "P6\n320 170\n255\n";
    for (const auto pixel : frame) {
        lv_color32_t rgb;
        rgb.full = lv_color_to32(pixel);
        const char bytes[] = {static_cast<char>(rgb.ch.red), static_cast<char>(rgb.ch.green), static_cast<char>(rgb.ch.blue)};
        file.write(bytes, 3);
    }
    assert(file.good());
    // LVGL cannot load the next screen from a deleted active screen.
    lv_scr_load(parkingScreen);
    lv_obj_del(page);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    const std::string directory = argv[1];
    lv_init();
    static lv_disp_draw_buf_t draw;
    lv_disp_draw_buf_init(&draw, drawBuffer.data(), nullptr, drawBuffer.size());
    static lv_disp_drv_t driver;
    lv_disp_drv_init(&driver);
    driver.hor_res = 320; driver.ver_res = 170; driver.draw_buf = &draw; driver.flush_cb = flush;
    lv_disp_drv_register(&driver);
    parkingScreen = lv_scr_act();
    using namespace FiveTratumDisplay;
    char buffer[80];
    auto *page = screen();
    auto overview = summary(page);
    updateSummaryRate(overview.hashrate, overview.rateUnit, 3820);
    formatPowerEfficiency(buffer, sizeof(buffer), 84.2f, 3820); lv_label_set_text(overview.powerEfficiency, buffer);
    updatePowerLabels(overview.power, overview.efficiency, 84.2f, 3820);
    updateSummaryTemperatures(overview.temperature, overview.vrExternal, overview.vrInternal, 58.2f, 50, 91, true);
    lv_label_set_text(overview.fan, "4200 RPM");
    formatShares(buffer, sizeof(buffer), 12345, 2); lv_label_set_text(overview.shares, buffer);
    lv_label_set_text(overview.tempFan, "Max temp 58.2 C  |  Fan 4200 RPM");
    lv_label_set_text(overview.route, "Links 2/2");
    formatMuxStatus(buffer, sizeof(buffer), MuxState::Connected, MuxState::Unverified, true, 0);
    lv_label_set_text(overview.mux, buffer);
    lv_label_set_text(overview.ip, "192.0.2.10");
    capture(page, directory + "/display-summary-simulated.ppm");

    page = screen();
    auto details = asics(page);
    formatOperatingPoint(buffer, sizeof(buffer), 500, 1130); lv_label_set_text(details.point, buffer);
    const float measuredChipC[4]{56, 57, 58, 0};
    for (int i = 0; i < 4; ++i) {
        formatChipHashrate(buffer, sizeof(buffer), i == 2 ? 0 : 955, i != 3); lv_label_set_text(details.hashrates[i], buffer);
    }
    updateAsicTemperatures(details.thermal, details.temperatureTitle, details.temperatures,
                           measuredChipC, 58.2f, 50, 91, true);
    assert(!strcmp(lv_label_get_text(details.thermal), "Board 58.2 C | VR ext 50.0 C | int 91.0 C"));
    assert(!strcmp(lv_label_get_text(details.point), "Configured 500 MHz  /  1130 mV"));
    assert(!lv_obj_has_flag(details.temperatureTitle, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(details.temperatures[3], LV_OBJ_FLAG_HIDDEN));
    capture(page, directory + "/display-asics-simulated.ppm");
    page = screen();
    details = asics(page);
    lv_label_set_text(details.page, "ASIC 5-8 / 8");
    const float partialChipC[4]{58, 59, 0, 0};
    for (int i = 0; i < 4; ++i) {
        std::snprintf(buffer, sizeof(buffer), "ASIC %d", i + 5); lv_label_set_text(details.indices[i], buffer);
        formatChipHashrate(buffer, sizeof(buffer), i == 2 ? 0 : 975 + i * 10, i != 3); lv_label_set_text(details.hashrates[i], buffer);
    }
    updateAsicTemperatures(details.thermal, details.temperatureTitle, details.temperatures,
                           partialChipC, 59, 50, 91, true);
    capture(page, directory + "/display-asics-5-8-unavailable.ppm");

    page = screen();
    auto config = settings(page);
    lv_label_set_text(config.pool, "Primary pool");
    lv_label_set_text(config.ip, "192.0.2.10");
    lv_label_set_text(config.host, "a-very-long-simulated-hostname.example.invalid");
    lv_label_set_text(config.port, "Port 7331  |  Shared chain route");
    lv_label_set_text(config.frequency, "500 MHz"); lv_label_set_text(config.voltage, "1130 mV");
    lv_label_set_text(config.fan, "Fan auto");
    formatShares(buffer, sizeof(buffer), 12345, 2); lv_label_set_text(config.shares, buffer);
    lv_label_set_text(config.best, "999.99P");
    lv_label_set_text(config.uptime, "123d 23h 59m 59s");
    lv_label_set_text(config.electrical, "Last board: 12.13 V / 6.94 A / Core 1.130 V");
    capture(page, directory + "/display-settings-simulated.ppm");

    page = screen(); splash(page, "NerdQAxe++", false); capture(page, directory + "/display-splash.ppm");
    page = screen(); splash(page, "NerdQAxe++", true); capture(page, directory + "/display-connecting.ppm");
    page = screen(); auto *ssid = portal(page); lv_label_set_text(ssid, "5tratumFW_TEST"); capture(page, directory + "/display-portal-simulated.ppm");
    page = screen();
    auto *fault = warning(lv_layer_top(), "MINER OVERHEATED", "Fault code #00000001");
    // Faults remain visible across normal page changes and splash cleanup.
    capture(page, directory + "/display-safety-simulated.ppm");
    page = screen(); summary(page);
    lv_obj_update_layout(lv_layer_top());
    assert(lv_obj_get_parent(fault) == lv_layer_top());
    capture(page, directory + "/display-safety-after-page-change.ppm");
    lv_obj_del(fault);
    page = screen(); warning(page, "Block candidate", "Pool acceptance is not verified.\nPress a button to dismiss."); capture(page, directory + "/display-candidate-simulated.ppm");
    page = screen(); overview = summary(page);
    updateSummaryRate(overview.hashrate, overview.rateUnit, std::nanf(""));
    updatePowerLabels(overview.power, overview.efficiency, 84.2f, std::nanf(""));
    lv_obj_update_layout(page);
    assert(!std::strcmp(lv_label_get_text(overview.hashrate), "--"));
    assert(!std::strcmp(lv_label_get_text(overview.efficiency), "-- J/TH"));
    capture(page, directory + "/display-unavailable.ppm");
    page = screen(); overview = summary(page);
    updateSummaryRate(overview.hashrate, overview.rateUnit, 0);
    updatePowerLabels(overview.power, overview.efficiency, 12.0f, 0);
    lv_obj_update_layout(page);
    assert(!std::strcmp(lv_label_get_text(overview.hashrate), "0"));
    assert(!std::strcmp(lv_label_get_text(overview.rateUnit), "GH/s"));
    assert(!std::strcmp(lv_label_get_text(overview.efficiency), "-- J/TH"));
    formatMuxStatus(buffer, sizeof(buffer), MuxState::Disconnected, MuxState::Disconnected, true, 0);
    lv_label_set_text(overview.mux, buffer);
    capture(page, directory + "/display-dual-disconnected.ppm");
    std::array<lv_color_t, 130 * 130> qr{};
    for (auto &pixel : qr) pixel = lv_color_white();
    for (int y = 8; y < 122; y += 4)
        for (int x = 8; x < 122; x += 4)
            if ((x + y) % 3) for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) qr[(y + dy) * 130 + x + dx] = lv_color_black();
    page = screen(); authenticator(page, qr.data(), 130);
    capture(page, directory + "/display-enrollment-layout-simulated.ppm");
    const MuxCoinContext btc{true, "bitcoin", "BTC", "Bitcoin"};
    const MuxCoinContext dgb{true, "digibyte", "DGB", "DigiByte"};
    for (const auto &fixture : {btc, dgb, MuxCoinContext{}}) {
        page = screen();
        auto fields = coin(page);
        updateCoinIdentity(fields, fixture, fixture, {}, false, 98765);
        lv_label_set_text(fields.hashrate, "4.06 TH/s");
        lv_label_set_text(fields.temperature, "54.7 C");
        lv_obj_update_layout(page);
        if (fixture.available) assert(!strcmp(lv_label_get_text(fields.name), fixture.name));
        assert(!strcmp(lv_label_get_text(fields.price), isBitcoinCoinContext(fixture) ? "BTC price $98765" : "Price unavailable"));
        capture(page, directory + "/display-coin-" + (fixture.available ? std::string(fixture.ticker) : "unknown") + "-simulated.ppm");
    }
    page = screen();
    auto coinFields = coin(page);
    updateCoinIdentity(coinFields, {}, dgb, btc, true, 98765);
    lv_obj_update_layout(page);
    assert(!strcmp(lv_label_get_text(coinFields.name), "P1 route: DGB"));
    assert(!strcmp(lv_label_get_text(coinFields.source), "P2 route: BTC"));
    assert(!strcmp(lv_label_get_text(coinFields.price), "Price unavailable"));
    capture(page, directory + "/display-coin-dual-simulated.ppm");
    page = screen();
    coinFields = coin(page);
    const MuxCoinContext maximal{true, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "WWWWWWWWWWWW",
        "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW"};
    updateCoinIdentity(coinFields, maximal, maximal, {}, false, 98765);
    capture(page, directory + "/display-coin-bounded-simulated.ppm");
    page = screen(); overview = summary(page);
    formatHashrateCoinTitle(buffer, sizeof(buffer), dgb); lv_label_set_text(overview.coinTitle, buffer);
    updateSummaryRate(overview.hashrate, overview.rateUnit, 4064);
    updatePowerLabels(overview.power, overview.efficiency, 82.5f, 4064);
    updateSummaryTemperatures(overview.temperature, overview.vrExternal, overview.vrInternal, 54.7f, 50, 91, true);
    lv_label_set_text(overview.fan, "4200 RPM");
    capture(page, directory + "/display-summary-DGB-simulated.ppm");
    const MuxWorkContext dgbJob{true, "dgb-job", "1b0404cb", true, 20000000U, true, 16307.420938523983, 3};
    const MuxWorkContext btcJob{true, "btc-job", "1d00ffff", true, 900000U, true, 1, 89};
    MuxCoinContext jobCoins[2]{dgb, {}};
    MuxWorkContext jobs[2]{dgbJob, {}};
    page = screen(); auto jobFields = miningJobs(page);
    updateMiningJobs(jobFields, jobCoins, jobs, false, 0);
    assert(!strcmp(lv_label_get_text(jobFields.coin[0]), "P1 / DGB"));
    assert(!strcmp(lv_label_get_text(jobFields.height[0]), "Block 20000000"));
    assert(!strcmp(lv_label_get_text(jobFields.difficulty[0]), "16.3K"));
    assert(!strcmp(lv_label_get_text(jobFields.height[1]), "Block --"));
    capture(page, directory + "/display-mining-job-DGB-simulated.ppm");
    page = screen(); jobFields = miningJobs(page);
    jobCoins[1] = btc; jobs[1] = btcJob;
    updateMiningJobs(jobFields, jobCoins, jobs, true, 0);
    assert(!strcmp(lv_label_get_text(jobFields.height[0]), "Block 20000000"));
    assert(!strcmp(lv_label_get_text(jobFields.height[1]), "Block 900000"));
    assert(!strcmp(lv_label_get_text(jobFields.age[1]), "Job 89s old"));
    capture(page, directory + "/display-mining-job-dual-simulated.ppm");
    page = screen(); jobFields = miningJobs(page);
    jobCoins[0] = {}; jobs[0] = {};
    updateMiningJobs(jobFields, jobCoins, jobs, false, 1);
    assert(!strcmp(lv_label_get_text(jobFields.height[0]), "Block --"));
    assert(!strcmp(lv_label_get_text(jobFields.source), "Candidate block / bdiff / P2 active"));
    capture(page, directory + "/display-mining-job-fallback-simulated.ppm");
    page = screen(); jobFields = miningJobs(page);
    jobCoins[1] = {}; jobs[1] = {};
    updateMiningJobs(jobFields, jobCoins, jobs, true, 0);
    for (int i = 0; i < 2; ++i) {
        assert(!strcmp(lv_label_get_text(jobFields.height[i]), "Block --"));
        assert(!strcmp(lv_label_get_text(jobFields.difficulty[i]), "--"));
        assert(!strcmp(lv_label_get_text(jobFields.nBits[i]), "nBits --"));
        assert(!strcmp(lv_label_get_text(jobFields.age[i]), "Job unavailable"));
    }
    capture(page, directory + "/display-mining-job-unknown-simulated.ppm");
    page = screen(); jobFields = miningJobs(page);
    jobCoins[0] = dgb; jobs[0] = dgbJob;
    jobs[0].heightAvailable = false; jobs[0].difficultyAvailable = false;
    updateMiningJobs(jobFields, jobCoins, jobs, false, 0);
    assert(!strcmp(lv_label_get_text(jobFields.height[0]), "Block --"));
    assert(!strcmp(lv_label_get_text(jobFields.difficulty[0]), "--"));
    assert(!strcmp(lv_label_get_text(jobFields.nBits[0]), "nBits 1b0404cb"));
    capture(page, directory + "/display-mining-job-partial-simulated.ppm");
    page = screen(); jobFields = miningJobs(page);
    jobCoins[0] = maximal; jobs[0] = dgbJob;
    jobs[0].height = UINT32_MAX; jobs[0].networkDifficulty = std::numeric_limits<double>::max();
    updateMiningJobs(jobFields, jobCoins, jobs, false, 0);
    assert(!strcmp(lv_label_get_text(jobFields.height[0]), "Block 4294967295"));
    capture(page, directory + "/display-mining-job-bounded-simulated.ppm");
    BootStatusHistory bootHistory;
    bootHistory.push("Settings loaded"); bootHistory.push("Display ready");
    page = screen(); auto bootFields = bootStatus(page, "NerdQAxe++", "5tratumFW-qa-0.1.0-beta.1");
    updateBootStatus(bootFields, bootHistory);
    assert(!strcmp(lv_label_get_text(bootFields.lines[0]), ""));
    assert(!strcmp(lv_label_get_text(bootFields.lines[3]), "Display ready"));
    capture(page, directory + "/display-boot-early-simulated.ppm");
    page = screen(); bootFields = bootStatus(page, "NerdQAxe++", "5tratumFW-qa-0.1.0-beta.1");
    bootHistory.push("Network started"); bootHistory.push("Web server started"); bootHistory.push("Waiting for network IP");
    updateBootStatus(bootFields, bootHistory);
    assert(!strcmp(lv_label_get_text(bootFields.lines[0]), "Display ready"));
    assert(!strcmp(lv_label_get_text(bootFields.lines[3]), "Waiting for network IP"));
    capture(page, directory + "/display-boot-network-simulated.ppm");
    page = screen(); bootFields = bootStatus(page, "NerdQAxe++", "5tratumFW-qa-0.1.0-beta.1");
    for (const char *stage : {"Clock sync requested", "ASIC chain starting", "ASIC chain ready", "Waiting for Stratum work"})
        bootHistory.push(stage);
    updateBootStatus(bootFields, bootHistory);
    assert(!strcmp(lv_label_get_text(bootFields.lines[0]), "Clock sync requested"));
    assert(!strcmp(lv_label_get_text(bootFields.lines[3]), "Waiting for Stratum work"));
    capture(page, directory + "/display-boot-asics-simulated.ppm");
    page = screen(); bootFields = bootStatus(page, maximal.name, "5tratumFW-qa-0.1.0-beta.1");
    bootHistory.push(std::string(1000,'W').c_str()); updateBootStatus(bootFields, bootHistory);
    capture(page, directory + "/display-boot-bounded-simulated.ppm");
    page = screen(); auto brandFields = brandSummary(page, "NerdQAxe++");
    lv_label_set_text(brandFields.coin, "Mining / DGB");
    formatHashrate(buffer, sizeof(buffer), 4064); lv_label_set_text(brandFields.hashrate, buffer);
    lv_label_set_text(brandFields.powerTemp, "82.5 W / 54.7 C");
    formatMuxStatus(buffer, sizeof(buffer), MuxState::Connected, MuxState::Connected, true, 0);
    lv_label_set_text(brandFields.mux, buffer);
    capture(page, directory + "/display-brand-live-simulated.ppm");
    page = screen(); brandFields = brandSummary(page, "NerdQAxe++");
    capture(page, directory + "/display-brand-unknown-simulated.ppm");
    page = screen(); brandSummary(page, "NerdQAxe++");
    fault = warning(lv_layer_top(), "MINER OVERHEATED", "Fault code #00000001");
    assert(lv_obj_get_parent(fault) == lv_layer_top());
    capture(page, directory + "/display-brand-safety-priority-simulated.ppm");
    lv_obj_del(fault);

    // The QAxe's real sensor topology: shared board/regulator readings but
    // zero per-chip values. No shared temperature is ever copied to a chip.
    const float noChipC[4]{};
    FiveTratumTelemetry::TemperatureSample thermal{54, 50, 91, 100};
    page = screen(); details = asics(page);
    formatOperatingPoint(buffer, sizeof(buffer), 500, 1130); lv_label_set_text(details.point, buffer);
    for (int i = 0; i < 4; ++i) {
        formatChipHashrate(buffer, sizeof(buffer), 1000 + i, true);
        lv_label_set_text(details.hashrates[i], buffer);
    }
    updateAsicTemperatures(details.thermal, details.temperatureTitle, details.temperatures,
                           noChipC, thermal.asicBoardC, thermal.vrExternalC, thermal.vrInternalC, thermal.isFresh(100));
    assert(!strcmp(lv_label_get_text(details.thermal), "Board 54.0 C | VR ext 50.0 C | int 91.0 C"));
    assert(lv_obj_has_flag(details.temperatureTitle, LV_OBJ_FLAG_HIDDEN));
    for (auto *label : details.temperatures) {
        assert(lv_obj_has_flag(label, LV_OBJ_FLAG_HIDDEN));
        assert(!strcmp(lv_label_get_text(label), ""));
    }
    capture(page, directory + "/display-asics-shared-thermal-no-chip-simulated.ppm");

    page = screen(); overview = summary(page);
    updateSummaryRate(overview.hashrate, overview.rateUnit, 4006);
    updatePowerLabels(overview.power, overview.efficiency, 82.5f, 4006);
    lv_label_set_text(overview.fan, "4200 RPM");
    updateSummaryTemperatures(overview.temperature, overview.vrExternal, overview.vrInternal,
                              thermal.asicBoardC, thermal.vrExternalC, thermal.vrInternalC, thermal.isFresh(15000100));
    assert(!strcmp(lv_label_get_text(overview.temperature), "ASIC board 54.0 C"));
    assert(!strcmp(lv_label_get_text(overview.vrExternal), "VR external 50.0 C"));
    assert(!strcmp(lv_label_get_text(overview.vrInternal), "VR internal 91.0 C"));
    capture(page, directory + "/display-summary-shared-thermal-simulated.ppm");

    page = screen(); overview = summary(page);
    updateSummaryTemperatures(overview.temperature, overview.vrExternal, overview.vrInternal, 150, 150, 150, true);
    lv_label_set_text(overview.fan, "65535 RPM");
    capture(page, directory + "/display-summary-thermal-bounds-simulated.ppm");
    page = screen(); details = asics(page);
    updateAsicTemperatures(details.thermal, details.temperatureTitle, details.temperatures,
                           measuredChipC, 150, 150, 150, true);
    capture(page, directory + "/display-asics-thermal-bounds-simulated.ppm");

    page = screen(); details = asics(page);
    updateAsicTemperatures(details.thermal, details.temperatureTitle, details.temperatures,
                           measuredChipC, thermal.asicBoardC, thermal.vrExternalC, thermal.vrInternalC, thermal.isFresh(15000101));
    assert(!strcmp(lv_label_get_text(details.thermal), "Thermal readings unavailable / stale"));
    assert(lv_obj_has_flag(details.temperatureTitle, LV_OBJ_FLAG_HIDDEN));
    for (auto *label : details.temperatures) assert(lv_obj_has_flag(label, LV_OBJ_FLAG_HIDDEN));
    capture(page, directory + "/display-asics-thermal-expired-simulated.ppm");
    page = screen(); overview = summary(page);
    updateSummaryTemperatures(overview.temperature, overview.vrExternal, overview.vrInternal,
                              thermal.asicBoardC, thermal.vrExternalC, thermal.vrInternalC, thermal.isFresh(15000101));
    assert(!strcmp(lv_label_get_text(overview.temperature), "ASIC board --"));
    assert(!strcmp(lv_label_get_text(overview.vrExternal), "VR external --"));
    assert(!strcmp(lv_label_get_text(overview.vrInternal), "VR internal --"));
    capture(page, directory + "/display-summary-thermal-expired-simulated.ppm");
    page = screen(); details = asics(page);
    updateAsicTemperatures(details.thermal, details.temperatureTitle, details.temperatures,
                           noChipC, 0, std::nanf(""), -1, true);
    assert(!strcmp(lv_label_get_text(details.thermal), "Thermal readings unavailable"));
    capture(page, directory + "/display-asics-thermal-unavailable-simulated.ppm");
    page = screen(); overview = summary(page);
    updateSummaryTemperatures(overview.temperature, overview.vrExternal, overview.vrInternal, 54, 50, 91, true);
    fault = warning(lv_layer_top(), "MINER OVERHEATED", "Fault code #00000001");
    assert(lv_obj_get_parent(fault) == lv_layer_top());
    capture(page, directory + "/display-shared-thermal-safety-priority-simulated.ppm");
    lv_obj_del(fault);
    std::cout << "{\"passed\":true,\"nativeLvgl\":\"8.3.11\",\"resolution\":[320,170],\"pages\":"
              << capturedPages << ",\"fixtureOnly\":true}\n";
}
