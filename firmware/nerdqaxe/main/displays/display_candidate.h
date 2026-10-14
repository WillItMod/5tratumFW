#pragma once

// Display-only candidate presentation. No proof, ledger or mining state changes.
#include "lvgl.h"
#include <cstdint>

LV_FONT_DECLARE(ui_font_OpenSansBold13);
LV_FONT_DECLARE(ui_font_OpenSansBold14);
LV_FONT_DECLARE(ui_font_OpenSansBold24);

namespace FiveTratumDisplay {

constexpr unsigned CandidateVariantCount = 6;
constexpr uint32_t CandidateBurstMs = 2400;

// A variant is selected once for each event, never again during a redraw.
// Choosing from the five other variants avoids an immediate repeat without
// allocating a shuffled list or retaining a history of candidate events.
class CandidateVariantCycle {
    unsigned previous_ = CandidateVariantCount;
public:
    unsigned next(uint32_t entropy) {
        unsigned selected = entropy % CandidateVariantCount;
        if (previous_ < CandidateVariantCount) {
            selected = entropy % (CandidateVariantCount - 1);
            if (selected >= previous_) ++selected;
        }
        previous_ = selected;
        return selected;
    }
};

inline const char *candidateHeading(unsigned variant) {
    static const char *const captions[CandidateVariantCount] = {
        "Donut candidate", "Potential donut", "Glaze pending",
        "Fresh from the miner", "Sprinkle of hope", "Looking tasty",
    };
    return captions[variant % CandidateVariantCount];
}

struct CandidatePresentation {
    lv_obj_t *container = nullptr;
    lv_obj_t *image = nullptr;
    lv_obj_t *shine = nullptr;
    lv_obj_t *sprinkles[10]{};
    int imageX = 0, imageY = 0, imageSide = 80;
    int sparkleX = 0, sparkleY = 0;
    int width = 0, height = 0;
    unsigned variant = 0;
    bool motionFinished = false;
};

inline lv_obj_t *candidateRect(lv_obj_t *parent, int x, int y, int width, int height,
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

inline lv_obj_t *candidateText(lv_obj_t *parent, const char *value, int x, int y,
                               int width, const lv_font_t *font, uint32_t color,
                               bool centered = false) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_remove_style_all(label);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, centered ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label, value);
    return label;
}

inline CandidatePresentation candidatePresentation(lv_obj_t *parent,
                                                    const lv_img_dsc_t *image,
                                                    unsigned variant) {
    CandidatePresentation view;
    view.variant = variant % CandidateVariantCount;
    view.width = lv_disp_get_hor_res(nullptr);
    view.height = lv_disp_get_ver_res(nullptr);
    const bool large = view.width >= 440 && view.height >= 280;
    static const uint32_t accents[CandidateVariantCount] = {
        0xFFC06C, 0xFF8FB5, 0xDCAA7D, 0x4AE2ED, 0xC7A3FF, 0xFFCF82,
    };
    const uint32_t accent = accents[view.variant];
    static const char *const compactCaptions[CandidateVariantCount] = {
        "Donut\ncandidate", "Potential\ndonut", "Glaze\npending",
        "Fresh from\nthe miner", "Sprinkle of\nhope", "Looking\ntasty",
    };
    view.container = candidateRect(parent, 0, 0, view.width, view.height, 0x070E1B);
    lv_obj_set_style_bg_grad_color(view.container, lv_color_hex(0x152333), 0);
    lv_obj_set_style_bg_grad_dir(view.container, LV_GRAD_DIR_VER, 0);
    const int heroCenterX = large ? view.width / 2 : 62;
    const int heroCenterY = large ? 117 : 82;
    const int ringSide = large ? 182 : 116;
    auto *halo = candidateRect(view.container, heroCenterX - ringSide / 2,
                               heroCenterY - ringSide / 2, ringSide, ringSide,
                               accent, LV_RADIUS_CIRCLE, true);
    lv_obj_set_style_border_opa(halo, LV_OPA_40, 0);
    const int innerSide = ringSide - 14;
    auto *inner = candidateRect(view.container, heroCenterX - innerSide / 2,
                                heroCenterY - innerSide / 2, innerSide, innerSide,
                                accent, LV_RADIUS_CIRCLE, true);
    lv_obj_set_style_border_opa(inner, LV_OPA_20, 0);
    view.imageSide = large ? 144 : 100;
    view.imageX = heroCenterX - view.imageSide / 2;
    view.imageY = heroCenterY - view.imageSide / 2;
    view.image = lv_img_create(view.container);
    lv_img_set_src(view.image, image);
    lv_img_set_pivot(view.image, 0, 0);
    lv_img_set_zoom(view.image, static_cast<uint16_t>(view.imageSide * LV_IMG_ZOOM_NONE / 80));
    lv_obj_set_pos(view.image, view.imageX, view.imageY);
    lv_obj_clear_flag(view.image, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    const int textX = large ? 16 : 124;
    const int textWidth = large ? view.width - 32 : view.width - 136;
    candidateText(view.container, "BLOCK CANDIDATE", 12, large ? 12 : 8,
                  view.width - 24, &ui_font_OpenSansBold13, accent, large);
    candidateText(view.container, large ? candidateHeading(view.variant) : compactCaptions[view.variant], textX,
                  large ? 214 : 36, textWidth, &ui_font_OpenSansBold24, 0xEDF6FF, large);
    candidateText(view.container, "Awaiting verification", textX,
                  large ? 254 : 110, textWidth, &ui_font_OpenSansBold13, accent, large);
    candidateRect(view.container, 12, view.height - 34, view.width - 24, 1, 0x294159);
    candidateText(view.container, "Press a button to dismiss", 12, view.height - 23,
                  view.width - 24, &lv_font_montserrat_10, 0x9EB3C9, true);
    view.sparkleX = heroCenterX + view.imageSide / 4;
    view.sparkleY = heroCenterY - view.imageSide / 3;
    view.shine = candidateRect(view.container, view.sparkleX, view.sparkleY, 15, 15,
                               0xFFF7E8, 2);
    lv_obj_set_style_bg_opa(view.shine, LV_OPA_TRANSP, 0);
    candidateRect(view.shine, 6, 0, 3, 15, 0xFFF7E8, 2);
    candidateRect(view.shine, 0, 6, 15, 3, 0xFFF7E8, 2);
    // The confetti stays in the artwork region, clear of all candidate text.
    for (unsigned i = 0; i < 10; ++i) {
        const int column = static_cast<int>(i % 5);
        const int row = static_cast<int>(i / 5);
        const int x = heroCenterX - ringSide / 2 + 9 + column * (ringSide - 22) / 4;
        const int y = heroCenterY - ringSide / 2 + 10 + row * (ringSide - 26);
        view.sprinkles[i] = candidateRect(view.container, x, y, i % 2 ? 3 : 6,
                                         i % 2 ? 6 : 3, i % 3 ? accent : 0xEDF6FF, 1);
    }
    return view;
}

// One finite display burst, driven by the existing display loop. A disabled
// animation flag stops it permanently for this event; no new timers/animations
// own pointers to deleted overlay widgets.
inline void animateCandidate(CandidatePresentation &view, uint32_t elapsedMs, bool enabled) {
    if (!view.container || view.motionFinished) return;
    if (!enabled || elapsedMs >= CandidateBurstMs) {
        lv_obj_set_pos(view.image, view.imageX, view.imageY);
        lv_obj_add_flag(view.shine, LV_OBJ_FLAG_HIDDEN);
        for (auto *spark : view.sprinkles) lv_obj_add_flag(spark, LV_OBJ_FLAG_HIDDEN);
        view.motionFinished = true;
        return;
    }
    // Triangular easing needs no floating-point trig, buffers or image decode.
    const uint32_t phase = elapsedMs % 800;
    const int bob = phase < 400 ? static_cast<int>(phase) * 6 / 400 - 3
                              : 3 - static_cast<int>(phase - 400) * 6 / 400;
    lv_obj_set_y(view.image, view.imageY + bob);
    lv_obj_set_y(view.shine, view.sparkleY + bob);
    lv_obj_set_style_opa(view.shine, static_cast<lv_opa_t>(80 + (400 - (phase < 400 ? phase : 800 - phase)) * 175 / 400), 0);
    for (unsigned i = 0; i < 10; ++i) {
        const int ringSide = view.width >= 440 && view.height >= 280 ? 182 : 116;
        const int centerY = view.width >= 440 && view.height >= 280 ? 117 : 82;
        const int y = centerY - ringSide / 2 + 10 + static_cast<int>(i / 5) * (ringSide - 26);
        const int drift = static_cast<int>((elapsedMs + i * 71) % 600) * 8 / 600;
        lv_obj_set_y(view.sprinkles[i], y + drift);
    }
}

inline void dismissCandidate(CandidatePresentation &view) {
    if (view.container) lv_obj_del(view.container);
    view = CandidatePresentation{};
}

} // namespace FiveTratumDisplay
