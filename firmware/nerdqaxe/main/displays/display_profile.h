#pragma once
#include <cstdint>

// Display-only profiles. A pixel count or inherited board version is not a
// controller/flash identity. Public builds retain the original profile.
#ifndef FIVETRATUM_LCD_PROFILE
#define FIVETRATUM_LCD_PROFILE 0
#endif

namespace FiveTratumDisplay {
struct DisplayProfile {
    const char *name;
    int width, height;
    uint32_t pixelClock;
    int bufferPixels;
    bool bgr;
    int gapY;
    int mirrorMode; // 0: stock single axis; 1: both follow flip; 2: neither.
    bool experimental;
};

#if FIVETRATUM_LCD_PROFILE == 0
inline constexpr DisplayProfile PanelProfile = {
    "tdisplay-s3-320x170", 320, 170, 6528000, 13600, false, 35, 0, false};
#elif FIVETRATUM_LCD_PROFILE == 1
// brunneis / AguiMr command-compatible 3.5-inch display; see display-profile docs.
inline constexpr DisplayProfile PanelProfile = {
    "experimental-qaxe-480x320-rgb", 480, 320, 12288000, 25600, false, 0, 1, true};
#elif FIVETRATUM_LCD_PROFILE == 2
// XTVDDICT YYSLUPING display settings only. Its 32MB flash layout is NOT imported.
inline constexpr DisplayProfile PanelProfile = {
    "experimental-qaxe-480x320-bgr", 480, 320, 12000000, 25600, true, 0, 2, true};
#else
#error "Unknown 5tratumFW LCD profile"
#endif

inline constexpr bool panelMirrorX(bool flip) {
    return PanelProfile.mirrorMode == 0 ? !flip : PanelProfile.mirrorMode == 1 && flip;
}
inline constexpr bool panelMirrorY(bool flip) {
    return PanelProfile.mirrorMode == 0 ? flip : PanelProfile.mirrorMode == 1 && flip;
}
} // namespace FiveTratumDisplay
