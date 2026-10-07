#include "pool_schedule.h"
#include "http_server/operating_profiles.h"
#include "mining_schedule.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/task.h"
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define SCHEDULE_BLOB_MAX 2048
#define MIN_VALID_EPOCH 1704067200LL
static const char *TAG = "pool_schedule";
static pthread_mutex_t schedule_mutex = PTHREAD_MUTEX_INITIALIZER;
static PoolSchedule saved;
static uint32_t revision;
static bool initialized, started, storage_valid;
static int selected_slot = -1;

static bool integer(const cJSON *item, double min, double max)
{
    return cJSON_IsNumber(item) && isfinite(item->valuedouble) &&
           item->valuedouble >= min && item->valuedouble <= max && floor(item->valuedouble) == item->valuedouble;
}
static bool exact_fields(const cJSON *object, const char *const *names, unsigned count)
{
    if (!cJSON_IsObject(object)) return false;
    unsigned found = 0;
    for (const cJSON *field = object->child; field; field = field->next) {
        if (!field->string) return false;
        bool known = false;
        for (unsigned i = 0; i < count; i++) if (!strcmp(field->string, names[i])) known = true;
        if (!known) return false;
        for (const cJSON *other = field->next; other; other = other->next)
            if (other->string && !strcmp(field->string, other->string)) return false;
        found++;
    }
    return found == count;
}
bool pool_schedule_parse(const cJSON *json, PoolSchedule *schedule)
{
    static const char *const root_fields[] = {"schemaVersion", "enabled", "utcOffsetMinutes", "events"};
    static const char *const event_fields[] = {"enabled", "dayMask", "timeMinutes", "slot"};
    if (!schedule || !exact_fields(json, root_fields, 4)) return false;
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(json, "schemaVersion");
    const cJSON *enabled = cJSON_GetObjectItemCaseSensitive(json, "enabled");
    const cJSON *offset = cJSON_GetObjectItemCaseSensitive(json, "utcOffsetMinutes");
    const cJSON *events = cJSON_GetObjectItemCaseSensitive(json, "events");
    if (!integer(version, 1, 1) || !cJSON_IsBool(enabled) || !integer(offset, -720, 840) ||
        (int)offset->valuedouble % 15 || !cJSON_IsArray(events) || cJSON_GetArraySize(events) > POOL_SCHEDULE_MAX_EVENTS) return false;
    PoolSchedule result = {.enabled = cJSON_IsTrue(enabled), .utc_offset_minutes = (int)offset->valuedouble};
    for (const cJSON *event = events->child; event; event = event->next) {
        if (!exact_fields(event, event_fields, 4)) return false;
        const cJSON *on = cJSON_GetObjectItemCaseSensitive(event, "enabled");
        const cJSON *days = cJSON_GetObjectItemCaseSensitive(event, "dayMask");
        const cJSON *minute = cJSON_GetObjectItemCaseSensitive(event, "timeMinutes");
        const cJSON *slot = cJSON_GetObjectItemCaseSensitive(event, "slot");
        if (!cJSON_IsBool(on) || !integer(days, 1, 127) || !integer(minute, 0, 1439) || !integer(slot, 0, 9)) return false;
        PoolScheduleEvent next = {.enabled = cJSON_IsTrue(on), .day_mask = (unsigned)days->valuedouble,
                                  .time_minutes = (unsigned)minute->valuedouble, .slot = (unsigned)slot->valuedouble};
        for (unsigned i = 0; i < result.event_count; i++) {
            const PoolScheduleEvent *old = &result.events[i];
            if (next.enabled && old->enabled && (next.day_mask & old->day_mask) && next.time_minutes == old->time_minutes) return false;
        }
        result.events[result.event_count++] = next;
    }
    *schedule = result;
    return true;
}
int pool_schedule_select(const PoolSchedule *schedule, unsigned day, unsigned minute)
{
    if (!schedule || !schedule->enabled || day > 6 || minute > 1439 || schedule->event_count > POOL_SCHEDULE_MAX_EVENTS) return -1;
    unsigned best_age = 10080;
    int slot = -1;
    for (unsigned i = 0; i < schedule->event_count; i++) {
        const PoolScheduleEvent *event = &schedule->events[i];
        if (!event->enabled || !event->day_mask || event->day_mask > 127 || event->time_minutes > 1439 || event->slot > 9) continue;
        for (unsigned d = 0; d < 7; d++) if (event->day_mask & (1u << d)) {
            unsigned age = (day * 1440 + minute + 10080 - (d * 1440 + event->time_minutes)) % 10080;
            if (age < best_age) { best_age = age; slot = event->slot; }
        }
    }
    return slot;
}
bool pool_schedule_clock(time_t epoch, bool synchronized, int offset, struct tm *local)
{
    if (!synchronized || (int64_t)epoch < MIN_VALID_EPOCH || offset < -720 || offset > 840 || offset % 15 || !local) return false;
    int64_t wall = (int64_t)epoch + offset * 60;
    time_t value = (time_t)wall;
    if ((int64_t)value != wall) return false;
    // Deliberately independent of the power scheduler's named TZ and DST rules.
    return gmtime_r(&value, local) != NULL;
}
static cJSON *serialize(const PoolSchedule *schedule)
{
    cJSON *root = cJSON_CreateObject();
    if (!root || !cJSON_AddNumberToObject(root, "schemaVersion", 1) ||
        !cJSON_AddBoolToObject(root, "enabled", schedule->enabled) ||
        !cJSON_AddNumberToObject(root, "utcOffsetMinutes", schedule->utc_offset_minutes)) goto failed;
    cJSON *events = cJSON_AddArrayToObject(root, "events");
    if (!events) goto failed;
    for (unsigned i = 0; i < schedule->event_count; i++) {
        const PoolScheduleEvent *event = &schedule->events[i];
        cJSON *entry = cJSON_CreateObject();
        if (!entry) goto failed;
        if (!cJSON_AddBoolToObject(entry, "enabled", event->enabled) || !cJSON_AddNumberToObject(entry, "dayMask", event->day_mask) ||
            !cJSON_AddNumberToObject(entry, "timeMinutes", event->time_minutes) || !cJSON_AddNumberToObject(entry, "slot", event->slot) ||
            !cJSON_AddItemToArray(events, entry)) { cJSON_Delete(entry); goto failed; }
    }
    return root;
failed:
    cJSON_Delete(root); return NULL;
}
bool operating_profile_pool_referenced(unsigned slot)
{
    bool referenced = false;
    pthread_mutex_lock(&schedule_mutex);
    // A corrupt stored schedule must be repaired before any pool slot is erased.
    if (initialized && !storage_valid) referenced = true;
    for (unsigned i = 0; i < saved.event_count; i++) if (saved.events[i].slot == slot) referenced = true;
    pthread_mutex_unlock(&schedule_mutex);
    return referenced;
}
esp_err_t pool_schedule_init(GlobalState *state)
{
    (void)state;
    PoolSchedule initial = {0};
    nvs_handle_t handle;
    esp_err_t error = nvs_open("5fw_profiles", NVS_READONLY, &handle);
    char *raw = NULL;
    if (error == ESP_OK) {
        size_t length = 0;
        error = nvs_get_blob(handle, "pool_schedule", NULL, &length);
        if (error == ESP_OK) {
            if (!length || length > SCHEDULE_BLOB_MAX) error = ESP_ERR_INVALID_SIZE;
            else {
                raw = malloc(length);
                error = raw ? nvs_get_blob(handle, "pool_schedule", raw, &length) : ESP_ERR_NO_MEM;
                if (error == ESP_OK) {
                    cJSON *json = raw[length - 1] == 0 && strlen(raw) == length - 1 && !strstr(raw, "\\u0000") ? cJSON_ParseWithOpts(raw, NULL, true) : NULL;
                    if (!pool_schedule_parse(json, &initial)) error = ESP_ERR_INVALID_STATE;
                    cJSON_Delete(json);
                }
            }
        }
        nvs_close(handle);
    }
    free(raw);
    if (error == ESP_ERR_NVS_NOT_FOUND) error = ESP_OK;
    pthread_mutex_lock(&schedule_mutex);
    saved = error == ESP_OK ? initial : (PoolSchedule){0};
    storage_valid = error == ESP_OK;
    initialized = true; selected_slot = -1; revision++;
    pthread_mutex_unlock(&schedule_mutex);
    if (error != ESP_OK) ESP_LOGE(TAG, "Stored pool schedule unavailable; current route retained");
    // A pool schedule failure never interrupts the miner's existing route or
    // prevents HTTP recovery. This is independent of the power safety policy.
    return ESP_OK;
}
typedef struct { const PoolSchedule *schedule; const char *raw; } SaveContext;
static esp_err_t commit_schedule(void *value)
{
    SaveContext *context = value;
    pthread_mutex_lock(&schedule_mutex);
    nvs_handle_t handle;
    esp_err_t error = nvs_open("5fw_profiles", NVS_READWRITE, &handle);
    if (error == ESP_OK) {
        size_t old_length = 0;
        esp_err_t previous = nvs_get_blob(handle, "pool_schedule", NULL, &old_length);
        char *old = NULL;
        if (previous == ESP_OK) {
            if (old_length && old_length <= SCHEDULE_BLOB_MAX) {
                old = malloc(old_length);
                error = old ? nvs_get_blob(handle, "pool_schedule", old, &old_length) : ESP_ERR_NO_MEM;
            }
        } else if (previous != ESP_ERR_NVS_NOT_FOUND) error = previous;
        if (error == ESP_OK) {
            error = nvs_set_blob(handle, "pool_schedule", context->raw, strlen(context->raw) + 1);
            if (error == ESP_OK) {
                error = nvs_commit(handle);
                if (error != ESP_OK) {
                    if (old) nvs_set_blob(handle, "pool_schedule", old, old_length);
                    else nvs_erase_key(handle, "pool_schedule");
                    nvs_commit(handle);
                }
            }
        }
        free(old);
        nvs_close(handle);
    }
    if (error == ESP_OK) {
        saved = *context->schedule; storage_valid = true; revision++; selected_slot = -1;
    }
    pthread_mutex_unlock(&schedule_mutex);
    return error;
}
esp_err_t pool_schedule_save(const PoolSchedule *schedule)
{
    if (!schedule || schedule->event_count > POOL_SCHEDULE_MAX_EVENTS) return ESP_ERR_INVALID_ARG;
    cJSON *json = serialize(schedule);
    if (!json) return ESP_ERR_NO_MEM;
    PoolSchedule validated;
    bool valid = pool_schedule_parse(json, &validated);
    char *raw = valid ? cJSON_PrintUnformatted(json) : NULL;
    cJSON_Delete(json);
    if (!valid) return ESP_ERR_INVALID_ARG;
    if (!raw) return ESP_ERR_NO_MEM;
    if (strlen(raw) + 1 > SCHEDULE_BLOB_MAX) { free(raw); return ESP_ERR_INVALID_SIZE; }
    uint16_t mask = 0;
    for (unsigned i = 0; i < validated.event_count; i++) mask |= 1u << validated.events[i].slot;
    SaveContext context = {.schedule = &validated, .raw = raw};
    // Profiles lock -> schedule lock: references are checked and committed atomically with deletion/edit guards.
    esp_err_t error = operating_profiles_with_pool_slots(mask, commit_schedule, &context);
    free(raw);
    return error;
}
static bool revision_current(void *value)
{
    pthread_mutex_lock(&schedule_mutex);
    bool current = storage_valid && revision == *(uint32_t *)value;
    pthread_mutex_unlock(&schedule_mutex);
    return current;
}
void pool_schedule_tick(void)
{
    pthread_mutex_lock(&schedule_mutex);
    PoolSchedule schedule = saved;
    uint32_t generation = revision;
    bool ready = initialized && storage_valid;
    pthread_mutex_unlock(&schedule_mutex);
    struct tm local;
    if (!ready || !pool_schedule_clock(time(NULL), mining_schedule_owns_clock(), schedule.utc_offset_minutes, &local)) return;
    int slot = pool_schedule_select(&schedule, local.tm_wday, local.tm_hour * 60 + local.tm_min);
    if (slot < 0) {
        pthread_mutex_lock(&schedule_mutex);
        if (generation == revision) selected_slot = -1;
        pthread_mutex_unlock(&schedule_mutex);
        return;
    }
    uint32_t desired_hash, current_hash;
    if (!operating_profile_pool_hash(slot, &desired_hash) || !operating_profile_current_pool_hash(false, &current_hash)) return;
    esp_err_t error = ESP_OK;
    if (desired_hash != current_hash)
        error = operating_profile_apply_pool_guarded(slot, false, revision_current, &generation);
    pthread_mutex_lock(&schedule_mutex);
    if (error == ESP_OK && generation == revision) selected_slot = slot;
    pthread_mutex_unlock(&schedule_mutex);
    // Only the primary route transaction is requested. No power, ASIC, fan or fallback settings are changed here.
    if (error != ESP_OK) ESP_LOGW(TAG, "Pool schedule apply deferred; current route retained");
}
cJSON *pool_schedule_get_json(void)
{
    pthread_mutex_lock(&schedule_mutex);
    PoolSchedule schedule = saved;
    int selected = selected_slot;
    bool valid = storage_valid;
    pthread_mutex_unlock(&schedule_mutex);
    cJSON *root = serialize(&schedule);
    cJSON *binding = operating_profiles_binding();
    if (!root || !binding) goto failed;
    struct tm local;
    bool clock = pool_schedule_clock(time(NULL), mining_schedule_owns_clock(), schedule.utc_offset_minutes, &local);
    if (!cJSON_AddBoolToObject(root, "clockValid", clock) || !cJSON_AddStringToObject(root, "timezoneMode", "fixed-utc-offset") ||
        !cJSON_AddStringToObject(root, "poolTarget", "primary") || !cJSON_AddNumberToObject(root, "maxEvents", POOL_SCHEDULE_MAX_EVENTS) ||
        !cJSON_AddBoolToObject(root, "storageValid", valid) ||
        !(clock && schedule.enabled && selected >= 0 ? cJSON_AddNumberToObject(root, "selectedSlot", selected) : cJSON_AddNullToObject(root, "selectedSlot"))) goto failed;
    while (binding->child) {
        cJSON *field = cJSON_DetachItemViaPointer(binding, binding->child);
        if (!cJSON_AddItemToObject(root, field->string, field)) { cJSON_Delete(field); goto failed; }
    }
    cJSON_Delete(binding); return root;
failed: cJSON_Delete(root); cJSON_Delete(binding); return NULL;
}
static void schedule_task(void *unused)
{
    (void)unused;
    while (1) { pool_schedule_tick(); vTaskDelay(pdMS_TO_TICKS(20000)); }
}
esp_err_t pool_schedule_start(void)
{
    if (!initialized) return ESP_ERR_INVALID_STATE;
    if (started) return ESP_OK;
    if (xTaskCreate(schedule_task, "pool schedule", 4096, NULL, 2, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    started = true;
    return ESP_OK;
}
