#include "http_utils.h"
#include "handler_profiles.h"
#include "pool_schedule.h"
#include "config_transaction.h"
#include "nvs.h"
#include "sntp.h"
#include <cassert>
#include <map>
#include <cstring>
#include <iostream>
httpd_handle_t http_server=0;System SYSTEM_MODULE;StratumManager manager;StratumManager *STRATUM_MANAGER=&manager;SNTP sntp;
std::map<std::string,std::string> flash;bool failWrite=false;int writes=0;JsonDocument active,schedule;
int nvs_open(const char *,int mode,nvs_handle_t *h){*h=1;return mode==NVS_READONLY && flash.empty()?ESP_ERR_NVS_NOT_FOUND:ESP_OK;}
int nvs_get_blob(nvs_handle_t,const char *key,void *out,size_t *len){auto it=flash.find(key);if(it==flash.end())return ESP_ERR_NVS_NOT_FOUND;if(out)memcpy(out,it->second.data(),it->second.size());*len=it->second.size();return ESP_OK;}
int nvs_set_blob(nvs_handle_t,const char *key,const void *buffer,size_t len){if(failWrite)return -7;flash[key]=std::string(static_cast<const char *>(buffer),len);++writes;return ESP_OK;}
int nvs_commit(nvs_handle_t){return ESP_OK;}void nvs_close(nvs_handle_t){}
int nvs_erase_key(nvs_handle_t,const char *key){if(failWrite)return -7;return flash.erase(key)?ESP_OK:ESP_ERR_NVS_NOT_FOUND;}
namespace Config {
char *cfgGetStrAlloc(const char *path,const char *def){return strdup(active[path] | def);}
uint16_t cfgGetU16(const char *path,uint16_t def){return active[path] | def;}
bool cfgApplyPatchPersisted(JsonObjectConst patch){return FiveTratumProfiles::applyPersistedPatch(active,patch,[](const JsonDocument &,void *){return !failWrite;},nullptr);}
}
namespace FiveTratumProfiles {
bool readPoolSchedule(JsonDocument &doc){doc=schedule;return true;}
bool savePoolSchedule(JsonObjectConst s){if(failWrite)return false;schedule.set(s);++writes;return true;}
}
int main(){
    assert(!deserializeJson(active,R"({"asicfrequency":500,"asicvoltage":1130,"stratumurl":"mux.test","stratumport":7331,"stratumuser":"fixture.worker","stratumpass":"original-secret","stratumtls":1,"stratumesub":1,"sv2_proto":1,"sv2_auth_pk":"fixturekey","sv2_chtype":1,"cb_verify_mode":2,"cb_max_fee":32,"cb_vfy_force":1,"fbstratumurl":"other.test","fan1speed":71,"overheat_temp":72,"pool_mode":1,"pool_balance":37,"vr_frequency":25011})"));
    assert(!deserializeJson(schedule,R"({"schemaVersion":1,"enabled":false,"utcOffsetMinutes":0,"events":[]})"));
    auto call=[&](auto fn,const char *json="",bool authorized=true,bool otp=true){httpd_req_t r;r.body=json;r.content_len=r.body.size();r.authorized=authorized;r.otp=otp;fn(&r);return r;};
    auto get=call(GET_five_tratum_profiles);JsonDocument response;assert(!deserializeJson(response,get.response));assert(response["tuning"].size()==10 && response["pools"].size()==10 && writes==0);assert(response["identity"]["deviceId"]=="5tfw:0102030405060708090a0b0c0d0e0f10");
    auto denied=call(POST_five_tratum_profiles,R"({"type":"pool","slot":0,"name":"BTC","captureCurrent":"primary"})",true,false);assert(denied.status=="401" && writes==0);
    denied=call(POST_five_tratum_profiles,R"({"type":"pool","slot":0,"name":"BTC","captureCurrent":"primary"})",false);assert(denied.status=="401" && writes==0);
    auto saved=call(POST_five_tratum_profiles,R"({"type":"pool","slot":0,"name":"DGB","captureCurrent":"primary"})");assert(saved.status=="200 OK" && writes==1 && manager.loads==0);
    const std::string original=flash["p00"];assert(original.find("original-secret")!=std::string::npos);
    get=call(GET_five_tratum_profiles);assert(get.response.find("original-secret")==std::string::npos && get.response.find("passwordConfigured")!=std::string::npos);
    failWrite=true;saved=call(POST_five_tratum_profiles,R"({"type":"pool","slot":0,"name":"Replacement","host":"replacement.test","password":"replace"})");assert(saved.status=="507 Insufficient Storage" && flash["p00"]==original);
    auto applied=call(POST_five_tratum_profiles_apply,R"({"type":"tuning","frequencyMHz":577,"coreVoltageMv":1137})");assert(applied.status=="507 Insufficient Storage" && active["asicfrequency"]==500 && SYSTEM_MODULE.board.loads==0);
    failWrite=false;applied=call(POST_five_tratum_profiles_apply,R"({"type":"tuning","frequencyMHz":577,"coreVoltageMv":1137})");assert(applied.status=="200 OK" && active["asicfrequency"]==577 && active["asicvoltage"]==1137 && active["fan1speed"]==71 && SYSTEM_MODULE.board.loads==1);
    applied=call(POST_five_tratum_profiles_apply,R"({"type":"pool","slot":0,"poolTarget":"primary","asicIndex":0})");assert(applied.status=="409 Conflict" && manager.loads==0);
    applied=call(POST_five_tratum_profiles_apply,R"({"type":"pool","slot":9,"poolTarget":"primary"})");assert(applied.status=="409 Conflict");
    applied=call(POST_five_tratum_profiles_apply,R"({"type":"pool","slot":0,"poolTarget":"primary"})");assert(applied.status=="200 OK" && manager.loads==1 && active["stratumpass"]=="original-secret" && active["fbstratumurl"]=="other.test" && active["pool_mode"]==1 && active["pool_balance"]==37 && active["vr_frequency"]==25011);
    auto bad=call(POST_five_tratum_pool_schedule,R"({"schemaVersion":1,"enabled":true,"utcOffsetMinutes":0,"events":[{"enabled":true,"dayMask":127,"timeMinutes":720,"slot":9}]})");assert(bad.status=="409 Conflict");
    auto good=call(POST_five_tratum_pool_schedule,R"({"schemaVersion":1,"enabled":true,"utcOffsetMinutes":0,"events":[{"enabled":true,"dayMask":127,"timeMinutes":720,"slot":0}]})");assert(good.status=="200 OK");
    auto clear=call(POST_five_tratum_profiles,R"({"type":"pool","slot":0,"clear":true})");assert(clear.status=="409 Conflict" && flash["p00"]==original);
    get=call(GET_five_tratum_pool_schedule);response.clear();assert(!deserializeJson(response,get.response));assert(!response["clockValid"].as<bool>() && response["selectedSlot"].isNull());
    httpd_req_t huge;huge.content_len=8193;huge.body="{}";POST_five_tratum_profiles(&huge);assert(huge.status=="400 Bad Request");
    // Firmware timepoint selector: wrap Sunday back to Saturday, then select latest daily event.
    assert(!deserializeJson(schedule,R"({"schemaVersion":1,"enabled":true,"utcOffsetMinutes":60,"events":[{"enabled":true,"dayMask":64,"timeMinutes":1080,"slot":0},{"enabled":true,"dayMask":127,"timeMinutes":720,"slot":1}]})"));
    assert(FiveTratumProfiles::selectedScheduledSlot(schedule.as<JsonObjectConst>(),0,1)==0);
    assert(FiveTratumProfiles::selectedScheduledSlot(schedule.as<JsonObjectConst>(),0,721)==1);
    assert(!deserializeJson(schedule,R"({"schemaVersion":1,"enabled":true,"utcOffsetMinutes":0,"events":[{"enabled":true,"dayMask":127,"timeMinutes":720,"slot":0},{"enabled":true,"dayMask":127,"timeMinutes":720,"slot":0}]})"));assert(!FiveTratumProfiles::validPoolSchedule(schedule.as<JsonObjectConst>()));
    std::cout<<"Actual HTTP profile/storage paths passed auth, ten slots, credentials, NVS failure, per-ASIC rejection, deletion guard and week-wrap schedule checks\n";
}
