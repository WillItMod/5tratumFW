#include "pool_schedule.h"
#include "profile_store.h"
#include "handler_capabilities.h"
#include "global_state.h"
#include "nvs_config.h"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "sntp.h"
#include <cstdlib>
#include <ctime>
#include <pthread.h>

extern SNTP sntp;
namespace FiveTratumProfiles {
static pthread_mutex_t scheduleMutex=PTHREAD_MUTEX_INITIALIZER;
static TaskHandle_t scheduleTask=nullptr;
static int lastSlot=-1;
bool readPoolSchedule(JsonDocument &doc) {
    doc.clear();nvs_handle_t h;esp_err_t err=nvs_open("pf5tfw",NVS_READONLY,&h);
    if(err==ESP_ERR_NVS_NOT_FOUND) {doc["schemaVersion"]=1;doc["enabled"]=false;doc["utcOffsetMinutes"]=0;doc["events"].to<JsonArray>();return true;}
    if(err!=ESP_OK)return false;
    size_t len=0;err=nvs_get_blob(h,"schedule",nullptr,&len);
    if(err==ESP_ERR_NVS_NOT_FOUND) {nvs_close(h);doc["schemaVersion"]=1;doc["enabled"]=false;doc["utcOffsetMinutes"]=0;doc["events"].to<JsonArray>();return true;}
    if(err!=ESP_OK || len==0 || len>2048) {nvs_close(h);return false;}
    char *data=static_cast<char *>(malloc(len));if(!data){nvs_close(h);return false;}
    err=nvs_get_blob(h,"schedule",data,&len);nvs_close(h);
    bool ok=err==ESP_OK && !deserializeJson(doc,data,len,DeserializationOption::NestingLimit(3)) && validPoolSchedule(doc.as<JsonObjectConst>());free(data);return ok;
}
bool savePoolSchedule(JsonObjectConst s) {
    if(!validPoolSchedule(s) || measureJson(s)>2048)return false;
    pthread_mutex_lock(&scheduleMutex);
    for(JsonObjectConst e:s["events"].as<JsonArrayConst>()) {JsonDocument r;if(readSlot(Kind::Pool,e["slot"].as<unsigned>(),r)!=StoreResult::Ok){pthread_mutex_unlock(&scheduleMutex);return false;}}
    char buffer[2049];const size_t len=serializeJson(s,buffer,sizeof(buffer));nvs_handle_t h;esp_err_t err=nvs_open("pf5tfw",NVS_READWRITE,&h);
    if(err==ESP_OK){err=nvs_set_blob(h,"schedule",buffer,len);if(err==ESP_OK)err=nvs_commit(h);nvs_close(h);}
    if(err==ESP_OK) lastSlot=-1;
    pthread_mutex_unlock(&scheduleMutex);return err==ESP_OK;
}
static void tick() {
    // No network time means no pool change; power control and mining pause state are untouched.
    if(!sntp.isTimeSynced() || !STRATUM_MANAGER)return;
    time_t now=time(nullptr);if(now<1704067200)return;
    pthread_mutex_lock(&scheduleMutex);
    JsonDocument schedule,record,patch;
    if(!readPoolSchedule(schedule)){pthread_mutex_unlock(&scheduleMutex);return;}
    time_t wall=now+schedule["utcOffsetMinutes"].as<int>()*60;tm local{};gmtime_r(&wall,&local);
    int slot=selectedScheduledSlot(schedule.as<JsonObjectConst>(),local.tm_wday,local.tm_hour*60+local.tm_min);
    if(slot<0){lastSlot=-1;pthread_mutex_unlock(&scheduleMutex);return;}
    if(slot==lastSlot){pthread_mutex_unlock(&scheduleMutex);return;}
    if(readSlot(Kind::Pool,slot,record)==StoreResult::Ok){
        buildActivePatch(patch.to<JsonObject>(),Kind::Pool,record.as<JsonObjectConst>(),0);
        if(!patch.overflowed() && Config::cfgApplyPatchPersisted(patch.as<JsonObjectConst>())) {STRATUM_MANAGER->loadSettings();lastSlot=slot;}
    }
    pthread_mutex_unlock(&scheduleMutex);
}
void startProfileScheduler(){if(!scheduleTask)xTaskCreate([](void *){for(;;){tick();vTaskDelay(pdMS_TO_TICKS(20000));}},"pool_profiles",6144,nullptr,2,&scheduleTask);}
}
