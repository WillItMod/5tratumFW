#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "sdk_stubs.h"
#include "nvs_config.h"

typedef enum { STORE_STRING, STORE_U16, STORE_I32, STORE_U64 } StoreType;
typedef struct {
    char key[32];
    StoreType type;
    char string[256];
    uint64_t number;
} Entry;

static Entry store[64];
static size_t entry_count;
static esp_err_t flash_result = ESP_OK;
static esp_err_t open_result = ESP_OK;
static esp_err_t write_open_result = ESP_OK;
static unsigned flash_calls, erase_calls, open_calls, write_opens, close_calls;
static unsigned writes, commits, tasks;
static esp_err_t set_result = ESP_OK, commit_result = ESP_OK;
static char written_key[32];

static void put(const char *key, StoreType type, const char *string, uint64_t number)
{
    assert(entry_count < sizeof(store) / sizeof(store[0]));
    Entry *entry = &store[entry_count++];
    snprintf(entry->key, sizeof(entry->key), "%s", key);
    entry->type = type;
    if (string) snprintf(entry->string, sizeof(entry->string), "%s", string);
    entry->number = number;
}

static Entry *find(const char *key)
{
    for (size_t i = 0; i < entry_count; i++) {
        if (strcmp(store[i].key, key) == 0) return &store[i];
    }
    return NULL;
}

const char *esp_err_to_name(esp_err_t err) { (void)err; return "host error"; }
esp_err_t nvs_flash_init(void) { flash_calls++; return flash_result; }
esp_err_t nvs_flash_erase(void) { erase_calls++; return ESP_OK; }
esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *handle)
{
    assert(strcmp(name, "main") == 0);
    open_calls++;
    if (mode == NVS_READWRITE) write_opens++;
    if (mode == NVS_READWRITE && write_open_result != ESP_OK) return write_open_result;
    if (open_result == ESP_OK) *handle = 1;
    return open_result;
}
void nvs_close(nvs_handle_t handle) { assert(handle == 1); close_calls++; }
esp_err_t nvs_get_stats(const char *name, nvs_stats_t *stats)
{
    (void)name;
    memset(stats, 0, sizeof(*stats));
    return ESP_OK;
}
esp_err_t nvs_find_key(nvs_handle_t handle, const char *key, void *type)
{
    (void)handle; (void)type;
    return find(key) ? ESP_OK : ESP_ERR_NVS_NOT_FOUND;
}
esp_err_t nvs_get_str(nvs_handle_t handle, const char *key, char *out, size_t *length)
{
    (void)handle;
    Entry *entry = find(key);
    if (!entry) return ESP_ERR_NVS_NOT_FOUND;
    if (entry->type != STORE_STRING) return ESP_ERR_NVS_TYPE_MISMATCH;
    size_t needed = strlen(entry->string) + 1;
    if (!out) { *length = needed; return ESP_OK; }
    if (*length < needed) { *length = needed; return ESP_ERR_NVS_INVALID_LENGTH; }
    memcpy(out, entry->string, needed);
    *length = needed;
    return ESP_OK;
}
static esp_err_t get_number(const char *key, StoreType type, uint64_t *out)
{
    Entry *entry = find(key);
    if (!entry) return ESP_ERR_NVS_NOT_FOUND;
    if (entry->type != type) return ESP_ERR_NVS_TYPE_MISMATCH;
    *out = entry->number;
    return ESP_OK;
}
esp_err_t nvs_get_u16(nvs_handle_t handle, const char *key, uint16_t *out)
{
    (void)handle;
    uint64_t value;
    esp_err_t err = get_number(key, STORE_U16, &value);
    if (err == ESP_OK) *out = value;
    return err;
}
esp_err_t nvs_get_i32(nvs_handle_t handle, const char *key, int32_t *out)
{
    (void)handle;
    uint64_t value;
    esp_err_t err = get_number(key, STORE_I32, &value);
    if (err == ESP_OK) *out = (int32_t)value;
    return err;
}
esp_err_t nvs_get_u64(nvs_handle_t handle, const char *key, uint64_t *out)
{
    (void)handle;
    return get_number(key, STORE_U64, out);
}
esp_err_t nvs_set_str(nvs_handle_t h, const char *k, const char *v) { (void)h; (void)v; writes++; snprintf(written_key, sizeof(written_key), "%s", k); return set_result; }
esp_err_t nvs_set_u16(nvs_handle_t h, const char *k, uint16_t v) { (void)h; (void)k; (void)v; writes++; return ESP_OK; }
esp_err_t nvs_set_i32(nvs_handle_t h, const char *k, int32_t v) { (void)h; (void)k; (void)v; writes++; return ESP_OK; }
esp_err_t nvs_set_u64(nvs_handle_t h, const char *k, uint64_t v) { (void)h; (void)k; (void)v; writes++; return ESP_OK; }
esp_err_t nvs_erase_key(nvs_handle_t h, const char *k) { (void)h; (void)k; writes++; return ESP_OK; }
esp_err_t nvs_commit(nvs_handle_t h) { (void)h; commits++; return commit_result; }
QueueHandle_t xQueueCreate(unsigned count, unsigned size) { (void)count; (void)size; return (void *)1; }
BaseType_t xQueueReceive(QueueHandle_t q, void *v, unsigned t) { (void)q; (void)v; (void)t; return 0; }
BaseType_t xQueueSend(QueueHandle_t q, const void *v, unsigned t) { (void)q; (void)v; (void)t; return pdPASS; }
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (void *)1; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, unsigned t) { (void)s; (void)t; return pdPASS; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { (void)s; return pdPASS; }
BaseType_t xTaskCreate(void (*fn)(void *), const char *name, unsigned size, void *arg, unsigned priority, TaskHandle_t *task)
{
    (void)fn; (void)size; (void)arg; (void)priority; (void)task;
    assert(strcmp(name, "nvs_task") == 0);
    tasks++;
    return pdPASS;
}

static void assert_string(NvsConfigKey key, const char *expected)
{
    char *value = nvs_config_get_string(key);
    assert(value != NULL && strcmp(value, expected) == 0);
    free(value);
}

static void assert_untouched(const Entry *before)
{
    assert(memcmp(before, store, sizeof(store)) == 0);
    assert(erase_calls == 0 && writes == 0 && commits == 0);
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    const char *scenario = argv[1];
    const char *board = argv[2];
    if (strcmp(scenario, "missing") != 0) {
        if (strcmp(scenario, "wrong-type") == 0) put("boardversion", STORE_U16, NULL, 601);
        else put("boardversion", STORE_STRING, board, 0);
    }
    // Unknown settings and both old/new values must survive without rewriting.
    put("private-setting", STORE_STRING, "keep exact bytes", 0);
    put("asicvoltage", STORE_U16, NULL, 1173);
    put("autofanspeed", STORE_U16, NULL, 0);
    put("minfanspeed", STORE_U16, NULL, 31);
    put("temptarget", STORE_U16, NULL, 59);
    put("wifissid", STORE_STRING, "pilot-network", 0);
    put("wifipass", STORE_STRING, "test-fixture-password", 0);
    put("stratumurl", STORE_STRING, "pilot.pool.invalid", 0);
    put("stratumuser", STORE_STRING, "", 0);
    put("fbstratumurl", STORE_STRING, "backup.pool.invalid", 0);
    put("displayTimeout", STORE_I32, NULL, (uint64_t)-1);
    put("bestdiff", STORE_U64, NULL, 1234567890123ULL);
    bool legacy = strcmp(scenario, "legacy") == 0;
    put("asicfrequency", STORE_U16, NULL, 650);
    put("fanspeed", STORE_U16, NULL, 73);
    if (legacy) {
        put("stratumprot", STORE_U16, NULL, 1);
        put("fbstratumprot", STORE_U16, NULL, 0);
        put("sv2chantype", STORE_U16, NULL, 1);
        put("fbSv2ChanType", STORE_U16, NULL, 0);
    } else {
        put("asicfrequency_f", STORE_STRING, "625.125", 0);
        put("manualfanspeed", STORE_U16, NULL, 77);
        put("stratumprot", STORE_STRING, "SV1", 0);
        put("fbstratumprot", STORE_STRING, "SV2", 0);
        put("sv2chantype", STORE_STRING, "extended", 0);
    }
    bool saved_decode = strncmp(scenario, "decode-saved-", 13) == 0;
    bool primary_decode = strcmp(scenario, "decode-saved-10") == 0 || strcmp(scenario, "decode-saved-11") == 0;
    bool fallback_decode = strcmp(scenario, "decode-saved-01") == 0 || strcmp(scenario, "decode-saved-11") == 0;
    if (saved_decode) {
        put("stratumdecode", STORE_U16, NULL, primary_decode);
        put("fbstratumdecode", STORE_U16, NULL, fallback_decode);
    }
    if (strcmp(scenario, "schedule-wrong-type") == 0) put("mining_schedule", STORE_U16, NULL, 1);
    if (strcmp(scenario, "schedule-malformed") == 0) put("mining_schedule", STORE_STRING, "not JSON", 0);
    Entry before[64];
    memcpy(before, store, sizeof(store));
    if (strcmp(scenario, "flash-error") == 0) flash_result = (esp_err_t)strtol(board, NULL, 0);
    if (strcmp(scenario, "open-error") == 0) open_result = ESP_ERR_NVS_NOT_FOUND;
    if (strcmp(scenario, "write-open-error") == 0) write_open_result = ESP_FAIL;
    esp_err_t err = nvs_config_init();
    assert_untouched(before);
    assert(flash_calls == 1);
    if (strcmp(scenario, "flash-error") == 0) {
        assert(err == flash_result && open_calls == 0 && tasks == 0);
    } else if (strcmp(scenario, "open-error") == 0) {
        assert(err == open_result && open_calls == 1 && write_opens == 0 && tasks == 0);
    } else if (strcmp(scenario, "write-open-error") == 0) {
        assert(err == write_open_result && open_calls == 2 && write_opens == 1 && tasks == 0 && close_calls == 1);
    } else if (strcmp(scenario, "missing") == 0) {
        assert(err == ESP_ERR_NVS_NOT_FOUND && write_opens == 0 && tasks == 0 && close_calls == 1);
    } else if (strcmp(scenario, "wrong-type") == 0) {
        assert(err == ESP_ERR_NVS_TYPE_MISMATCH && write_opens == 0 && tasks == 0 && close_calls == 1);
    } else if (strcmp(scenario, "unsupported") == 0) {
        assert(err != ESP_OK && write_opens == 0 && tasks == 0 && close_calls == 1);
    } else {
        assert(err == ESP_OK && write_opens == 1 && tasks == 1 && close_calls == 1);
        assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY) == (legacy ? 650.0f : 625.125f));
        assert(nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE) == 1173);
        assert(nvs_config_get_u16(NVS_CONFIG_MANUAL_FAN_SPEED) == (legacy ? 73 : 77));
        assert(!nvs_config_get_bool(NVS_CONFIG_AUTO_FAN_SPEED));
        assert(nvs_config_get_u16(NVS_CONFIG_MIN_FAN_SPEED) == 31);
        assert(nvs_config_get_u16(NVS_CONFIG_TEMP_TARGET) == 59);
        assert(nvs_config_get_i32(NVS_CONFIG_DISPLAY_TIMEOUT) == -1);
        assert(nvs_config_get_u64(NVS_CONFIG_BEST_DIFF) == 1234567890123ULL);
        assert_string(NVS_CONFIG_WIFI_SSID, "pilot-network");
        assert_string(NVS_CONFIG_WIFI_PASS, "test-fixture-password");
        assert_string(NVS_CONFIG_STRATUM_URL, "pilot.pool.invalid");
        assert_string(NVS_CONFIG_STRATUM_USER, "");
        assert_string(NVS_CONFIG_FALLBACK_STRATUM_URL, "backup.pool.invalid");
        assert_string(NVS_CONFIG_STRATUM_PROTOCOL, legacy ? "SV2" : "SV1");
        assert_string(NVS_CONFIG_FALLBACK_STRATUM_PROTOCOL, legacy ? "SV1" : "SV2");
        assert_string(NVS_CONFIG_SV2_CHANNEL_TYPE, legacy ? "standard" : "extended");
        if (legacy) assert_string(NVS_CONFIG_FALLBACK_SV2_CHANNEL_TYPE, "extended");
        assert(nvs_config_get_bool(NVS_CONFIG_STRATUM_DECODE_COINBASE_TX) == primary_decode);
        assert(nvs_config_get_bool(NVS_CONFIG_FALLBACK_STRATUM_DECODE_COINBASE_TX) == fallback_decode);
        GlobalState state = {0};
        assert(device_config_init(&state) == ESP_OK);
        assert(strcmp(state.DEVICE_CONFIG.board_version, board) == 0);
        assert(state.DEVICE_CONFIG.family.id == GAMMA);
        assert(state.DEVICE_CONFIG.family.asic.id == BM1370);
        assert(state.DEVICE_CONFIG.family.asic_count == 1);
        assert(state.DEVICE_CONFIG.EMC2101 && state.DEVICE_CONFIG.TPS546);
        assert(state.DEVICE_CONFIG.power_consumption_target == (strcmp(board, "601") == 0 ? 19 : 22));
        assert_untouched(before);
        if (strncmp(scenario, "schedule-", 9) == 0) {
            char *document = NULL;
            esp_err_t read_error = nvs_config_read_stored_string(NVS_CONFIG_MINING_SCHEDULE, &document);
            if (strcmp(scenario, "schedule-wrong-type") == 0) assert(read_error == ESP_ERR_NVS_TYPE_MISMATCH && document == NULL);
            else if (strcmp(scenario, "schedule-malformed") == 0) assert(read_error == ESP_OK && strcmp(document, "not JSON") == 0);
            else assert(read_error == ESP_ERR_NVS_NOT_FOUND && document == NULL);
            free(document);
            assert_untouched(before);
        }
        if (strncmp(scenario, "schedule-save", 13) == 0) {
            char *old = nvs_config_get_string(NVS_CONFIG_MINING_SCHEDULE);
            if (strcmp(scenario, "schedule-save-failure") == 0) set_result = ESP_FAIL;
            if (strcmp(scenario, "schedule-save-commit-failure") == 0) commit_result = ESP_FAIL;
            const char *document = "{\"enabled\":false,\"timezone\":\"Europe/London\",\"windows\":[]}";
            esp_err_t write_error = nvs_config_set_string_sync(NVS_CONFIG_MINING_SCHEDULE, document);
            assert(writes == 1 && strcmp(written_key, "mining_schedule") == 0 && erase_calls == 0);
            assert(commits == (set_result == ESP_OK ? 1 : 0));
            assert(write_error == ((set_result != ESP_OK || commit_result != ESP_OK) ? ESP_FAIL : ESP_OK));
            assert_string(NVS_CONFIG_MINING_SCHEDULE, write_error == ESP_OK ? document : old);
            assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY) == 625.125f);
            assert(nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE) == 1173);
            free(old);
        }
        if (strcmp(scenario, "profile-guard") == 0) {
            Settings *identity = nvs_config_get_settings(NVS_CONFIG_BOARD_VERSION);
            DeviceConfig previous = state.DEVICE_CONFIG;
            const char *unsupported[] = {"000", "", "600", "603", "650", "800", "801", "601x"};
            for (size_t i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); i++) {
                free(identity->value[0].str);
                identity->value[0].str = strdup(unsupported[i]);
                assert(device_config_init(&state) == ESP_ERR_NOT_SUPPORTED);
                assert(memcmp(&previous, &state.DEVICE_CONFIG, sizeof(previous)) == 0);
            }
            assert_untouched(before);
        }
    }
    puts("PASS");
    return 0;
}
