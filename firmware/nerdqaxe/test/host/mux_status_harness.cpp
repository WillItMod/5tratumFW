#include <cassert>
#include <cstring>
#include <string>
#include <limits>
#include "mux_status.h"

static const char *GOOD = R"({"id":null,"method":"mining.5tratum.status","params":[{"protocolVersion":1,"product":"5tratMUX","workScope":"chain-broadcast","independentWorkAssignment":false,"ttlSeconds":90}]})";

static bool receive(const char *json, MuxPeerStatus &state, uint64_t generation, int64_t nowUs) {
    JsonDocument doc;
    assert(!deserializeJson(doc, json));
    return consumeMuxStatusNotification(doc, state, generation, nowUs);
}

static JsonDocument v2(const char *job = "job-a", const char *bits = "1b0404cb") {
    JsonDocument doc;
    assert(!deserializeJson(doc, GOOD));
    auto status = doc["params"][0].as<JsonObject>();
    status["protocolVersion"] = 2;
    status["coin"]["id"] = "digibyte";
    status["coin"]["ticker"] = "DGB";
    status["coin"]["name"] = "DigiByte";
    auto work = status["workContext"].to<JsonObject>();
    work["jobId"] = job;
    work["height"] = 20000000U;
    work["nBits"] = bits;
    work["networkDifficulty"] = 16307.420938523983;
    work["ageSeconds"] = 0;
    work["source"] = "forwarded-stratum-job";
    return doc;
}

static void notify(MuxPeerStatus &state, uint64_t generation,
                   const char *job = "job-a", const char *bits = "1b0404cb") {
    JsonDocument doc;
    doc["id"] = nullptr;
    doc["method"] = "mining.notify";
    auto params = doc["params"].to<JsonArray>();
    params.add(job); params.add("hash"); params.add("cb1"); params.add("cb2");
    params.add<JsonArray>(); params.add("20000000"); params.add(bits); params.add("60000000"); params.add(true);
    state.observeMiningNotify(doc, generation);
    assert(!consumeMuxStatusNotification(doc, state, generation, 0));
}

static void acceptV2(MuxPeerStatus &state, uint64_t generation, int64_t nowUs) {
    notify(state, generation);
    auto doc = v2();
    assert(consumeMuxStatusNotification(doc, state, generation, nowUs));
    assert(state.snapshot(nowUs).workContext.available);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    const std::string scenario(argv[1]);
    MuxPeerStatus primary;
    const uint64_t first = primary.beginConnection();
    if (scenario == "freshness") {
        assert(!primary.snapshot(0).acknowledged);
        assert(primary.snapshot(0).connected);
        assert(receive(GOOD, primary, first, 1000000));
        assert(primary.snapshot(1000000).acknowledged);
        assert(primary.snapshot(90999999).acknowledged);
        assert(primary.snapshot(91000000).expired);
        assert(!primary.snapshot(91000000).acknowledged);
        assert(primary.snapshot(91000000).ageSeconds == 90);
        assert(receive(GOOD, primary, first, 92000000));
        assert(primary.snapshot(92000000).acknowledged);
        assert(primary.snapshot(91999999).expired); // monotonic rollback fails closed
        assert(primary.snapshot(INT64_MAX).ageSeconds == UINT32_MAX);
    } else if (scenario == "reconnect") {
        receive(GOOD, primary, first, 0);
        primary.disconnect();
        assert(!primary.snapshot(1).connected && !primary.snapshot(1).acknowledged);
        receive(GOOD, primary, first, 1); // late old callback after close
        assert(!primary.snapshot(1).acknowledged);
        const uint64_t second = primary.beginConnection();
        assert(second != first);
        receive(GOOD, primary, first, 2); // old connection callback cannot renew
        assert(!primary.snapshot(2).acknowledged);
        receive(GOOD, primary, second, 3);
        assert(primary.snapshot(3).acknowledged);
        primary.invalidate(first); // old invalidation cannot erase current proof
        assert(primary.snapshot(3).acknowledged);
        primary.disconnect(); // reconnect/config-change/stop all call this
        assert(!primary.snapshot(4).acknowledged && !primary.snapshot(4).expired);
    } else if (scenario == "fallback") {
        MuxPeerStatus fallback;
        const auto fallbackGeneration = fallback.beginConnection();
        receive(GOOD, primary, first, 0);
        assert(primary.snapshot(1).acknowledged);
        assert(!fallback.snapshot(1).acknowledged); // ordinary fallback is not MUX
        receive(GOOD, fallback, fallbackGeneration, 2);
        primary.disconnect();
        assert(!primary.snapshot(3).acknowledged && fallback.snapshot(3).acknowledged);
        fallback.disconnect();
        assert(!fallback.snapshot(4).acknowledged);
    } else if (scenario == "unrelated") {
        assert(!receive(R"({"id":6,"result":true,"error":null})", primary, first, 0));
        assert(!receive(R"({"id":null,"method":"mining.notify","params":[]})", primary, first, 0));
        assert(!receive(R"({"method":"mining.5tratum.status-extra","params":[]})", primary, first, 0));
        assert(!primary.snapshot(0).acknowledged);
    } else if (scenario == "invalid") {
        JsonDocument good;
        assert(!deserializeJson(good, GOOD));
        for (const char *bad : {"true", "false", "\"1\"", "1.0", "1.5", "null", "-1", "4294967296"}) {
            JsonDocument value;
            assert(!deserializeJson(value, bad));
            JsonDocument doc = good;
            doc["params"][0]["protocolVersion"] = value.as<JsonVariantConst>();
            primary.acknowledge(first, 0);
            assert(consumeMuxStatusNotification(doc, primary, first, 1));
            assert(!primary.snapshot(1).acknowledged);
        }
        for (const char *field : {"product", "workScope", "ttlSeconds", "independentWorkAssignment"}) {
            JsonDocument doc = good;
            doc["params"][0].remove(field);
            assert(consumeMuxStatusNotification(doc, primary, first, 1));
            assert(!primary.snapshot(1).acknowledged);
        }
        for (const char *bad : {
            R"({"id":6,"method":"mining.5tratum.status","result":true})",
            R"({"id":null,"method":"mining.5tratum.status","params":[true]})",
            R"({"id":null,"method":"mining.5tratum.status","params":[]})",
            R"({"id":null,"method":"mining.5tratum.status","params":[{},{}]})"
        }) {
            primary.acknowledge(first, 0);
            assert(receive(bad, primary, first, 1)); // invalid numeric ID is still consumed
            assert(!primary.snapshot(1).acknowledged);
        }
        for (const char *field : {"ttlSeconds", "independentWorkAssignment", "product", "workScope", "extra"}) {
            JsonDocument doc = good;
            if (!strcmp(field, "ttlSeconds")) doc["params"][0][field] = 900;
            else if (!strcmp(field, "independentWorkAssignment")) doc["params"][0][field] = true;
            else doc["params"][0][field] = "untrusted";
            assert(consumeMuxStatusNotification(doc, primary, first, 1));
            assert(!primary.snapshot(1).acknowledged);
        }
        JsonDocument oversized = good;
        oversized["padding"] = std::string(1024, 'x');
        assert(consumeMuxStatusNotification(oversized, primary, first, 1));
        assert(!primary.snapshot(1).acknowledged);
        for (const char *field : {"product", "workScope"}) {
            JsonDocument doc = good;
            const std::string text = !strcmp(field, "product") ? "5tratMUX" : "chain-broadcast";
            doc["params"][0][field] = text + std::string("\0evil", 5);
            assert(consumeMuxStatusNotification(doc, primary, first, 1));
            assert(!primary.snapshot(1).acknowledged);
        }
        JsonDocument extraEnvelope = good;
        extraEnvelope["result"] = true;
        assert(consumeMuxStatusNotification(extraEnvelope, primary, first, 1));
        assert(!primary.snapshot(1).acknowledged);
    } else if (scenario == "coin") {
        JsonDocument btc;
        assert(!deserializeJson(btc, GOOD));
        btc["params"][0]["coin"]["id"] = "bitcoin";
        btc["params"][0]["coin"]["ticker"] = "BTC";
        btc["params"][0]["coin"]["name"] = "Bitcoin";
        assert(consumeMuxStatusNotification(btc, primary, first, 100));
        auto known = primary.snapshot(100);
        assert(known.coin.available && isBitcoinCoinContext(known.coin));
        assert(isBitcoinCoinContext(selectMuxCoinContext(known, {}, false, 0)));
        assert(!selectMuxCoinContext(known, {}, false, 1).available);
        assert(!selectMuxCoinContext(known, {}, true, 0).available);
        MuxPeerStatus second;
        const auto secondGeneration = second.beginConnection();
        assert(consumeMuxStatusNotification(btc, second, secondGeneration, 100));
        assert(isBitcoinCoinContext(selectMuxCoinContext(known, second.snapshot(100), true, 1)));
        JsonDocument dgb = btc;
        dgb["params"][0]["coin"]["id"] = "digibyte";
        dgb["params"][0]["coin"]["ticker"] = "DGB";
        dgb["params"][0]["coin"]["name"] = "DigiByte";
        assert(consumeMuxStatusNotification(dgb, second, secondGeneration, 200));
        assert(!selectMuxCoinContext(known, second.snapshot(200), true, 0).available);
        assert(!isBitcoinCoinContext(selectMuxCoinContext(known, second.snapshot(200), false, 1)));
        assert(!primary.snapshot(90000100).coin.available);
        // Unknown/legacy clears immediately, not after a 90-second timeout.
        assert(receive(GOOD, primary, first, 300));
        assert(primary.snapshot(300).acknowledged && !primary.snapshot(300).coin.available);
        btc["params"][0]["coin"] = nullptr;
        assert(consumeMuxStatusNotification(btc, second, secondGeneration, 300));
        assert(second.snapshot(300).acknowledged && !second.snapshot(300).coin.available);
        assert(consumeMuxStatusNotification(dgb, primary, first, 400));
        primary.disconnect();
        assert(!primary.snapshot(400).coin.available);
        const auto newGeneration = primary.beginConnection();
        assert(consumeMuxStatusNotification(dgb, primary, first, 500));
        assert(!primary.snapshot(500).coin.available);
        assert(consumeMuxStatusNotification(dgb, primary, newGeneration, 500));
        assert(primary.snapshot(500).coin.available);
    } else if (scenario == "coin-leading-digit") {
        // The actual MUX catalog's 5TRAT declaration starts with a digit.
        // A route change must replace preceding coin/work without losing ACK.
        for (const char *id : {"bitcoin", "bitcoin-cash", "5trat", "bitcoin"}) {
            notify(primary, first);
            auto doc = v2();
            doc["params"][0]["coin"]["id"] = id;
            doc["params"][0]["coin"]["ticker"] = !strcmp(id,"5trat") ? "5TRAT" : "BTC";
            consumeMuxStatusNotification(doc, primary, first, 100);
            const auto view = primary.snapshot(100);
            assert(view.connected && view.acknowledged && !view.expired);
            assert(view.coin.available && !strcmp(view.coin.id,id));
            assert(view.workContext.available && !strcmp(view.workContext.jobId,"job-a"));
        }
        for (const char *id : {"-5trat", "_5trat", "5TRAT", "5trat!", "5trat coin", "5trat\n", "\xc3\xa9"}) {
            acceptV2(primary, first, 100);
            auto doc = v2();
            doc["params"][0]["coin"]["id"] = id;
            consumeMuxStatusNotification(doc, primary, first, 101);
            const auto view = primary.snapshot(101);
            assert(view.connected && !view.acknowledged && !view.expired);
            assert(!view.coin.available && !view.workContext.available);
        }
        // A valid digit-leading ID still cannot revive a mismatched job.
        notify(primary, first, "observed-job");
        auto mismatch = v2("different-job");
        mismatch["params"][0]["coin"]["id"] = "5trat";
        mismatch["params"][0]["coin"]["ticker"] = "5TRAT";
        consumeMuxStatusNotification(mismatch, primary, first, 102);
        assert(!primary.snapshot(102).acknowledged);
    } else if (scenario == "coin-invalid") {
        JsonDocument good;
        assert(!deserializeJson(good, GOOD));
        good["params"][0]["coin"]["id"] = "bitcoin";
        good["params"][0]["coin"]["ticker"] = "BTC";
        good["params"][0]["coin"]["name"] = "Bitcoin";
        for (const char *field : {"id", "ticker", "name"}) {
            for (const char *bad : {"", "\n", "not valid!"}) {
                JsonDocument doc = good;
                doc["params"][0]["coin"][field] = bad;
                // Printable punctuation in a display name is valid.
                if (!strcmp(field, "name") && !strcmp(bad, "not valid!")) continue;
                assert(consumeMuxStatusNotification(doc, primary, first, 1));
                assert(!primary.snapshot(1).acknowledged && !primary.snapshot(1).coin.available);
            }
            for (int kind = 0; kind < 4; ++kind) {
                JsonDocument doc = good;
                if (kind == 0) doc["params"][0]["coin"][field] = std::string(80, 'W');
                if (kind == 1) doc["params"][0]["coin"][field] = std::string("BTC\0X", 5);
                if (kind == 2) doc["params"][0]["coin"][field] = 1;
                if (kind == 3) doc["params"][0]["coin"].remove(field);
                assert(consumeMuxStatusNotification(doc, primary, first, 1));
                assert(!primary.snapshot(1).acknowledged && !primary.snapshot(1).coin.available);
            }
        }
        JsonDocument extra = good;
        extra["params"][0]["coin"]["password"] = "irrelevant";
        assert(consumeMuxStatusNotification(extra, primary, first, 1));
        assert(!primary.snapshot(1).acknowledged);
        JsonDocument maximal = good;
        maximal["params"][0]["coin"]["id"] = std::string(32, 'a');
        maximal["params"][0]["coin"]["ticker"] = std::string(12, 'W');
        maximal["params"][0]["coin"]["name"] = std::string(48, 'W');
        assert(consumeMuxStatusNotification(maximal, primary, first, 1));
        assert(primary.snapshot(1).coin.available);
        assert(strlen(primary.snapshot(1).coin.name) == 48);
    } else if (scenario == "work") {
        acceptV2(primary, first, 1000000);
        auto view = primary.snapshot(1000000);
        assert(view.acknowledged && !strcmp(view.coin.ticker, "DGB"));
        assert(view.workContext.available && !strcmp(view.workContext.jobId, "job-a"));
        assert(view.workContext.heightAvailable && view.workContext.height == 20000000);
        assert(view.workContext.difficultyAvailable && view.workContext.networkDifficulty > 16307);
        assert(!strcmp(view.workContext.nBits, "1b0404cb"));
        auto doc = v2();
        doc["params"][0]["workContext"]["height"] = nullptr;
        doc["params"][0]["workContext"]["networkDifficulty"] = nullptr;
        doc["params"][0]["coin"] = nullptr;
        assert(consumeMuxStatusNotification(doc, primary, first, 2000000));
        view = primary.snapshot(2000000);
        assert(!view.coin.available && view.workContext.available);
        assert(!view.workContext.heightAvailable && !view.workContext.difficultyAvailable);
        doc["params"][0]["workContext"] = nullptr;
        assert(consumeMuxStatusNotification(doc, primary, first, 3000000));
        assert(primary.snapshot(3000000).acknowledged && !primary.snapshot(3000000).workContext.available);
        acceptV2(primary, first, 4000000);
        receive(GOOD, primary, first, 5000000);
        assert(!primary.snapshot(5000000).workContext.available); // legacy downgrade
        auto legacy = v2();
        legacy["params"][0]["protocolVersion"] = 1;
        legacy["params"][0].remove("workContext");
        consumeMuxStatusNotification(legacy, primary, first, 6000000);
        assert(primary.snapshot(6000000).coin.available && !primary.snapshot(6000000).workContext.available);
    } else if (scenario == "work-binding") {
        auto doc = v2();
        consumeMuxStatusNotification(doc, primary, first, 0);
        assert(!primary.snapshot(0).acknowledged); // no observed mining.notify
        notify(primary, first, "different-job");
        consumeMuxStatusNotification(doc, primary, first, 1);
        assert(!primary.snapshot(1).acknowledged);
        notify(primary, first, "job-a", "1d00ffff");
        consumeMuxStatusNotification(doc, primary, first, 2);
        assert(!primary.snapshot(2).acknowledged); // matching ID is insufficient
        notify(primary, first, "job-a", "1B0404CB"); // valid Stratum hex normalization
        consumeMuxStatusNotification(doc, primary, first, 3);
        assert(primary.snapshot(3).workContext.available);
        notify(primary, first, "next-job");
        assert(!primary.snapshot(4).workContext.available && !primary.snapshot(4).coin.available);
        consumeMuxStatusNotification(doc, primary, first, 4);
        assert(!primary.snapshot(4).acknowledged); // late preceding job cannot revive
        acceptV2(primary, first, 5);
        JsonDocument malformed;
        deserializeJson(malformed, R"({"method":"mining.notify","params":[]})");
        primary.observeMiningNotify(malformed, first);
        assert(!primary.snapshot(6).workContext.available && !primary.snapshot(6).coin.available);
        acceptV2(primary, first, 6);
        assert(!deserializeJson(malformed, R"({"method":"mining.notify\u0000suffix","params":[]})"));
        primary.observeMiningNotify(malformed, first);
        assert(!primary.snapshot(6).workContext.available && !primary.snapshot(6).coin.available);
        primary.disconnect();
        const auto second = primary.beginConnection();
        notify(primary, first); // late old connection cannot bind a job
        consumeMuxStatusNotification(doc, primary, second, 7);
        assert(!primary.snapshot(7).acknowledged);
        acceptV2(primary, second, 8);
        notify(primary, first, "wrong");
        primary.invalidate(first);
        assert(primary.snapshot(9).workContext.available);
    } else if (scenario == "work-age") {
        notify(primary, first);
        auto doc = v2();
        doc["params"][0]["workContext"]["ageSeconds"] = 89;
        consumeMuxStatusNotification(doc, primary, first, 1000000);
        assert(primary.snapshot(1999999).workContext.available);
        auto expired = primary.snapshot(2000000);
        assert(expired.acknowledged && !expired.workContext.available && !expired.coin.available);
        // An age regression in a heartbeat cannot extend the existing job.
        doc["params"][0]["workContext"]["ageSeconds"] = 0;
        consumeMuxStatusNotification(doc, primary, first, 2000000);
        assert(!primary.snapshot(2000000).workContext.available && !primary.snapshot(2000000).coin.available);
        consumeMuxStatusNotification(doc, primary, first, 2100000);
        assert(!primary.snapshot(2100000).workContext.available); // repeat cannot resurrect
        auto unknown = v2(); unknown["params"][0]["workContext"] = nullptr;
        consumeMuxStatusNotification(unknown, primary, first, 2200000);
        consumeMuxStatusNotification(doc, primary, first, 2300000);
        assert(!primary.snapshot(2300000).workContext.available); // null cannot reset age bound
        acceptV2(primary, first, 3000000);
        auto fresh = v2();
        consumeMuxStatusNotification(fresh, primary, first, 33000000);
        assert(primary.snapshot(33000000).workContext.ageSeconds == 30);
        consumeMuxStatusNotification(fresh, primary, first, 63000000);
        assert(primary.snapshot(92999999).workContext.available);
        assert(!primary.snapshot(93000000).workContext.available);
        assert(primary.snapshot(93000000).acknowledged); // peer heartbeat remains fresh
        assert(!primary.snapshot(62999999).workContext.available); // monotonic rollback
        assert(!primary.snapshot(INT64_MAX).workContext.available);
    } else if (scenario == "work-pools") {
        MuxPeerStatus secondary;
        const auto second = secondary.beginConnection();
        acceptV2(primary, first, 100);
        notify(secondary, second, "job-b", "1d00ffff");
        auto doc = v2("job-b", "1d00ffff");
        doc["params"][0]["coin"]["id"] = "bitcoin";
        doc["params"][0]["coin"]["ticker"] = "BTC";
        doc["params"][0]["coin"]["name"] = "Bitcoin";
        doc["params"][0]["workContext"]["height"] = 900000;
        doc["params"][0]["workContext"]["networkDifficulty"] = 1;
        consumeMuxStatusNotification(doc, secondary, second, 100);
        const auto p1 = primary.snapshot(101), p2 = secondary.snapshot(101);
        assert(!strcmp(selectMuxWorkContext(p1,p2,false,0).jobId,"job-a"));
        assert(!strcmp(selectMuxWorkContext(p1,p2,false,1).jobId,"job-b"));
        assert(!selectMuxWorkContext(p1,p2,true,0).available);
        assert(!selectMuxCoinContext(p1,p2,true,0).available);
        primary.disconnect();
        assert(!selectMuxWorkContext(primary.snapshot(102),p2,false,0).available);
        assert(selectMuxWorkContext(primary.snapshot(102),p2,false,1).available);
    } else if (scenario == "work-invalid") {
        for (const char *field : {"jobId", "height", "nBits", "networkDifficulty", "ageSeconds", "source"}) {
            acceptV2(primary, first, 0);
            auto doc = v2(); doc["params"][0]["workContext"].remove(field);
            assert(consumeMuxStatusNotification(doc, primary, first, 1));
            assert(!primary.snapshot(1).acknowledged && !primary.snapshot(1).workContext.available);
        }
        for (const char *field : {"height", "ageSeconds", "networkDifficulty"}) {
            for (const char *bad : {"true", "false", "\"1\"", "-1", "1e1000"}) {
                acceptV2(primary, first, 0);
                auto doc = v2(); JsonDocument value; assert(!deserializeJson(value, bad));
                doc["params"][0]["workContext"][field] = value.as<JsonVariantConst>();
                assert(consumeMuxStatusNotification(doc, primary, first, 1));
                assert(!primary.snapshot(1).acknowledged);
            }
        }
        for (const char *bits : {"1B0404CB", "1234567", "123456789", "1z0404cb", ""}) {
            acceptV2(primary, first, 0);
            auto doc = v2("job-a", bits); consumeMuxStatusNotification(doc, primary, first, 1);
            assert(!primary.snapshot(1).acknowledged);
        }
        for (const char *field : {"coin", "workContext"}) {
            acceptV2(primary, first, 0);
            auto doc = v2(); doc["params"][0].remove(field);
            consumeMuxStatusNotification(doc, primary, first, 1);
            assert(!primary.snapshot(1).acknowledged);
        }
        for (const char *bad : {"", "line\nfeed", "nonascii\xc3\xa9"}) {
            acceptV2(primary, first, 0);
            auto doc = v2(bad); consumeMuxStatusNotification(doc, primary, first, 1);
            assert(!primary.snapshot(1).acknowledged);
        }
        for (const char *field : {"extra", "source", "ageSeconds", "height", "networkDifficulty"}) {
            acceptV2(primary, first, 0);
            auto doc = v2();
            if (!strcmp(field,"ageSeconds")) doc["params"][0]["workContext"][field] = 90;
            else if (!strcmp(field,"height")) doc["params"][0]["workContext"][field] = uint64_t(4294967296ULL);
            else if (!strcmp(field,"networkDifficulty")) doc["params"][0]["workContext"][field] = 0;
            else doc["params"][0]["workContext"][field] = "untrusted";
            consumeMuxStatusNotification(doc, primary, first, 1);
            assert(!primary.snapshot(1).acknowledged);
        }
        const std::string maximumJob(64, 'J');
        notify(primary, first, maximumJob.c_str());
        auto maximal = v2(maximumJob.c_str());
        maximal["params"][0]["workContext"]["height"] = UINT32_MAX;
        maximal["params"][0]["workContext"]["ageSeconds"] = 89;
        consumeMuxStatusNotification(maximal, primary, first, 2);
        assert(primary.snapshot(2).workContext.height == UINT32_MAX);
        assert(strlen(primary.snapshot(2).workContext.jobId) == 64);
        maximal["params"][0]["workContext"]["jobId"] = std::string(65,'J');
        consumeMuxStatusNotification(maximal, primary, first, 3);
        assert(!primary.snapshot(3).acknowledged);
        acceptV2(primary, first, 4);
        auto doc = v2();
        consumeMuxStatusNotification(doc, primary, first, 5, 1024);
        assert(primary.snapshot(5).workContext.available); // exact wire bound supported
        consumeMuxStatusNotification(doc, primary, first, 5, 1025); // raw whitespace cannot bypass bound
        assert(!primary.snapshot(5).acknowledged);
    } else return 2;
    return 0;
}
