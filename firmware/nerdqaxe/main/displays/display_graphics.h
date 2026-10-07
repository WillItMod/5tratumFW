#pragma once

// Label bindings only. Inputs come from the existing guarded telemetry path.
#include "lvgl.h"
#include <cmath>
#include <cstdio>

namespace FiveTratumDisplay {

inline void updateSummaryRate(lv_obj_t *value, lv_obj_t *unit, float gh) {
    char buffer[32];
    if (!std::isfinite(gh) || gh < 0) {
        lv_label_set_text(value, "--");
        lv_label_set_text(unit, "GH/s");
        return;
    }
    if (gh >= 1000) std::snprintf(buffer, sizeof(buffer), "%.2f", static_cast<double>(gh) / 1000.0);
    else std::snprintf(buffer, sizeof(buffer), "%.0f", static_cast<double>(gh));
    lv_label_set_text(value, buffer);
    lv_label_set_text(unit, gh >= 1000 ? "TH/s" : "GH/s");
}

inline void updatePowerLabels(lv_obj_t *powerLabel, lv_obj_t *efficiencyLabel, float watts, float gh) {
    char buffer[32];
    if (std::isfinite(watts) && watts > 0) std::snprintf(buffer, sizeof(buffer), "%.1f W", static_cast<double>(watts));
    else std::snprintf(buffer, sizeof(buffer), "-- W");
    lv_label_set_text(powerLabel, buffer);
    if (std::isfinite(watts) && watts > 0 && std::isfinite(gh) && gh > 0)
        std::snprintf(buffer, sizeof(buffer), "%.1f J/TH", static_cast<double>(watts) * 1000.0 / gh);
    else std::snprintf(buffer, sizeof(buffer), "-- J/TH");
    lv_label_set_text(efficiencyLabel, buffer);
}

} // namespace FiveTratumDisplay
