#include "tuning_schedule.h"
#include "http_server/operating_profiles.h"
#include "mining_schedule.h"
#include "nvs_config.h"
#include "nvs.h"
#include "freertos/task.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define SCHEDULE_BLOB_MAX 2048
static pthread_mutex_t schedule_mutex = PTHREAD_MUTEX_INITIALIZER;
static GlobalState *state;
static TuningSchedule saved;
static uint32_t revision;
static bool initialized, started, storage_valid;
static int applied_slot = -1;
static int64_t applied_occurrence = -1;
static const char *last_error;

bool tuning_schedule_parse(const cJSON *json, TuningSchedule *schedule)
{
    TuningSchedule next;
    if (!schedule || !pool_schedule_parse(json,&next)) return false;
    bool has_event = false;
    for (unsigned i=0;i<next.event_count;i++) has_event |= next.events[i].enabled;
    if (next.enabled && !has_event) return false;
    *schedule = next;
    return true;
}
int tuning_schedule_select(const TuningSchedule *schedule, unsigned day, unsigned minute, unsigned *age_minutes)
{
    if (!schedule || !schedule->enabled || day>6 || minute>1439 || schedule->event_count>TUNING_SCHEDULE_MAX_EVENTS) return -1;
    unsigned best=10080;
    int slot=-1;
    for (unsigned i=0;i<schedule->event_count;i++) {
        const PoolScheduleEvent *event=&schedule->events[i];
        if (!event->enabled || !event->day_mask || event->day_mask>127 || event->time_minutes>1439 || event->slot>9) continue;
        for (unsigned d=0;d<7;d++) if (event->day_mask & (1u<<d)) {
            unsigned age=(day*1440+minute+10080-(d*1440+event->time_minutes))%10080;
            if (age<best) { best=age;slot=event->slot; }
        }
    }
    if (age_minutes && slot>=0) *age_minutes=best;
    return slot;
}
static cJSON *serialize(const TuningSchedule *schedule)
{
    cJSON *root=cJSON_CreateObject();
    if (!root || !cJSON_AddNumberToObject(root,"schemaVersion",1) || !cJSON_AddBoolToObject(root,"enabled",schedule->enabled) || !cJSON_AddNumberToObject(root,"utcOffsetMinutes",schedule->utc_offset_minutes)) goto failed;
    cJSON *events=cJSON_AddArrayToObject(root,"events");
    if (!events) goto failed;
    for (unsigned i=0;i<schedule->event_count;i++) {
        const PoolScheduleEvent *event=&schedule->events[i];
        cJSON *entry=cJSON_CreateObject();
        if (!entry) goto failed;
        if (!cJSON_AddBoolToObject(entry,"enabled",event->enabled) || !cJSON_AddNumberToObject(entry,"dayMask",event->day_mask) || !cJSON_AddNumberToObject(entry,"timeMinutes",event->time_minutes) || !cJSON_AddNumberToObject(entry,"slot",event->slot) || !cJSON_AddItemToArray(events,entry)) { cJSON_Delete(entry);goto failed; }
    }
    return root;
failed:cJSON_Delete(root);return NULL;
}
static bool supported(void)
{
    if (!state) return false;
    const DeviceConfig *config=&state->DEVICE_CONFIG;
    return config->family.id==GAMMA && config->family.asic.id==BM1370 && config->family.asic_count==1 && config->board_version && (!strcmp(config->board_version,"601") || !strcmp(config->board_version,"602"));
}
static const char *runtime_wait(void)
{
    if (!supported() || !state->SYSTEM_MODULE.mining_runtime_ready || state->SELF_TEST_MODULE.is_active) return "waiting-runtime";
    if (state->SYSTEM_MODULE.mining_paused || state->SYSTEM_MODULE.hardware_fault || state->SYSTEM_MODULE.overheat_mode || nvs_config_get_bool(NVS_CONFIG_OVERHEAT_MODE) || mining_schedule_applied_paused()) return "paused";
    if (!state->ASIC_initalized || state->SYSTEM_MODULE.pools_unavailable) return "waiting-runtime";
    return NULL;
}
bool operating_profile_tuning_referenced(unsigned slot)
{
    pthread_mutex_lock(&schedule_mutex);
    bool referenced=initialized && !storage_valid;
    for (unsigned i=0;i<saved.event_count;i++) if(saved.events[i].slot==slot) referenced=true;
    pthread_mutex_unlock(&schedule_mutex);
    return referenced;
}
esp_err_t tuning_schedule_init(GlobalState *global)
{
    TuningSchedule initial={0};
    nvs_handle_t handle;
    esp_err_t error=nvs_open("5fw_profiles",NVS_READONLY,&handle);
    char *raw=NULL;
    if (error==ESP_OK) {
        size_t length=0;
        error=nvs_get_blob(handle,"tuning_schedule",NULL,&length);
        if (error==ESP_OK) {
            if (!length || length>SCHEDULE_BLOB_MAX) error=ESP_ERR_INVALID_SIZE;
            else {
                raw=malloc(length);
                error=raw?nvs_get_blob(handle,"tuning_schedule",raw,&length):ESP_ERR_NO_MEM;
                if (error==ESP_OK) {
                    cJSON *json=raw[length-1]==0 && strlen(raw)==length-1 && !strstr(raw,"\\u0000")?cJSON_ParseWithOpts(raw,NULL,true):NULL;
                    if(!tuning_schedule_parse(json,&initial)) error=ESP_ERR_INVALID_STATE;
                    cJSON_Delete(json);
                }
            }
        }
        nvs_close(handle);
    }
    free(raw);
    if (error==ESP_ERR_NVS_NOT_FOUND) error=ESP_OK;
    pthread_mutex_lock(&schedule_mutex);
    state=global;saved=error==ESP_OK?initial:(TuningSchedule){0};storage_valid=error==ESP_OK;
    initialized=true;revision++;applied_slot=-1;applied_occurrence=-1;
    last_error=error==ESP_OK?NULL:"Invalid or unreadable stored tuning schedule";
    pthread_mutex_unlock(&schedule_mutex);
    // Corrupt tuning policy is recoverable through HTTP and never changes power.
    return ESP_OK;
}
typedef struct {const TuningSchedule *schedule;const char *raw;} SaveContext;
static esp_err_t commit_schedule(void *value)
{
    SaveContext *context=value;
    pthread_mutex_lock(&schedule_mutex);
    nvs_handle_t handle;
    esp_err_t error=nvs_open("5fw_profiles",NVS_READWRITE,&handle);
    if (error==ESP_OK) {
        size_t old_length=0;
        esp_err_t previous=nvs_get_blob(handle,"tuning_schedule",NULL,&old_length);
        char *old=NULL;
        if (previous==ESP_OK && old_length && old_length<=SCHEDULE_BLOB_MAX) {
            old=malloc(old_length);error=old?nvs_get_blob(handle,"tuning_schedule",old,&old_length):ESP_ERR_NO_MEM;
        } else if(previous!=ESP_OK && previous!=ESP_ERR_NVS_NOT_FOUND) error=previous;
        if (error==ESP_OK) {
            error=nvs_set_blob(handle,"tuning_schedule",context->raw,strlen(context->raw)+1);
            if (error==ESP_OK) {
                error=nvs_commit(handle);
                if (error!=ESP_OK) {
                    if(old)nvs_set_blob(handle,"tuning_schedule",old,old_length);else nvs_erase_key(handle,"tuning_schedule");
                    nvs_commit(handle);
                }
            }
        }
        free(old);nvs_close(handle);
    }
    if (error==ESP_OK) {saved=*context->schedule;storage_valid=true;revision++;applied_slot=-1;applied_occurrence=-1;last_error=NULL;}
    pthread_mutex_unlock(&schedule_mutex);
    return error;
}
esp_err_t tuning_schedule_save(const TuningSchedule *schedule)
{
    if(!schedule || schedule->event_count>TUNING_SCHEDULE_MAX_EVENTS)return ESP_ERR_INVALID_ARG;
    cJSON *json=serialize(schedule);
    if(!json)return ESP_ERR_NO_MEM;
    TuningSchedule validated;
    bool valid=tuning_schedule_parse(json,&validated);
    char *raw=valid?cJSON_PrintUnformatted(json):NULL;
    cJSON_Delete(json);
    if(!valid)return ESP_ERR_INVALID_ARG;
    if(!raw)return ESP_ERR_NO_MEM;
    if(strlen(raw)+1>SCHEDULE_BLOB_MAX){free(raw);return ESP_ERR_INVALID_SIZE;}
    uint16_t mask=0;
    for(unsigned i=0;i<validated.event_count;i++)mask|=1u<<validated.events[i].slot;
    SaveContext context={.schedule=&validated,.raw=raw};
    // Profiles lock -> schedule lock, shared with the slot-edit/delete guards.
    esp_err_t error=operating_profiles_with_tuning_slots(mask,commit_schedule,&context);
    free(raw);return error;
}
typedef struct {uint32_t generation;bool permitted;} ActivationContext;
static bool activation_allowed(void *value)
{
    ActivationContext *context=value;
    pthread_mutex_lock(&schedule_mutex);
    bool allowed=storage_valid && saved.enabled && revision==context->generation && !runtime_wait();
    context->permitted=allowed;
    pthread_mutex_unlock(&schedule_mutex);
    return allowed;
}
void tuning_schedule_tick(void)
{
    pthread_mutex_lock(&schedule_mutex);
    TuningSchedule schedule=saved;
    uint32_t generation=revision;
    bool ready=initialized && storage_valid;
    int64_t previous=applied_occurrence;
    pthread_mutex_unlock(&schedule_mutex);
    time_t now=time(NULL);
    struct tm local;
    if(!ready || !schedule.enabled || !pool_schedule_clock(now,mining_schedule_owns_clock(),schedule.utc_offset_minutes,&local))return;
    unsigned age=0;
    int slot=tuning_schedule_select(&schedule,local.tm_wday,local.tm_hour*60+local.tm_min,&age);
    int64_t occurrence=(int64_t)(now/60)-age;
    if(slot<0 || occurrence==previous || runtime_wait())return;
    ActivationContext context={.generation=generation};
    esp_err_t error=operating_profile_apply_tuning_guarded(slot,activation_allowed,&context);
    pthread_mutex_lock(&schedule_mutex);
    if(generation==revision) {
        if(error==ESP_OK){applied_slot=slot;applied_occurrence=occurrence;last_error=NULL;}
        else if(context.permitted)last_error=error==ESP_ERR_NVS_NOT_FOUND?"Scheduled tuning slot is empty":"Unable to activate scheduled tuning; retrying";
    }
    pthread_mutex_unlock(&schedule_mutex);
}
cJSON *tuning_schedule_get_json(void)
{
    pthread_mutex_lock(&schedule_mutex);
    TuningSchedule schedule=saved;
    int applied=applied_slot;
    int64_t occurrence=applied_occurrence;
    bool valid=storage_valid;
    const char *error=last_error;
    pthread_mutex_unlock(&schedule_mutex);
    time_t now=time(NULL);
    struct tm local;
    bool clock=pool_schedule_clock(now,mining_schedule_owns_clock(),schedule.utc_offset_minutes,&local);
    unsigned age=0;
    int selected=clock?tuning_schedule_select(&schedule,local.tm_wday,local.tm_hour*60+local.tm_min,&age):-1;
    const char *status=valid?"disabled":"error";
    if(valid && schedule.enabled) {
        const char *wait=clock?runtime_wait():"waiting-clock";
        status=wait?wait:error?"error":selected>=0 && (int64_t)(now/60)-age==occurrence?"applied":"pending";
    }
    cJSON *root=serialize(&schedule),*binding=operating_profiles_binding();
    if(!root || !binding)goto failed;
    if(!cJSON_AddBoolToObject(root,"supported",supported()) || !cJSON_AddBoolToObject(root,"clockValid",clock) || !cJSON_AddStringToObject(root,"timezoneMode","fixed-utc-offset") || !cJSON_AddNumberToObject(root,"maxEvents",TUNING_SCHEDULE_MAX_EVENTS) || !cJSON_AddBoolToObject(root,"storageValid",valid) || !cJSON_AddStringToObject(root,"status",status) || !(error?cJSON_AddStringToObject(root,"error",error):cJSON_AddNullToObject(root,"error")) || !(selected<0?cJSON_AddNullToObject(root,"selectedSlot"):cJSON_AddNumberToObject(root,"selectedSlot",selected)) || !(applied<0?cJSON_AddNullToObject(root,"appliedSlot"):cJSON_AddNumberToObject(root,"appliedSlot",applied)))goto failed;
    while(binding->child) {
        cJSON *field=cJSON_DetachItemViaPointer(binding,binding->child);
        if(!cJSON_AddItemToObject(root,field->string,field)){cJSON_Delete(field);goto failed;}
    }
    cJSON_Delete(binding);return root;
failed:cJSON_Delete(root);cJSON_Delete(binding);return NULL;
}
static void schedule_task(void *unused)
{
    (void)unused;
    while(1){tuning_schedule_tick();vTaskDelay(pdMS_TO_TICKS(20000));}
}
esp_err_t tuning_schedule_start(void)
{
    if(!initialized)return ESP_ERR_INVALID_STATE;
    if(started)return ESP_OK;
    if(xTaskCreate(schedule_task,"tuning schedule",4096,NULL,2,NULL)!=pdPASS)return ESP_ERR_NO_MEM;
    started=true;return ESP_OK;
}
