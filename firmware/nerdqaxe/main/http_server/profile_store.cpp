#include "profile_store.h"
#include "nvs.h"
#include <cstdio>
#include <cstdlib>

namespace FiveTratumProfiles {
namespace {
const char *const PROFILE_NAMESPACE = "pf5tfw";
bool keyFor(Kind kind, unsigned slot, char (&key)[5]) {
    if (slot>=SLOT_COUNT) return false;
    std::snprintf(key,sizeof(key),"%c%02u",kind==Kind::Tuning?'t':'p',slot); return true;
}
}
// HTTP server runs on a normal internal-memory stack; never invoke from PSRAM tasks.
StoreResult readSlot(Kind kind, unsigned slot, JsonDocument &record) {
    record.clear(); char key[5]; if (!keyFor(kind,slot,key)) return StoreResult::Failure;
    nvs_handle_t handle;
    esp_err_t err=nvs_open(PROFILE_NAMESPACE,NVS_READONLY,&handle);
    if (err==ESP_ERR_NVS_NOT_FOUND) return StoreResult::Empty;
    if (err!=ESP_OK) return StoreResult::Failure;
    size_t len=0; err=nvs_get_blob(handle,key,nullptr,&len);
    if (err==ESP_ERR_NVS_NOT_FOUND) {nvs_close(handle);return StoreResult::Empty;}
    if (err!=ESP_OK || !len || len>MAX_SLOT_BYTES) {nvs_close(handle);return StoreResult::Failure;}
    char *buffer=static_cast<char *>(malloc(len));
    if (!buffer) {nvs_close(handle);return StoreResult::Failure;}
    err=nvs_get_blob(handle,key,buffer,&len); nvs_close(handle);
    bool ok=err==ESP_OK && !deserializeJson(record,buffer,len,DeserializationOption::NestingLimit(2)) && validRecord(record.as<JsonObjectConst>(),kind);
    free(buffer); if (!ok) record.clear();
    return ok?StoreResult::Ok:StoreResult::Failure;
}
StoreResult saveSlot(Kind kind, unsigned slot, JsonObjectConst record) {
    char key[5]; if (!keyFor(kind,slot,key) || !validRecord(record,kind)) return StoreResult::Failure;
    const size_t len=measureJson(record);
    if (!len || len>MAX_SLOT_BYTES) return StoreResult::Failure;
    char *buffer=static_cast<char *>(malloc(len+1)); if (!buffer) return StoreResult::Failure;
    serializeJson(record,buffer,len+1);
    nvs_handle_t handle; esp_err_t err=nvs_open(PROFILE_NAMESPACE,NVS_READWRITE,&handle);
    if (err==ESP_OK) {
        err=nvs_set_blob(handle,key,buffer,len);
        if (err==ESP_OK) err=nvs_commit(handle);
        nvs_close(handle);
    }
    free(buffer); return err==ESP_OK?StoreResult::Ok:StoreResult::Failure;
}
StoreResult clearSlot(Kind kind,unsigned slot) {
    char key[5]; if (!keyFor(kind,slot,key)) return StoreResult::Failure;
    nvs_handle_t handle; esp_err_t err=nvs_open(PROFILE_NAMESPACE,NVS_READWRITE,&handle);
    if (err==ESP_OK) {
        err=nvs_erase_key(handle,key);
        if (err==ESP_ERR_NVS_NOT_FOUND) err=ESP_OK;
        if (err==ESP_OK) err=nvs_commit(handle);
        nvs_close(handle);
    }
    return err==ESP_OK?StoreResult::Ok:StoreResult::Failure;
}
}
