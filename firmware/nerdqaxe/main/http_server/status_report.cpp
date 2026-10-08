#include "status_report.h"
#include "boards/five_tratum_model_labels.h"

#include <cmath>
#include <cstring>

namespace FiveTratumStatus {
namespace {

bool validVersion(const char *value) {
    if (!value) return false;
    const size_t length = strnlen(value, 33);
    if (!length || length > 32) return false;
    for (size_t i = 0; i < length; ++i)
        if (static_cast<unsigned char>(value[i]) < 32 ||
            static_cast<unsigned char>(value[i]) > 126) return false;
    return true;
}

bool validIdentity(const char *value) {
    constexpr size_t size = FiveTratumCapabilities::DEVICE_ID_SIZE;
    if (strnlen(value, size) != size - 1 || strncmp(value, "5tfw:", 5)) return false;
    bool nonzero = false;
    for (size_t i = 5; i < size - 1; ++i) {
        const char c = value[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
        nonzero |= c != '0';
    }
    return nonzero;
}

bool validHardwareLabel(const char *value) {
    if (!value) return false;
    const size_t length = strnlen(value, 65);
    if (!length || length > 64) return false;
    for (size_t i = 0; i < length; ++i) {
        const unsigned char c = value[i];
        if (c < 32 || c > 126) return false;
    }
    return true;
}

void coinReport(JsonObject object, const MuxCoinContext &coin) {
    if (coin.available) {
        // Context selection returns a value. Copy its strings into the JSON
        // document; borrowing temporary snapshot buffers would outlive them.
        object["id"] = JsonString(coin.id, false);
        object["ticker"] = JsonString(coin.ticker, false);
        object["name"] = JsonString(coin.name, false);
        object["source"] = "5tratMUX-route-advertisement";
    } else {
        object["id"] = nullptr;
        object["ticker"] = nullptr;
        object["name"] = nullptr;
        object["source"] = "unavailable";
    }
}

void workReport(JsonVariant object, const MuxWorkContext &work) {
    if (!work.available) { object.set(nullptr); return; }
    JsonObject report = object.to<JsonObject>();
    report["jobId"] = JsonString(work.jobId, false);
    report["nBits"] = JsonString(work.nBits, false);
    if (work.heightAvailable) report["height"] = work.height;
    else report["height"] = nullptr;
    if (work.difficultyAvailable) report["networkDifficulty"] = work.networkDifficulty;
    else report["networkDifficulty"] = nullptr;
    report["ageSeconds"] = work.ageSeconds;
    report["source"] = "forwarded-stratum-job";
}

} // namespace

bool buildReport(JsonDocument &doc, const Snapshot &s) {
    doc.clear();
    if (!validVersion(s.firmwareVersion) || !validIdentity(s.deviceId) ||
        (!validHardwareLabel(s.boardModel) && !FiveTratumModels::isOctaxeGamma(s.boardModel)) ||
        !validHardwareLabel(s.asicModel) || s.nowUs < 0 ||
        s.asicCount < 1 || s.asicCount > MAX_ASICS ||
        s.activePool < 0 || s.activePool > 1) return false;

    doc["schemaVersion"] = 1;
    JsonObject firmware = doc["firmware"].to<JsonObject>();
    firmware["product"] = "5tratumFW";
    firmware["version"] = s.firmwareVersion;
    doc["identity"]["deviceId"] = JsonString(s.deviceId, false);
    JsonObject hardware = doc["hardware"].to<JsonObject>();
    hardware["boardModel"] = JsonString(s.boardModel, false);
    hardware["asicModel"] = JsonString(s.asicModel, false);
    hardware["asicCount"] = s.asicCount;
    doc["observedUptimeSeconds"] = s.uptimeSeconds;
    JsonObject work = doc["work"].to<JsonObject>();
    work["scope"] = "chain-broadcast";
    work["independentAssignment"] = false;
    work["poolMode"] = s.dual ? "dual" : "failover";
    if (s.dual) work["activePool"] = nullptr;
    else work["activePool"] = s.activePool;

    JsonArray asics = doc["asics"].to<JsonArray>();
    for (int index = 0; index < s.asicCount; ++index) {
        JsonObject asic = asics.add<JsonObject>();
        const auto &sample = s.rates[index];
        const bool fresh = sample.isFresh(s.nowUs);
        asic["index"] = index;
        asic["fresh"] = fresh;
        if (fresh) asic["hashRateGHs"] = sample.ghPerSecond;
        else asic["hashRateGHs"] = nullptr;
        if (sample.available && sample.capturedAtUs > 0 &&
            s.nowUs >= sample.capturedAtUs)
            asic["sampleAgeSeconds"] = (s.nowUs - sample.capturedAtUs) / 1000000.0;
        else asic["sampleAgeSeconds"] = nullptr;
        asic["hashrateSource"] = "hardware-counter-estimate";
        const float temperature = s.temperaturesC[index];
        if (std::isfinite(temperature) && temperature > 0 && temperature < 200)
            asic["temperatureC"] = temperature;
        else asic["temperatureC"] = nullptr;
        asic["temperatureSource"] = "cached-board-reading";
    }

    JsonObject mux = doc["mux"].to<JsonObject>();
    mux["independentWorkAssignment"] = false;
    JsonArray pools = mux["pools"].to<JsonArray>();
    for (int index = 0; index < 2; ++index) {
        const MuxStatusSnapshot &peer = s.pools[index];
        JsonObject pool = pools.add<JsonObject>();
        pool["index"] = index;
        // A TCP connection alone must never become a MUX acknowledgement.
        pool["connected"] = peer.acknowledged;
        pool["transportConnected"] = peer.connected;
        pool["expired"] = peer.expired;
        pool["serverId"] = nullptr;
        pool["ttlSeconds"] = peer.ttlSeconds;
        if (peer.acknowledged || peer.expired) pool["statusAgeSeconds"] = peer.ageSeconds;
        else pool["statusAgeSeconds"] = nullptr;
        coinReport(pool["coin"].to<JsonObject>(),
                   peer.acknowledged ? peer.coin : MuxCoinContext{});
        workReport(pool["workContext"].to<JsonVariant>(),
                   peer.acknowledged ? peer.workContext : MuxWorkContext{});
    }
    coinReport(doc["coin"].to<JsonObject>(),
               selectMuxCoinContext(s.pools[0], s.pools[1], s.dual, s.activePool));
    workReport(doc["workContext"].to<JsonVariant>(),
               selectMuxWorkContext(s.pools[0], s.pools[1], s.dual, s.activePool));
    if (doc.overflowed() || measureJson(doc) > MAX_RESPONSE_BYTES) {
        doc.clear();
        return false;
    }
    return true;
}

} // namespace FiveTratumStatus
