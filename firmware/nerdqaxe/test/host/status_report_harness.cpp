#include "status_report.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>

using FiveTratumStatus::Snapshot;

struct FailingAllocator : ArduinoJson::Allocator {
    void *allocate(size_t) override { return nullptr; }
    void deallocate(void *) override {}
    void *reallocate(void *, size_t) override { return nullptr; }
};

static MuxStatusSnapshot peer(const char *id, const char *ticker) {
    MuxStatusSnapshot result;
    result.connected = true;
    result.acknowledged = true;
    result.coin.available = true;
    strcpy(result.coin.id, id);
    strcpy(result.coin.ticker, ticker);
    strcpy(result.coin.name, "Configured route");
    return result;
}

int main(int argc, char **argv) {
    const char *scenario = argc == 2 ? argv[1] : "fresh";
    Snapshot s;
    s.firmwareVersion = "5tratumFW-qa-web-a3";
    strcpy(s.deviceId, "5tfw:0102030405060708090a0b0c0d0e0f10");
    s.boardModel = "NerdQAxe++";
    s.asicModel = "BM1370";
    s.nowUs = 30000000;
    s.uptimeSeconds = 30;
    s.asicCount = 4;
    for (int i = 0; i < s.asicCount; ++i) {
        s.rates[i] = {1000.0f + i, 25000000, true};
        s.temperaturesC[i] = 50.0f + i;
    }
    if (!strcmp(scenario, "zero")) s.rates[0].ghPerSecond = 0;
    if (!strcmp(scenario, "unavailable")) {
        s.rates[0].available = false;
        s.rates[1].capturedAtUs = 14000000;
        s.rates[2].capturedAtUs = 31000000;
        s.rates[3].ghPerSecond = std::numeric_limits<float>::quiet_NaN();
        s.temperaturesC[0] = 0;
        s.temperaturesC[1] = -1;
        s.temperaturesC[2] = std::numeric_limits<float>::infinity();
    }
    if (!strcmp(scenario, "boundary")) s.rates[0].capturedAtUs = 15000000;
    if (!strcmp(scenario, "tcp-only")) s.pools[0].connected = true;
    if (!strcmp(scenario, "known")) s.pools[0] = peer("bitcoin", "BTC");
    if (!strcmp(scenario, "mixed")) {
        s.dual = true;
        s.pools[0] = peer("bitcoin", "BTC");
        s.pools[1] = peer("digibyte", "DGB");
    }
    if (!strcmp(scenario, "expired")) {
        s.pools[0] = peer("bitcoin", "BTC");
        s.pools[0].acknowledged = false;
        s.pools[0].expired = true;
        s.pools[0].ageSeconds = 90;
    }
    if (!strcmp(scenario, "work") || !strcmp(scenario, "work-dual") ||
        !strcmp(scenario, "work-fallback") || !strcmp(scenario, "work-expired") ||
        !strcmp(scenario, "work-nullable")) {
        s.pools[0] = peer("digibyte", "DGB");
        s.pools[0].workContext = {true, "dgb-job", "1b0404cb", true, 20000000U, true, 16307.420938523983, 89};
        if (!strcmp(scenario, "work-dual") || !strcmp(scenario, "work-fallback")) {
            s.pools[1] = peer("bitcoin", "BTC");
            s.pools[1].workContext = {true, "btc-job", "1d00ffff", true, 900000U, true, 1, 3};
            s.dual = !strcmp(scenario, "work-dual");
            s.activePool = 1;
        }
        if (!strcmp(scenario, "work-expired")) {
            s.pools[0].acknowledged = false;
            s.pools[0].expired = true;
            s.pools[0].ageSeconds = 90;
        }
        if (!strcmp(scenario, "work-nullable")) {
            s.pools[0].workContext.heightAvailable = false;
            s.pools[0].workContext.difficultyAvailable = false;
        }
    }
    if (!strcmp(scenario, "max")) {
        s.asicCount = FiveTratumStatus::MAX_ASICS;
        for (int i = 0; i < s.asicCount; ++i)
            s.rates[i] = {1000.25f, 25000000, true};
    }
    if (!strcmp(scenario, "invalid-count")) s.asicCount = 65;
    if (!strcmp(scenario, "invalid-version")) s.firmwareVersion = "bad\nversion";
    if (!strcmp(scenario, "invalid-identity")) s.deviceId[5] = 'x';
    FailingAllocator failing;
    JsonDocument standard;
    JsonDocument failed(&failing);
    JsonDocument &doc = !strcmp(scenario, "oom") ? failed : standard;
    const bool built = FiveTratumStatus::buildReport(doc, s);
    if (!built) {
        assert(doc.isNull());
        std::cout << "{\"built\":false}";
    } else {
        assert(measureJson(doc) <= FiveTratumStatus::MAX_RESPONSE_BYTES);
        serializeJson(doc, std::cout);
    }
    std::cout << '\n';
}
