#pragma once

// A bounded, read-only value copied from the controller's RAM cache. Display
// code never owns the controller and cannot obtain settings or start tuning.
#include "presentation.h"

namespace FiveTratumAutotune {
struct DisplaySnapshot {
    bool available=false, valid=false, active=false, ownsSettings=false;
    bool restoring=false, continuous=false;
    bool fault=false, updating=false, storageBlocked=false;
    fwt_state state=FWT_DISABLED;
    fwt_priority priority=FWT_BALANCED;
    uint64_t capturedAtUs=0;
    fwt_point current{};
    double hashRateGHs=0, errorPercentage=0, powerW=0, asicTempC=0, vrmTempC=0;
    double maxAsicTempC=0, maxVrmTempC=0, maxPowerW=0;
    fwt_search_view search{};

    bool fresh(uint64_t now) const {
        return available && valid && !fault && !updating && !storageBlocked && capturedAtUs>0 && now>=capturedAtUs &&
               now-capturedAtUs<=FWT_FRESH_US;
    }
};
static_assert(sizeof(DisplaySnapshot)<=256,"LCD tuning cache must remain bounded");
}
