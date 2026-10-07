#include "handler_profiles.h"
#include "profile_store.h"
#include "pool_schedule.h"
#include "handler_capabilities.h"
#include "esp_app_desc.h"
#include "sntp.h"
#include <ctime>
extern SNTP sntp;
#include "http_cors.h"
#include "http_utils.h"
#include "psram_allocator.h"
#include "nvs_config.h"
#include <cstdlib>

using namespace FiveTratumProfiles;
namespace {
esp_err_t result(httpd_req_t *req,const char *status,const char *error=nullptr) {
    httpd_resp_set_status(req,status); JsonDocument doc; doc["ok"]=error==nullptr;
    if (error) doc["error"]=error; else doc["restartRequired"]=false;
    return sendJsonResponse(req,doc);
}
bool prepare(httpd_req_t *req,bool write) {
    if (is_network_allowed(req)!=ESP_OK) {httpd_resp_send_err(req,HTTPD_401_UNAUTHORIZED,"Unauthorized");return false;}
    httpd_resp_set_type(req,"application/json");httpd_resp_set_hdr(req,"Cache-Control","no-store");
    if (set_cors_headers(req)!=ESP_OK) {httpd_resp_send_500(req);return false;}
    if (write && validateOTP(req)!=ESP_OK) return false;
    return true;
}
bool addIdentity(JsonDocument &doc,Board *board) {
    char id[39];if(!board || !readFiveTratumPhysicalDeviceId(id)) return false;
    doc["identity"]["deviceId"]=id;
    doc["hardware"]["boardModel"]=board->getDeviceModel();doc["hardware"]["asicModel"]=board->getAsicModel();doc["hardware"]["asicCount"]=board->getAsicCount();
    doc["firmware"]["product"]="5tratumFW";doc["firmware"]["version"]=esp_app_get_description()->version;return !doc.overflowed();
}
Limits boardLimits(Board *board) {
    Limits limits;
    limits.maxFrequency=board->getAbsMaxAsicFrequency()>0?board->getAbsMaxAsicFrequency():board->getDefaultAsicFrequency();
    limits.minVoltage=board->getAbsMinAsicVoltageMillis()>0?board->getAbsMinAsicVoltageMillis():1005;
    limits.maxVoltage=board->getAbsMaxAsicVoltageMillis()>0?board->getAbsMaxAsicVoltageMillis():1400;
    return limits;
}
bool loadRequest(httpd_req_t *req, JsonDocument &doc, Request &request,bool apply) {
    if (req->content_len==0 || req->content_len>MAX_REQUEST_BYTES) {result(req,"400 Bad Request","invalid-profile");return false;}
    if (getJsonData(req,doc)!=ESP_OK) return false;
    if (!parseRequest(doc.as<JsonObjectConst>(),apply,request)) {result(req,"400 Bad Request","invalid-profile");return false;}
    return true;
}
void stringField(JsonDocument &doc,const char *field,char *value) {doc[field]=value?value:"";free(value);}
void capturePool(JsonDocument &doc,int pool) {
    stringField(doc,"host",pool?Config::getStratumFallbackURL():Config::getStratumURL());
    stringField(doc,"user",pool?Config::getStratumFallbackUser():Config::getStratumUser());
    stringField(doc,"password",pool?Config::getStratumFallbackPass():Config::getStratumPass());
    doc["port"]=pool?Config::getStratumFallbackPortNumber():Config::getStratumPortNumber();
    doc["tls"]=pool?Config::isStratumFallbackTLS():Config::isStratumTLS();
    doc["extranonceSubscribe"]=pool?Config::isStratumFallbackEnonceSubscribe():Config::isStratumEnonceSubscribe();
    doc["protocol"]=pool?Config::getFallbackStratumProtocol():Config::getStratumProtocol();
    stringField(doc,"authorityPubkey",pool?Config::getFallbackSV2AuthorityPubkey():Config::getSV2AuthorityPubkey());
    doc["channelType"]=pool?Config::getFallbackSV2ChannelType():Config::getSV2ChannelType();
    doc["coinbaseVerifyMode"]=Config::getCoinbaseVerifyMode(pool);
    doc["coinbaseMaxFee"]=Config::getCoinbaseMaxFee(pool)/10.0;
    doc["coinbaseVerifyForce"]=Config::getCoinbaseVerifyForce(pool);
}
}
esp_err_t GET_five_tratum_profiles(httpd_req_t *req) {
    ConGuard guard(http_server,req);if (!prepare(req,false)) return ESP_FAIL;
    Board *board=SYSTEM_MODULE.getBoard();if (!board) return result(req,"503 Service Unavailable","storage-unavailable");
    PSRAMAllocator allocator;JsonDocument doc(&allocator),record(&allocator);
    doc["schemaVersion"]=1;
    if(!addIdentity(doc,board)) return result(req,"503 Service Unavailable","storage-unavailable");
    auto tuning=doc["tuning"].to<JsonArray>();auto pools=doc["pools"].to<JsonArray>();
    for (unsigned i=0;i<SLOT_COUNT;++i) for (Kind kind:{Kind::Tuning,Kind::Pool}) {
        auto status=readSlot(kind,i,record);
        if (status==StoreResult::Failure) return result(req,"503 Service Unavailable","storage-unavailable");
        writePublicSlot((kind==Kind::Tuning?tuning:pools).add<JsonObject>(),i,kind,record.as<JsonObjectConst>());
    }
    const auto limits=boardLimits(board);
    auto freq=doc["limits"]["frequency"].to<JsonObject>();freq["min"]=limits.minFrequency;freq["max"]=limits.maxFrequency;freq["step"]=1;freq["quantization"]="nearest-pll";
    auto volt=doc["limits"]["coreVoltage"].to<JsonObject>();volt["min"]=limits.minVoltage;volt["max"]=limits.maxVoltage;volt["step"]=1;
    doc["current"]["frequencyMHz"]=board->getAsicFrequency();doc["current"]["coreVoltageMv"]=board->getAsicVoltageMillis();
    doc["independentWorkAssignment"]=false;
    if (doc.overflowed()) return result(req,"503 Service Unavailable","storage-unavailable");
    return sendJsonResponse(req,doc);
}
esp_err_t POST_five_tratum_profiles(httpd_req_t *req) {
    ConGuard guard(http_server,req);if (!prepare(req,true)) return ESP_FAIL;
    PSRAMAllocator allocator;JsonDocument input(&allocator),old(&allocator),captured(&allocator),record(&allocator);Request request;
    if (!loadRequest(req,input,request,false)) return ESP_FAIL;
    if (request.clear) {
        if(request.kind==Kind::Pool) {JsonDocument schedule(&allocator);if(!readPoolSchedule(schedule)) return result(req,"503 Service Unavailable","storage-unavailable");if(scheduleReferences(schedule.as<JsonObjectConst>(),request.slot)) return result(req,"409 Conflict","profile-in-use");}
        return clearSlot(request.kind,request.slot)==StoreResult::Ok?result(req,"200 OK"):result(req,"507 Insufficient Storage","storage-unavailable");
    }
    Board *board=SYSTEM_MODULE.getBoard();if (!board) return result(req,"503 Service Unavailable","storage-unavailable");
    if (readSlot(request.kind,request.slot,old)==StoreResult::Failure) return result(req,"503 Service Unavailable","storage-unavailable");
    if (request.capturePool>=0) capturePool(captured,request.capturePool);
    if (!buildRecord(input.as<JsonObjectConst>(),request,old.as<JsonObjectConst>(),captured.as<JsonObjectConst>(),boardLimits(board),record)) return result(req,"400 Bad Request","invalid-profile");
    return saveSlot(request.kind,request.slot,record.as<JsonObjectConst>())==StoreResult::Ok?result(req,"200 OK"):result(req,"507 Insufficient Storage","storage-unavailable");
}
esp_err_t POST_five_tratum_profiles_apply(httpd_req_t *req) {
    ConGuard guard(http_server,req);if (!prepare(req,true)) return ESP_FAIL;
    PSRAMAllocator allocator;JsonDocument input(&allocator),record(&allocator),patch(&allocator);Request request;
    // Per-ASIC targets are unsupported by this broadcast driver, even if an older UI sends them.
    if (!req->content_len || req->content_len>MAX_REQUEST_BYTES) return result(req,"400 Bad Request","invalid-profile");
    if (getJsonData(req,input)!=ESP_OK) return ESP_FAIL;
    if (!input["asicIndex"].isNull() || !input["coreIndex"].isNull() || (!input["poolTarget"].isNull() && strcmp(input["poolTarget"] | "","primary")!=0 && strcmp(input["poolTarget"] | "","fallback")!=0)) return result(req,"409 Conflict","unsupported-target");
    if (!parseRequest(input.as<JsonObjectConst>(),true,request)) return result(req,"400 Bad Request","invalid-profile");
    if (request.directTuning) {record["frequencyMHz"]=input["frequencyMHz"];record["coreVoltageMv"]=input["coreVoltageMv"];}
    auto status=request.directTuning?StoreResult::Ok:readSlot(request.kind,request.slot,record);
    if (status==StoreResult::Empty) return result(req,"409 Conflict","empty-slot");
    if (status!=StoreResult::Ok) return result(req,"503 Service Unavailable","storage-unavailable");
    Board *board=SYSTEM_MODULE.getBoard();if (!board) return result(req,"503 Service Unavailable","storage-unavailable");
    if (request.kind==Kind::Tuning && !validTuning(record.as<JsonObjectConst>(),boardLimits(board))) return result(req,"400 Bad Request","invalid-profile");
    buildActivePatch(patch.to<JsonObject>(),request.kind,record.as<JsonObjectConst>(),request.targetPool);
    if (patch.overflowed() || !Config::cfgApplyPatchPersisted(patch.as<JsonObjectConst>())) return result(req,"507 Insufficient Storage","storage-unavailable");
    if (request.kind==Kind::Tuning) board->loadSettings();
    else if (STRATUM_MANAGER) STRATUM_MANAGER->loadSettings();
    return result(req,"200 OK");
}

esp_err_t GET_five_tratum_pool_schedule(httpd_req_t *req) {
    ConGuard guard(http_server,req);if(!prepare(req,false))return ESP_FAIL;
    PSRAMAllocator allocator;JsonDocument doc(&allocator);
    if(!readPoolSchedule(doc) || !addIdentity(doc,SYSTEM_MODULE.getBoard())) return result(req,"503 Service Unavailable","storage-unavailable");
    bool clockValid=sntp.isTimeSynced() && time(nullptr)>=1704067200;doc["clockValid"]=clockValid;doc["timezoneMode"]="fixed-utc-offset";
    int selected=-1;if(clockValid){time_t wall=time(nullptr)+doc["utcOffsetMinutes"].as<int>()*60;tm local{};gmtime_r(&wall,&local);
        // Metadata added above is excluded from pure schedule validation.
        JsonDocument schedule(&allocator);readPoolSchedule(schedule);selected=selectedScheduledSlot(schedule.as<JsonObjectConst>(),local.tm_wday,local.tm_hour*60+local.tm_min);}
    if(selected<0)doc["selectedSlot"]=nullptr;else doc["selectedSlot"]=selected;
    doc["poolTarget"]="primary";doc["maxEvents"]=MAX_POOL_EVENTS;return sendJsonResponse(req,doc);
}
esp_err_t POST_five_tratum_pool_schedule(httpd_req_t *req) {
    ConGuard guard(http_server,req);if(!prepare(req,true))return ESP_FAIL;
    if(!req->content_len || req->content_len>MAX_REQUEST_BYTES)return result(req,"400 Bad Request","invalid-profile");
    PSRAMAllocator allocator;JsonDocument doc(&allocator),record(&allocator);if(getJsonData(req,doc)!=ESP_OK)return ESP_FAIL;
    if(!validPoolSchedule(doc.as<JsonObjectConst>()))return result(req,"400 Bad Request","invalid-profile");
    for(JsonObjectConst e:doc["events"].as<JsonArrayConst>()) if(readSlot(Kind::Pool,e["slot"].as<unsigned>(),record)!=StoreResult::Ok)return result(req,"409 Conflict","empty-slot");
    return savePoolSchedule(doc.as<JsonObjectConst>())?result(req,"200 OK"):result(req,"507 Insufficient Storage","storage-unavailable");
}
