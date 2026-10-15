// Production profile selection/boot latch/metadata, without LCD or NVS access.
#include <cassert>
#include <cstdio>
#include <cstring>
#include "display_profile_json.h"

using namespace FiveTratumDisplay;

static void checkMetadata(const char *saved, const DisplayProfileRuntime &runtime,
                          bool valid, bool restart, size_t count) {
    JsonDocument doc;
    appendDisplayRuntimeMetadata(doc.to<JsonObject>(), saved, runtime.active());
    assert(doc["displayProfileCapabilityVersion"] == 1);
    assert(doc["displayProfileStorageValid"].as<bool>() == valid);
    assert(doc["displayRestartRequired"].as<bool>() == restart);
    assert(doc["displayProfileOptions"].size() == count);
    assert(std::strcmp(doc["displayActiveProfile"].as<const char *>(), runtime.active().id) == 0);
    assert(doc["displayWidth"] == runtime.active().width);
    assert(doc["displayHeight"] == runtime.active().height);
    assert(doc["displayProfileExperimental"].as<bool>() == runtime.active().experimental);
}

int main() {
    constexpr size_t count = SupportsRuntimeWideProfiles ? 3 : 1;
    DisplayProfileRuntime stock;
    stock.latch("320x170");
    assert(stock.latched());
    assert(stock.active().width == 320 && stock.active().height == 170);
    assert(stock.active().pixelClock == 6528000 && stock.active().bufferPixels == 13600);
    assert(stock.active().gapY == 35 && !stock.active().bgr);
    assert(panelMirrorX(false, stock.active()) && !panelMirrorY(false, stock.active()));
    assert(!panelMirrorX(true, stock.active()) && panelMirrorY(true, stock.active()));
    checkMetadata("320x170", stock, true, false, count);

    // A settings save does not relatch or resize live DMA buffers. Reverting
    // the saved choice to the active profile removes the pending restart.
    if (SupportsRuntimeWideProfiles) {
        checkMetadata("480x320-rgb", stock, true, true, count);
        stock.latch("480x320-rgb");
        assert(stock.active().width == 320 && stock.active().bufferPixels == 13600);
        checkMetadata("320x170", stock, true, false, count);
        for (const auto *id : {"480x320-rgb", "480x320-bgr"}) {
            DisplayProfileRuntime rebooted;
            rebooted.latch(id);
            assert(rebooted.active().width == 480 && rebooted.active().height == 320);
            assert(rebooted.active().bufferPixels == 25600 && rebooted.active().gapY == 0);
            assert(rebooted.active().bgr == (std::strcmp(id, "480x320-bgr") == 0));
            assert(rebooted.active().pixelClock == (rebooted.active().bgr ? 12000000 : 12288000));
            assert(!panelMirrorX(false, rebooted.active()) && !panelMirrorY(false, rebooted.active()));
            assert(panelMirrorX(true, rebooted.active()) && panelMirrorY(true, rebooted.active()));
            checkMetadata(id, rebooted, true, false, count);
            checkMetadata("320x170", rebooted, true, true, count);
        }
    } else {
        // A stale QAxe display key must never select a large panel on OctAxe.
        DisplayProfileRuntime otherBoard;
        otherBoard.latch("480x320-bgr");
        assert(otherBoard.active().width == 320 && !otherBoard.active().bgr);
        checkMetadata("480x320-bgr", otherBoard, false, false, count);
    }

    for (const char *invalid : {"", "480x320", "320X170", "320x170-extra"}) {
        DisplayProfileRuntime badStorage;
        badStorage.latch(invalid);
        assert(badStorage.active().width == 320);
        checkMetadata(invalid, badStorage, false, false, count);
    }
    DisplayProfileRuntime absent;
    absent.latch(nullptr);
    assert(absent.active().width == 320);
    checkMetadata(nullptr, absent, false, false, count);
    // Inversion is a user toggle relative to the established stock visuals.
    static_assert(panelInvertColors(false));
    static_assert(!panelInvertColors(true));
    std::printf("runtime display latch/metadata passed (%s)\n", SupportsRuntimeWideProfiles ? "QAxe" : "OctAxe");
}
