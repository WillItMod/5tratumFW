#pragma once

// Native LVGL320x170 layouts shared by firmware and the host render test.
// Existing fonts and a12KiB established emblem; no theme-sized background.
#include "lvgl.h"
#include "display_logo.h"
#include "display_graphics.h"
#include "display_data.h"
#include "boot_status.h"
#include <cstdio>
#include <cmath>

LV_FONT_DECLARE(ui_font_OpenSansBold13);
LV_FONT_DECLARE(ui_font_OpenSansBold14);
LV_FONT_DECLARE(ui_font_OpenSansBold24);
LV_FONT_DECLARE(ui_font_OpenSansBold45);

namespace FiveTratumDisplay {

constexpr uint32_t Background = 0x070E1B;
constexpr uint32_t Foreground = 0xEDF6FF;
constexpr uint32_t Muted = 0x9EB3C9;
constexpr uint32_t Cyan = 0x4AE2ED;
constexpr uint32_t Line = 0x22384D;
constexpr uint32_t Danger = 0xFF6B6B;
constexpr uint32_t Surface = 0x101F30;
constexpr uint32_t Amber = 0xFFC06C;

inline lv_obj_t *rectangle(lv_obj_t *parent, int x, int y, int width, int height,
                          uint32_t color, int radius = 0, bool outline = false) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, outline ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(obj, outline ? 1 : 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

enum class Icon { Chip, Power, Thermometer, Fan, Clock, Network, Trophy, Check, Voltage };

inline void stroke(lv_obj_t *parent, int x, int y, const lv_point_t *points,
                   uint16_t count, uint32_t color) {
    lv_obj_t *line = lv_line_create(parent);
    lv_obj_set_pos(line, x, y);
    lv_line_set_points(line, points, count);
    lv_obj_set_style_line_color(line, lv_color_hex(color), 0);
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
}

// Native category pictograms; no decorative fill masquerades as a measurement.
inline void icon(lv_obj_t *parent, Icon kind, int x, int y, uint32_t color = Cyan) {
    static const lv_point_t bolt[] = {{11,1},{4,10},{10,10},{7,19},{16,7},{10,7},{11,1}};
    static const lv_point_t check[] = {{2,10},{7,15},{17,4}};
    static const lv_point_t clock[] = {{10,4},{10,10},{15,12}};
    static const lv_point_t voltage[] = {{2,5},{7,15},{12,5},{17,15}};
    static const lv_point_t network[] = {{3,15},{3,9},{17,9},{17,15},{17,9},{10,9},{10,3}};
    static const lv_point_t cup[] = {{4,3},{16,3},{14,10},{10,13},{6,10},{4,3}};
    static const lv_point_t stem[] = {{10,13},{10,18},{5,18},{15,18}};
    static const lv_point_t fanA[] = {{10,10},{5,2},{2,5},{10,10},{18,5},{15,2},{10,10}};
    static const lv_point_t fanB[] = {{10,10},{15,18},{18,15},{10,10},{2,15},{5,18},{10,10}};
    switch (kind) {
        case Icon::Chip:
            rectangle(parent, x+4, y+4, 12, 12, color, 2, true);
            rectangle(parent, x+8, y+8, 4, 4, color, 1);
            for (int pin = 6; pin <= 12; pin += 3) {
                rectangle(parent, x+pin, y+1, 1, 3, color);
                rectangle(parent, x+pin, y+16, 1, 3, color);
                rectangle(parent, x+1, y+pin, 3, 1, color);
                rectangle(parent, x+16, y+pin, 3, 1, color);
            }
            break;
        case Icon::Power: stroke(parent, x, y, bolt, 7, color); break;
        case Icon::Check: stroke(parent, x, y, check, 3, color); break;
        case Icon::Voltage: stroke(parent, x, y, voltage, 4, color); break;
        case Icon::Clock:
            rectangle(parent, x+1, y+1, 18, 18, color, LV_RADIUS_CIRCLE, true);
            stroke(parent, x, y, clock, 3, color);
            break;
        case Icon::Thermometer:
            rectangle(parent, x+7, y+1, 6, 13, color, 3, true);
            rectangle(parent, x+5, y+11, 10, 9, color, LV_RADIUS_CIRCLE, true);
            rectangle(parent, x+9, y+6, 2, 10, color, 1);
            break;
        case Icon::Fan:
            stroke(parent, x, y, fanA, 7, color);
            stroke(parent, x, y, fanB, 7, color);
            rectangle(parent, x+7, y+7, 6, 6, color, LV_RADIUS_CIRCLE);
            break;
        case Icon::Network:
            stroke(parent, x, y, network, 7, color);
            rectangle(parent, x+7, y, 6, 5, color, 1);
            rectangle(parent, x, y+14, 6, 5, color, 1);
            rectangle(parent, x+14, y+14, 6, 5, color, 1);
            break;
        case Icon::Trophy:
            stroke(parent, x, y, cup, 7, color);
            stroke(parent, x, y, stem, 4, color);
            break;
    }
}

inline lv_obj_t *screen() {
    lv_obj_t *obj = lv_obj_create(nullptr);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, 320, 170);
    lv_obj_set_style_bg_color(obj, lv_color_hex(Background), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

inline lv_obj_t *text(lv_obj_t *parent, const char *value, int x, int y, int width,
                       const lv_font_t *font = &ui_font_OpenSansBold13,
                       uint32_t color = Foreground) {
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_width(obj, width);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
    lv_label_set_text(obj, value);
    return obj;
}

inline void divider(lv_obj_t *parent, int y) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, 8, y);
    lv_obj_set_size(obj, 304, 1);
    lv_obj_set_style_bg_color(obj, lv_color_hex(Line), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

inline lv_obj_t *logo(lv_obj_t *parent, int x, int y, int size) {
    lv_obj_t *obj = lv_img_create(parent);
    lv_img_set_src(obj, &firmwareLogo);
    lv_img_set_pivot(obj, 0, 0);
    lv_img_set_zoom(obj, size * 256 / 240);
    lv_obj_set_pos(obj, x, y);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

inline lv_obj_t *header(lv_obj_t *parent, const char *page) {
    auto *mark = lv_img_create(parent);
    lv_img_set_src(mark, &firmwareWordmark);
    lv_img_set_pivot(mark, 0, 0);
    lv_img_set_zoom(mark, 192);
    lv_obj_set_pos(mark, 6, 0);
    lv_obj_clear_flag(mark, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *title = text(parent, page, 180, 8, 132, &lv_font_montserrat_10, Cyan);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_RIGHT, 0);
    divider(parent, 29);
    return title;
}

inline lv_obj_t *hiddenValue(lv_obj_t *parent) {
    lv_obj_t *obj = text(parent, "--", 0, 0, 1);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    return obj;
}

struct SummaryWidgets {
    lv_obj_t *hashrate, *powerEfficiency, *shares, *route, *mux, *ip, *tempFan;
    lv_obj_t *rateUnit, *power, *efficiency, *temperature, *vrExternal, *vrInternal, *fan;
    lv_obj_t *coinTitle;
};

inline SummaryWidgets summary(lv_obj_t *parent) {
    header(parent, "");
    SummaryWidgets fields{};
    rectangle(parent, 8, 35, 178, 103, Surface, 7);
    rectangle(parent, 192, 35, 120, 103, Surface, 7);
    rectangle(parent, 8, 35, 3, 103, Cyan, 1);
    fields.ip = text(parent, "--", 176, 8, 112, &lv_font_montserrat_10, Muted);
    lv_obj_set_style_text_align(fields.ip, LV_TEXT_ALIGN_RIGHT, 0);
    // Replace the page title with the actual network-type image in UI binding.
    fields.coinTitle = text(parent, "TOTAL HASHRATE", 20, 41, 154, &lv_font_montserrat_10, Muted);
    fields.hashrate = text(parent, "--", 17, 53, 162, &ui_font_OpenSansBold45, Cyan);
    lv_obj_set_style_text_align(fields.hashrate, LV_TEXT_ALIGN_CENTER, 0);
    fields.rateUnit = text(parent, "GH/s", 39, 113, 100, &ui_font_OpenSansBold14, Muted);
    lv_obj_set_style_text_align(fields.rateUnit, LV_TEXT_ALIGN_CENTER, 0);
    icon(parent, Icon::Chip, 151, 111, Cyan);
    icon(parent, Icon::Power, 198, 39, Cyan);
    icon(parent, Icon::Voltage, 198, 60, Muted);
    icon(parent, Icon::Fan, 198, 79, Cyan);
    fields.power = text(parent, "-- W", 224, 40, 82, &ui_font_OpenSansBold14);
    fields.efficiency = text(parent, "-- J/TH", 224, 61, 82, &ui_font_OpenSansBold13, Muted);
    fields.fan = text(parent, "-- RPM", 224, 81, 82, &lv_font_montserrat_10);
    fields.temperature = text(parent, "ASIC board --", 198, 96, 108, &lv_font_montserrat_10, Amber);
    fields.vrExternal = text(parent, "VR external --", 198, 109, 108, &lv_font_montserrat_10, Amber);
    fields.vrInternal = text(parent, "VR internal --", 198, 122, 108, &lv_font_montserrat_10, Amber);
    lv_label_set_long_mode(fields.temperature, LV_LABEL_LONG_CLIP);
    lv_label_set_long_mode(fields.vrExternal, LV_LABEL_LONG_CLIP);
    lv_label_set_long_mode(fields.vrInternal, LV_LABEL_LONG_CLIP);
    fields.powerEfficiency = hiddenValue(parent);
    fields.tempFan = hiddenValue(parent);
    fields.route = text(parent, "Link --", 246, 141, 66, &lv_font_montserrat_10, Muted);
    lv_obj_set_style_text_align(fields.route, LV_TEXT_ALIGN_RIGHT, 0);
    fields.shares = text(parent, "Accepted --  |  Rejected --", 8, 141, 236, &lv_font_montserrat_10);
    fields.mux = text(parent, "5tratMUX: unverified", 8, 157, 304, &lv_font_montserrat_10, Muted);
    lv_obj_update_layout(parent); // Resolve inactive-page label widths before live updates.
    return fields;
}

inline void updateSummaryTemperatures(lv_obj_t *board, lv_obj_t *external, lv_obj_t *internal,
                                      float boardC, float vrExternalC, float vrInternalC, bool fresh) {
    char buffer[40];
    formatLabelledTemperature(buffer, sizeof(buffer), "ASIC board", boardC, fresh);
    lv_label_set_text(board, buffer);
    formatLabelledTemperature(buffer, sizeof(buffer), "VR external", vrExternalC, fresh);
    lv_label_set_text(external, buffer);
    formatLabelledTemperature(buffer, sizeof(buffer), "VR internal", vrInternalC, fresh);
    lv_label_set_text(internal, buffer);
}

struct CoinWidgets {
    lv_obj_t *identity, *name, *source, *price, *hashrate, *temperature;
};

inline CoinWidgets coin(lv_obj_t *parent) {
    header(parent, "COIN / ROUTE");
    CoinWidgets fields{};
    fields.identity = text(parent, "Mining", 8, 38, 304, &ui_font_OpenSansBold24, Cyan);
    fields.name = text(parent, "Coin identity unavailable", 8, 75, 304, &ui_font_OpenSansBold13);
    fields.source = text(parent, "Pool did not declare a coin", 8, 95, 304, &lv_font_montserrat_10, Muted);
    fields.price = text(parent, "Price unavailable", 8, 114, 304, &ui_font_OpenSansBold13, Muted);
    divider(parent, 137);
    fields.hashrate = text(parent, "--", 8, 146, 176, &ui_font_OpenSansBold13, Cyan);
    fields.temperature = text(parent, "--", 199, 146, 113, &ui_font_OpenSansBold13, Amber);
    lv_obj_set_style_text_align(fields.temperature, LV_TEXT_ALIGN_RIGHT, 0);
    return fields;
}

inline void setBoundedCoinName(lv_obj_t *label, const char *name) {
    lv_obj_update_layout(label); // Inactive pages also need their declared width.
    char bounded[52];
    std::snprintf(bounded, sizeof(bounded), "%.48s", name);
    const lv_font_t *font = lv_obj_get_style_text_font(label, 0);
    lv_point_t extent;
    lv_txt_get_size(&extent, bounded, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (extent.x > lv_obj_get_width(label)) {
        size_t keep = strlen(bounded);
        do {
            if (keep) --keep;
            std::snprintf(bounded + keep, sizeof(bounded) - keep, "...");
            lv_txt_get_size(&extent, bounded, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        } while (keep && extent.x > lv_obj_get_width(label));
    }
    lv_label_set_text(label, bounded);
}

inline void updateCoinIdentity(const CoinWidgets &fields, const MuxCoinContext &context,
                               const MuxCoinContext &p1, const MuxCoinContext &p2,
                               bool dual, uint32_t btcUsd) {
    char buffer[80];
    if (dual && !context.available) {
        lv_label_set_text(fields.identity, "Pool coin context");
        formatCoinRoute(buffer, sizeof(buffer), p1, 0); lv_label_set_text(fields.name, buffer);
        formatCoinRoute(buffer, sizeof(buffer), p2, 1); lv_label_set_text(fields.source, buffer);
    } else if (context.available) {
        lv_label_set_text(fields.identity, context.ticker);
        setBoundedCoinName(fields.name, context.name);
        lv_label_set_text(fields.source, "5tratMUX route declaration");
    } else {
        lv_label_set_text(fields.identity, "Mining");
        lv_label_set_text(fields.name, "Coin identity unavailable");
        lv_label_set_text(fields.source, "Pool did not declare a coin");
    }
    formatCoinPrice(buffer, sizeof(buffer), context, btcUsd);
    lv_label_set_text(fields.price, buffer);
}

struct MiningJobWidgets {
    lv_obj_t *coin[2], *height[2], *difficulty[2], *nBits[2], *age[2];
    lv_obj_t *source;
};

inline MiningJobWidgets miningJobs(lv_obj_t *parent) {
    header(parent, "MINING JOBS");
    MiningJobWidgets fields{};
    for (int i = 0; i < 2; ++i) {
        const int x = 8 + i * 156;
        rectangle(parent, x, 35, 148, 113, Surface, 5);
        fields.coin[i] = text(parent, i ? "P2 / unknown" : "P1 / unknown", x + 6, 40, 136,
                              &ui_font_OpenSansBold13, Cyan);
        fields.height[i] = text(parent, "Block --", x + 6, 64, 136, &ui_font_OpenSansBold13);
        text(parent, "DIFFICULTY (BDIFF)", x + 6, 84, 136, &lv_font_montserrat_10, Muted);
        fields.difficulty[i] = text(parent, "--", x + 6, 98, 136, &ui_font_OpenSansBold13);
        fields.nBits[i] = text(parent, "nBits --", x + 6, 118, 136, &lv_font_montserrat_10, Muted);
        fields.age[i] = text(parent, "Job unavailable", x + 6, 134, 136, &lv_font_montserrat_10, Muted);
    }
    fields.source = text(parent, "Candidate block / bdiff / Shared work", 8, 153, 304,
                         &lv_font_montserrat_10, Muted);
    return fields;
}

inline void updateMiningJobs(const MiningJobWidgets &fields, const MuxCoinContext coins[2],
                             const MuxWorkContext work[2], bool dual, int active) {
    char buffer[80];
    for (int i = 0; i < 2; ++i) {
        std::snprintf(buffer, sizeof(buffer), "P%d / %s", i + 1,
                      coins[i].available ? coins[i].ticker : "unknown");
        setBoundedCoinName(fields.coin[i], buffer);
        formatWorkHeight(buffer, sizeof(buffer), work[i]); lv_label_set_text(fields.height[i], buffer);
        formatWorkDifficulty(buffer, sizeof(buffer), work[i]); lv_label_set_text(fields.difficulty[i], buffer);
        formatWorkNBits(buffer, sizeof(buffer), work[i]); lv_label_set_text(fields.nBits[i], buffer);
        formatWorkAge(buffer, sizeof(buffer), work[i]); lv_label_set_text(fields.age[i], buffer);
    }
    if (dual) std::snprintf(buffer, sizeof(buffer), "Candidate block / bdiff / Shared work");
    else std::snprintf(buffer, sizeof(buffer), "Candidate block / bdiff / P%d active", active == 1 ? 2 : 1);
    lv_label_set_text(fields.source, buffer);
}

struct AsicWidgets {
    lv_obj_t *point, *page, *thermal, *temperatureTitle;
    lv_obj_t *indices[4];
    lv_obj_t *temperatures[4];
    lv_obj_t *hashrates[4];
};

inline AsicWidgets asics(lv_obj_t *parent) {
    AsicWidgets fields{};
    fields.page = header(parent, "ASIC 1-4 / 4");
    fields.thermal = text(parent, "Thermal readings unavailable", 8, 34, 304, &lv_font_montserrat_10, Amber);
    lv_label_set_long_mode(fields.thermal, LV_LABEL_LONG_CLIP);
    fields.point = text(parent, "Configured clock / voltage: --", 8, 48, 304, &lv_font_montserrat_10, Cyan);
    text(parent, "COUNTER GH/s <=15s", 104, 62, 130, &lv_font_montserrat_10, Muted);
    fields.temperatureTitle = text(parent, "CHIP: LAST", 243, 62, 69, &lv_font_montserrat_10, Muted);
    lv_obj_add_flag(fields.temperatureTitle, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 4; ++i) {
        char index[16];
        std::snprintf(index, sizeof(index), "ASIC %d", i + 1);
        const int y = 77 + i * 18;
        rectangle(parent, 8, y-2, 304, 18, Surface, 3);
        icon(parent, Icon::Chip, 10, y-3, i % 2 ? Muted : Cyan);
        fields.indices[i] = text(parent, index, 36, y, 64, &ui_font_OpenSansBold13);
        fields.hashrates[i] = text(parent, "--", 104, y, 126, &ui_font_OpenSansBold13, Cyan);
        fields.temperatures[i] = text(parent, "", 239, y, 67, &ui_font_OpenSansBold13);
        lv_label_set_long_mode(fields.temperatures[i], LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(fields.temperatures[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_add_flag(fields.temperatures[i], LV_OBJ_FLAG_HIDDEN);
    }
    icon(parent, Icon::Network, 8, 150, Muted);
    text(parent, "Shared work  |  Split unavailable", 34, 155, 278, &lv_font_montserrat_10, Muted);
    lv_obj_update_layout(parent);
    return fields;
}

inline void updateAsicTemperatures(lv_obj_t *thermal, lv_obj_t *title, lv_obj_t *const labels[4],
                                   const float chipC[4], float boardC, float externalC,
                                   float internalC, bool fresh) {
    char buffer[80];
    formatSharedTemperatures(buffer, sizeof(buffer), boardC, externalC, internalC, fresh);
    lv_label_set_text(thermal, buffer);
    bool anyChipTemperature = false;
    for (int i = 0; i < 4; ++i) {
        const bool measured = fresh && temperatureAvailable(chipC[i]);
        if (measured) {
            formatTemperature(buffer, sizeof(buffer), chipC[i]);
            lv_label_set_text(labels[i], buffer);
            lv_obj_clear_flag(labels[i], LV_OBJ_FLAG_HIDDEN);
            anyChipTemperature = true;
        } else {
            lv_label_set_text(labels[i], "");
            lv_obj_add_flag(labels[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (anyChipTemperature) lv_obj_clear_flag(title, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(title, LV_OBJ_FLAG_HIDDEN);
}

struct SettingsWidgets {
    lv_obj_t *ip, *pool, *host, *port, *frequency, *voltage, *fan, *shares;
    lv_obj_t *route, *uptime, *best, *electrical;
};

inline SettingsWidgets settings(lv_obj_t *parent) {
    header(parent, "SETTINGS / ROUTE");
    SettingsWidgets fields{};
    fields.ip = text(parent, "--", 176, 35, 136, &lv_font_montserrat_10, Muted);
    lv_obj_set_style_text_align(fields.ip, LV_TEXT_ALIGN_RIGHT, 0);
    fields.pool = text(parent, "Pool --", 8, 34, 160, &ui_font_OpenSansBold13, Cyan);
    fields.host = text(parent, "--", 8, 51, 304, &ui_font_OpenSansBold13);
    lv_obj_set_height(fields.host, 18);
    fields.port = text(parent, "Port --", 8, 71, 304, &lv_font_montserrat_10, Muted);
    text(parent, "CONFIGURED: shared clock / voltage / fan", 8, 82, 304, &lv_font_montserrat_10, Muted);
    rectangle(parent, 8, 91, 304, 31, Surface, 5);
    icon(parent, Icon::Chip, 12, 96, Cyan);
    icon(parent, Icon::Voltage, 111, 96, Cyan);
    icon(parent, Icon::Fan, 219, 96, Cyan);
    fields.frequency = text(parent, "-- MHz", 37, 100, 71, &ui_font_OpenSansBold13);
    fields.voltage = text(parent, "-- mV", 136, 100, 79, &ui_font_OpenSansBold13);
    fields.fan = text(parent, "Fan --", 244, 100, 68, &ui_font_OpenSansBold13);
    fields.electrical = text(parent, "Last board: input --  /  current --  /  Vout --", 8, 127, 304, &lv_font_montserrat_10, Muted);
    icon(parent, Icon::Clock, 8, 145, Muted);
    fields.uptime = text(parent, "--", 34, 151, 135, &lv_font_montserrat_10, Muted);
    icon(parent, Icon::Trophy, 177, 145, Amber);
    text(parent, "BD", 202, 151, 19, &lv_font_montserrat_10, Muted);
    fields.best = text(parent, "--", 229, 148, 83, &ui_font_OpenSansBold13, Amber);
    fields.route = hiddenValue(parent);
    fields.shares = hiddenValue(parent);
    return fields;
}

inline lv_obj_t *splash(lv_obj_t *parent, const char *model, bool connecting) {
    logo(parent, 40, 0, 240);
    text(parent, model, 8, 151, 150, &lv_font_montserrat_10, Muted);
    lv_obj_t *status = text(parent, connecting ? "Connecting..." : "Starting...", 165, 149, 147,
                            &ui_font_OpenSansBold13, Muted);
    lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_RIGHT, 0);
    return status;
}

struct BootWidgets { lv_obj_t *lines[BootStatusHistory::LINE_COUNT]; };

inline BootWidgets bootStatus(lv_obj_t *parent, const char *model, const char *version) {
    logo(parent, 80, 0, 160);
    auto *device = text(parent, model, 8, 5, 100, &lv_font_montserrat_10, Muted);
    setBoundedCoinName(device, model);
    text(parent, "STARTUP", 235, 5, 77, &lv_font_montserrat_10, Muted);
    BootWidgets fields{};
    for (size_t i = 0; i < BootStatusHistory::LINE_COUNT; ++i)
        fields.lines[i] = text(parent, "", 8, 102 + static_cast<int>(i) * 12, 304,
                              &lv_font_montserrat_10, i == BootStatusHistory::LINE_COUNT - 1 ? Cyan : Muted);
    divider(parent, 151);
    auto *build = text(parent, version, 8, 156, 304, &lv_font_montserrat_10, Muted);
    setBoundedCoinName(build, version);
    return fields;
}

inline void updateBootStatus(const BootWidgets &fields, const BootStatusHistory &history) {
    for (size_t i = 0; i < BootStatusHistory::LINE_COUNT; ++i) {
        const int index = static_cast<int>(i) - static_cast<int>(BootStatusHistory::LINE_COUNT - history.size());
        const char *stage = index < 0 ? "" : history.line(index);
        lv_label_set_text(fields.lines[i], stage);
        setBoundedCoinName(fields.lines[i], stage);
    }
}

struct BrandWidgets { lv_obj_t *coin, *hashrate, *powerTemp, *mux; };

inline BrandWidgets brandSummary(lv_obj_t *parent, const char *model) {
    (void)model;
    logo(parent, 40, 0, 240);
    BrandWidgets fields{};
    fields.coin = text(parent, "Mining", 8, 144, 184, &lv_font_montserrat_10, Muted);
    fields.hashrate = text(parent, "--", 199, 144, 113, &lv_font_montserrat_10, Cyan);
    lv_obj_set_style_text_align(fields.hashrate, LV_TEXT_ALIGN_RIGHT, 0);
    fields.powerTemp = hiddenValue(parent);
    fields.mux = text(parent, "5tratMUX: unverified", 8, 157, 304, &lv_font_montserrat_10, Muted);
    return fields;
}

inline lv_obj_t *portal(lv_obj_t *parent) {
    header(parent, "WI-FI SETUP");
    text(parent, "Connect to setup Wi-Fi", 8, 42, 304, &ui_font_OpenSansBold14, Cyan);
    lv_obj_t *ssid = text(parent, "--", 8, 70, 304, &ui_font_OpenSansBold14);
    text(parent, "Open 192.168.4.1", 8, 100, 304, &ui_font_OpenSansBold14);
    text(parent, "Set your network and mining connection.", 8, 139, 304, &lv_font_montserrat_10, Muted);
    return ssid;
}

inline lv_obj_t *warning(lv_obj_t *parent, const char *message, const char *detail) {
    lv_obj_t *overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, 320, 170);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(Background), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    header(overlay, "DEVICE WARNING");
    lv_obj_t *title = text(overlay, message, 8, 45, 304, &ui_font_OpenSansBold24, Danger);
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(title, 77);
    lv_obj_t *info = text(overlay, detail, 8, 129, 304, &ui_font_OpenSansBold13, Muted);
    lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
    return overlay;
}

inline lv_obj_t *authenticator(lv_obj_t *parent, void *buffer, int side) {
    header(parent, "AUTHENTICATOR");
    lv_obj_t *canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(canvas, buffer, side, side, LV_IMG_CF_TRUE_COLOR);
    lv_obj_align(canvas, LV_ALIGN_RIGHT_MID, -8, 16);
    lv_obj_t *label = text(parent, "Scan with your\nAuthenticator app.\n\nAny button cancels.",
                           8, 47, 166, &ui_font_OpenSansBold13);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    return canvas;
}

} // namespace FiveTratumDisplay
