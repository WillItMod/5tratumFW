// Execute the production constexpr profile selected by the real CMake helper.
// This reports display configuration, not physical controller qualification.
#include <cstdio>
#include <cstdint>
#include "display_profile.h"

using namespace FiveTratumDisplay;

static_assert(PanelProfile.width > 0 && PanelProfile.height > 0);
static_assert(PanelProfile.pixelClock > 0);
static_assert(PanelProfile.bufferPixels > 0);
static_assert(PanelProfile.bufferPixels < PanelProfile.width * PanelProfile.height);
static_assert(PanelProfile.bufferPixels * sizeof(uint16_t) <= 51200);

int main() {
    std::printf(
        "{\"profile\":\"%s\",\"width\":%d,\"height\":%d,"
        "\"pixelClock\":%u,\"bufferPixels\":%d,\"bufferBytes\":%zu,"
        "\"bgr\":%s,\"gapY\":%d,\"experimental\":%s,"
        "\"mirrorNormal\":[%s,%s],\"mirrorFlipped\":[%s,%s]}\n",
        PanelProfile.name, PanelProfile.width, PanelProfile.height,
        static_cast<unsigned>(PanelProfile.pixelClock), PanelProfile.bufferPixels,
        PanelProfile.bufferPixels * sizeof(uint16_t),
        PanelProfile.bgr ? "true" : "false", PanelProfile.gapY,
        PanelProfile.experimental ? "true" : "false",
        panelMirrorX(false) ? "true" : "false", panelMirrorY(false) ? "true" : "false",
        panelMirrorX(true) ? "true" : "false", panelMirrorY(true) ? "true" : "false");
}
