#include "nvs_config.h"
#include "sv2_protocol.h"
#include "global_state.h"
#include <esp_err.h>
#include "esp_log.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "display.h"
#include "theme_api.h"
#include "scoreboard.h"
#include "5tratumfw.h"

#define NVS_CONFIG_NAMESPACE "main"
#define NVS_STR_LIMIT (4000 - 1) // See nvs_set_str

#ifdef CONFIG_STRATUM_EXTRANONCE_SUBSCRIBE
    #define STRATUM_EXTRANONCE_SUBSCRIBE 1
#else
    #define STRATUM_EXTRANONCE_SUBSCRIBE 0
#endif

#ifdef CONFIG_FALLBACK_STRATUM_EXTRANONCE_SUBSCRIBE
    #define FALLBACK_STRATUM_EXTRANONCE_SUBSCRIBE 1
#else
    #define FALLBACK_STRATUM_EXTRANONCE_SUBSCRIBE 0
#endif

#define FALLBACK_KEY_ASICFREQUENCY "asicfrequency" // Since v2.10.0 (https://github.com/bitaxeorg/ESP-Miner/pull/1051)
#define FALLBACK_KEY_FANSPEED "fanspeed"           // Since v2.11.0 (https://github.com/bitaxeorg/ESP-Miner/pull/1331)

typedef struct {
    NvsConfigKey key;
    ConfigType type;
    ConfigValue value;
    int index;
} ConfigUpdate;

static const char * TAG = "nvs_config";

static QueueHandle_t nvs_save_queue = NULL;
static nvs_handle_t handle;
static SemaphoreHandle_t nvs_cache_mutex = NULL;

static Settings settings[NVS_CONFIG_COUNT] = {
    [NVS_CONFIG_WIFI_SSID]                             = {.nvs_key_name = "wifissid",        .type = TYPE_STR,   .default_value = {.str = (char *)CONFIG_ESP_WIFI_SSID},                .rest_name = "ssid",                               .min = 1,  .max = 32},
    [NVS_CONFIG_WIFI_PASS]                             = {.nvs_key_name = "wifipass",        .type = TYPE_STR,   .default_value = {.str = (char *)CONFIG_ESP_WIFI_PASSWORD},            .rest_name = "wifiPass",                           .min = 0,  .max = 63},
    [NVS_CONFIG_HOSTNAME]                              = {.nvs_key_name = "hostname",        .type = TYPE_STR,   .default_value = {.str = (char *)CONFIG_LWIP_LOCAL_HOSTNAME},          .rest_name = "hostname",                           .min = 1,  .max = 32},

    [NVS_CONFIG_STRATUM_PROTOCOL]                      = {.nvs_key_name = "stratumprot",     .type = TYPE_STR,   .default_value = {.str = STRATUM_V1},                                  .rest_name = "stratumProtocol",                    .min = 3,  .max = 3},
    [NVS_CONFIG_STRATUM_URL]                           = {.nvs_key_name = "stratumurl",      .type = TYPE_STR,   .default_value = {.str = (char *)CONFIG_STRATUM_URL},                  .rest_name = "stratumURL",                         .min = 0,  .max = NVS_STR_LIMIT},
    [NVS_CONFIG_STRATUM_PORT]                          = {.nvs_key_name = "stratumport",     .type = TYPE_U16,   .default_value = {.u16 = CONFIG_STRATUM_PORT},                         .rest_name = "stratumPort",                        .min = 0,  .max = UINT16_MAX},
    [NVS_CONFIG_STRATUM_USER]                          = {.nvs_key_name = "stratumuser",     .type = TYPE_STR,   .default_value = {.str = (char *)CONFIG_STRATUM_USER},                 .rest_name = "stratumUser",                        .min = 0,  .max = NVS_STR_LIMIT},
    [NVS_CONFIG_STRATUM_PASS]                          = {.nvs_key_name = "stratumpass",     .type = TYPE_STR,   .default_value = {.str = (char *)CONFIG_STRATUM_PW},                   .rest_name = "stratumPassword",                    .min = 0,  .max = NVS_STR_LIMIT},
    [NVS_CONFIG_STRATUM_DIFFICULTY]                    = {.nvs_key_name = "stratumdiff",     .type = TYPE_U16,   .default_value = {.u16 = CONFIG_STRATUM_DIFFICULTY},                   .rest_name = "stratumSuggestedDifficulty",         .min = 0,  .max = UINT16_MAX},
    [NVS_CONFIG_STRATUM_EXTRANONCE_SUBSCRIBE]          = {.nvs_key_name = "stratumxnsub",    .type = TYPE_BOOL,  .default_value = {.b   = (bool)STRATUM_EXTRANONCE_SUBSCRIBE},          .rest_name = "stratumExtranonceSubscribe",         .min = 0,  .max = 1},
    [NVS_CONFIG_STRATUM_TLS]                           = {.nvs_key_name = "stratumtls",      .type = TYPE_U16,   .default_value = {.u16 = (uint16_t)CONFIG_STRATUM_TLS},                .rest_name = "stratumTLS",                         .min = 0,  .max = 3},
    [NVS_CONFIG_STRATUM_CERT]                          = {.nvs_key_name = "stratumcert",     .type = TYPE_STR,   .default_value = {.str = (char *)CONFIG_STRATUM_CERT},                 .rest_name = "stratumCert",                        .min = 0,  .max = NVS_STR_LIMIT},
    [NVS_CONFIG_SV2_CHANNEL_TYPE]                      = {.nvs_key_name = "sv2chantype",     .type = TYPE_STR,   .default_value = {.str = SV2_CHANNEL_TYPE_EXTENDED},                   .rest_name = "stratumV2ChannelType",               .min = 8,  .max = 8},
    [NVS_CONFIG_SV2_AUTHORITY_PUBKEY]                  = {.nvs_key_name = "sv2authpubkey",   .type = TYPE_STR,   .default_value = {.str = ""},                                          .rest_name = "stratumV2AuthorityPubkey",           .min = 0,  .max = 52},   
    [NVS_CONFIG_STRATUM_DECODE_COINBASE_TX]            = {.nvs_key_name = "stratumdecode",   .type = TYPE_BOOL,  .default_value = {.b   = false},                                       .rest_name = "stratumDecodeCoinbase",              .min = 0,  .max = 1},
    [NVS_CONFIG_FALLBACK_STRATUM_PROTOCOL]             = {.nvs_key_name = "fbstratumprot",   .type = TYPE_STR,   .default_value = {.str = STRATUM_V1},                                  .rest_name = "fallbackStratumProtocol",            .min = 3,  .max = 3},
    [NVS_CONFIG_FALLBACK_STRATUM_URL]                  = {.nvs_key_name = "fbstratumurl",    .type = TYPE_STR,   .default_value = {.str = (char *)CONFIG_FALLBACK_STRATUM_URL},         .rest_name = "fallbackStratumURL",                 .min = 0,  .max = NVS_STR_LIMIT},
    [NVS_CONFIG_FALLBACK_STRATUM_PORT]                 = {.nvs_key_name = "fbstratumport",   .type = TYPE_U16,   .default_value = {.u16 = CONFIG_FALLBACK_STRATUM_PORT},                .rest_name = "fallbackStratumPort",                .min = 0,  .max = UINT16_MAX},
    [NVS_CONFIG_FALLBACK_STRATUM_USER]                 = {.nvs_key_name = "fbstratumuser",   .type = TYPE_STR,   .default_value = {.str = (char *)CONFIG_FALLBACK_STRATUM_USER},        .rest_name = "fallbackStratumUser",                .min = 0,  .max = NVS_STR_LIMIT},
    [NVS_CONFIG_FALLBACK_STRATUM_PASS]                 = {.nvs_key_name = "fbstratumpass",   .type = TYPE_STR,   .default_value = {.str = (char *)CONFIG_FALLBACK_STRATUM_PW},          .rest_name = "fallbackStratumPassword",            .min = 0,  .max = NVS_STR_LIMIT},
    [NVS_CONFIG_FALLBACK_STRATUM_DIFFICULTY]           = {.nvs_key_name = "fbstratumdiff",   .type = TYPE_U16,   .default_value = {.u16 = CONFIG_FALLBACK_STRATUM_DIFFICULTY},          .rest_name = "fallbackStratumSuggestedDifficulty", .min = 0,  .max = UINT16_MAX},
    [NVS_CONFIG_FALLBACK_STRATUM_EXTRANONCE_SUBSCRIBE] = {.nvs_key_name = "stratumfbxnsub",  .type = TYPE_BOOL,  .default_value = {.b   = (bool)FALLBACK_STRATUM_EXTRANONCE_SUBSCRIBE}, .rest_name = "fallbackStratumExtranonceSubscribe", .min = 0,  .max = 1},
    [NVS_CONFIG_FALLBACK_STRATUM_TLS]                  = {.nvs_key_name = "fbstratumtls",    .type = TYPE_U16,   .default_value = {.u16 = (uint16_t)CONFIG_FALLBACK_STRATUM_TLS},       .rest_name = "fallbackStratumTLS",                 .min = 0,  .max = 3},
    [NVS_CONFIG_FALLBACK_STRATUM_CERT]                 = {.nvs_key_name = "fbstratumcert",   .type = TYPE_STR,   .default_value = {.str = (char *)CONFIG_FALLBACK_STRATUM_CERT},        .rest_name = "fallbackStratumCert",                .min = 0,  .max = NVS_STR_LIMIT},
    [NVS_CONFIG_FALLBACK_SV2_CHANNEL_TYPE]             = {.nvs_key_name = "fbsv2chantype",   .type = TYPE_STR,   .default_value = {.str = SV2_CHANNEL_TYPE_EXTENDED},                   .rest_name = "fallbackStratumV2ChannelType",       .min = 8,  .max = 8},
    [NVS_CONFIG_FALLBACK_SV2_AUTHORITY_PUBKEY]         = {.nvs_key_name = "fbsv2authpubk",   .type = TYPE_STR,   .default_value = {.str = ""},                                          .rest_name = "fallbackStratumV2AuthorityPubkey",   .min = 0,  .max = 52},
    [NVS_CONFIG_FALLBACK_STRATUM_DECODE_COINBASE_TX]   = {.nvs_key_name = "fbstratumdecode", .type = TYPE_BOOL,  .default_value = {.b   = false},                                       .rest_name = "fallbackStratumDecodeCoinbase",      .min = 0,  .max = 1},
    [NVS_CONFIG_USE_FALLBACK_STRATUM]                  = {.nvs_key_name = "usefbstartum",    .type = TYPE_BOOL,                                                                         .rest_name = "useFallbackStratum",                 .min = 0,  .max = 1},

    [NVS_CONFIG_ASIC_FREQUENCY]                        = {.nvs_key_name = "asicfrequency_f", .type = TYPE_FLOAT, .default_value = {.f   = CONFIG_ASIC_FREQUENCY},                       .rest_name = "frequency",                          .min = 1,  .max = UINT16_MAX},
    [NVS_CONFIG_ASIC_VOLTAGE]                          = {.nvs_key_name = "asicvoltage",     .type = TYPE_U16,   .default_value = {.u16 = CONFIG_ASIC_VOLTAGE},                         .rest_name = "coreVoltage",                        .min = 1,  .max = UINT16_MAX},
    [NVS_CONFIG_OVERCLOCK_ENABLED]                     = {.nvs_key_name = "oc_enabled",      .type = TYPE_BOOL,                                                                         .rest_name = "overclockEnabled",                   .min = 0,  .max = 1},
    
    [NVS_CONFIG_DISPLAY]                               = {.nvs_key_name = "display",         .type = TYPE_STR,   .default_value = {.str = DEFAULT_DISPLAY},                             .rest_name = "display",                            .min = 0,  .max = NVS_STR_LIMIT},
    [NVS_CONFIG_ROTATION]                              = {.nvs_key_name = "rotation",        .type = TYPE_U16,                                                                          .rest_name = "rotation",                           .min = 0,  .max = 270},
    [NVS_CONFIG_INVERT_SCREEN]                         = {.nvs_key_name = "invertscreen",    .type = TYPE_BOOL,                                                                         .rest_name = "invertscreen",                       .min = 0,  .max = 1},
    [NVS_CONFIG_DISPLAY_OFFSET]                        = {.nvs_key_name = "displayOffset",   .type = TYPE_U16,   .default_value = {.u16 = LCD_SH1107_PARAM_DEFAULT_DISP_OFFSET },       .rest_name = "displayOffset",                      .min = 0,  .max = UINT8_MAX},
    [NVS_CONFIG_DISPLAY_TIMEOUT]                       = {.nvs_key_name = "displayTimeout",  .type = TYPE_I32,   .default_value = {.i32 = -1},                                          .rest_name = "displayTimeout",                     .min = -1, .max = UINT16_MAX},

    [NVS_CONFIG_AUTO_FAN_SPEED]                        = {.nvs_key_name = "autofanspeed",    .type = TYPE_BOOL,  .default_value = {.b   = true},                                        .rest_name = "autofanspeed",                       .min = 0,  .max = 1},
    [NVS_CONFIG_MANUAL_FAN_SPEED]                      = {.nvs_key_name = "manualfanspeed",  .type = TYPE_U16,   .default_value = {.u16 = 100},                                         .rest_name = "manualFanSpeed",                     .min = 0,  .max = 100},
    [NVS_CONFIG_MIN_FAN_SPEED]                         = {.nvs_key_name = "minfanspeed",     .type = TYPE_U16,   .default_value = {.u16 = 25},                                          .rest_name = "minFanSpeed",                        .min = 0,  .max = 99},
    [NVS_CONFIG_TEMP_TARGET]                           = {.nvs_key_name = "temptarget",      .type = TYPE_U16,   .default_value = {.u16 = 60},                                          .rest_name = "temptarget",                         .min = 35, .max = 66},
    [NVS_CONFIG_OVERHEAT_MODE]                         = {.nvs_key_name = "overheat_mode",   .type = TYPE_BOOL,                                                                         .rest_name = "overheat_mode",                      .min = 0,  .max = 0},

    [NVS_CONFIG_STATISTICS_FREQUENCY]                  = {.nvs_key_name = "statsFrequency",  .type = TYPE_U16,                                                                          .rest_name = "statsFrequency",                     .min = 0,  .max = UINT16_MAX},

    [NVS_CONFIG_BEST_DIFF]                             = {.nvs_key_name = "bestdiff",        .type = TYPE_U64},
    [NVS_CONFIG_SELF_TEST]                             = {.nvs_key_name = "selftest",        .type = TYPE_BOOL},
    [NVS_CONFIG_SWARM]                                 = {.nvs_key_name = "swarmconfig",     .type = TYPE_STR},
    [NVS_CONFIG_THEME_SCHEME]                          = {.nvs_key_name = "themescheme",     .type = TYPE_STR,   .default_value = {.str = DEFAULT_THEME}},
    [NVS_CONFIG_THEME_COLORS]                          = {.nvs_key_name = "themecolors",     .type = TYPE_STR,   .default_value = {.str = DEFAULT_COLORS}},
    [NVS_CONFIG_SCOREBOARD]                            = {.nvs_key_name = "scoreboard",      .type = TYPE_STR,   .array_size = MAX_SCOREBOARD},
    
    [NVS_CONFIG_BOARD_VERSION]                         = {.nvs_key_name = "boardversion",    .type = TYPE_STR,   .default_value = {.str = "000"}},
    [NVS_CONFIG_DEVICE_MODEL]                          = {.nvs_key_name = "devicemodel",     .type = TYPE_STR,   .default_value = {.str = "unknown"}},
    [NVS_CONFIG_ASIC_MODEL]                            = {.nvs_key_name = "asicmodel",       .type = TYPE_STR,   .default_value = {.str = "unknown"}},
    [NVS_CONFIG_PLUG_SENSE]                            = {.nvs_key_name = "plug_sense",      .type = TYPE_BOOL},
    [NVS_CONFIG_ASIC_ENABLE]                           = {.nvs_key_name = "asic_enable",     .type = TYPE_BOOL},
    [NVS_CONFIG_EMC2101]                               = {.nvs_key_name = "EMC2101",         .type = TYPE_BOOL},
    [NVS_CONFIG_EMC2103]                               = {.nvs_key_name = "EMC2103",         .type = TYPE_BOOL},
    [NVS_CONFIG_EMC2302]                               = {.nvs_key_name = "EMC2302",         .type = TYPE_BOOL},
    [NVS_CONFIG_EMC_INTERNAL_TEMP]                     = {.nvs_key_name = "emc_int_temp",    .type = TYPE_BOOL},
    [NVS_CONFIG_EMC_IDEALITY_FACTOR]                   = {.nvs_key_name = "emc_ideality_f",  .type = TYPE_U16},
    [NVS_CONFIG_EMC_BETA_COMPENSATION]                 = {.nvs_key_name = "emc_beta_comp",   .type = TYPE_U16},
    [NVS_CONFIG_TEMP_OFFSET]                           = {.nvs_key_name = "temp_offset",     .type = TYPE_I32},
    [NVS_CONFIG_DS4432U]                               = {.nvs_key_name = "DS4432U",         .type = TYPE_BOOL},
    [NVS_CONFIG_INA260]                                = {.nvs_key_name = "INA260",          .type = TYPE_BOOL},
    [NVS_CONFIG_TPS546]                                = {.nvs_key_name = "TPS546",          .type = TYPE_BOOL},
    [NVS_CONFIG_TMP1075]                               = {.nvs_key_name = "TMP1075",         .type = TYPE_BOOL},
    [NVS_CONFIG_POWER_CONSUMPTION_TARGET]              = {.nvs_key_name = "power_cons_tgt",  .type = TYPE_U16},
    [NVS_CONFIG_SELF_TEST_TEMP_TARGET]                 = {.nvs_key_name = "selftest_temp",   .type = TYPE_U16,   .default_value = {.u16 = 65}},
    [NVS_CONFIG_SELF_TEST_TEMP_WARMUP]                 = {.nvs_key_name = "selftest_warm",   .type = TYPE_U16,   .default_value = {.u16 = 55}},
    [NVS_CONFIG_SELF_TEST_TEMP_MAX]                    = {.nvs_key_name = "selftest_max",    .type = TYPE_U16,   .default_value = {.u16 = 70}},
    [NVS_CONFIG_MINING_SCHEDULE]                       = {.nvs_key_name = "mining_schedule", .type = TYPE_STR,   .default_value = {.str = "{\"enabled\":false,\"timezone\":\"UTC\",\"windows\":[]}"}},
};

Settings *nvs_config_get_settings(NvsConfigKey key)
{
    if (key < 0 || key >= NVS_CONFIG_COUNT) {
        ESP_LOGE(TAG, "Invalid key enum %d", key);
        return NULL;
    }
    return &settings[key];
}

static int get_array_size(const Settings * setting)
{
    return (setting->array_size > 0) ? setting->array_size : 1;
}

static void get_nvs_key_name(const Settings * setting, const int index, char dest[static NVS_KEY_NAME_MAX_SIZE])
{
    if (setting->array_size > 0) {
        int width = 1;
        for (int t = setting->array_size - 1; t >= 10 && width < 5; t /= 10) width++;
        snprintf(dest, NVS_KEY_NAME_MAX_SIZE, "%s_%0*d", setting->nvs_key_name, width, index + 1);
    } else {
        snprintf(dest, NVS_KEY_NAME_MAX_SIZE, "%s", setting->nvs_key_name);
    }
}

// Interpret older keys in RAM only. An OTA boot must not migrate or erase NVS.
// User changes still use the existing save task and downgrade-compatible keys.
static bool nvs_config_load_legacy(NvsConfigKey key, const Settings *setting, ConfigValue *value)
{
    uint16_t val;
    if (key == NVS_CONFIG_ASIC_FREQUENCY) {
        if (nvs_find_key(handle, setting->nvs_key_name, NULL) == ESP_ERR_NVS_NOT_FOUND &&
            nvs_get_u16(handle, FALLBACK_KEY_ASICFREQUENCY, &val) == ESP_OK) {
            value->f = val;
            return true;
        }
    }
    if (key == NVS_CONFIG_MANUAL_FAN_SPEED) {
        if (nvs_find_key(handle, setting->nvs_key_name, NULL) == ESP_ERR_NVS_NOT_FOUND &&
            nvs_get_u16(handle, FALLBACK_KEY_FANSPEED, &val) == ESP_OK) {
            value->u16 = val;
            return true;
        }
    }
    if (key == NVS_CONFIG_STRATUM_PROTOCOL || key == NVS_CONFIG_FALLBACK_STRATUM_PROTOCOL) {
        if (nvs_get_u16(handle, setting->nvs_key_name, &val) == ESP_OK) {
            value->str = strdup((val == 1) ? STRATUM_V2 : STRATUM_V1);
            return value->str != NULL;
        }
    }
    if (key == NVS_CONFIG_SV2_CHANNEL_TYPE || key == NVS_CONFIG_FALLBACK_SV2_CHANNEL_TYPE) {
        esp_err_t res = nvs_get_u16(handle, setting->nvs_key_name, &val);
        if (res == ESP_ERR_NVS_NOT_FOUND) {
            res = nvs_get_u16(handle, "fbSv2ChanType", &val);
        }
        if (res == ESP_OK) {
            value->str = strdup((val == 1) ? SV2_CHANNEL_TYPE_STANDARD : SV2_CHANNEL_TYPE_EXTENDED);
            return value->str != NULL;
        }
    }
    return false;
}


// Operating settings journal v1. This is the authoritative bounded snapshot
// once activated; original legacy keys remain intact for recovery/downgrade.
#define OPERATING_JOURNAL_KEY "5fw_operating"
#define OPERATING_JOURNAL_MAX 32768
#define OPERATING_JOURNAL_COUNT (NVS_CONFIG_USE_FALLBACK_STRATUM - NVS_CONFIG_STRATUM_PROTOCOL + 4)
static bool operating_journal_active;

static bool operating_key(NvsConfigKey key)
{
    return (key >= NVS_CONFIG_STRATUM_PROTOCOL && key <= NVS_CONFIG_USE_FALLBACK_STRATUM) ||
           key == NVS_CONFIG_ASIC_FREQUENCY || key == NVS_CONFIG_ASIC_VOLTAGE || key == NVS_CONFIG_OVERCLOCK_ENABLED;
}

static void put16(uint8_t *p, uint16_t n) { p[0] = n; p[1] = n >> 8; }
static uint16_t get16(const uint8_t *p) { return p[0] | ((uint16_t)p[1] << 8); }
static void put32(uint8_t *p, uint32_t n) { for (int i = 0; i < 4; i++) p[i] = n >> (8 * i); }
static uint32_t get32(const uint8_t *p) { return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint32_t journal_checksum(const uint8_t *p, size_t size)
{
    uint32_t value = 2166136261u;
    for (size_t i = 0; i < size; i++) value = (value ^ p[i]) * 16777619u;
    return value;
}

static bool mutation_valid(const NvsConfigMutation *entry)
{
    Settings *setting = nvs_config_get_settings(entry->key);
    if (!operating_key(entry->key) || !setting || entry->type != setting->type) return false;
    switch (entry->type) {
    case TYPE_STR: return entry->value.str && strlen(entry->value.str) <= NVS_STR_LIMIT;
    case TYPE_FLOAT: return isfinite(entry->value.f) && entry->value.f >= 1 && entry->value.f <= UINT16_MAX;
    case TYPE_U16: return entry->value.u16 >= setting->min && entry->value.u16 <= setting->max;
    case TYPE_BOOL: return true;
    default: return false;
    }
}

static uint8_t *journal_encode(const NvsConfigMutation *entries, size_t count, size_t *size)
{
    size_t length = 12;
    for (size_t i = 0; i < count; i++) {
        if (!mutation_valid(&entries[i])) return NULL;
        length += 5 + (entries[i].type == TYPE_STR ? strlen(entries[i].value.str) + 1 : entries[i].type == TYPE_FLOAT ? 4 : 2);
    }
    if (count != OPERATING_JOURNAL_COUNT || length > OPERATING_JOURNAL_MAX) return NULL;
    uint8_t *blob = malloc(length);
    if (!blob) return NULL;
    memcpy(blob, "5FWJ", 4); blob[4] = 1; blob[5] = count; put16(blob + 6, length);
    size_t offset = 12;
    for (size_t i = 0; i < count; i++) {
        const NvsConfigMutation *entry = &entries[i];
        size_t bytes = entry->type == TYPE_STR ? strlen(entry->value.str) + 1 : entry->type == TYPE_FLOAT ? 4 : 2;
        put16(blob + offset, entry->key); blob[offset + 2] = entry->type; put16(blob + offset + 3, bytes); offset += 5;
        if (entry->type == TYPE_STR) memcpy(blob + offset, entry->value.str, bytes);
        else if (entry->type == TYPE_FLOAT) { uint32_t bits; memcpy(&bits, &entry->value.f, 4); put32(blob + offset, bits); }
        else put16(blob + offset, entry->type == TYPE_BOOL ? (entry->value.b ? 1 : 0) : entry->value.u16);
        offset += bytes;
    }
    put32(blob + 8, journal_checksum(blob + 12, length - 12)); *size = length;
    return blob;
}

static void mutations_free(NvsConfigMutation *entries, size_t count)
{
    for (size_t i = 0; i < count; i++) if (entries[i].type == TYPE_STR) free(entries[i].value.str);
}

static esp_err_t journal_decode(const uint8_t *blob, size_t size, NvsConfigMutation *entries)
{
    if (size < 12 || size > OPERATING_JOURNAL_MAX || memcmp(blob, "5FWJ", 4) || blob[4] != 1 ||
        blob[5] != OPERATING_JOURNAL_COUNT || get16(blob + 6) != size || get32(blob + 8) != journal_checksum(blob + 12, size - 12)) return ESP_ERR_INVALID_STATE;
    memset(entries, 0, sizeof(*entries) * OPERATING_JOURNAL_COUNT);
    bool seen[NVS_CONFIG_COUNT] = {0}; size_t offset = 12, read = 0;
    esp_err_t error = ESP_ERR_INVALID_STATE;
    while (read < OPERATING_JOURNAL_COUNT) {
        if (offset + 5 > size) goto failed;
        NvsConfigMutation *entry = &entries[read];
        entry->key = get16(blob + offset); entry->type = blob[offset + 2];
        size_t bytes = get16(blob + offset + 3); offset += 5;
        if (!operating_key(entry->key) || entry->key >= NVS_CONFIG_COUNT || seen[entry->key] || offset + bytes > size) goto failed;
        seen[entry->key] = true;
        if (entry->type == TYPE_STR) {
            if (!bytes || bytes > NVS_STR_LIMIT + 1 || blob[offset + bytes - 1] || memchr(blob + offset, 0, bytes - 1)) goto failed;
            entry->value.str = malloc(bytes);
            if (!entry->value.str) { error = ESP_ERR_NO_MEM; goto failed; }
            memcpy(entry->value.str, blob + offset, bytes);
        } else if (entry->type == TYPE_FLOAT && bytes == 4) { uint32_t bits = get32(blob + offset); memcpy(&entry->value.f, &bits, 4); }
        else if ((entry->type == TYPE_U16 || entry->type == TYPE_BOOL) && bytes == 2) {
            uint16_t value = get16(blob + offset);
            if (entry->type == TYPE_BOOL) { if (value > 1) goto failed; entry->value.b = value != 0; }
            else entry->value.u16 = value;
        } else goto failed;
        read++; offset += bytes;
        if (!mutation_valid(entry)) goto failed;
    }
    if (offset != size) goto failed;
    return ESP_OK;
failed:
    mutations_free(entries, read + (read < OPERATING_JOURNAL_COUNT && entries[read].type == TYPE_STR && entries[read].value.str ? 1 : 0));
    return error;
}

static void journal_install(NvsConfigMutation *entries)
{
    for (size_t i = 0; i < OPERATING_JOURNAL_COUNT; i++) {
        Settings *setting = &settings[entries[i].key];
        if (setting->type == TYPE_STR) free(setting->value[0].str);
        setting->value[0] = entries[i].value;
    }
}

static esp_err_t journal_load(void)
{
    size_t size = 0;
    esp_err_t error = nvs_get_blob(handle, OPERATING_JOURNAL_KEY, NULL, &size);
    if (error == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (error != ESP_OK || size < 12 || size > OPERATING_JOURNAL_MAX) return ESP_ERR_INVALID_STATE;
    uint8_t *blob = malloc(size);
    if (!blob) return ESP_ERR_NO_MEM;
    error = nvs_get_blob(handle, OPERATING_JOURNAL_KEY, blob, &size);
    NvsConfigMutation entries[OPERATING_JOURNAL_COUNT];
    if (error == ESP_OK) error = journal_decode(blob, size, entries);
    free(blob);
    if (error == ESP_OK) { journal_install(entries); operating_journal_active = true; }
    return error;
}

esp_err_t nvs_config_apply_atomic(const NvsConfigMutation *changes, size_t count)
{
    if (!changes || !count || count > OPERATING_JOURNAL_COUNT || !nvs_cache_mutex) return ESP_ERR_INVALID_ARG;
    for (size_t i = 0; i < count; i++) {
        if (!mutation_valid(&changes[i])) return ESP_ERR_INVALID_ARG;
        for (size_t j = 0; j < i; j++) if (changes[i].key == changes[j].key) return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(nvs_cache_mutex, portMAX_DELAY);
    NvsConfigMutation before[OPERATING_JOURNAL_COUNT], after[OPERATING_JOURNAL_COUNT], decoded[OPERATING_JOURNAL_COUNT];
    size_t index = 0;
    for (NvsConfigKey key = 0; key < NVS_CONFIG_COUNT; key++) if (operating_key(key)) {
        before[index] = (NvsConfigMutation){.key = key, .type = settings[key].type, .value = settings[key].value[0]};
        after[index] = before[index];
        for (size_t i = 0; i < count; i++) if (changes[i].key == key) after[index] = changes[i];
        index++;
    }
    size_t old_size = 0, new_size = 0;
    uint8_t *old_blob = journal_encode(before, index, &old_size), *new_blob = journal_encode(after, index, &new_size);
    esp_err_t error = ESP_ERR_NO_MEM;
    bool prepared = false;
    if (old_blob && new_blob) { error = journal_decode(new_blob, new_size, decoded); prepared = error == ESP_OK; }
    nvs_handle_t writer;
    if (prepared) {
        error = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READWRITE, &writer);
        if (error == ESP_OK) {
            error = nvs_set_blob(writer, OPERATING_JOURNAL_KEY, new_blob, new_size);
            if (error == ESP_OK) {
                error = nvs_commit(writer);
                if (error != ESP_OK) {
                    // Retain the previous authoritative snapshot on a commit
                    // error too. Never erase the partition or touch identity.
                    if (nvs_set_blob(writer, OPERATING_JOURNAL_KEY, old_blob, old_size) == ESP_OK && nvs_commit(writer) == ESP_OK) operating_journal_active = true;
                }
            }
            nvs_close(writer);
        }
    }
    if (error == ESP_OK) { journal_install(decoded); operating_journal_active = true; }
    else if (prepared) mutations_free(decoded, OPERATING_JOURNAL_COUNT);
    free(old_blob); free(new_blob);
    xSemaphoreGive(nvs_cache_mutex);
    return error;
}

static void nvs_config_apply_fallback(NvsConfigKey key, Settings * setting)
{
    if (key == NVS_CONFIG_ASIC_FREQUENCY) {
        nvs_set_u16(handle, FALLBACK_KEY_ASICFREQUENCY, (uint16_t) setting->value[0].f);
    }
    if (key == NVS_CONFIG_MANUAL_FAN_SPEED) {
        nvs_set_u16(handle, FALLBACK_KEY_FANSPEED, setting->value[0].u16);
    }
}

static void nvs_task(void *pvParameters)
{
    while (1) {
        ConfigUpdate update;
        if (xQueueReceive(nvs_save_queue, &update, portMAX_DELAY) == pdTRUE) {
            Settings *setting = nvs_config_get_settings(update.key);
            if (setting && setting->type == update.type) {
                if (operating_key(update.key) && operating_journal_active && update.index == 0) {
                    NvsConfigMutation mutation = {.key = update.key, .type = update.type, .value = update.value};
                    if (nvs_config_apply_atomic(&mutation, 1) != ESP_OK) ESP_LOGE(TAG, "Operating settings commit failed; prior values retained");
                    if (update.type == TYPE_STR) free(update.value.str);
                    continue;
                }
                esp_err_t ret = ESP_OK;

                char key[NVS_KEY_NAME_MAX_SIZE];
                get_nvs_key_name(setting, update.index, key);

                // NVS flash write is AFTER releasing the mutex so getters are never blocked
                char *old_str = NULL;
                char nvs_str_buf[32]; // for TYPE_FLOAT serialisation
                xSemaphoreTake(nvs_cache_mutex, portMAX_DELAY);
                switch (update.type) {
                    case TYPE_STR:
                        old_str = setting->value[update.index].str;
                        setting->value[update.index].str = update.value.str;
                        break;
                    case TYPE_U16:
                        setting->value[update.index].u16 = update.value.u16;
                        break;
                    case TYPE_I32:
                        setting->value[update.index].i32 = update.value.i32;
                        break;
                    case TYPE_U64:
                        setting->value[update.index].u64 = update.value.u64;
                        break;
                    case TYPE_FLOAT:
                        setting->value[update.index].f = update.value.f;
                        snprintf(nvs_str_buf, sizeof(nvs_str_buf), "%f", update.value.f);
                        break;
                    case TYPE_BOOL:
                        setting->value[update.index].b = update.value.b;
                        break;
                }
                xSemaphoreGive(nvs_cache_mutex);

                switch (update.type) {
                    case TYPE_STR:
                        ret = nvs_set_str(handle, key, update.value.str);
                        break;
                    case TYPE_U16:
                        ret = nvs_set_u16(handle, key, update.value.u16);
                        break;
                    case TYPE_I32:
                        ret = nvs_set_i32(handle, key, update.value.i32);
                        break;
                    case TYPE_U64:
                        ret = nvs_set_u64(handle, key, update.value.u64);
                        break;
                    case TYPE_FLOAT:
                        ret = nvs_set_str(handle, key, nvs_str_buf);
                        break;
                    case TYPE_BOOL:
                        ret = nvs_set_u16(handle, key, update.value.b ? 1 : 0);
                        break;
                }

                nvs_config_apply_fallback(update.key, setting);

                if (ret == ESP_OK) {
                    ret = nvs_commit(handle);
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to commit data to NVS");
                    }
                }
                if (old_str) free(old_str);
            } 
            else if (update.type == TYPE_STR) {
                free(update.value.str);
            }
        }
    }
}

esp_err_t nvs_config_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed (%s); preserving flash and stopping boot", esp_err_to_name(err));
        return err;
    }

    // Read the real stored identity before loading any defaults or starting a writer.
    err = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Stored board identity is unavailable (%s)", esp_err_to_name(err));
        return err;
    }
    char board_version[4] = {0};
    size_t board_version_len = sizeof(board_version);
    err = nvs_get_str(handle, settings[NVS_CONFIG_BOARD_VERSION].nvs_key_name,
                      board_version, &board_version_len);
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Stored board identity cannot be read (%s)", esp_err_to_name(err));
        return err;
    }
    if (!five_stratum_fw_board_supported(board_version)) {
        ESP_LOGE(TAG, "This " FIVE_STRATUM_FW_NAME " pilot requires stored board 601 or 602");
        return ESP_ERR_NOT_SUPPORTED;
    }

    err = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not open nvs");
        return err;
    }
        
    nvs_stats_t stats;
    err = nvs_get_stats(NULL, &stats);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Used entries: %lu", stats.used_entries);
        ESP_LOGI(TAG, "Free entries: %lu", stats.free_entries);
        ESP_LOGI(TAG, "Available entries: %lu", stats.available_entries);
        ESP_LOGI(TAG, "Total entries: %lu", stats.total_entries);
    } else {
        ESP_LOGE(TAG, "Error getting NVS stats: %s\n", esp_err_to_name(err));
    }

    // Load all
    for (NvsConfigKey key = 0; key < NVS_CONFIG_COUNT; key++) {
        Settings *setting = &settings[key];

        esp_err_t ret;

        int count = get_array_size(setting);
        setting->value = calloc(count, sizeof(ConfigValue));
        if (setting->value == NULL) {
            ESP_LOGE(TAG, "Failed to allocate settings cache");
            return ESP_ERR_NO_MEM;
        }

        for (int idx = 0; idx < count; idx++) {
            char nvs_key[NVS_KEY_NAME_MAX_SIZE];
            get_nvs_key_name(setting, idx, nvs_key);

            if (idx == 0 && nvs_config_load_legacy(key, setting, &setting->value[idx])) {
                continue;
            }
            switch (setting->type) {
                case TYPE_STR: {
                    size_t len = 0;
                    esp_err_t ret = nvs_get_str(handle, nvs_key, NULL, &len);
                    if (ret == ESP_OK && len > 0) {
                        char *buf = malloc(len);
                        if (buf) {
                            ret = nvs_get_str(handle, nvs_key, buf, &len);
                            if (ret == ESP_OK) {
                                setting->value[idx].str = buf;
                                break;
                            }
                            free(buf);
                        }
                    }

                    const char *def = setting->default_value.str ? setting->default_value.str : "";
                    setting->value[idx].str = strdup(def);
                    if (setting->value[idx].str == NULL) {
                        return ESP_ERR_NO_MEM;
                    }
                    break;
                }
                case TYPE_U16: {
                    uint16_t val;
                    ret = nvs_get_u16(handle, nvs_key, &val);
                    setting->value[idx].u16 = (ret == ESP_OK) ? val : setting->default_value.u16;
                    break;
                }
                case TYPE_I32: {
                    int32_t val;
                    ret = nvs_get_i32(handle, nvs_key, &val);
                    setting->value[idx].i32 = (ret == ESP_OK) ? val : setting->default_value.i32;
                    break;
                }
                case TYPE_U64: {
                    uint64_t val;
                    ret = nvs_get_u64(handle, nvs_key, &val);
                    setting->value[idx].u64 = (ret == ESP_OK) ? val : setting->default_value.u64;
                    break;
                }
                case TYPE_FLOAT: {
                    char buf[32];
                    size_t len = sizeof(buf);
                    ret = nvs_get_str(handle, nvs_key, buf, &len);
                    if (ret == ESP_OK) {
                        char *end;
                        float parsed = strtof(buf, &end);
                        if (end != buf && *end == '\0') {
                            setting->value[idx].f = parsed;
                        } else {
                            ESP_LOGW(TAG, "Corrupt float in NVS for %s ('%s'), using default", setting->nvs_key_name, buf);
                            setting->value[idx].f = setting->default_value.f;
                        }
                    } else {
                        setting->value[idx].f = setting->default_value.f;
                    }
                    break;
                }
                case TYPE_BOOL: {
                    uint16_t val;
                    ret = nvs_get_u16(handle, nvs_key, &val);
                    setting->value[idx].b = (ret == ESP_OK) ? (val != 0) : setting->default_value.b;
                    break;
                }
            }
        }
    }

    // Replay only after the persisted board identity was validated, before any
    // hardware is initialized. Corrupt journals stop boot without defaults.
    err = journal_load();
    if (err != ESP_OK) return err;

    nvs_save_queue = xQueueCreate(20, sizeof(ConfigUpdate));
    if (nvs_save_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create nvs_save_queue");
        return ESP_ERR_NO_MEM;
    }

    nvs_cache_mutex = xSemaphoreCreateMutex();
    if (!nvs_cache_mutex) {
        ESP_LOGE(TAG, "Failed to create nvs_cache_mutex");
        return ESP_FAIL;
    }

    TaskHandle_t task_handle;

    // nvs_task heap _must_ be internal memory
    BaseType_t task_result = xTaskCreate(nvs_task, "nvs_task", 8192, NULL, 5, &task_handle); 
    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create nvs_task");

        return ESP_FAIL;
    }
    return ESP_OK;
}

char *nvs_config_get_string(NvsConfigKey key)
{
    Settings *setting = nvs_config_get_settings(key);
    if (!setting) {
        ESP_LOGE(TAG, "Invalid key %d", key);
        return NULL;
    }
    if (setting->type != TYPE_STR || setting->array_size > 1) {
        ESP_LOGE(TAG, "Wrong type for %s (str)", setting->nvs_key_name);
        return NULL;
    }
    xSemaphoreTake(nvs_cache_mutex, portMAX_DELAY);
    char *result = strdup(setting->value[0].str);
    xSemaphoreGive(nvs_cache_mutex);
    return result;
}

char *nvs_config_get_string_indexed(NvsConfigKey key, int index)
{
    Settings *setting = nvs_config_get_settings(key);
    if (!setting) {
        ESP_LOGE(TAG, "Invalid key %d", key);
        return NULL;
    }
    if (setting->type != TYPE_STR || setting->array_size < 1) {
        ESP_LOGE(TAG, "Wrong type for %s (indexed str)", setting->nvs_key_name);
        return NULL;
    }
    if (index < 0 || index >= setting->array_size) {
        ESP_LOGE(TAG, "Index out of bounds for key %s (%d)", setting->nvs_key_name, index);
        return NULL;
    }
    xSemaphoreTake(nvs_cache_mutex, portMAX_DELAY);
    char *result = strdup(setting->value[index].str);
    xSemaphoreGive(nvs_cache_mutex);
    return result;
}

void nvs_config_set_string(NvsConfigKey key, const char *value)
{
    Settings *setting = nvs_config_get_settings(key);
    if (!setting || setting->type != TYPE_STR || (setting->value[0].str && strcmp(setting->value[0].str, value) == 0)) return;

    ConfigUpdate update = { .key = key, .type = TYPE_STR, .value.str = strdup(value) };
    if (!update.value.str) return;
    xQueueSend(nvs_save_queue, &update, portMAX_DELAY);
}

esp_err_t nvs_config_read_stored_string(NvsConfigKey key, char **value)
{
    Settings *setting = nvs_config_get_settings(key);
    if (key != NVS_CONFIG_MINING_SCHEDULE || setting == NULL || value == NULL) return ESP_ERR_INVALID_ARG;
    *value = NULL;
    nvs_handle_t reader;
    esp_err_t err = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READONLY, &reader);
    if (err != ESP_OK) return err == ESP_ERR_NVS_NOT_FOUND ? ESP_ERR_INVALID_STATE : err;
    size_t length = 0;
    err = nvs_get_str(reader, setting->nvs_key_name, NULL, &length);
    if (err == ESP_OK) {
        if (length == 0 || length > 2048) err = ESP_ERR_INVALID_SIZE;
        else {
            *value = malloc(length);
            err = *value ? nvs_get_str(reader, setting->nvs_key_name, *value, &length) : ESP_ERR_NO_MEM;
        }
    }
    nvs_close(reader);
    if (err != ESP_OK) { free(*value); *value = NULL; }
    return err;
}

esp_err_t nvs_config_set_string_sync(NvsConfigKey key, const char *value)
{
    Settings *setting = nvs_config_get_settings(key);
    if (key != NVS_CONFIG_MINING_SCHEDULE || setting == NULL || value == NULL) return ESP_ERR_INVALID_ARG;
    char *copy = strdup(value);
    if (copy == NULL) return ESP_ERR_NO_MEM;
    nvs_handle_t writer;
    esp_err_t err = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READWRITE, &writer);
    if (err == ESP_OK) {
        err = nvs_set_str(writer, setting->nvs_key_name, copy);
        if (err == ESP_OK) err = nvs_commit(writer);
        nvs_close(writer);
    }
    if (err == ESP_OK) {
        xSemaphoreTake(nvs_cache_mutex, portMAX_DELAY);
        char *old = setting->value[0].str;
        setting->value[0].str = copy;
        xSemaphoreGive(nvs_cache_mutex);
        free(old);
    } else free(copy);
    return err;
}

void nvs_config_set_string_indexed(NvsConfigKey key, int index, const char *value)
{
    Settings *setting = nvs_config_get_settings(key);
    if (!setting || setting->type != TYPE_STR || setting->array_size < 1) return;
    if (index < 0 || index >= setting->array_size) return;
    if (setting->value[index].str && strcmp(setting->value[index].str, value) == 0) return;

    ConfigUpdate update = { .key = key, .type = TYPE_STR, .value.str = strdup(value), .index = index };
    if (!update.value.str) return;
    xQueueSend(nvs_save_queue, &update, portMAX_DELAY);
}

uint16_t nvs_config_get_u16(NvsConfigKey key)
{
    Settings *setting = nvs_config_get_settings(key);
    if (!setting) {
        ESP_LOGE(TAG, "Invalid key %d", key);
        return 0;
    }
    if (setting->type != TYPE_U16) {
        ESP_LOGE(TAG, "Wrong type for %s (u16)", setting->nvs_key_name);
        return 0;
    }
    xSemaphoreTake(nvs_cache_mutex, portMAX_DELAY);
    uint16_t result = setting->value[0].u16;
    xSemaphoreGive(nvs_cache_mutex);
    return result;
}

void nvs_config_set_u16(NvsConfigKey key, uint16_t value)
{
    Settings *setting = nvs_config_get_settings(key);
    if (!setting || setting->type != TYPE_U16 || setting->value[0].u16 == value) return;

    ConfigUpdate update = { .key = key, .type = TYPE_U16, .value.u16 = value };
    xQueueSend(nvs_save_queue, &update, portMAX_DELAY);
}

int32_t nvs_config_get_i32(NvsConfigKey key)
{
    Settings *setting = nvs_config_get_settings(key);
    if (!setting) {
        ESP_LOGE(TAG, "Invalid key %d", key);
        return 0;
    }
    if (setting->type != TYPE_I32) {
        ESP_LOGE(TAG, "Wrong type for %s (i32)", setting->nvs_key_name);
        return 0;
    }
    xSemaphoreTake(nvs_cache_mutex, portMAX_DELAY);
    int32_t result = setting->value[0].i32;
    xSemaphoreGive(nvs_cache_mutex);
    return result;
}

void nvs_config_set_i32(NvsConfigKey key, int32_t value)
{
    Settings *setting = nvs_config_get_settings(key);
    if (!setting || setting->type != TYPE_I32 || setting->value[0].i32 == value) return;

    ConfigUpdate update = { .key = key, .type = TYPE_I32, .value.i32 = value };
    xQueueSend(nvs_save_queue, &update, portMAX_DELAY);
}

uint64_t nvs_config_get_u64(NvsConfigKey key)
{
    Settings *setting = nvs_config_get_settings(key);
    if (!setting) {
        ESP_LOGE(TAG, "Invalid key %d", key);
        return 0;
    }
    if (setting->type != TYPE_U64) {
        ESP_LOGE(TAG, "Wrong type for %s (u64)", setting->nvs_key_name);
        return 0;
    }
    xSemaphoreTake(nvs_cache_mutex, portMAX_DELAY);
    uint64_t result = setting->value[0].u64;
    xSemaphoreGive(nvs_cache_mutex);
    return result;
}

void nvs_config_set_u64(NvsConfigKey key, uint64_t value)
{
    Settings *setting = nvs_config_get_settings(key);
    if (!setting || setting->type != TYPE_U64 || setting->value[0].u64 == value) return;

    ConfigUpdate update = { .key = key, .type = TYPE_U64, .value.u64 = value };
    xQueueSend(nvs_save_queue, &update, portMAX_DELAY);
}

float nvs_config_get_float(NvsConfigKey key)
{
    Settings *setting = nvs_config_get_settings(key);
    if (!setting) {
        ESP_LOGE(TAG, "Invalid key %d", key);
        return 0;
    }
    if (setting->type != TYPE_FLOAT) {
        ESP_LOGE(TAG, "Wrong type for %s (float)", setting->nvs_key_name);
        return 0;
    }
    xSemaphoreTake(nvs_cache_mutex, portMAX_DELAY);
    float result = setting->value[0].f;
    xSemaphoreGive(nvs_cache_mutex);
    return result;
}

void nvs_config_set_float(NvsConfigKey key, float value)
{
    Settings *setting = nvs_config_get_settings(key);
    if (!setting || setting->type != TYPE_FLOAT || fabsf(setting->value[0].f - value) < 0.001f) return;

    ConfigUpdate update = { .key = key, .type = TYPE_FLOAT, .value.f = value };
    xQueueSend(nvs_save_queue, &update, portMAX_DELAY);
}

bool nvs_config_get_bool(NvsConfigKey key)
{
    Settings *setting = nvs_config_get_settings(key);
    if (!setting) {
        ESP_LOGE(TAG, "Invalid key %d", key);
        return false;
    }
    if (setting->type != TYPE_BOOL) {
        ESP_LOGE(TAG, "Wrong type for %s (bool)", setting->nvs_key_name);
        return false;
    }
    xSemaphoreTake(nvs_cache_mutex, portMAX_DELAY);
    bool result = setting->value[0].b;
    xSemaphoreGive(nvs_cache_mutex);
    return result;
}

void nvs_config_set_bool(NvsConfigKey key, bool value)
{
    Settings *setting = nvs_config_get_settings(key);
    if (!setting || setting->type != TYPE_BOOL || setting->value[0].b == value) return;

    ConfigUpdate update = { .key = key, .type = TYPE_BOOL, .value.b = value };
    xQueueSend(nvs_save_queue, &update, portMAX_DELAY);
}
