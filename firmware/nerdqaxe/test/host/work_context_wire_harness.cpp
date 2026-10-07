// Cross-repository wire check: actual MUX sender frames through the firmware's
// actual parser/state and read-only serializer. Host fixtures, no device I/O.
#include "status_report.h"
#include "displays/display_data.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

static MuxStatusSnapshot receive(const char *path, MuxPeerStatus &peer,
                                uint64_t generation, int64_t &nowUs) {
    std::ifstream input(path);
    assert(input.good());
    std::string line;
    int frames = 0, advertisements = 0;
    while (std::getline(input, line)) {
        assert(line.size() <= 16384);
        JsonDocument doc;
        assert(!deserializeJson(doc, line));
        peer.observeMiningNotify(doc, generation);
        if (consumeMuxStatusNotification(doc, peer, generation, nowUs, line.size())) ++advertisements;
        ++frames;
        nowUs += 1000;
    }
    assert(frames == 2 && advertisements == 1);
    const auto status = peer.snapshot(nowUs);
    assert(status.connected && status.acknowledged && status.workContext.available);
    return status;
}

int main(int argc, char **argv) {
    assert(argc == 2 || argc == 4);
    MuxPeerStatus peers[2];
    uint64_t generations[2] = {peers[0].beginConnection(), peers[1].beginConnection()};
    int64_t nowUs = 1000000;
    const auto status = receive(argv[1], peers[0], generations[0], nowUs);
    const bool dual = argc == 4;
    if (dual) {
        // Stagger observations to prove that one stale session does not erase
        // the other. This is a synthetic monotonic clock, never wall time.
        nowUs += 45000000;
        receive(argv[2], peers[1], generations[1], nowUs);
        const std::string mode = argv[3];
        if (mode == "primary-expired") nowUs = 91001000;
        else if (mode == "primary-disconnected") peers[0].disconnect();
        else if (mode == "primary-reconnected") {
            peers[0].beginConnection();
            assert(!peers[0].acknowledge(generations[0], nowUs,
                                       status.coin, status.workContext, true));
        } else if (mode == "primary-new-notify") {
            JsonDocument next;
            auto params = next["params"].to<JsonArray>();
            next["method"] = "mining.notify";
            params.add("new-job-without-advertisement");
            for (int i = 1; i < 9; ++i) params.add("");
            params[6] = "1d00ffff";
            peers[0].observeMiningNotify(next, generations[0]);
        } else assert(mode == "fresh");
    }
    FiveTratumStatus::Snapshot snapshot;
    snapshot.firmwareVersion = "5tratumFW-work-v2-host-proof";
    strcpy(snapshot.deviceId, "5tfw:0102030405060708090a0b0c0d0e0f10");
    snapshot.boardModel = "NerdQAxe++";
    snapshot.asicModel = "BM1370";
    snapshot.asicCount = 4;
    snapshot.nowUs = nowUs;
    snapshot.dual = dual;
    snapshot.pools[0] = peers[0].snapshot(nowUs);
    if (dual) snapshot.pools[1] = peers[1].snapshot(nowUs);
    JsonDocument result;
    assert(FiveTratumStatus::buildReport(result, snapshot));
    char text[80];
    auto display = result["hostDisplayProof"].to<JsonObject>();
    FiveTratumDisplay::formatWorkHeight(text, sizeof(text), snapshot.pools[0].workContext);
    display["block"] = JsonString(text, false);
    FiveTratumDisplay::formatWorkDifficulty(text, sizeof(text), snapshot.pools[0].workContext);
    display["difficulty"] = JsonString(text, false);
    FiveTratumDisplay::formatWorkNBits(text, sizeof(text), snapshot.pools[0].workContext);
    display["nBits"] = JsonString(text, false);
    display["difficultyUnit"] = "bdiff";
    display["fixtureOnly"] = true;
    if (dual) {
        auto routes = display["routes"].to<JsonArray>();
        for (int i = 0; i < 2; ++i) {
            const auto &pool = snapshot.pools[i];
            auto route = routes.add<JsonObject>();
            route["index"] = i;
            route["coin"] = pool.coin.available ? pool.coin.ticker : "unknown";
            FiveTratumDisplay::formatWorkHeight(text, sizeof(text), pool.workContext);
            route["block"] = JsonString(text, false);
            FiveTratumDisplay::formatWorkDifficulty(text, sizeof(text), pool.workContext);
            route["difficulty"] = JsonString(text, false);
            FiveTratumDisplay::formatWorkNBits(text, sizeof(text), pool.workContext);
            route["nBits"] = JsonString(text, false);
        }
    }
    serializeJson(result, std::cout);
    std::cout << '\n';
}
