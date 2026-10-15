#pragma once
#include <cstdint>
#include <cstring>
#include <initializer_list>

// Display-only profiles. A pixel count or inherited board version is not a
// controller/flash identity. Public builds retain the original boot default;
// only QAxe exposes an explicit persisted choice of the larger profiles.
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
    const char *id;
};

inline constexpr DisplayProfile StandardPanelProfile = {
    "tdisplay-s3-320x170", 320, 170, 6528000, 13600, false, 35, 0, false, "320x170"};
// brunneis / AguiMr command-compatible 3.5-inch display; see display-profile docs.
inline constexpr DisplayProfile WideRgbPanelProfile = {
    "experimental-qaxe-480x320-rgb", 480, 320, 12288000, 25600, false, 0, 1, true, "480x320-rgb"};
// XTVDDICT YYSLUPING display settings only. Its 32MB flash layout is NOT imported.
// Both large profiles rotate by 180 degrees when requested; neither applies
// the stock small panel's single-axis mirror to the normal orientation.
inline constexpr DisplayProfile WideBgrPanelProfile = {
    "experimental-qaxe-480x320-bgr", 480, 320, 12000000, 25600, true, 0, 1, true, "480x320-bgr"};

#if FIVETRATUM_LCD_PROFILE == 0
inline constexpr DisplayProfile PanelProfile = StandardPanelProfile;
#elif FIVETRATUM_LCD_PROFILE == 1
inline constexpr DisplayProfile PanelProfile = WideRgbPanelProfile;
#elif FIVETRATUM_LCD_PROFILE == 2
inline constexpr DisplayProfile PanelProfile = WideBgrPanelProfile;
#else
#error "Unknown 5tratumFW LCD profile"
#endif

#if defined(FIVETRATUM_RUNTIME_WIDE_DISPLAY)
inline constexpr bool SupportsRuntimeWideProfiles = FIVETRATUM_RUNTIME_WIDE_DISPLAY == 1;
#elif defined(NERDQAXEPLUS2)
inline constexpr bool SupportsRuntimeWideProfiles = true;
#else
inline constexpr bool SupportsRuntimeWideProfiles = false;
#endif

inline const DisplayProfile *findDisplayProfile(const char *id) {
    if (!id) return nullptr;
    for (const auto *profile : {&StandardPanelProfile, &WideRgbPanelProfile, &WideBgrPanelProfile})
        if (std::strcmp(id, profile->id) == 0) return profile;
    return nullptr;
}
inline bool displayProfileSupported(const DisplayProfile *profile, bool wide = SupportsRuntimeWideProfiles) {
    return profile && (!profile->experimental || wide);
}
inline const DisplayProfile &savedDisplayProfile(const char *id, bool wide = SupportsRuntimeWideProfiles) {
    const auto *profile = findDisplayProfile(id);
    return displayProfileSupported(profile, wide) ? *profile : StandardPanelProfile;
}
inline constexpr bool panelMirrorX(bool flip, const DisplayProfile &profile = PanelProfile) {
    return profile.mirrorMode == 0 ? !flip : profile.mirrorMode == 1 && flip;
}
inline constexpr bool panelMirrorY(bool flip, const DisplayProfile &profile = PanelProfile) {
    return profile.mirrorMode == 0 ? flip : profile.mirrorMode == 1 && flip;
}
inline constexpr bool panelInvertColors(bool requestedInvert) {
    // False retains the existing hardware inversion and therefore stock colours.
    return !requestedInvert;
}

// Only display initialization may latch this state. Saving settings never
// changes a live DMA buffer, viewport or bus while rendering is in progress.
class DisplayProfileRuntime {
    const DisplayProfile *m_active = &PanelProfile;
    bool m_latched = false;
public:
    void latch(const char *saved, bool wide = SupportsRuntimeWideProfiles) {
        if (m_latched) return;
        m_active = &savedDisplayProfile(saved, wide);
        m_latched = true;
    }
    const DisplayProfile &active() const { return *m_active; }
    bool latched() const { return m_latched; }
};
inline DisplayProfileRuntime displayProfileRuntime;

inline bool displayRestartRequired(const char *saved, const DisplayProfile &active,
                                   bool wide = SupportsRuntimeWideProfiles) {
    return std::strcmp(savedDisplayProfile(saved, wide).id, active.id) != 0;
}
} // namespace FiveTratumDisplay
