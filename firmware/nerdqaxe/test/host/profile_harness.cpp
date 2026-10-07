#include "profile_schema.h"
#include "config_transaction.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace FiveTratumProfiles;
bool commitOk(const JsonDocument &,void *) {return true;}
bool commitFail(const JsonDocument &,void *) {return false;}
int main() {
    Limits limits; JsonDocument input, old, captured, record, publicSlot, patch, current;
    Request request;
    auto parse=[&](const char *json,bool apply=false) { input.clear(); assert(!deserializeJson(input,json));return parseRequest(input.as<JsonObjectConst>(),apply,request); };
    assert(parse(R"({"type":"tuning","slot":9,"name":"Custom 577","frequencyMHz":577,"coreVoltageMv":1137})"));
    assert(buildRecord(input.as<JsonObjectConst>(),request,{}, {},limits,record));
    assert(record["frequencyMHz"].as<int>()==577);
    writePublicSlot(publicSlot.to<JsonObject>(),9,Kind::Tuning,record.as<JsonObjectConst>());
    assert(publicSlot["configured"].as<bool>() && publicSlot["slot"].as<int>()==9);
    buildActivePatch(patch.to<JsonObject>(),Kind::Tuning,record.as<JsonObjectConst>(),-1);
    assert(patch.size()==2 && patch["asicfrequency"].as<int>()==577 && patch["asicvoltage"].as<int>()==1137);
    assert(!deserializeJson(current,R"({"asicfrequency":500,"asicvoltage":1130,"fan1speed":71,"pid_d":9,"overheat_temp":72,"stratumurl":"original","fbstratumurl":"other","pool_mode":1,"pool_balance":37,"vr_frequency":25011})"));
    assert(!applyPersistedPatch(current,patch.as<JsonObjectConst>(),commitFail,nullptr));
    assert(current["asicfrequency"].as<int>()==500 && current["asicvoltage"].as<int>()==1130);
    assert(applyPersistedPatch(current,patch.as<JsonObjectConst>(),commitOk,nullptr));
    assert(current["asicfrequency"].as<int>()==577 && current["fan1speed"].as<int>()==71 && current["pid_d"].as<int>()==9 && current["pool_balance"].as<int>()==37 && current["vr_frequency"].as<int>()==25011);
    for (const char *bad:{R"({"type":"tuning","slot":10,"name":"bad","frequencyMHz":500,"coreVoltageMv":1130})",R"({"type":"tuning","slot":false,"name":"bad","frequencyMHz":500,"coreVoltageMv":1130})",R"({"type":"tuning","slot":0,"name":"bad","frequencyMHz":500,"coreVoltageMv":1130,"asicIndex":0})",R"({"type":"pool","slot":0,"name":"bad","captureCurrent":"core0"})",R"({"type":"pool","slot":0,"name":"bad","captureCurrent":"primary","password":"x"})",R"({"type":"tuning","slot":0,"name":"   ","frequencyMHz":500,"coreVoltageMv":1130})"}) assert(!parse(bad));
    for (const char *bad:{R"({"type":"tuning","slot":0,"name":"bad","frequencyMHz":0,"coreVoltageMv":1130})",R"({"type":"tuning","slot":0,"name":"bad","frequencyMHz":801,"coreVoltageMv":1130})",R"({"type":"tuning","slot":0,"name":"bad","frequencyMHz":500.25,"coreVoltageMv":1130})",R"({"type":"tuning","slot":0,"name":"bad","frequencyMHz":500,"coreVoltageMv":1401})",R"({"type":"tuning","slot":0,"name":"bad","frequencyMHz":true,"coreVoltageMv":1130})"}) { assert(parse(bad));assert(!buildRecord(input.as<JsonObjectConst>(),request,{}, {},limits,record)); }
    assert(!deserializeJson(captured,R"({"host":"mux.test","port":7331,"user":"fixture.worker","password":"secret-not-public","protocol":1,"tls":true,"extranonceSubscribe":false,"authorityPubkey":"fixturekey","channelType":1,"coinbaseVerifyMode":2,"coinbaseMaxFee":3.2,"coinbaseVerifyForce":true})"));
    assert(parse(R"({"type":"pool","slot":0,"name":"DGB route","captureCurrent":"primary"})"));
    assert(buildRecord(input.as<JsonObjectConst>(),request,{},captured.as<JsonObjectConst>(),limits,record));
    assert(validRecord(record.as<JsonObjectConst>(),Kind::Pool)); old=record;
    publicSlot.clear();writePublicSlot(publicSlot.to<JsonObject>(),0,Kind::Pool,record.as<JsonObjectConst>());
    assert(publicSlot["password"].isNull() && publicSlot["passwordConfigured"].as<bool>());
    std::string redacted;serializeJson(publicSlot,redacted);assert(redacted.find("secret-not-public")==std::string::npos);
    assert(parse(R"({"type":"pool","slot":0,"name":"Edited","host":"other.test"})"));
    assert(buildRecord(input.as<JsonObjectConst>(),request,old.as<JsonObjectConst>(),{},limits,record));
    assert(strcmp(record["password"],"secret-not-public")==0);
    assert(parse(R"({"type":"pool","slot":0,"name":"Edited","password":""})"));
    assert(buildRecord(input.as<JsonObjectConst>(),request,old.as<JsonObjectConst>(),{},limits,record));assert(strcmp(record["password"],"")==0);
    assert(parse(R"({"type":"pool","slot":0,"name":"Edited","password":null})"));
    assert(!buildRecord(input.as<JsonObjectConst>(),request,old.as<JsonObjectConst>(),{},limits,record));
    record=old;patch.clear();buildActivePatch(patch.to<JsonObject>(),Kind::Pool,record.as<JsonObjectConst>(),0);
    assert(patch.size()==12 && strcmp(patch["stratumpass"],"secret-not-public")==0 && patch["cb_max_fee"].as<int>()==32);
    assert(applyPersistedPatch(current,patch.as<JsonObjectConst>(),commitOk,nullptr));
    assert(strcmp(current["fbstratumurl"],"other")==0 && current["pool_mode"].as<int>()==1 && current["pool_balance"].as<int>()==37 && current["overheat_temp"].as<int>()==72);
    assert(parse(R"({"type":"pool","slot":0,"poolTarget":"fallback"})",true));assert(request.targetPool==1);
    assert(!parse(R"({"type":"pool","slot":0,"poolTarget":"asic0"})",true));
    assert(!parse(R"({"type":"pool","slot":0,"poolTarget":"primary","coreIndex":0})",true));
    assert(parse(R"({"type":"pool","slot":9,"clear":true})"));assert(request.clear);
    publicSlot.clear();writePublicSlot(publicSlot.to<JsonObject>(),9,Kind::Pool,{});assert(!publicSlot["configured"].as<bool>() && publicSlot["name"].isNull());
    std::cout << "profile validation, credentials, exact patch and commit-before-publish tests passed\n";
}
