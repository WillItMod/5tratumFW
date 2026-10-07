#include "displays/boot_status.h"
#include <cassert>
#include <cstring>
#include <string>
#include <limits>

using namespace FiveTratumDisplay;

int main() {
    BootStatusHistory history;
    assert(history.size() == 0 && !strcmp(history.line(999), ""));
    assert(!history.push(nullptr) && !history.push("") && !history.push("   "));
    for (const char *stage : {"Settings loaded", "Board initialized", "Display ready", "Network started"})
        assert(history.push(stage));
    assert(history.size() == 4 && !strcmp(history.line(0), "Settings loaded"));
    assert(!history.push("Network started"));
    assert(history.push("Waiting for network IP"));
    assert(history.size() == 4 && !strcmp(history.line(0), "Board initialized"));
    assert(!strcmp(history.line(3), "Waiting for network IP"));
    assert(history.push("Bad\ncontrol\tvalue\r"));
    assert(!strcmp(history.line(3), "Bad control value"));
    const std::string longStage(10000, 'W');
    assert(history.push(longStage.c_str()));
    assert(strlen(history.line(3)) == BootStatusHistory::LINE_BYTES - 1);

    StartupDisplayGate early;
    assert(!early.complete()); // Initialization finished before splash timeout.
    assert(early.isComplete() && early.requestMining());
    StartupDisplayGate late;
    assert(!late.requestMining()); // Splash timeout cannot hide ongoing startup.
    assert(!late.isComplete() && late.complete());
    assert(late.isComplete() && late.requestMining());

    IdleBrandCycle cycle;
    const BrandViewConditions allowed{true, true, true, false, false, false, false, true};
    assert(!cycle.due(119999999, 0, allowed));
    assert(cycle.due(120000000, 90000000, allowed));
    assert(!cycle.due(120000000, 90000001, allowed));
    assert(!cycle.due(120000000, 120000001, allowed));
    assert(!cycle.due(-1, 0, allowed));
    assert(!cycle.due(std::numeric_limits<int64_t>::max(), -1, allowed));
    for (int blocked = 0; blocked < 8; ++blocked) {
        auto view = allowed;
        switch (blocked) {
            case 0: view.miningOverview = false; break; // Settings, QR, identify, boot.
            case 1: view.displayOn = false; break;      // Never wake an auto-off screen.
            case 2: view.bootComplete = false; break;
            case 3: view.overlay = true; break;        // Fault or block candidate.
            case 4: view.enrollmentActive = true; break;
            case 5: view.shutdown = true; break;
            case 6: view.animation = true; break;
            case 7: view.telemetryFresh = false; break;
        }
        assert(!cycle.due(120000000, 0, view));
    }
    cycle.shown(120000000);
    assert(!cycle.due(239999999, 0, allowed));
    assert(cycle.due(240000000, 0, allowed));
    assert(!cycle.due(119999999, 0, allowed)); // Clock rollback fails closed.
    assert(IdleBrandCycle::DURATION_US == 3000000);
}
