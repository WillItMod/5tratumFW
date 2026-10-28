#include "http_utils.h"
#include "handler_profiles.h"
#include "pool_schedule.h"
#include "tuning_schedule.h"
#include "global_state.h"
#include "config_transaction.h"
#include "nvs.h"
#include "sntp.h"
#include <cassert>
#include <map>
#include <cstring>
#include <iostream>
#include <ctime>
httpd_handle_t http_server=0;System SYSTEM_MODULE;StratumManager manager;StratumManager *STRATUM_MANAGER=&manager;SNTP sntp;
PowerManagementTask POWER_MANAGEMENT_MODULE;
namespace FiveTratumMining {bool supported=true,paused=false,writesAllowed=true;}
static time_t wall=1791374400;
extern "C" time_t time(time_t *out){if(out)*out=wall;return wall;}
static void at(const char *utc){tm value{};assert(strptime(utc,"%Y-%m-%dT%H:%M:%S",&value));wall=timegm(&value);}
std::map<std::string,std::string> flash,pending;bool failWrite=false,failCommit=false;int writes=0;JsonDocument active,schedule;
int nvs_open(const char *,int mode,nvs_handle_t *h){*h=1;return mode==NVS_READONLY && flash.empty()?ESP_ERR_NVS_NOT_FOUND:ESP_OK;}
int nvs_get_blob(nvs_handle_t,const char *key,void *out,size_t *len){auto it=flash.find(key);if(it==flash.end())return ESP_ERR_NVS_NOT_FOUND;if(out)memcpy(out,it->second.data(),it->second.size());*len=it->second.size();return ESP_OK;}
int nvs_set_blob(nvs_handle_t,const char *key,const void *buffer,size_t len){if(failWrite)return -7;pending[key]=std::string(static_cast<const char *>(buffer),len);++writes;return ESP_OK;}
int nvs_commit(nvs_handle_t){if(failCommit)return -7;for(const auto &value:pending)flash[value.first]=value.second;return ESP_OK;}void nvs_close(nvs_handle_t){pending.clear();}
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
    active[NVS_CONFIG_SV2_REQUIRE_AUTH]=1;active[NVS_CONFIG_FB_SV2_REQUIRE_AUTH]=0;
    assert(!deserializeJson(schedule,R"({"schemaVersion":1,"enabled":false,"utcOffsetMinutes":0,"events":[]})"));
    auto call=[&](auto fn,const char *json="",bool authorized=true,bool otp=true){httpd_req_t r;r.body=json;r.content_len=r.body.size();r.authorized=authorized;r.otp=otp;fn(&r);return r;};
    auto get=call(GET_five_tratum_profiles);JsonDocument response;assert(!deserializeJson(response,get.response));assert(response["tuning"].size()==10 && response["pools"].size()==10 && writes==0);assert(response["identity"]["deviceId"]=="5tfw:0102030405060708090a0b0c0d0e0f10");
    auto denied=call(POST_five_tratum_profiles,R"({"type":"pool","slot":0,"name":"BTC","captureCurrent":"primary"})",true,false);assert(denied.status=="401" && writes==0);
    denied=call(POST_five_tratum_profiles,R"({"type":"pool","slot":0,"name":"BTC","captureCurrent":"primary"})",false);assert(denied.status=="401" && writes==0);
    auto saved=call(POST_five_tratum_profiles,R"({"type":"pool","slot":0,"name":"DGB","captureCurrent":"primary"})");assert(saved.status=="200 OK" && writes==1 && manager.loads==0);
    const std::string original=flash["p00"];assert(original.find("original-secret")!=std::string::npos);
    assert(original.find("\"sv2RequireAuth\":true")!=std::string::npos);
    get=call(GET_five_tratum_profiles);assert(get.response.find("original-secret")==std::string::npos && get.response.find("passwordConfigured")!=std::string::npos);
    failWrite=true;saved=call(POST_five_tratum_profiles,R"({"type":"pool","slot":0,"name":"Replacement","host":"replacement.test","password":"replace"})");assert(saved.status=="507 Insufficient Storage" && flash["p00"]==original);
    auto applied=call(POST_five_tratum_profiles_apply,R"({"type":"tuning","frequencyMHz":577,"coreVoltageMv":1137})");assert(applied.status=="507 Insufficient Storage" && active["asicfrequency"]==500 && SYSTEM_MODULE.board.loads==0);
    failWrite=false;applied=call(POST_five_tratum_profiles_apply,R"({"type":"tuning","frequencyMHz":577,"coreVoltageMv":1137})");assert(applied.status=="200 OK" && active["asicfrequency"]==577 && active["asicvoltage"]==1137 && active["fan1speed"]==71 && SYSTEM_MODULE.board.loads==1);
    applied=call(POST_five_tratum_profiles_apply,R"({"type":"pool","slot":0,"poolTarget":"primary","asicIndex":0})");assert(applied.status=="409 Conflict" && manager.loads==0);
    applied=call(POST_five_tratum_profiles_apply,R"({"type":"pool","slot":9,"poolTarget":"primary"})");assert(applied.status=="409 Conflict");
    applied=call(POST_five_tratum_profiles_apply,R"({"type":"pool","slot":0,"poolTarget":"primary"})");assert(applied.status=="200 OK" && manager.loads==1 && active["stratumpass"]=="original-secret" && active["fbstratumurl"]=="other.test" && active["pool_mode"]==1 && active["pool_balance"]==37 && active["vr_frequency"]==25011);
    assert(active[NVS_CONFIG_SV2_REQUIRE_AUTH]==1 && active[NVS_CONFIG_FB_SV2_REQUIRE_AUTH]==0);
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
    // Actual tuning schedule HTTP/storage/runtime: store and reads never apply.
    const char *daily=R"({"schemaVersion":1,"enabled":true,"utcOffsetMinutes":0,"events":[{"enabled":true,"dayMask":127,"timeMinutes":540,"slot":0},{"enabled":true,"dayMask":127,"timeMinutes":1260,"slot":1},{"enabled":true,"dayMask":127,"timeMinutes":0,"slot":2}]})";
    auto savedTuning=call(POST_five_tratum_profiles,R"({"type":"tuning","slot":0,"name":"Day","frequencyMHz":600,"coreVoltageMv":1150})");assert(savedTuning.status=="200 OK");
    savedTuning=call(POST_five_tratum_profiles,R"({"type":"tuning","slot":1,"name":"Evening","frequencyMHz":550,"coreVoltageMv":1125})");assert(savedTuning.status=="200 OK");
    savedTuning=call(POST_five_tratum_profiles,R"({"type":"tuning","slot":2,"name":"Night","frequencyMHz":500,"coreVoltageMv":1100})");assert(savedTuning.status=="200 OK");
    const int initialLoads=SYSTEM_MODULE.board.loads;
    const int initialClock=active["asicfrequency"];
    denied=call(POST_five_tratum_tuning_schedule,daily,true,false);assert(denied.status=="401");
    denied=call(GET_five_tratum_tuning_schedule,"",false);assert(denied.status=="401");
    auto tuningSave=call(POST_five_tratum_tuning_schedule,daily);assert(tuningSave.status=="200 OK" && SYSTEM_MODULE.board.loads==initialLoads && active["asicfrequency"]==initialClock);
    const auto persistedSchedule=flash["tune_sched"];
    const int afterSaveWrites=writes;
    for(int i=0;i<2;++i){get=call(GET_five_tratum_tuning_schedule);response.clear();assert(!deserializeJson(response,get.response));assert(response["status"]=="waiting-clock" && response["selectedSlot"].isNull());FiveTratumProfiles::updateTuningSchedule();}
    assert(writes==afterSaveWrites && SYSTEM_MODULE.board.loads==initialLoads);
    clear=call(POST_five_tratum_profiles,R"({"type":"tuning","slot":0,"clear":true})");assert(clear.status=="409 Conflict");
    // Boundaries use the profile from the latest event, including exactly midnight.
    sntp.synchronized=true;
    auto selected=[&](){get=call(GET_five_tratum_tuning_schedule);response.clear();assert(!deserializeJson(response,get.response));return response["selectedSlot"].as<int>();};
    at("2026-10-07T08:59:59");assert(selected()==2);FiveTratumProfiles::updateTuningSchedule();assert(active["asicfrequency"]==500 && active["asicvoltage"]==1100);
    at("2026-10-07T09:00:00");assert(selected()==0);FiveTratumProfiles::updateTuningSchedule();assert(active["asicfrequency"]==600 && active["asicvoltage"]==1150);
    int lastLoads=SYSTEM_MODULE.board.loads;FiveTratumProfiles::updateTuningSchedule();assert(SYSTEM_MODULE.board.loads==lastLoads);
    at("2026-10-07T20:59:59");assert(selected()==0);
    at("2026-10-07T21:00:00");assert(selected()==1);FiveTratumProfiles::updateTuningSchedule();assert(active["asicfrequency"]==550);
    at("2026-10-08T00:00:00");assert(selected()==2);FiveTratumProfiles::updateTuningSchedule();assert(active["asicfrequency"]==500);
    assert(active["fan1speed"]==71 && active["overheat_temp"]==72 && active["pool_balance"]==37 && active["vr_frequency"]==25011);
    // Missing/corrupt slots and failed config writes are retried, never marked applied.
    at("2026-10-08T09:00:00");auto dayRecord=flash["t00"];flash.erase("t00");lastLoads=SYSTEM_MODULE.board.loads;
    FiveTratumProfiles::updateTuningSchedule();assert(SYSTEM_MODULE.board.loads==lastLoads && active["asicfrequency"]==500);assert(selected()==0 && response["status"]=="error");
    flash["t00"]=dayRecord;failWrite=true;FiveTratumProfiles::updateTuningSchedule();assert(SYSTEM_MODULE.board.loads==lastLoads && active["asicfrequency"]==500);
    failWrite=false;FiveTratumProfiles::updateTuningSchedule();assert(active["asicfrequency"]==600 && SYSTEM_MODULE.board.loads==lastLoads+1);assert(selected()==0 && response["status"]=="applied" && response["appliedSlot"]==0);
    // Manual/scheduled pause, protection, startup, and unsupported boards defer tuning.
    at("2026-10-08T21:00:00");lastLoads=SYSTEM_MODULE.board.loads;
    FiveTratumMining::paused=true;FiveTratumProfiles::updateTuningSchedule();assert(selected()==1 && response["status"]=="paused");assert(SYSTEM_MODULE.board.loads==lastLoads);
    FiveTratumMining::paused=false;FiveTratumMining::writesAllowed=false;FiveTratumProfiles::updateTuningSchedule();assert(SYSTEM_MODULE.board.loads==lastLoads);
    FiveTratumMining::writesAllowed=true;SYSTEM_MODULE.error=Board::Error::TEMP_FAULT;FiveTratumProfiles::updateTuningSchedule();assert(SYSTEM_MODULE.board.loads==lastLoads);
    SYSTEM_MODULE.error=Board::Error::NONE;POWER_MANAGEMENT_MODULE.shutdown=true;FiveTratumProfiles::updateTuningSchedule();assert(SYSTEM_MODULE.board.loads==lastLoads);
    POWER_MANAGEMENT_MODULE.shutdown=false;SYSTEM_MODULE.board.initialized=false;FiveTratumProfiles::updateTuningSchedule();assert(SYSTEM_MODULE.board.loads==lastLoads);
    SYSTEM_MODULE.board.initialized=true;FiveTratumMining::supported=false;FiveTratumProfiles::updateTuningSchedule();assert(SYSTEM_MODULE.board.loads==lastLoads);
    FiveTratumMining::supported=true;POWER_MANAGEMENT_MODULE.protectionReady=false;FiveTratumProfiles::updateTuningSchedule();assert(SYSTEM_MODULE.board.loads==lastLoads);
    POWER_MANAGEMENT_MODULE.protectionReady=true;FiveTratumProfiles::updateTuningSchedule();assert(SYSTEM_MODULE.board.loads==lastLoads+1 && active["asicfrequency"]==550);
    // Same slot on a later event still reasserts the requested profile; manual
    // changes between events remain in place until the following event.
    tuningSave=call(POST_five_tratum_tuning_schedule,R"({"schemaVersion":1,"enabled":true,"utcOffsetMinutes":0,"events":[{"enabled":true,"dayMask":127,"timeMinutes":540,"slot":0}]})");assert(tuningSave.status=="200 OK");
    FiveTratumProfiles::updateTuningSchedule();assert(active["asicfrequency"]==600);
    applied=call(POST_five_tratum_profiles_apply,R"({"type":"tuning","frequencyMHz":577,"coreVoltageMv":1137})");assert(applied.status=="200 OK");
    FiveTratumProfiles::updateTuningSchedule();assert(active["asicfrequency"]==577);
    at("2026-10-09T09:00:00");FiveTratumProfiles::updateTuningSchedule();assert(active["asicfrequency"]==600);
    // Lost network time cannot tune, including after a trusted-clock jump.
    sntp.synchronized=false;at("2026-10-10T09:00:00");lastLoads=SYSTEM_MODULE.board.loads;
    FiveTratumProfiles::updateTuningSchedule();get=call(GET_five_tratum_tuning_schedule);response.clear();assert(!deserializeJson(response,get.response));assert(response["status"]=="waiting-clock" && SYSTEM_MODULE.board.loads==lastLoads);
    sntp.synchronized=true;FiveTratumProfiles::updateTuningSchedule();assert(SYSTEM_MODULE.board.loads==lastLoads+1);
    // An unsuccessful schedule replacement must preserve both the old blob
    // and its already-applied occurrence, rather than silently reactivating it.
    const auto beforeCommitFailure=flash["tune_sched"];lastLoads=SYSTEM_MODULE.board.loads;
    failCommit=true;tuningSave=call(POST_five_tratum_tuning_schedule,daily);assert(tuningSave.status=="507 Insufficient Storage" && flash["tune_sched"]==beforeCommitFailure);failCommit=false;
    FiveTratumProfiles::updateTuningSchedule();assert(SYSTEM_MODULE.board.loads==lastLoads);assert(selected()==0 && response["status"]=="applied");
    // Week wrap and offset crossing midnight select the latest Saturday event.
    tuningSave=call(POST_five_tratum_tuning_schedule,R"({"schemaVersion":1,"enabled":true,"utcOffsetMinutes":60,"events":[{"enabled":true,"dayMask":64,"timeMinutes":1380,"slot":1},{"enabled":true,"dayMask":1,"timeMinutes":30,"slot":2}]})");assert(tuningSave.status=="200 OK");
    at("2026-10-10T23:29:59");assert(selected()==1);at("2026-10-10T23:30:00");assert(selected()==2);at("2026-10-17T21:59:59");assert(selected()==2);at("2026-10-17T22:00:00");assert(selected()==1);
    // Trusted corrections in either direction reevaluate the most recent wall
    // event; they never synthesize each intermediate occurrence.
    FiveTratumProfiles::updateTuningSchedule();assert(active["asicfrequency"]==550);
    at("2026-10-11T00:00:00");FiveTratumProfiles::updateTuningSchedule();assert(active["asicfrequency"]==500);
    at("2026-11-07T22:00:00");lastLoads=SYSTEM_MODULE.board.loads;FiveTratumProfiles::updateTuningSchedule();assert(active["asicfrequency"]==550 && SYSTEM_MODULE.board.loads==lastLoads+1);
    // Whole-document validation and durable commit failures preserve old policy.
    const auto beforeFailedSave=flash["tune_sched"];
    for(const char *invalid:{R"({"schemaVersion":1,"enabled":true,"utcOffsetMinutes":0,"events":[]})",R"({"schemaVersion":1,"enabled":true,"utcOffsetMinutes":10,"events":[]})",R"({"schemaVersion":1,"enabled":true,"utcOffsetMinutes":0,"events":[{"enabled":true,"dayMask":127,"timeMinutes":0,"slot":true}]})",R"({"schemaVersion":1,"enabled":true,"utcOffsetMinutes":0,"events":[{"enabled":true,"dayMask":127,"timeMinutes":0,"slot":0},{"enabled":true,"dayMask":1,"timeMinutes":0,"slot":1}]})"}) {
        auto rejected=call(POST_five_tratum_tuning_schedule,invalid);assert(rejected.status=="400 Bad Request" && flash["tune_sched"]==beforeFailedSave);
    }
    auto missing=call(POST_five_tratum_tuning_schedule,R"({"schemaVersion":1,"enabled":false,"utcOffsetMinutes":0,"events":[{"enabled":false,"dayMask":1,"timeMinutes":0,"slot":9}]})");assert(missing.status=="409 Conflict" && flash["tune_sched"]==beforeFailedSave);
    failCommit=true;tuningSave=call(POST_five_tratum_tuning_schedule,daily);assert(tuningSave.status=="507 Insufficient Storage" && flash["tune_sched"]==beforeFailedSave);failCommit=false;
    // Saving disabled neither changes tuning nor applies after subsequent ticks.
    tuningSave=call(POST_five_tratum_tuning_schedule,R"({"schemaVersion":1,"enabled":false,"utcOffsetMinutes":-720,"events":[]})");assert(tuningSave.status=="200 OK");lastLoads=SYSTEM_MODULE.board.loads;const auto activeBeforeDisable=active["asicfrequency"].as<int>();
    FiveTratumProfiles::updateTuningSchedule();get=call(GET_five_tratum_tuning_schedule);response.clear();assert(!deserializeJson(response,get.response));assert(response["status"]=="disabled" && response["selectedSlot"].isNull() && SYSTEM_MODULE.board.loads==lastLoads && active["asicfrequency"]==activeBeforeDisable);
    assert(!persistedSchedule.empty());
    std::cout<<"Actual HTTP profile/storage paths passed auth, ten slots, credentials, NVS failure, per-ASIC rejection, deletion guard and week-wrap schedule checks\n";
    std::cout<<"Scheduled tuning passed side-effect-free reads/save, clock gating, daily boundaries, fixed offset/week wrap, paused/protection/startup guards, atomic validation/storage, retry and repeated-slot tests\n";
}
