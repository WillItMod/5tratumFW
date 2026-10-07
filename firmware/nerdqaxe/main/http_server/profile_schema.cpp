#include "profile_schema.h"
#include "sdkconfig.h"
#include "../nvs_config.h"
#include <cmath>
#include <cstring>

namespace FiveTratumProfiles {
namespace {
bool text(JsonVariantConst v, size_t max, bool empty = true) {
    if (!v.is<const char *>()) return false;
    const char *s = v.as<const char *>();
    size_t n = strlen(s);
    if (n > max || (!empty && n == 0)) return false;
    for (size_t i = 0; i < n; ++i) if (static_cast<unsigned char>(s[i]) < 32 || s[i] == 127) return false;
    return true;
}
bool integer(JsonVariantConst v, unsigned min, unsigned max) {
    return !v.is<bool>() && v.is<unsigned>() && v.as<unsigned>() >= min && v.as<unsigned>() <= max;
}
bool knownKeys(JsonObjectConst o, const char *const *keys, size_t count) {
    for (JsonPairConst p : o) {
        bool found = false;
        for (size_t i=0; i<count; ++i) if (strcmp(p.key().c_str(), keys[i]) == 0) { found=true; break; }
        if (!found) return false;
    }
    return true;
}
const char *const poolFields[] = {"host", "port", "user", "password", "protocol", "tls", "extranonceSubscribe", "authorityPubkey", "channelType", "coinbaseVerifyMode", "coinbaseMaxFee", "coinbaseVerifyForce"};
const char *const saveKeys[] = {"type", "slot", "name", "clear", "captureCurrent", "frequencyMHz", "coreVoltageMv", "host", "port", "user", "password", "protocol", "tls", "extranonceSubscribe", "authorityPubkey", "channelType", "coinbaseVerifyMode", "coinbaseMaxFee", "coinbaseVerifyForce"};
const char *const applyKeys[] = {"type", "slot", "poolTarget", "frequencyMHz", "coreVoltageMv"};
bool validPool(JsonObjectConst r) {
    if (!text(r["host"], 128, false) || !text(r["user"], 128) || !text(r["password"], 128) || !integer(r["port"],1,65535)) return false;
    const char *host = r["host"];
    for (const char *s=host; *s; ++s) if (*s == '/' || *s == ' ' || *s == '@') return false;
    if (!integer(r["protocol"],0,1) || !integer(r["channelType"],0,1) || !text(r["authorityPubkey"], 66)) return false;
    if (!r["tls"].is<bool>() || !r["extranonceSubscribe"].is<bool>() || !r["coinbaseVerifyForce"].is<bool>()) return false;
    if (!integer(r["coinbaseVerifyMode"],0,2) || !r["coinbaseMaxFee"].is<double>() || r["coinbaseMaxFee"].is<bool>()) return false;
    double fee = r["coinbaseMaxFee"].as<double>();
    if (!std::isfinite(fee) || fee<0 || fee>100 || std::fabs(fee*10-std::round(fee*10))>1e-6) return false;
    return true;
}
}

bool parseRequest(JsonObjectConst in, bool apply, Request &r) {
    if (in.isNull() || !text(in["type"],6,false)) return false;
    const char *type = in["type"];
    if (strcmp(type,"tuning") == 0) r.kind=Kind::Tuning;
    else if (strcmp(type,"pool") == 0) r.kind=Kind::Pool;
    else return false;
    r.clear=false; r.directTuning=false; r.capturePool=-1; r.targetPool=-1;
    if (apply && r.kind==Kind::Tuning && in["slot"].isNull()) {
        r.directTuning=true; return in.size()==3 && !in["frequencyMHz"].isNull() && !in["coreVoltageMv"].isNull();
    }
    if (!integer(in["slot"],0,SLOT_COUNT-1)) return false;
    r.slot=in["slot"].as<unsigned>();
    if (apply) {
        if (!knownKeys(in,applyKeys,sizeof(applyKeys)/sizeof(*applyKeys))) return false;
        if (r.kind==Kind::Tuning) return in.size()==2;
        if (!text(in["poolTarget"],8,false)) return false;
        if (strcmp(in["poolTarget"],"primary")==0) r.targetPool=0;
        else if (strcmp(in["poolTarget"],"fallback")==0) r.targetPool=1;
        else return false;
        return in.size()==3;
    }
    if (!knownKeys(in,saveKeys,sizeof(saveKeys)/sizeof(*saveKeys))) return false;
    if (!in["clear"].isNull()) {
        if (!in["clear"].is<bool>() || !in["clear"].as<bool>() || in.size()!=3) return false;
        r.clear=true; return true;
    }
    if (!text(in["name"],48,false)) return false;
    const char *name=in["name"];
    bool visible=false; for (const char *p=name; *p; ++p) if (*p!=' ') visible=true;
    if (!visible) return false;
    if (r.kind==Kind::Tuning) return in.size()==5 && !in["frequencyMHz"].isNull() && !in["coreVoltageMv"].isNull();
    if (!in["frequencyMHz"].isNull() || !in["coreVoltageMv"].isNull()) return false;
    if (!in["captureCurrent"].isNull()) {
        if (!text(in["captureCurrent"],8,false) || in.size()!=4) return false;
        if (strcmp(in["captureCurrent"],"primary")==0) r.capturePool=0;
        else if (strcmp(in["captureCurrent"],"fallback")==0) r.capturePool=1;
        else return false;
    }
    return true;
}

bool validTuning(JsonObjectConst r, const Limits &l) {
    return integer(r["frequencyMHz"],l.minFrequency,l.maxFrequency) && integer(r["coreVoltageMv"],l.minVoltage,l.maxVoltage);
}
bool validRecord(JsonObjectConst r, Kind kind) {
    if (!text(r["name"],48,false) || !integer(r["schemaVersion"],1,1)) return false;
    if (kind==Kind::Tuning) return r.size()==4 && integer(r["frequencyMHz"],1,65535) && integer(r["coreVoltageMv"],1,65535);
    if (r.size()!=14) return false;
    for (JsonPairConst p:r) {
        if (strcmp(p.key().c_str(),"name")==0 || strcmp(p.key().c_str(),"schemaVersion")==0) continue;
        bool found=false; for (auto key:poolFields) if (strcmp(p.key().c_str(),key)==0) {found=true;break;}
        if (!found) return false;
    }
    return validPool(r);
}
bool buildRecord(JsonObjectConst in, const Request &r, JsonObjectConst old, JsonObjectConst captured, const Limits &limits, JsonDocument &out) {
    out.clear();
    if (r.clear) return false;
    out["schemaVersion"]=1; out["name"]=in["name"];
    if (r.kind==Kind::Tuning) {
        out["frequencyMHz"]=in["frequencyMHz"]; out["coreVoltageMv"]=in["coreVoltageMv"];
        return !out.overflowed() && validTuning(out.as<JsonObjectConst>(),limits);
    }
    JsonObjectConst base=r.capturePool>=0 ? captured:old;
    out["host"]=base["host"] | ""; out["port"]=base["port"] | 3333;
    out["user"]=base["user"] | ""; out["password"]=base["password"] | "";
    out["protocol"]=base["protocol"] | 0; out["tls"]=base["tls"] | false;
    out["extranonceSubscribe"]=base["extranonceSubscribe"] | false;
    out["authorityPubkey"]=base["authorityPubkey"] | ""; out["channelType"]=base["channelType"] | 0;
    out["coinbaseVerifyMode"]=base["coinbaseVerifyMode"] | 0;
    out["coinbaseMaxFee"]=base["coinbaseMaxFee"] | 3.0;
    out["coinbaseVerifyForce"]=base["coinbaseVerifyForce"] | false;
    if (r.capturePool<0) for (JsonPairConst pair:in) for (auto key:poolFields) if (strcmp(pair.key().c_str(),key)==0) out[key]=pair.value();
    return !out.overflowed() && measureJson(out)<=MAX_SLOT_BYTES && validRecord(out.as<JsonObjectConst>(),Kind::Pool);
}
void writePublicSlot(JsonObject out, unsigned slot, Kind kind, JsonObjectConst record) {
    out["slot"]=slot; const bool valid=validRecord(record,kind); out["configured"]=valid;
    if (!valid) { out["name"]=nullptr; return; }
    for (JsonPairConst pair:record) {
        if (strcmp(pair.key().c_str(),"schemaVersion")==0 || strcmp(pair.key().c_str(),"password")==0) continue;
        out[pair.key()]=pair.value();
    }
    if (kind==Kind::Pool) out["passwordConfigured"]=strlen(record["password"].as<const char *>())>0;
}
void buildActivePatch(JsonObject p, Kind kind, JsonObjectConst r, int pool) {
    if (kind==Kind::Tuning) {p[NVS_CONFIG_ASIC_FREQ]=r["frequencyMHz"];p[NVS_CONFIG_ASIC_VOLTAGE]=r["coreVoltageMv"];return;}
    p[pool?NVS_CONFIG_STRATUM_FALLBACK_URL:NVS_CONFIG_STRATUM_URL]=r["host"];
    p[pool?NVS_CONFIG_STRATUM_FALLBACK_PORT:NVS_CONFIG_STRATUM_PORT]=r["port"];
    p[pool?NVS_CONFIG_STRATUM_FALLBACK_USER:NVS_CONFIG_STRATUM_USER]=r["user"];
    p[pool?NVS_CONFIG_STRATUM_FALLBACK_PASS:NVS_CONFIG_STRATUM_PASS]=r["password"];
    p[pool?NVS_CONFIG_STRATUM_FALLBACK_TLS:NVS_CONFIG_STRATUM_TLS]=r["tls"].as<bool>()?1:0;
    p[pool?NVS_CONFIG_STRATUM_FALLBACK_ENONCE_SUB:NVS_CONFIG_STRATUM_ENONCE_SUB]=r["extranonceSubscribe"].as<bool>()?1:0;
    p[pool?NVS_CONFIG_FB_STRATUM_PROTOCOL:NVS_CONFIG_STRATUM_PROTOCOL]=r["protocol"];
    p[pool?NVS_CONFIG_FB_SV2_AUTHORITY_PUBKEY:NVS_CONFIG_SV2_AUTHORITY_PUBKEY]=r["authorityPubkey"];
    p[pool?NVS_CONFIG_FB_SV2_CHANNEL_TYPE:NVS_CONFIG_SV2_CHANNEL_TYPE]=r["channelType"];
    p[pool?NVS_CONFIG_FB_COINBASE_VERIFY_MODE:NVS_CONFIG_COINBASE_VERIFY_MODE]=r["coinbaseVerifyMode"];
    p[pool?NVS_CONFIG_FB_COINBASE_MAX_FEE:NVS_CONFIG_COINBASE_MAX_FEE]=static_cast<uint16_t>(std::round(r["coinbaseMaxFee"].as<double>()*10));
    p[pool?NVS_CONFIG_FB_COINBASE_VERIFY_FORCE:NVS_CONFIG_COINBASE_VERIFY_FORCE]=r["coinbaseVerifyForce"].as<bool>()?1:0;
}
}
