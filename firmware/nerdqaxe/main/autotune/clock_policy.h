#pragma once
#include <cmath>
#include <cstdint>
namespace FiveTratumAutotune {
// An experimental project admission boundary, not a board or silicon rating.
constexpr uint16_t advancedClockMaxMHz=1200;
inline bool clockFrequencySupported(double frequency) {
    if(!std::isfinite(frequency) || std::floor(frequency)!=frequency || frequency<100 || frequency>advancedClockMaxMHz) return false;
    // Match the actual Nerd Asic::sendHashFrequency search. This is a pure
    // preflight: no PLL frame is authorized solely by a numeric range check.
    for(int reference=2;reference>0;--reference)
        for(int first=7;first>0;--first)
            for(int second=7;second>0;--second) {
                if(first<second) continue;
                const int feedback=static_cast<int>(std::round(frequency/25.0*(reference*first*second)));
                const double actual=25.0*feedback/(reference*first*second);
                if(feedback>=0xa0 && feedback<=0xef && std::fabs(frequency-actual)<=2.0) return true;
            }
    return false;
}
}
