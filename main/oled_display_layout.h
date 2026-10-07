#ifndef FIVE_TRATUM_OLED_DISPLAY_LAYOUT_H
#define FIVE_TRATUM_OLED_DISPLAY_LAYOUT_H

#include "lvgl.h"
#include "oled_display_data.h"
#include "oled_display_assets.h"
#include <string.h>

LV_FONT_DECLARE(lv_font_portfolio_6x8);

typedef struct {
    lv_obj_t *screen, *title, *rows[3], *rate, *unit, *temperature, *power, *thermal_fill, *qr;
    FiveTratumOledPage page;
} FiveTratumOledView;

/* Native 8x8 pixel glyphs: rate, temperature, power, fan, network. */
static const uint8_t oled_glyph_data[][16] = {
    {0,0,0,255,255,255,255,255, 0x7e,0x42,0x5a,0x5a,0x42,0x7e,0x24,0x24},
    {0,0,0,255,255,255,255,255, 0x18,0x24,0x24,0x3c,0x3c,0x7e,0x7e,0x3c},
    {0,0,0,255,255,255,255,255, 0x0c,0x18,0x30,0x7e,0x0c,0x18,0x30,0x20},
    {0,0,0,255,255,255,255,255, 0x18,0x1a,0x1c,0xff,0x38,0x58,0x18,0x00},
    {0,0,0,255,255,255,255,255, 0x7e,0x81,0x3c,0x42,0x18,0x24,0x00,0x18},
};
static const lv_image_dsc_t oled_glyphs[] = {
    {.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_I1,.w=8,.h=8,.stride=1},.data_size=16,.data=oled_glyph_data[0]},
    {.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_I1,.w=8,.h=8,.stride=1},.data_size=16,.data=oled_glyph_data[1]},
    {.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_I1,.w=8,.h=8,.stride=1},.data_size=16,.data=oled_glyph_data[2]},
    {.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_I1,.w=8,.h=8,.stride=1},.data_size=16,.data=oled_glyph_data[3]},
    {.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_I1,.w=8,.h=8,.stride=1},.data_size=16,.data=oled_glyph_data[4]},
};
static inline lv_obj_t *oled_label(lv_obj_t *parent, int x, int y, int width, const char *text, bool scroll)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, &lv_font_portfolio_6x8, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_line_space(label, 0, 0);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, scroll ? LV_LABEL_LONG_SCROLL_CIRCULAR : LV_LABEL_LONG_CLIP);
    lv_obj_set_style_anim_duration(label, 7000, 0);
    lv_label_set_text(label, text);
    return label;
}
static inline void oled_image(lv_obj_t *parent, int x, int y, const lv_image_dsc_t *asset)
{
    lv_obj_t *image = lv_image_create(parent);
    lv_image_set_src(image, asset);
    lv_obj_set_pos(image, x, y);
}
static inline FiveTratumOledView oled_create_view_on(lv_obj_t *parent, FiveTratumOledPage page)
{
    FiveTratumOledView v = {.page=page};
    v.screen = lv_obj_create(parent);
    lv_obj_remove_style_all(v.screen);
    lv_obj_set_size(v.screen, 128, 32);
    lv_obj_set_style_bg_color(v.screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(v.screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(v.screen, LV_OBJ_FLAG_SCROLLABLE);
    if (page == OLED_BOOT || page == OLED_CREDITS) {
        oled_image(v.screen, 0, 0, &oled_logo_32);
        oled_label(v.screen, 36, 0, 92, "5tratumFW", false);
        for (int row=0; row<3; ++row) v.rows[row] = oled_label(v.screen, 36, 8+row*8, 92, "--", true);
        return v;
    }
    oled_image(v.screen, 0, 0, &oled_logo_8);
    oled_label(v.screen, 10, 0, 60, "5tratumFW", false);
    v.title = oled_label(v.screen, 74, 0, 54, "", false);
    lv_obj_set_style_text_align(v.title, LV_TEXT_ALIGN_RIGHT, 0);
    if (page == OLED_RATE) {
        v.rate = oled_label(v.screen, 0, 8, 90, "--", false);
        lv_obj_set_style_text_font(v.rate, &oled_large_font, 0);
        v.unit = oled_label(v.screen, 84, 16, 36, "GH/s", false);
        oled_image(v.screen, 119, 8, &oled_glyphs[0]);
        oled_image(v.screen, 0, 24, &oled_glyphs[1]);
        v.temperature = oled_label(v.screen, 10, 24, 38, "--C", false);
        oled_image(v.screen, 50, 24, &oled_glyphs[2]);
        v.power = oled_label(v.screen, 60, 24, 48, "--W", false);
        lv_obj_t *track = lv_obj_create(v.screen);
        lv_obj_remove_style_all(track);
        lv_obj_set_pos(track, 114, 25);
        lv_obj_set_size(track, 14, 6);
        lv_obj_set_style_border_color(track, lv_color_white(), 0);
        lv_obj_set_style_border_width(track, 1, 0);
        v.thermal_fill = lv_obj_create(track);
        lv_obj_remove_style_all(v.thermal_fill);
        lv_obj_set_pos(v.thermal_fill, 1, 1);
        lv_obj_set_height(v.thermal_fill, 4);
        lv_obj_set_style_bg_opa(v.thermal_fill, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(v.thermal_fill, lv_color_white(), 0);
    } else {
        for (int row=0; row<3; ++row) v.rows[row] = oled_label(v.screen, 0, 8+row*8, 128, "--", true);
        if (page == OLED_HEALTH) {
            oled_image(v.screen, 0, 8, &oled_glyphs[1]);
            oled_image(v.screen, 0, 16, &oled_glyphs[3]);
            lv_obj_set_x(v.rows[0], 10); lv_obj_set_width(v.rows[0], 118);
            lv_obj_set_x(v.rows[1], 10); lv_obj_set_width(v.rows[1], 118);
        } else if (page == OLED_WIFI) {
            oled_image(v.screen, 0, 8, &oled_glyphs[4]);
            lv_obj_set_x(v.rows[0], 10); lv_obj_set_width(v.rows[0], 118);
        } else if (page == OLED_SETUP || page == OLED_CONNECTION) {
            lv_obj_add_flag(v.title, LV_OBJ_FLAG_HIDDEN);
            for (int row=0; row<3; ++row) lv_obj_set_width(v.rows[row], 94);
            v.qr = lv_qrcode_create(v.screen);
            lv_qrcode_set_size(v.qr, 32);
            lv_qrcode_set_dark_color(v.qr, lv_color_black());
            lv_qrcode_set_light_color(v.qr, lv_color_white());
            lv_obj_set_pos(v.qr, 96, 0);
        }
    }
    return v;
}
static inline FiveTratumOledView oled_create_view(FiveTratumOledPage page)
{
    return oled_create_view_on(NULL, page);
}
static inline void oled_render_view(FiveTratumOledView *v, const FiveTratumOledData *d)
{
    char a[48], b[48], c[48], x[20], y[20];
    const char *title = "";
    switch (v->page) {
    case OLED_RATE:
        lv_label_set_text(v->title, oled_mining_state(d));
        if (oled_rate_available(d)) {
            bool tera = d->rate_gh >= 1000;
            snprintf(a, sizeof(a), tera ? "%.2f" : "%.0f", (double)(tera ? d->rate_gh/1000 : d->rate_gh));
            /* Bound display width for exceptional rates; never show an overflowing number. */
            if (strlen(a) > 7) strcpy(a, "--");
            lv_label_set_text(v->rate, a);
            lv_label_set_text(v->unit, tera ? "TH/s" : "GH/s");
        } else {
            lv_label_set_text(v->rate, "--");
            lv_label_set_text(v->unit, "GH/s");
        }
        oled_number(a, sizeof(a), d->temperature_c, "C", 0);
        oled_number(b, sizeof(b), d->power_w, "W", 1);
        lv_label_set_text(v->temperature, a); lv_label_set_text(v->power, b);
        lv_obj_set_width(v->thermal_fill, oled_positive(d->temperature_c) ? (int)fminf(12, fmaxf(1, (d->temperature_c-30)/50*12)) : 0);
        return;
    case OLED_HEALTH:
        title = "HEALTH";
        oled_number(x, sizeof(x), d->temperature_c, "C", 1);
        oled_number(y, sizeof(y), d->vr_temperature_c, "C", 0);
        if (oled_positive(d->temperature2_c)) {
            oled_number(y, sizeof(y), d->temperature2_c, "C", 1);
            snprintf(a, sizeof(a), "%s / %s", x, y);
        } else snprintf(a, sizeof(a), "%s VR %s", x, y);
        if (isfinite(d->fan_percent) && d->fan_percent >= 0 && d->fan_percent <= 100)
            snprintf(b, sizeof(b), "%uRPM %.0f%%", d->fan_rpm, (double)d->fan_percent);
        else snprintf(b, sizeof(b), "%uRPM --%%", d->fan_rpm);
        if (oled_rate_available(d) && oled_positive(d->rate_gh) && oled_positive(d->power_w))
            snprintf(c, sizeof(c), "Eff %.1f J/TH", (double)(d->power_w*1000/d->rate_gh));
        else strcpy(c, "Eff -- J/TH");
        break;
    case OLED_SHARES:
        title = "SHARES";
        oled_count(x, sizeof(x), d->accepted); oled_count(y, sizeof(y), d->rejected);
        snprintf(a, sizeof(a), "A %s  R %s", x, y);
        snprintf(b, sizeof(b), "Session %s", oled_text(d->best_session));
        snprintf(c, sizeof(c), "Best %s", oled_text(d->best_ever));
        break;
    case OLED_ROUTE:
        title = "ROUTE";
        oled_route(a, sizeof(a), d);
        snprintf(b, sizeof(b), "%s", oled_text(d->host));
        snprintf(c, sizeof(c), "IP %s", oled_text(d->ip));
        break;
    case OLED_OPERATING:
        title = "SETTINGS";
        oled_number(x, sizeof(x), d->configured_mhz, "MHz", 0);
        oled_number(y, sizeof(y), d->configured_mv, "mV", 0);
        snprintf(a, sizeof(a), "Set %s %s", x, y);
        oled_number(x, sizeof(x), d->core_mv, "mV", 0);
        oled_number(y, sizeof(y), d->input_mv/1000, "V", 1);
        snprintf(b, sizeof(b), "In %s Core %s", y, x);
        snprintf(c, sizeof(c), "ASIC %s", oled_mining_state(d));
        break;
    case OLED_WIFI:
        title = "NETWORK";
        snprintf(a, sizeof(a), "%s", oled_text(d->ssid));
        if (d->wifi_connected && d->rssi > -128 && d->rssi < 0) snprintf(b, sizeof(b), "RSSI %ddBm %s", d->rssi, d->rssi > -60 ? "Good" : d->rssi > -70 ? "Fair" : "Weak");
        else strcpy(b, "RSSI --dBm");
        snprintf(c, sizeof(c), "Up %" PRIu64 "d %" PRIu64 "h %" PRIu64 "m", d->uptime_seconds/86400, d->uptime_seconds/3600%24, d->uptime_seconds/60%60);
        break;
    case OLED_JOB:
        title = "JOB";
        if (d->block_height > 0) snprintf(a, sizeof(a), "Height %d", d->block_height);
        else strcpy(a, "Height --");
        snprintf(b, sizeof(b), "Net diff %s", oled_text(d->network_difficulty));
        snprintf(c, sizeof(c), "%s", oled_text(d->scriptsig));
        break;
    case OLED_CANDIDATE:
        title = "NOTICE";
        strcpy(a, "BLOCK CANDIDATE");
        snprintf(b, sizeof(b), "Diff %s", oled_text(d->best_session));
        strcpy(c, "Check pool acceptance");
        break;
    case OLED_FAULT:
        title = d->hardware_fault ? "FAULT" : "ASIC";
        snprintf(a, sizeof(a), "%s", oled_text(d->message));
        strcpy(b, d->hardware_fault ? (d->applied_paused ? "ASIC paused" : "Stop requested") : oled_mining_state(d));
        snprintf(c, sizeof(c), "IP %s", oled_text(d->ip));
        break;
    case OLED_OVERHEAT:
        title = "FAULT";
        strcpy(a, d->applied_paused ? "OVERHEAT - PAUSED" : "OVERHEAT - STOP REQ");
        strcpy(b, "Check cooling/settings");
        snprintf(c, sizeof(c), "IP %s", oled_text(d->ip));
        break;
    case OLED_SELF_TEST:
        /* These scrolling labels own a copy. Preserve complete factory
         * cancel/reset instructions instead of clipping through row buffers. */
        lv_label_set_text(v->title, "TEST");
        lv_label_set_text(v->rows[0], oled_text(d->message));
        lv_label_set_text(v->rows[1], oled_text(d->result));
        lv_label_set_text(v->rows[2], oled_text(d->finished));
        return;
    case OLED_UPDATE:
        title = "UPDATE";
        strcpy(a, "Firmware update");
        snprintf(b, sizeof(b), "%s", oled_text(d->filename));
        snprintf(c, sizeof(c), "%s", oled_text(d->update_status));
        break;
    case OLED_SETUP:
        title = "SETUP";
        strcpy(a, "Connect setup Wi-Fi");
        snprintf(b, sizeof(b), "%s", oled_text(d->ap_ssid));
        strcpy(c, "Open 192.168.4.1");
        break;
    case OLED_CONNECTION:
        title = "WI-FI";
        snprintf(a, sizeof(a), "%s", oled_text(d->ssid));
        snprintf(b, sizeof(b), "%s", oled_text(d->message));
        snprintf(c, sizeof(c), "Setup %s", oled_text(d->ap_ssid));
        break;
    case OLED_BOOT:
        snprintf(a, sizeof(a), "Bitaxe %s", oled_text(d->board_name));
        snprintf(b, sizeof(b), "Board %s", oled_text(d->board_version));
        strcpy(c, "ESP-Miner");
        break;
    case OLED_CREDITS:
        strcpy(a, "Open source");
        strcpy(b, "ESP-Miner");
        strcpy(c, "OSMU / Bitaxe");
        break;
    }
    if (v->title) lv_label_set_text(v->title, title);
    if (v->qr) {
        char data[64];
        snprintf(data, sizeof(data), "WIFI:S:%s;;", oled_text(d->ap_ssid));
        lv_qrcode_update(v->qr, data, strlen(data));
    }
    lv_label_set_text(v->rows[0], a);
    lv_label_set_text(v->rows[1], b);
    lv_label_set_text(v->rows[2], c);
}
#endif
