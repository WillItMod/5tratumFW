#include "mining_schedule.h"
#include "nvs_config.h"
#include "nvs.h"
#include "esp_netif_sntp.h"
#include "esp_log.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

static const char *TAG = "mining_schedule";
typedef struct { const char *name; const char *posix; } ScheduleTimezone;
// POSIX footers from local system zoneinfo, including each named zone's DST.
static const ScheduleTimezone zones[] = {
    {"UTC", "UTC0"},
    {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Chicago", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Denver", "MST7MDT,M3.2.0,M11.1.0"},
    {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
    {"Asia/Tokyo", "JST-9"}, {"Asia/Shanghai", "CST-8"},
    {"Australia/Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
};

static GlobalState *state;
static SemaphoreHandle_t mutex;
static MiningScheduleSettings saved;
static MiningScheduleEngine engine;
static atomic_bool clock_valid;
static bool time_service_ready, time_service_started, applied_paused;
static time_t override_deadline;
static bool override_deadline_computed;
#define MIN_VALID_EPOCH 1704067200
static char config_error[96], power_error[96], time_error[96];
static char timezone_environment[128];

static const char *timezone_rule(const char *name)
{
    for (size_t i = 0; i < sizeof(zones) / sizeof(zones[0]); i++) {
        if (strcmp(name, zones[i].name) == 0) return zones[i].posix;
    }
    return NULL;
}

static bool parse_hhmm(const cJSON *item, uint16_t *minutes)
{
    if (!cJSON_IsString(item) || strlen(item->valuestring) != 5) return false;
    const char *s = item->valuestring;
    if (s[2] != ':' || s[0] < '0' || s[0] > '2' || s[1] < '0' || s[1] > '9' ||
        s[3] < '0' || s[3] > '5' || s[4] < '0' || s[4] > '9') return false;
    int hour = (s[0] - '0') * 10 + s[1] - '0';
    if (hour > 23) return false;
    *minutes = hour * 60 + (s[3] - '0') * 10 + s[4] - '0';
    return true;
}

static bool only_fields(const cJSON *object, const char *const *allowed, size_t count)
{
    if (!cJSON_IsObject(object)) return false;
    for (const cJSON *field = object->child; field; field = field->next) {
        bool found = false;
        for (size_t i = 0; i < count; i++) {
            if (field->string && strcmp(field->string, allowed[i]) == 0) found = true;
        }
        if (!found) return false;
        for (const cJSON *other = field->next; other; other = other->next) {
            if (other->string && strcmp(field->string, other->string) == 0) return false;
        }
    }
    return true;
}

bool mining_schedule_parse(const cJSON *json, MiningScheduleSettings *settings)
{
    static const char *const fields[] = {"enabled", "timezone", "windows"};
    static const char *const window_fields[] = {"days", "start", "end"};
    if (!settings || !only_fields(json, fields, 3)) return false;
    const cJSON *enabled = cJSON_GetObjectItemCaseSensitive(json, "enabled");
    const cJSON *timezone = cJSON_GetObjectItemCaseSensitive(json, "timezone");
    const cJSON *windows = cJSON_GetObjectItemCaseSensitive(json, "windows");
    if (!cJSON_IsBool(enabled) || !cJSON_IsString(timezone) ||
        !timezone_rule(timezone->valuestring) || !cJSON_IsArray(windows)) return false;
    int count = cJSON_GetArraySize(windows);
    if (count > 8 || (cJSON_IsTrue(enabled) && count == 0)) return false;
    memset(settings, 0, sizeof(*settings));
    settings->policy.enabled = cJSON_IsTrue(enabled);
    settings->policy.window_count = count;
    snprintf(settings->timezone, sizeof(settings->timezone), "%s", timezone->valuestring);
    for (int i = 0; i < count; i++) {
        const cJSON *window = cJSON_GetArrayItem(windows, i);
        if (!only_fields(window, window_fields, 3)) return false;
        const cJSON *days = cJSON_GetObjectItemCaseSensitive(window, "days");
        if (!cJSON_IsNumber(days) || !isfinite(days->valuedouble) ||
            days->valuedouble != days->valueint || days->valueint < 1 || days->valueint > 127) return false;
        settings->policy.windows[i].days = days->valueint;
        if (!parse_hhmm(cJSON_GetObjectItemCaseSensitive(window, "start"), &settings->policy.windows[i].start_minutes) ||
            !parse_hhmm(cJSON_GetObjectItemCaseSensitive(window, "end"), &settings->policy.windows[i].end_minutes)) return false;
    }
    return mining_schedule_config_valid(&settings->policy);
}

static cJSON *settings_json(const MiningScheduleSettings *settings)
{
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;
    if (!cJSON_AddBoolToObject(json, "enabled", settings->policy.enabled) ||
        !cJSON_AddStringToObject(json, "timezone", settings->timezone)) {
        cJSON_Delete(json); return NULL;
    }
    cJSON *windows = cJSON_AddArrayToObject(json, "windows");
    if (!windows) { cJSON_Delete(json); return NULL; }
    for (int i = 0; i < settings->policy.window_count; i++) {
        const MiningScheduleWindow *window = &settings->policy.windows[i];
        cJSON *entry = cJSON_CreateObject();
        if (!entry) { cJSON_Delete(json); return NULL; }
        char start[8], end[8];
        snprintf(start, sizeof(start), "%02u:%02u", window->start_minutes / 60, window->start_minutes % 60);
        snprintf(end, sizeof(end), "%02u:%02u", window->end_minutes / 60, window->end_minutes % 60);
        if (!cJSON_AddNumberToObject(entry, "days", window->days) ||
            !cJSON_AddStringToObject(entry, "start", start) ||
            !cJSON_AddStringToObject(entry, "end", end) ||
            !cJSON_AddItemToArray(windows, entry)) {
            cJSON_Delete(entry); cJSON_Delete(json); return NULL;
        }
    }
    return json;
}

static bool local_clock(struct tm *local, char *formatted, size_t size)
{
    if (!atomic_load(&clock_valid)) return false;
    time_t now = time(NULL);
    if (now < MIN_VALID_EPOCH) {
        atomic_store(&clock_valid, false);
        return false;
    }
    if (!localtime_r(&now, local)) return false;
    if (formatted) strftime(formatted, size, "%Y-%m-%dT%H:%M:%S%z", local);
    return true;
}

static void evaluate_locked(void)
{
    struct tm local = {0};
    bool valid = local_clock(&local, NULL, 0);
    if (!valid) override_deadline_computed = false;
    if (valid && override_deadline && time(NULL) >= override_deadline) {
        mining_schedule_engine_clear_override(&engine);
        override_deadline = 0;
    }
    state->SYSTEM_MODULE.mining_paused = mining_schedule_engine_evaluate(&engine, &saved.policy, valid, &local);
    if (config_error[0]) state->SYSTEM_MODULE.mining_paused = true;
    if (!engine.manual_override_active) {
        override_deadline = 0;
        override_deadline_computed = false;
    }
}

static void set_override_deadline_locked(void)
{
    override_deadline = 0;
    override_deadline_computed = false;
    if (!engine.manual_override_active || !saved.policy.enabled) return;
    time_t now = time(NULL);
    struct tm local;
    if (!local_clock(&local, NULL, 0)) return;
    // A complete weekly pause union can have no future edge. Compute this once
    // per override/configuration/valid-clock period, never on every power tick.
    override_deadline_computed = true;
    bool current = mining_schedule_is_paused(&saved.policy, true, &local);
    // Scan actual epoch minutes through localtime, including repeated/skipped
    // DST hours. A week plus one day covers the next weekly union boundary.
    time_t minute = now - now % 60;
    for (int i = 1; i <= 8 * 24 * 60; i++) {
        time_t candidate = minute + i * 60;
        if (localtime_r(&candidate, &local) && mining_schedule_is_paused(&saved.policy, true, &local) != current) {
            override_deadline = candidate;
            return;
        }
    }
}

esp_err_t mining_schedule_init(GlobalState *global)
{
    state = global;
    mutex = xSemaphoreCreateMutex();
    if (!mutex) return ESP_ERR_NO_MEM;
    mining_schedule_engine_init(&engine, false);
    strcpy(saved.timezone, "UTC");
    char *stored = NULL;
    esp_err_t read_error = nvs_config_read_stored_string(NVS_CONFIG_MINING_SCHEDULE, &stored);
    cJSON *json = read_error == ESP_OK ? cJSON_ParseWithOpts(stored, NULL, true) : NULL;
    if (read_error != ESP_ERR_NVS_NOT_FOUND && (read_error != ESP_OK || !mining_schedule_parse(json, &saved))) {
        memset(&saved, 0, sizeof(saved));
        strcpy(saved.timezone, "UTC");
        snprintf(config_error, sizeof(config_error), "Invalid stored mining schedule; paused until corrected");
    }
    cJSON_Delete(json);
    free(stored);
    snprintf(timezone_environment, sizeof(timezone_environment), "TZ=%s", timezone_rule(saved.timezone));
    if (putenv(timezone_environment) != 0) return ESP_ERR_NO_MEM;
    tzset();
    evaluate_locked();
    return ESP_OK;
}

static void time_synced(struct timeval *tv)
{
    atomic_store(&clock_valid, tv->tv_sec >= MIN_VALID_EPOCH);
}

void mining_schedule_start_time_sync(void)
{
    if (!mutex || time_service_ready) return;
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.start = false;
    config.sync_cb = time_synced;
    esp_err_t err = esp_netif_sntp_init(&config);
    xSemaphoreTake(mutex, portMAX_DELAY);
    time_service_ready = err == ESP_OK;
    if (err != ESP_OK) snprintf(time_error, sizeof(time_error), "Time synchronization initialization failed: %s", esp_err_to_name(err));
    xSemaphoreGive(mutex);
}

bool mining_schedule_owns_clock(void)
{
    struct tm local;
    return local_clock(&local, NULL, 0);
}

void mining_schedule_update(void)
{
    if (!mutex) return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (time_service_ready && !time_service_started && state->SYSTEM_MODULE.is_connected) {
        esp_err_t err = esp_netif_sntp_start();
        time_service_started = err == ESP_OK;
        if (err != ESP_OK) ESP_LOGW(TAG, "SNTP start failed: %s", esp_err_to_name(err));
    }
    evaluate_locked();
    if (engine.manual_override_active && !override_deadline_computed && atomic_load(&clock_valid)) set_override_deadline_locked();
    xSemaphoreGive(mutex);
}

void mining_schedule_manual_override(bool paused)
{
    if (!mutex) return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (config_error[0]) {
        // A resume cannot bypass unreadable persisted policy. Repair it first.
        xSemaphoreGive(mutex);
        return;
    }
    evaluate_locked();
    mining_schedule_engine_set_override(&engine, paused);
    evaluate_locked();
    set_override_deadline_locked();
    xSemaphoreGive(mutex);
}

void mining_schedule_clear_override(void)
{
    if (!mutex) return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    mining_schedule_engine_clear_override(&engine);
    override_deadline = 0;
    override_deadline_computed = false;
    evaluate_locked();
    xSemaphoreGive(mutex);
}

esp_err_t mining_schedule_save(const MiningScheduleSettings *settings)
{
    if (!mutex || !settings || !timezone_rule(settings->timezone) ||
        !mining_schedule_config_valid(&settings->policy) ||
        (settings->policy.enabled && settings->policy.window_count == 0)) return ESP_ERR_INVALID_ARG;
    cJSON *json = settings_json(settings);
    char *document = json ? cJSON_PrintUnformatted(json) : NULL;
    cJSON_Delete(json);
    if (!document) return ESP_ERR_NO_MEM;
    xSemaphoreTake(mutex, portMAX_DELAY);
    esp_err_t err = nvs_config_set_string_sync(NVS_CONFIG_MINING_SCHEDULE, document);
    if (err == ESP_OK) {
        if (!settings->policy.enabled) engine.effective_paused = state->SYSTEM_MODULE.mining_paused;
        saved = *settings;
        snprintf(timezone_environment, sizeof(timezone_environment), "TZ=%s", timezone_rule(saved.timezone));
        tzset();
        config_error[0] = '\0';
        evaluate_locked();
        set_override_deadline_locked();
    }
    xSemaphoreGive(mutex);
    free(document);
    return err;
}

bool mining_schedule_applied_paused(void)
{
    if (!mutex) return false;
    xSemaphoreTake(mutex, portMAX_DELAY);
    bool paused = applied_paused;
    xSemaphoreGive(mutex);
    return paused;
}

void mining_schedule_report_power(bool paused, const char *error)
{
    if (!mutex) return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    applied_paused = paused;
    snprintf(power_error, sizeof(power_error), "%s", error ? error : "");
    xSemaphoreGive(mutex);
}

cJSON *mining_schedule_get_json(void)
{
    if (!mutex) return NULL;
    xSemaphoreTake(mutex, portMAX_DELAY);
    evaluate_locked();
    cJSON *json = cJSON_CreateObject();
    cJSON *status = cJSON_CreateObject();
    cJSON *schedule = settings_json(&saved);
    if (!json || !status || !schedule) {
        cJSON_Delete(json); cJSON_Delete(status); cJSON_Delete(schedule);
        xSemaphoreGive(mutex);
        return NULL;
    }
    struct tm local = {0};
    char local_time[40];
    bool valid = local_clock(&local, local_time, sizeof(local_time));
    bool request = state->SYSTEM_MODULE.mining_paused || state->SYSTEM_MODULE.hardware_fault || state->SYSTEM_MODULE.pools_unavailable || !state->SYSTEM_MODULE.mining_runtime_ready;
    cJSON_AddBoolToObject(json, "supported", true);
    cJSON_AddItemToObject(json, "schedule", schedule);
    cJSON_AddBoolToObject(status, "clockValid", valid);
    if (valid) cJSON_AddStringToObject(status, "localTime", local_time);
    else cJSON_AddNullToObject(status, "localTime");
    cJSON_AddBoolToObject(status, "scheduledPause", mining_schedule_is_paused(&saved.policy, valid, &local));
    cJSON_AddStringToObject(status, "manualOverride", !engine.manual_override_active ? "none" : engine.manual_override_paused ? "paused" : "running");
    cJSON_AddBoolToObject(status, "requestedPaused", request);
    cJSON_AddBoolToObject(status, "appliedPaused", applied_paused);
    cJSON_AddBoolToObject(status, "transitionPending", request != applied_paused);
    const char *error = state->SYSTEM_MODULE.hardware_fault ? state->SYSTEM_MODULE.hardware_fault_msg : power_error[0] ? power_error : config_error[0] ? config_error : time_error;
    if (error[0]) cJSON_AddStringToObject(status, "error", error);
    else cJSON_AddNullToObject(status, "error");
    cJSON_AddItemToObject(json, "status", status);
    xSemaphoreGive(mutex);
    return json;
}
