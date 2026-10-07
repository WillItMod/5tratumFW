#include "operating_profiles.h"
#include "nvs_config.h"
#include "http_server.h"
#include "protocol_coordinator.h"
#include "esp_app_desc.h"
#include "esp_mac.h"
#include "mbedtls/sha256.h"
#include "nvs.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define PROFILE_BODY_MAX 4096
#define PROFILE_BLOB_MAX 4096
#define PROFILE_NAME_MAX 32
static GlobalState *state;
static int prebuffer = 4096;
// Serializes profile reads, writes and scheduled Apply against slot editing.
static pthread_mutex_t profiles_mutex = PTHREAD_MUTEX_INITIALIZER;
__attribute__((weak)) bool operating_profile_pool_referenced(unsigned slot) { (void)slot; return false; }

static const char *pool_fields[] = {"protocol", "host", "port", "user", "password", "suggestedDifficulty", "extranonceSubscribe", "tls", "certificate", "channelType", "authorityPubkey", "decodeCoinbase"};
static const NvsConfigKey pool_keys[] = {NVS_CONFIG_STRATUM_PROTOCOL, NVS_CONFIG_STRATUM_URL, NVS_CONFIG_STRATUM_PORT, NVS_CONFIG_STRATUM_USER, NVS_CONFIG_STRATUM_PASS, NVS_CONFIG_STRATUM_DIFFICULTY, NVS_CONFIG_STRATUM_EXTRANONCE_SUBSCRIBE, NVS_CONFIG_STRATUM_TLS, NVS_CONFIG_STRATUM_CERT, NVS_CONFIG_SV2_CHANNEL_TYPE, NVS_CONFIG_SV2_AUTHORITY_PUBKEY, NVS_CONFIG_STRATUM_DECODE_COINBASE_TX};
static const NvsConfigKey fallback_keys[] = {NVS_CONFIG_FALLBACK_STRATUM_PROTOCOL, NVS_CONFIG_FALLBACK_STRATUM_URL, NVS_CONFIG_FALLBACK_STRATUM_PORT, NVS_CONFIG_FALLBACK_STRATUM_USER, NVS_CONFIG_FALLBACK_STRATUM_PASS, NVS_CONFIG_FALLBACK_STRATUM_DIFFICULTY, NVS_CONFIG_FALLBACK_STRATUM_EXTRANONCE_SUBSCRIBE, NVS_CONFIG_FALLBACK_STRATUM_TLS, NVS_CONFIG_FALLBACK_STRATUM_CERT, NVS_CONFIG_FALLBACK_SV2_CHANNEL_TYPE, NVS_CONFIG_FALLBACK_SV2_AUTHORITY_PUBKEY, NVS_CONFIG_FALLBACK_STRATUM_DECODE_COINBASE_TX};

static bool text(const cJSON *item, size_t min, size_t max)
{
    if (!cJSON_IsString(item)) return false;
    size_t length = strlen(item->valuestring);
    if (length < min || length > max) return false;
    for (size_t i = 0; i < length; i++) if ((unsigned char)item->valuestring[i] < 32 || (unsigned char)item->valuestring[i] == 127) return false;
    return true;
}
static bool integer(const cJSON *item, double min, double max)
{
    return cJSON_IsNumber(item) && isfinite(item->valuedouble) && item->valuedouble >= min && item->valuedouble <= max && floor(item->valuedouble) == item->valuedouble;
}
static bool exact_fields(const cJSON *object, const char *const *allowed, size_t count)
{
    if (!cJSON_IsObject(object)) return false;
    for (const cJSON *item = object->child; item; item = item->next) {
        if (!item->string) return false;
        bool known = false;
        for (size_t i = 0; i < count; i++) if (!strcmp(allowed[i], item->string)) known = true;
        if (!known) return false;
        for (const cJSON *next = item->next; next; next = next->next) if (next->string && !strcmp(next->string, item->string)) return false;
    }
    return true;
}
static void bounds(float *min_freq, float *max_freq, unsigned *min_mv, unsigned *max_mv)
{
    const AsicConfig *asic = &state->DEVICE_CONFIG.family.asic;
    *min_freq = *max_freq = asic->frequency_options[0];
    *min_mv = *max_mv = asic->voltage_options[0];
    for (size_t i = 0; asic->frequency_options[i]; i++) { if (asic->frequency_options[i] < *min_freq) *min_freq = asic->frequency_options[i]; if (asic->frequency_options[i] > *max_freq) *max_freq = asic->frequency_options[i]; }
    for (size_t i = 0; asic->voltage_options[i]; i++) { if (asic->voltage_options[i] < *min_mv) *min_mv = asic->voltage_options[i]; if (asic->voltage_options[i] > *max_mv) *max_mv = asic->voltage_options[i]; }
}
static bool tuning_valid(const cJSON *profile)
{
    float min_freq, max_freq; unsigned min_mv, max_mv; bounds(&min_freq, &max_freq, &min_mv, &max_mv);
    const cJSON *frequency = cJSON_GetObjectItemCaseSensitive(profile, "frequencyMHz"), *voltage = cJSON_GetObjectItemCaseSensitive(profile, "coreVoltageMv");
    if (!cJSON_IsNumber(frequency) || !isfinite(frequency->valuedouble) || !integer(voltage, 1, UINT16_MAX)) return false;
    float current = nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY);
    bool frequency_same = (float)frequency->valuedouble == current;
    bool voltage_same = voltage->valuedouble == nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE);
    return (frequency_same || (frequency->valuedouble >= min_freq && frequency->valuedouble <= max_freq && fabs(frequency->valuedouble * 4 - round(frequency->valuedouble * 4)) < 0.00001)) &&
           (voltage_same || (voltage->valuedouble >= min_mv && voltage->valuedouble <= max_mv));
}
static bool pool_valid(const cJSON *profile)
{
    const cJSON *protocol = cJSON_GetObjectItemCaseSensitive(profile, "protocol"), *channel = cJSON_GetObjectItemCaseSensitive(profile, "channelType");
    const cJSON *certificate = cJSON_GetObjectItemCaseSensitive(profile, "certificate");
    if (!text(protocol, 3, 3) || (strcmp(protocol->valuestring, "SV1") && strcmp(protocol->valuestring, "SV2")) ||
        !text(channel, 8, 8) || (strcmp(channel->valuestring, "standard") && strcmp(channel->valuestring, "extended")) ||
        !text(cJSON_GetObjectItemCaseSensitive(profile, "host"), 1, 253) || !text(cJSON_GetObjectItemCaseSensitive(profile, "user"), 0, 512) ||
        !text(cJSON_GetObjectItemCaseSensitive(profile, "password"), 0, 512) || !text(cJSON_GetObjectItemCaseSensitive(profile, "authorityPubkey"), 0, 52) ||
        !cJSON_IsString(certificate) || strlen(certificate->valuestring) > 2048 ||
        !integer(cJSON_GetObjectItemCaseSensitive(profile, "port"), 1, UINT16_MAX) || !integer(cJSON_GetObjectItemCaseSensitive(profile, "suggestedDifficulty"), 0, UINT16_MAX) ||
        !integer(cJSON_GetObjectItemCaseSensitive(profile, "tls"), 0, 2)) return false;
    return cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(profile, "extranonceSubscribe")) && cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(profile, "decodeCoinbase"));
}
static void slot_key(bool pool, unsigned slot, char key[8]) { snprintf(key, 8, "%c%u", pool ? 'p' : 't', slot); }
static esp_err_t read_slot(bool pool, unsigned slot, cJSON **profile)
{
    *profile = NULL; if (slot >= FIVE_PROFILE_SLOTS) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle; esp_err_t err = nvs_open("5fw_profiles", NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    char key[8]; slot_key(pool, slot, key); size_t length = 0;
    err = nvs_get_blob(handle, key, NULL, &length);
    char *raw = NULL;
    if (err == ESP_OK) {
        if (!length || length > PROFILE_BLOB_MAX) err = ESP_ERR_INVALID_SIZE;
        else { raw = malloc(length); err = raw ? nvs_get_blob(handle, key, raw, &length) : ESP_ERR_NO_MEM; }
    }
    nvs_close(handle);
    if (err == ESP_OK) {
        if (raw[length - 1] || strlen(raw) != length - 1) err = ESP_ERR_INVALID_STATE;
        else *profile = cJSON_ParseWithOpts(raw, NULL, true);
        if (!*profile) err = ESP_ERR_INVALID_STATE;
    }
    free(raw); return err;
}
static esp_err_t save_slot(bool pool, unsigned slot, const cJSON *profile)
{
    char *raw = cJSON_PrintUnformatted(profile);
    if (!raw) return ESP_ERR_NO_MEM;
    size_t size = strlen(raw) + 1;
    if (size > PROFILE_BLOB_MAX) { free(raw); return ESP_ERR_INVALID_SIZE; }
    cJSON *previous = NULL; esp_err_t old_error = read_slot(pool, slot, &previous);
    if (old_error != ESP_OK && old_error != ESP_ERR_NVS_NOT_FOUND) { free(raw); return old_error; }
    char *old_raw = previous ? cJSON_PrintUnformatted(previous) : NULL;
    if (previous && !old_raw) { cJSON_Delete(previous); free(raw); return ESP_ERR_NO_MEM; }
    cJSON_Delete(previous);
    nvs_handle_t handle; esp_err_t error = nvs_open("5fw_profiles", NVS_READWRITE, &handle);
    if (error == ESP_OK) {
        char key[8]; slot_key(pool, slot, key);
        error = nvs_set_blob(handle, key, raw, size);
        if (error == ESP_OK) {
            error = nvs_commit(handle);
            if (error != ESP_OK) { if (old_raw) nvs_set_blob(handle, key, old_raw, strlen(old_raw) + 1); else nvs_erase_key(handle, key); nvs_commit(handle); }
        }
        nvs_close(handle);
    }
    free(old_raw); free(raw); return error;
}
static cJSON *capture_pool(bool fallback)
{
    cJSON *pool = cJSON_CreateObject(); if (!pool) return NULL;
    for (size_t i = 0; i < sizeof(pool_keys) / sizeof(pool_keys[0]); i++) {
        NvsConfigKey key = fallback ? fallback_keys[i] : pool_keys[i]; Settings *setting = nvs_config_get_settings(key);
        cJSON *value = NULL;
        if (setting->type == TYPE_STR) { char *string = nvs_config_get_string(key); if (string) value = cJSON_CreateString(string); free(string); }
        else if (setting->type == TYPE_BOOL) value = cJSON_CreateBool(nvs_config_get_bool(key));
        else value = cJSON_CreateNumber(nvs_config_get_u16(key));
        if (!value || !cJSON_AddItemToObject(pool, pool_fields[i], value)) { cJSON_Delete(value); cJSON_Delete(pool); return NULL; }
    }
    return pool;
}
cJSON *operating_profiles_binding(void)
{
    cJSON *root = cJSON_CreateObject(); if (!root) return NULL;
    uint8_t mac[6], digest[32]; char id[38] = "5tfw:";
    const unsigned char prefix[] = "5tratum/device/v1";
    unsigned char bytes[sizeof(prefix) + 6]; memcpy(bytes, prefix, sizeof(prefix));
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) { cJSON_Delete(root); return NULL; }
    memcpy(bytes + sizeof(prefix), mac, 6);
    if (mbedtls_sha256(bytes, sizeof(bytes), digest, 0) != 0) { cJSON_Delete(root); return NULL; }
    for (int i = 0; i < 16; i++) snprintf(id + 5 + i * 2, 3, "%02x", digest[i]);
    cJSON *identity = cJSON_AddObjectToObject(root, "identity"), *hardware = cJSON_AddObjectToObject(root, "hardware"), *firmware = cJSON_AddObjectToObject(root, "firmware");
    char *board = nvs_config_get_string(NVS_CONFIG_BOARD_VERSION); char model[32];
    if (!board) { cJSON_Delete(root); return NULL; }
    snprintf(model, sizeof(model), "%s %s", state->DEVICE_CONFIG.family.name, board); free(board);
    if (!identity || !hardware || !firmware || !cJSON_AddStringToObject(identity, "deviceId", id) ||
        !cJSON_AddStringToObject(hardware, "boardModel", model) || !cJSON_AddStringToObject(hardware, "asicModel", state->DEVICE_CONFIG.family.asic.name) ||
        !cJSON_AddNumberToObject(hardware, "asicCount", state->DEVICE_CONFIG.family.asic_count) || !cJSON_AddStringToObject(firmware, "product", "5tratumFW") || !cJSON_AddStringToObject(firmware, "version", esp_app_get_description()->version)) { cJSON_Delete(root); return NULL; }
    return root;
}
static cJSON *get_profiles(void)
{
    cJSON *root = operating_profiles_binding(); if (!root) return NULL;
    if (!cJSON_AddNumberToObject(root, "schemaVersion", 1)) goto failed;
    cJSON *tuning = cJSON_AddArrayToObject(root, "tuning"), *pools = cJSON_AddArrayToObject(root, "pools"); if (!tuning || !pools) goto failed;
    for (unsigned type = 0; type < 2; type++) for (unsigned slot = 0; slot < FIVE_PROFILE_SLOTS; slot++) {
        cJSON *profile = NULL; esp_err_t error = read_slot(type == 1, slot, &profile);
        if (error != ESP_OK && error != ESP_ERR_NVS_NOT_FOUND) goto failed;
        if (!profile) profile = cJSON_CreateObject();
        if (!profile) goto failed;
        bool configured = text(cJSON_GetObjectItemCaseSensitive(profile, "name"), 1, PROFILE_NAME_MAX);
        if (configured && type == 1 && !pool_valid(profile)) { cJSON_Delete(profile); goto failed; }
        // Construct the public view from an allowlist; even a damaged or empty
        // private slot can never leak its password or unknown stored fields.
        cJSON *view = cJSON_CreateObject();
        if (!view || !cJSON_AddNumberToObject(view, "slot", slot) || !cJSON_AddBoolToObject(view, "configured", configured)) { cJSON_Delete(view); cJSON_Delete(profile); goto failed; }
        if (configured) {
            const char *const tuning_fields[] = {"name", "frequencyMHz", "coreVoltageMv"};
            for (size_t field = 0; field < (type == 1 ? 13 : 3); field++) {
                const char *key = type == 1 ? field == 12 ? "name" : pool_fields[field] : tuning_fields[field];
                if (!strcmp(key, "password")) continue;
                cJSON *copy = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(profile, key), true);
                if (!copy || !cJSON_AddItemToObject(view, key, copy)) { cJSON_Delete(copy); cJSON_Delete(view); cJSON_Delete(profile); goto failed; }
            }
            if (type == 1 && !cJSON_AddBoolToObject(view, "passwordConfigured", cJSON_GetObjectItemCaseSensitive(profile, "password")->valuestring[0] != 0)) { cJSON_Delete(view); cJSON_Delete(profile); goto failed; }
        } else if (!cJSON_AddNullToObject(view, "name")) { cJSON_Delete(view); cJSON_Delete(profile); goto failed; }
        cJSON_Delete(profile); profile = view;
        if (!cJSON_AddItemToArray(type == 1 ? pools : tuning, profile)) { cJSON_Delete(profile); goto failed; }
    }
    float min_freq, max_freq; unsigned min_mv, max_mv; bounds(&min_freq, &max_freq, &min_mv, &max_mv);
    cJSON *limits = cJSON_AddObjectToObject(root, "limits"), *current = cJSON_AddObjectToObject(root, "current"); if (!limits || !current) goto failed;
    cJSON *frequency = cJSON_AddObjectToObject(limits, "frequency"), *voltage = cJSON_AddObjectToObject(limits, "coreVoltage"); if (!frequency || !voltage) goto failed;
    if (!cJSON_AddNumberToObject(frequency, "min", min_freq) || !cJSON_AddNumberToObject(frequency, "max", max_freq) || !cJSON_AddNumberToObject(frequency, "step", .25) || !cJSON_AddStringToObject(frequency, "quantization", "nearest-pll") ||
        !cJSON_AddNumberToObject(voltage, "min", min_mv) || !cJSON_AddNumberToObject(voltage, "max", max_mv) || !cJSON_AddNumberToObject(voltage, "step", 1) ||
        !cJSON_AddNumberToObject(current, "frequencyMHz", nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)) || !cJSON_AddNumberToObject(current, "coreVoltageMv", nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE))) goto failed;
    return root;
failed: cJSON_Delete(root); return NULL;
}
bool operating_profile_pool_exists(unsigned slot)
{
    pthread_mutex_lock(&profiles_mutex); cJSON *profile = NULL; esp_err_t error = read_slot(true, slot, &profile);
    bool exists = error == ESP_OK && text(cJSON_GetObjectItemCaseSensitive(profile, "name"), 1, PROFILE_NAME_MAX) && pool_valid(profile);
    cJSON_Delete(profile); pthread_mutex_unlock(&profiles_mutex); return exists;
}
bool operating_profile_pool_name(unsigned slot, char *name, size_t size)
{
    pthread_mutex_lock(&profiles_mutex); cJSON *profile = NULL; esp_err_t error = read_slot(true, slot, &profile);
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(profile, "name"); bool found = error == ESP_OK && text(value, 1, PROFILE_NAME_MAX);
    if (found) snprintf(name, size, "%s", value->valuestring);
    cJSON_Delete(profile);
    pthread_mutex_unlock(&profiles_mutex);
    return found;
}
static bool pool_hash(const cJSON *profile, uint32_t *hash)
{
    if (!hash || !pool_valid(profile)) return false;
    cJSON *normalized = cJSON_CreateObject();
    if (!normalized) return false;
    for (size_t i = 0; i < 12; i++) {
        cJSON *copy = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(profile, pool_fields[i]), true);
        if (!copy || !cJSON_AddItemToObject(normalized, pool_fields[i], copy)) { cJSON_Delete(copy); cJSON_Delete(normalized); return false; }
    }
    char *serialized = cJSON_PrintUnformatted(normalized); cJSON_Delete(normalized);
    if (serialized) { uint32_t value = 2166136261u; for (const unsigned char *p = (const unsigned char *)serialized; *p; p++) value = (value ^ *p) * 16777619u; *hash = value; }
    bool available = serialized != NULL; free(serialized); return available;
}
bool operating_profile_pool_hash(unsigned slot, uint32_t *hash)
{
    pthread_mutex_lock(&profiles_mutex); cJSON *profile = NULL; esp_err_t error = read_slot(true, slot, &profile);
    bool available = error == ESP_OK && pool_hash(profile, hash);
    cJSON_Delete(profile); pthread_mutex_unlock(&profiles_mutex); return available;
}
bool operating_profile_current_pool_hash(bool fallback, uint32_t *hash)
{
    pthread_mutex_lock(&profiles_mutex); cJSON *profile = capture_pool(fallback);
    bool available = profile && pool_hash(profile, hash);
    cJSON_Delete(profile); pthread_mutex_unlock(&profiles_mutex); return available;
}
esp_err_t operating_profiles_with_pool_slots(uint16_t slot_mask, esp_err_t (*commit)(void *), void *context)
{
    if (!commit || slot_mask & ~((1u << FIVE_PROFILE_SLOTS) - 1)) return ESP_ERR_INVALID_ARG;
    pthread_mutex_lock(&profiles_mutex); esp_err_t error = ESP_OK;
    for (unsigned slot = 0; slot < FIVE_PROFILE_SLOTS; slot++) if (slot_mask & (1u << slot)) {
        cJSON *profile = NULL; error = read_slot(true, slot, &profile);
        if (error == ESP_OK && !pool_valid(profile)) error = ESP_ERR_INVALID_ARG;
        cJSON_Delete(profile); if (error != ESP_OK) break;
    }
    if (error == ESP_OK) error = commit(context);
    pthread_mutex_unlock(&profiles_mutex); return error;
}
static esp_err_t apply_pool(const cJSON *profile, bool fallback)
{
    if (!pool_valid(profile)) return ESP_ERR_INVALID_ARG;
    NvsConfigMutation changes[12];
    for (size_t i = 0; i < 12; i++) {
        changes[i].key = fallback ? fallback_keys[i] : pool_keys[i]; changes[i].type = nvs_config_get_settings(changes[i].key)->type;
        cJSON *value = cJSON_GetObjectItemCaseSensitive(profile, pool_fields[i]);
        if (changes[i].type == TYPE_STR) changes[i].value.str = value->valuestring;
        else if (changes[i].type == TYPE_BOOL) changes[i].value.b = cJSON_IsTrue(value);
        else changes[i].value.u16 = value->valueint;
    }
    esp_err_t error = nvs_config_apply_atomic(changes, 12);
    if (error == ESP_OK && !fallback) protocol_coordinator_request_primary_reload(state);
    return error;
}
esp_err_t operating_profile_apply_pool_guarded(unsigned slot, bool fallback, bool (*allow)(void *), void *context)
{
    pthread_mutex_lock(&profiles_mutex); cJSON *profile = NULL;
    esp_err_t error = allow && !allow(context) ? ESP_ERR_INVALID_STATE : read_slot(true, slot, &profile);
    if (error == ESP_OK) error = apply_pool(profile, fallback);
    cJSON_Delete(profile); pthread_mutex_unlock(&profiles_mutex); return error;
}
esp_err_t operating_profile_apply_pool(unsigned slot, bool fallback)
{
    return operating_profile_apply_pool_guarded(slot, fallback, NULL, NULL);
}
static esp_err_t error_response(httpd_req_t *req, const char *status, const char *error)
{
    httpd_resp_set_status(req, status); httpd_resp_set_type(req, "application/json");
    char body[96]; snprintf(body, sizeof(body), "{\"ok\":false,\"error\":\"%s\"}", error); return httpd_resp_sendstr(req, body);
}
static bool allowed(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) { error_response(req, "401 Unauthorized", "unauthorized"); return false; }
    if (set_cors_headers(req) != ESP_OK) { httpd_resp_send_500(req); return false; }
    httpd_resp_set_hdr(req, "Cache-Control", "no-store"); return true;
}
static cJSON *read_body(httpd_req_t *req)
{
    if (!req->content_len || req->content_len >= PROFILE_BODY_MAX) { error_response(req, "400 Bad Request", "invalid-profile"); return NULL; }
    char *body = malloc(req->content_len + 1); if (!body) { error_response(req, "500 Internal Server Error", "allocation-failed"); return NULL; }
    size_t received = 0;
    while (received < req->content_len) { int count = httpd_req_recv(req, body + received, req->content_len - received); if (count <= 0) break; received += count; }
    body[received] = 0; cJSON *json = received == req->content_len && strlen(body) == received && !strstr(body, "\\u0000") ? cJSON_ParseWithOpts(body, NULL, true) : NULL; free(body);
    if (!json) error_response(req, "400 Bad Request", "invalid-profile");
    return json;
}
static esp_err_t get_handler(httpd_req_t *req)
{
    if (!allowed(req)) return ESP_OK;
    pthread_mutex_lock(&profiles_mutex); cJSON *root = get_profiles(); pthread_mutex_unlock(&profiles_mutex);
    if (!root) return error_response(req, "500 Internal Server Error", "storage-unavailable");
    httpd_resp_set_type(req, "application/json"); esp_err_t err = HTTP_send_json(req, root, &prebuffer); cJSON_Delete(root); return err;
}
static esp_err_t post_handler(httpd_req_t *req)
{
    if (!allowed(req)) return ESP_OK;
    cJSON *body = read_body(req); if (!body) return ESP_OK;
    static const char *const fields[] = {"type", "slot", "name", "clear", "captureCurrent", "frequencyMHz", "coreVoltageMv", "protocol", "host", "port", "user", "password", "suggestedDifficulty", "extranonceSubscribe", "tls", "certificate", "channelType", "authorityPubkey", "decodeCoinbase"};
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(body, "type"), *slot_json = cJSON_GetObjectItemCaseSensitive(body, "slot");
    bool valid = exact_fields(body, fields, sizeof(fields) / sizeof(fields[0])) && text(type, 4, 6) && (!strcmp(type->valuestring, "pool") || !strcmp(type->valuestring, "tuning")) && integer(slot_json, 0, 9);
    if (!valid) { cJSON_Delete(body); return error_response(req, "400 Bad Request", "invalid-profile"); }
    bool pool = !strcmp(type->valuestring, "pool"); unsigned slot = slot_json->valueint;
    pthread_mutex_lock(&profiles_mutex);
    if (pool && operating_profile_pool_referenced(slot)) { pthread_mutex_unlock(&profiles_mutex); cJSON_Delete(body); return error_response(req, "409 Conflict", "profile-in-use"); }
    cJSON *clear = cJSON_GetObjectItemCaseSensitive(body, "clear"), *name = cJSON_GetObjectItemCaseSensitive(body, "name"), *capture = cJSON_GetObjectItemCaseSensitive(body, "captureCurrent");
    cJSON *profile = NULL;
    if (cJSON_IsTrue(clear) && !name && !capture && !cJSON_GetObjectItemCaseSensitive(body, "frequencyMHz") && !cJSON_GetObjectItemCaseSensitive(body, "coreVoltageMv") && cJSON_GetArraySize(body) == 3) profile = cJSON_CreateObject();
    else if (text(name, 1, PROFILE_NAME_MAX)) {
        if (pool && cJSON_GetArraySize(body) == 4 && text(capture, 7, 8) && (!strcmp(capture->valuestring, "primary") || !strcmp(capture->valuestring, "fallback"))) profile = capture_pool(!strcmp(capture->valuestring, "fallback"));
        else if (!capture && !clear && (pool ? !cJSON_GetObjectItemCaseSensitive(body, "frequencyMHz") && !cJSON_GetObjectItemCaseSensitive(body, "coreVoltageMv") : cJSON_GetArraySize(body) == 5)) {
            profile = pool ? NULL : cJSON_CreateObject();
            if (pool) {
                esp_err_t old = read_slot(true, slot, &profile);
                if (old == ESP_ERR_NVS_NOT_FOUND) profile = cJSON_CreateObject();
            }
            if (profile) for (size_t i = 0; i < (pool ? 12 : 2); i++) {
                const char *field = pool ? pool_fields[i] : i == 0 ? "frequencyMHz" : "coreVoltageMv";
                cJSON *value = cJSON_GetObjectItemCaseSensitive(body, field);
                if (value) { cJSON_DeleteItemFromObjectCaseSensitive(profile, field); cJSON *copy = cJSON_Duplicate(value, true); if (!copy || !cJSON_AddItemToObject(profile, field, copy)) { cJSON_Delete(copy); cJSON_Delete(profile); profile = NULL; break; } }
            }
        }
        if (profile && !(pool ? pool_valid(profile) : tuning_valid(profile))) { cJSON_Delete(profile); profile = NULL; }
        if (profile) { cJSON_DeleteItemFromObjectCaseSensitive(profile, "name"); if (!cJSON_AddStringToObject(profile, "name", name->valuestring)) { cJSON_Delete(profile); profile = NULL; } }
    }
    esp_err_t err = profile ? save_slot(pool, slot, profile) : ESP_ERR_INVALID_ARG;
    cJSON_Delete(profile); cJSON_Delete(body); pthread_mutex_unlock(&profiles_mutex);
    if (err != ESP_OK) return error_response(req, err == ESP_ERR_INVALID_ARG || err == ESP_ERR_INVALID_SIZE ? "400 Bad Request" : err == ESP_ERR_NO_MEM ? "500 Internal Server Error" : "507 Insufficient Storage", err == ESP_ERR_INVALID_ARG || err == ESP_ERR_INVALID_SIZE ? "invalid-profile" : err == ESP_ERR_NO_MEM ? "allocation-failed" : "storage-unavailable");
    httpd_resp_set_type(req, "application/json"); return httpd_resp_sendstr(req, "{\"ok\":true,\"restartRequired\":false}");
}
static esp_err_t apply_handler(httpd_req_t *req)
{
    if (!allowed(req)) return ESP_OK;
    cJSON *body = read_body(req); if (!body) return ESP_OK;
    static const char *const fields[] = {"type", "slot", "poolTarget", "frequencyMHz", "coreVoltageMv"};
    cJSON *type = cJSON_GetObjectItemCaseSensitive(body, "type"), *slot_json = cJSON_GetObjectItemCaseSensitive(body, "slot"), *target = cJSON_GetObjectItemCaseSensitive(body, "poolTarget");
    bool pool = text(type, 4, 6) && !strcmp(type->valuestring, "pool");
    bool tuning = text(type, 4, 6) && !strcmp(type->valuestring, "tuning");
    bool valid = exact_fields(body, fields, 5) && (pool || tuning) && (!slot_json || integer(slot_json, 0, 9)) &&
        (pool ? slot_json && text(target, 7, 8) && (!strcmp(target->valuestring, "primary") || !strcmp(target->valuestring, "fallback")) && !cJSON_GetObjectItemCaseSensitive(body, "frequencyMHz") && !cJSON_GetObjectItemCaseSensitive(body, "coreVoltageMv") : !target && (slot_json ? !cJSON_GetObjectItemCaseSensitive(body, "frequencyMHz") && !cJSON_GetObjectItemCaseSensitive(body, "coreVoltageMv") : true));
    if (!valid) { cJSON_Delete(body); return error_response(req, "400 Bad Request", "invalid-profile"); }
    pthread_mutex_lock(&profiles_mutex);
    cJSON *profile = NULL; esp_err_t err = ESP_OK;
    if (slot_json) err = read_slot(pool, slot_json->valueint, &profile); else profile = cJSON_Duplicate(body, true);
    if (err == ESP_OK && !profile) err = ESP_ERR_NO_MEM;
    if (err == ESP_OK && !cJSON_GetObjectItemCaseSensitive(profile, pool ? "host" : "frequencyMHz")) err = slot_json ? ESP_ERR_NVS_NOT_FOUND : ESP_ERR_INVALID_ARG;
    if (err == ESP_OK && pool) err = apply_pool(profile, !strcmp(target->valuestring, "fallback"));
    else if (err == ESP_OK) {
        if (!tuning_valid(profile)) err = ESP_ERR_INVALID_ARG;
        else {
            NvsConfigMutation changes[] = {{.key = NVS_CONFIG_ASIC_FREQUENCY, .type = TYPE_FLOAT, .value.f = cJSON_GetObjectItemCaseSensitive(profile, "frequencyMHz")->valuedouble}, {.key = NVS_CONFIG_ASIC_VOLTAGE, .type = TYPE_U16, .value.u16 = cJSON_GetObjectItemCaseSensitive(profile, "coreVoltageMv")->valueint}, {.key = NVS_CONFIG_OVERCLOCK_ENABLED, .type = TYPE_BOOL, .value.b = true}};
            err = nvs_config_apply_atomic(changes, 3);
        }
    }
    bool restart_required = pool && !strcmp(target->valuestring, "fallback");
    cJSON_Delete(profile); cJSON_Delete(body); pthread_mutex_unlock(&profiles_mutex);
    if (err != ESP_OK) return error_response(req, err == ESP_ERR_NVS_NOT_FOUND ? "409 Conflict" : err == ESP_ERR_INVALID_ARG ? "400 Bad Request" : err == ESP_ERR_NO_MEM ? "500 Internal Server Error" : "507 Insufficient Storage", err == ESP_ERR_NVS_NOT_FOUND ? "empty-slot" : err == ESP_ERR_INVALID_ARG ? "invalid-profile" : err == ESP_ERR_NO_MEM ? "allocation-failed" : "storage-unavailable");
    httpd_resp_set_type(req, "application/json"); return httpd_resp_sendstr(req, restart_required ? "{\"ok\":true,\"restartRequired\":true}" : "{\"ok\":true,\"restartRequired\":false}");
}
esp_err_t operating_profiles_register(httpd_handle_t server, GlobalState *global)
{
    operating_profiles_init(global);
    const httpd_uri_t routes[] = {{.uri = "/api/5tratum/profiles", .method = HTTP_GET, .handler = get_handler}, {.uri = "/api/5tratum/profiles", .method = HTTP_POST, .handler = post_handler}, {.uri = "/api/5tratum/profiles/apply", .method = HTTP_POST, .handler = apply_handler}};
    for (size_t i = 0; i < 3; i++) { esp_err_t err = httpd_register_uri_handler(server, &routes[i]); if (err != ESP_OK) return err; }
    return ESP_OK;
}
void operating_profiles_init(GlobalState *global) { state = global; }
