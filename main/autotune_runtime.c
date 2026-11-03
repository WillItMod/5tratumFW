/* Original Gamma adapter for the shared 5tratumFW adaptive controller. */
#include "autotune_runtime.h"
#include "autotune_api.h"
#include "../firmware/nerdqaxe/main/autotune/core.h"
#include "global_state.h"
#include "nvs_config.h"
#include "mining_state.h"
#include "tasks/hashrate_monitor_task.h"
#include "power/TPS546.h"
#include "thermal/EMC2101.h"
#include "http_server/http_server.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "nvs.h"
#include "serial.h"
#include "driver/uart.h"
#include <stdatomic.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#define TUNE_NAMESPACE "5fw_tune"
#define TUNE_BODY_MAX 2048
#define TUNE_POLL_US UINT64_C(2000000)
#define TUNE_FRESH_US UINT64_C(15000000)
#define BACKUP_FIELDS 6
static const NvsConfigKey backup_keys[BACKUP_FIELDS] = {NVS_CONFIG_ASIC_FREQUENCY,
    NVS_CONFIG_ASIC_VOLTAGE, NVS_CONFIG_AUTO_FAN_SPEED, NVS_CONFIG_MANUAL_FAN_SPEED,
    NVS_CONFIG_MIN_FAN_SPEED, NVS_CONFIG_TEMP_TARGET};
static const char *backup_names[BACKUP_FIELDS] = {"frequencyMHz", "coreVoltageMv",
    "autoFanSpeed", "manualFanSpeed", "minFanSpeed", "fanTargetTempC"};

typedef struct {
    GlobalState *device;
    bool initialized, supported, storage_ok, backup_available, backup_restored;
    bool restore_pending, restore_committed, confirming, commanded, frozen;
    bool rail_wait, rail_commanded;
    uint64_t observer_until, last_poll, sensor_at, strict_success_at, applied_at, generation;
    uint64_t actuation_started, rail_sample_at;
    uint32_t owner_epoch, tx_before;
    unsigned confirmations, backup_matches, rail_confirmations;
    float rail_frequency;
    uint64_t backup_match_at;
    uint64_t last_owner_tick;
    char mac[13], board[8];
    fwt_controller core;
    fwt_history history;
    fwt_input input;
    TPS546_StrictTelemetry strict;
    bool sensor_ok;
    NvsConfigMutation backup[BACKUP_FIELDS];
    fwt_point target;
    const char *override_reason;
} GammaTuner;
static GammaTuner tuner;
static pthread_mutex_t tuner_mutex = PTHREAD_MUTEX_INITIALIZER;
/* Never hold this gate while taking the tuner/NVS mutex, or across a ramp or
 * delay. Only one bounded UART/PMBus frame and revocation's atomic fence. */
static pthread_mutex_t wire_gate = PTHREAD_MUTEX_INITIALIZER;
static atomic_uint_least32_t manual_epoch;
static atomic_bool transition_live;
static atomic_uint_least32_t transition_epoch;

void autotune_gamma_manual_override(void) {
    pthread_mutex_lock(&wire_gate);
    atomic_fetch_add(&manual_epoch,1);
    pthread_mutex_unlock(&wire_gate);
}
int autotune_gamma_wire_begin(bool safety_power_off) {
    if (!atomic_load(&transition_live)) return 1;
    pthread_mutex_lock(&wire_gate);
    if (!safety_power_off && atomic_load(&transition_live) &&
        atomic_load(&transition_epoch) != atomic_load(&manual_epoch)) {
        pthread_mutex_unlock(&wire_gate); return 0;
    }
    return 2;
}
void autotune_gamma_wire_end(int token) {if (token == 2) pthread_mutex_unlock(&wire_gate);}
void autotune_gamma_owner_idle(void) {
    /* Called only by the serialized power owner after the old transition has
     * returned or its rail-off/reset/flush sequence completed. */
    atomic_store(&transition_live,false);
}
bool autotune_gamma_transition_cancelled(void) {
    return atomic_load(&transition_live) && atomic_load(&transition_epoch) != atomic_load(&manual_epoch);
}
bool autotune_gamma_transition_allowed(void *opaque) {
    if (!atomic_load(&transition_live)) return true;
    GlobalState *state = opaque;
    if (autotune_gamma_transition_cancelled() || !state || !mining_state_work_allowed(state) ||
        state->SYSTEM_MODULE.overheat_mode) return false;
    TPS546_StrictTelemetry sample = {0}; float temperature = 0; uint16_t rpm = 0;
    if (TPS546_read_strict(&sample) != ESP_OK || EMC2101_read_strict(&temperature,&rpm) != ESP_OK ||
        rpm <= 100 || sample.status_vout || sample.status_iout || sample.status_input ||
        sample.status_temperature || sample.status_cml || sample.iout <= 0 || sample.iout >= 25 || sample.vin < 4.5 || sample.vin > 6.5 ||
        temperature >= 75 || sample.temperature_c >= 105 ||
        fabs(sample.vout*1000-state->POWER_MANAGEMENT_MODULE.applied_core_voltage_mv) > 30) return false;
    pthread_mutex_lock(&tuner_mutex);
    const bool reducing = tuner.target.frequencyMHz <= state->POWER_MANAGEMENT_MODULE.actual_frequency+2 &&
        tuner.target.coreVoltageMv <= state->POWER_MANAGEMENT_MODULE.applied_core_voltage_mv;
    const bool restoring = tuner.restore_pending;
    const bool headroom = temperature < (restoring ? 70 : tuner.core.config.maxAsicTempC) &&
        sample.temperature_c < (restoring ? 95 : tuner.core.config.maxVrmTempC) &&
        sample.vout*sample.iout+5 < (restoring ? 40 : tuner.core.config.maxPowerW);
    pthread_mutex_unlock(&tuner_mutex);
    return !autotune_gamma_transition_cancelled() && (reducing || headroom);
}

static bool exact_fields(const cJSON *object, const char *const *names, unsigned count) {
    if (!cJSON_IsObject(object)) return false;
    unsigned found = 0;
    for (const cJSON *item = object->child; item; item = item->next) {
        bool known = false;
        if (!item->string) return false;
        for (unsigned i = 0; i < count; ++i) known |= !strcmp(item->string, names[i]);
        if (!known) return false;
        for (const cJSON *next = item->next; next; next = next->next)
            if (next->string && !strcmp(item->string, next->string)) return false;
        ++found;
    }
    return found == count;
}
static bool number(const cJSON *object, const char *name, double *value) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble)) return false;
    *value = item->valuedouble;
    return true;
}
static bool parse_config(const cJSON *object, fwt_config *config) {
    const char *const names[] = {"priority", "continuous", "minFrequencyMHz", "maxFrequencyMHz",
        "minVoltageMv", "maxVoltageMv", "maxAsicTempC", "maxVrmTempC", "maxPowerW"};
    if (!exact_fields(object, names, 9)) return false;
    memset(config, 0, sizeof(*config));
    const cJSON *priority = cJSON_GetObjectItemCaseSensitive(object, "priority");
    const cJSON *continuous = cJSON_GetObjectItemCaseSensitive(object, "continuous");
    if (!cJSON_IsString(priority) || !cJSON_IsBool(continuous)) return false;
    if (!strcmp(priority->valuestring, "efficiency")) config->priority = FWT_EFFICIENCY;
    else if (!strcmp(priority->valuestring, "balanced")) config->priority = FWT_BALANCED;
    else if (!strcmp(priority->valuestring, "performance")) config->priority = FWT_PERFORMANCE;
    else return false;
    config->continuous = cJSON_IsTrue(continuous);
    return number(object,"minFrequencyMHz",&config->minFrequencyMHz) &&
        number(object,"maxFrequencyMHz",&config->maxFrequencyMHz) &&
        number(object,"minVoltageMv",&config->minVoltageMv) && number(object,"maxVoltageMv",&config->maxVoltageMv) &&
        number(object,"maxAsicTempC",&config->maxAsicTempC) && number(object,"maxVrmTempC",&config->maxVrmTempC) &&
        number(object,"maxPowerW",&config->maxPowerW) &&
        floor(config->minVoltageMv) == config->minVoltageMv && floor(config->maxVoltageMv) == config->maxVoltageMv &&
        fwt_config_valid(config,&tuner.core.limits);
}
static cJSON *config_json(const fwt_config *config) {
    cJSON *result = cJSON_CreateObject();
    if (!result) return NULL;
    if (!cJSON_AddStringToObject(result,"priority",fwt_priority_name(config->priority)) ||
        !cJSON_AddBoolToObject(result,"continuous",config->continuous) ||
        !cJSON_AddNumberToObject(result,"minFrequencyMHz",config->minFrequencyMHz) ||
        !cJSON_AddNumberToObject(result,"maxFrequencyMHz",config->maxFrequencyMHz) ||
        !cJSON_AddNumberToObject(result,"minVoltageMv",config->minVoltageMv) ||
        !cJSON_AddNumberToObject(result,"maxVoltageMv",config->maxVoltageMv) ||
        !cJSON_AddNumberToObject(result,"maxAsicTempC",config->maxAsicTempC) ||
        !cJSON_AddNumberToObject(result,"maxVrmTempC",config->maxVrmTempC) ||
        !cJSON_AddNumberToObject(result,"maxPowerW",config->maxPowerW)) {cJSON_Delete(result); return NULL;}
    return result;
}
static bool binding_valid(const cJSON *json) {
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(json,"schemaVersion");
    const cJSON *mac = cJSON_GetObjectItemCaseSensitive(json,"mac");
    const cJSON *board = cJSON_GetObjectItemCaseSensitive(json,"board");
    return cJSON_IsNumber(version) && version->valuedouble == 1 && cJSON_IsString(mac) &&
        cJSON_IsString(board) && !strcmp(mac->valuestring,tuner.mac) && !strcmp(board->valuestring,tuner.board);
}
static cJSON *bound_json(void) {
    cJSON *result = cJSON_CreateObject();
    if (!result || !cJSON_AddNumberToObject(result,"schemaVersion",1) ||
        !cJSON_AddStringToObject(result,"mac",tuner.mac) || !cJSON_AddStringToObject(result,"board",tuner.board)) {
        cJSON_Delete(result); return NULL;
    }
    return result;
}
static esp_err_t read_record(const char *key, cJSON **record) {
    *record = NULL;
    nvs_handle_t handle;
    esp_err_t err = nvs_open(TUNE_NAMESPACE,NVS_READONLY,&handle);
    if (err != ESP_OK) return err;
    size_t length = 0;
    err = nvs_get_blob(handle,key,NULL,&length);
    if (err != ESP_OK) {nvs_close(handle); return err;}
    if (length < 2 || length > TUNE_BODY_MAX) {nvs_close(handle); return ESP_ERR_INVALID_STATE;}
    char *bytes = malloc(length);
    if (!bytes) {nvs_close(handle); return ESP_ERR_NO_MEM;}
    err = nvs_get_blob(handle,key,bytes,&length);
    nvs_close(handle);
    if (err == ESP_OK && bytes[length-1] == 0 && strlen(bytes)+1 == length)
        *record = cJSON_ParseWithOpts(bytes,NULL,true);
    free(bytes);
    return err == ESP_OK && !*record ? ESP_ERR_INVALID_STATE : err;
}
static esp_err_t write_record(const char *key, const cJSON *record) {
    char *bytes = cJSON_PrintUnformatted(record);
    if (!bytes) return ESP_ERR_NO_MEM;
    const size_t length = strlen(bytes)+1;
    if (length > TUNE_BODY_MAX) {free(bytes); return ESP_ERR_INVALID_SIZE;}
    nvs_handle_t handle;
    esp_err_t err = nvs_open(TUNE_NAMESPACE,NVS_READWRITE,&handle);
    if (err == ESP_OK) {
        err = nvs_set_blob(handle,key,bytes,length);
        if (err == ESP_OK) err = nvs_commit(handle);
        nvs_close(handle);
    }
    /* Reopen and compare the durable bytes; an acknowledgement alone cannot
     * authorize the first automatic setting change. */
    if (err == ESP_OK) {
        err = nvs_open(TUNE_NAMESPACE,NVS_READONLY,&handle);
        if (err == ESP_OK) {
            char *verify = malloc(length);
            size_t size = length;
            if (!verify) err = ESP_ERR_NO_MEM;
            else {
                err = nvs_get_blob(handle,key,verify,&size);
                if (err == ESP_OK && (size != length || memcmp(bytes,verify,length))) err = ESP_ERR_INVALID_STATE;
                free(verify);
            }
            nvs_close(handle);
        }
    }
    free(bytes);
    return err;
}
static bool parse_backup(const cJSON *json) {
    const char *const keys[]={"schemaVersion","mac","board","settings"};
    if (!exact_fields(json,keys,4) || !binding_valid(json)) return false;
    const cJSON *fields = cJSON_GetObjectItemCaseSensitive(json,"settings");
    if (!exact_fields(fields,backup_names,BACKUP_FIELDS)) return false;
    NvsConfigMutation values[BACKUP_FIELDS] = {0};
    for (unsigned i = 0; i < BACKUP_FIELDS; ++i) {
        values[i].key = backup_keys[i];
        const cJSON *item = cJSON_GetObjectItemCaseSensitive(fields,backup_names[i]);
        if (i == 2) {
            if (!cJSON_IsBool(item)) return false;
            values[i].type = TYPE_BOOL; values[i].value.b = cJSON_IsTrue(item);
        } else {
            double value;
            if (!number(fields,backup_names[i],&value)) return false;
            if (i == 0) {
                if (value < tuner.core.limits.minFrequencyMHz || value > tuner.core.limits.maxFrequencyMHz) return false;
                values[i].type = TYPE_FLOAT; values[i].value.f = value;
            } else {
                const double minimum = i == 1 ? tuner.core.limits.minVoltageMv : i == 5 ? 35 : 0;
                const double maximum = i == 1 ? tuner.core.limits.maxVoltageMv : i == 3 ? 100 : i == 4 ? 99 : 66;
                if (value < minimum || value > maximum || floor(value) != value) return false;
                values[i].type = TYPE_U16; values[i].value.u16 = value;
            }
        }
    }
    memcpy(tuner.backup,values,sizeof(values));
    tuner.backup_available = true;
    return true;
}
static bool ensure_backup(void) {
    const bool previously_verified=tuner.backup_available;
    NvsConfigMutation original[BACKUP_FIELDS];
    memcpy(original,tuner.backup,sizeof(original));
    cJSON *record = NULL;
    esp_err_t existing = read_record("backup",&record);
    if (existing != ESP_ERR_NVS_NOT_FOUND) {
        bool valid = existing == ESP_OK && parse_backup(record);
        for(unsigned i=0;valid && previously_verified && i<BACKUP_FIELDS;++i)
            valid = tuner.backup[i].type == original[i].type &&
                (i == 0 ? tuner.backup[i].value.f == original[i].value.f :
                 i == 2 ? tuner.backup[i].value.b == original[i].value.b :
                          tuner.backup[i].value.u16 == original[i].value.u16);
        if (valid) valid=write_record("backup",record)==ESP_OK;
        if (!valid && previously_verified) {
            memcpy(tuner.backup,original,sizeof(original));
            tuner.backup_available=true;
        }
        cJSON_Delete(record);return valid;
    }
    /* A previously observed original cannot silently become a new snapshot
     * after deletion/corruption while this runtime is still alive. */
    if (previously_verified) return false;
    NvsConfigMutation values[BACKUP_FIELDS] = {0};
    if (!nvs_config_read_atomic(backup_keys,values,BACKUP_FIELDS)) return false;
    if (fabs(values[0].value.f-tuner.input.current.frequencyMHz) > 0.01 ||
        values[1].value.u16 != tuner.input.current.coreVoltageMv ||
        fabs(tuner.input.voltageReadbackMv-values[1].value.u16) > 30) return false;
    record = bound_json();
    cJSON *fields = record ? cJSON_AddObjectToObject(record,"settings") : NULL;
    bool valid = fields != NULL;
    for (unsigned i = 0; valid && i < BACKUP_FIELDS; ++i)
        valid = (i == 2 ? cJSON_AddBoolToObject(fields,backup_names[i],values[i].value.b) :
            cJSON_AddNumberToObject(fields,backup_names[i],i == 0 ? values[i].value.f : values[i].value.u16)) != NULL;
    /* Validate the complete would-be backup before it can become durable. */
    if (valid) valid = parse_backup(record);
    if (valid) {
        tuner.backup_available = false;
        valid = write_record("backup",record) == ESP_OK && parse_backup(record);
    }
    cJSON_Delete(record);
    return valid;
}

void autotune_gamma_init(GlobalState *state) {
    pthread_mutex_lock(&tuner_mutex);
    if (tuner.initialized) {pthread_mutex_unlock(&tuner_mutex); return;}
    memset(&tuner,0,sizeof(tuner));
    tuner.initialized = true; tuner.device = state; tuner.storage_ok = true;
    tuner.generation = 1;
    uint8_t mac[6];
    const char *board = state ? state->DEVICE_CONFIG.board_version : NULL;
    tuner.supported = state && board && (!strcmp(board,"601") || !strcmp(board,"602")) &&
        state->DEVICE_CONFIG.TPS546 && state->DEVICE_CONFIG.EMC2101 &&
        state->DEVICE_CONFIG.family.asic_count == 1 && state->DEVICE_CONFIG.family.asic.id == BM1370 &&
        state->DEVICE_CONFIG.family.asic.hash_domains == 4 && state->DEVICE_CONFIG.family.voltage_domains == 1 &&
        esp_read_mac(mac,ESP_MAC_WIFI_STA) == ESP_OK;
    if (tuner.supported) {
        snprintf(tuner.board,sizeof(tuner.board),"%s",board);
        for (unsigned i=0;i<6;++i) snprintf(tuner.mac+i*2,3,"%02x",mac[i]);
    }
    uint16_t mv = state ? nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE) : 1150;
    double mhz = state ? nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY) : 525;
    const fwt_limits limits = {.minFrequencyMHz=400,.maxFrequencyMHz=690,
        .minVoltageMv=1000,.maxVoltageMv=1250,.maxAsicTempC=70,.maxVrmTempC=95,
        .maxPowerW=40,.frequencyStepMHz=6.25,.voltageStepMv=5};
    const fwt_config config = {.priority=FWT_BALANCED,.continuous=false,
        .minFrequencyMHz=400,.maxFrequencyMHz=mhz,.minVoltageMv=1000,.maxVoltageMv=mv,
        .maxAsicTempC=60,.maxVrmTempC=70,.maxPowerW=state ? state->DEVICE_CONFIG.power_consumption_target : 22};
    fwt_init(&tuner.core,&limits,&config);
    if (tuner.supported) {
        cJSON *record = NULL;
        esp_err_t err = read_record("config",&record);
        if (err == ESP_OK) {
            fwt_config stored;
            const char *const keys[]={"schemaVersion","mac","board","config"};
            tuner.storage_ok = exact_fields(record,keys,4) && binding_valid(record) &&
                parse_config(cJSON_GetObjectItemCaseSensitive(record,"config"),&stored);
            if (tuner.storage_ok) fwt_init(&tuner.core,&limits,&stored);
        } else if (err != ESP_ERR_NVS_NOT_FOUND) tuner.storage_ok = false;
        cJSON_Delete(record); record = NULL;
        err = read_record("backup",&record);
        if (err == ESP_OK) tuner.storage_ok = parse_backup(record) && tuner.storage_ok;
        else if (err != ESP_ERR_NVS_NOT_FOUND) tuner.storage_ok = false;
        cJSON_Delete(record);
    }
    pthread_mutex_unlock(&tuner_mutex);
}

static void poll_input(GlobalState *state, uint64_t now) {
    const bool wanted = tuner.core.owns || tuner.restore_pending || now < tuner.observer_until;
    if (wanted && (!tuner.last_poll || now - tuner.last_poll >= TUNE_POLL_US)) {
        tuner.last_poll = now;
        float temperature = 0; uint16_t rpm = 0;
        TPS546_StrictTelemetry strict = {0};
        const uint64_t began = esp_timer_get_time();
        tuner.sensor_ok = TPS546_read_strict(&strict) == ESP_OK &&
            EMC2101_read_strict(&temperature,&rpm) == ESP_OK;
        tuner.sensor_at = began;
        if (tuner.sensor_ok) {
            tuner.strict = strict;
            tuner.strict_success_at = began;
            tuner.input.asicTempC = temperature;
            tuner.input.vrmTempC = strict.temperature_c;
            tuner.input.voltageReadbackMv = strict.vout * 1000.0;
            tuner.input.powerW = strict.vout * strict.iout + 5.0;
            tuner.input.fanOk = rpm > 100;
        }
    }
    now = esp_timer_get_time();
    GammaAutotuneCounterSample counters = hashrate_monitor_autotune_snapshot(state,now);
    fwt_input *input = &tuner.input;
    input->capturedAtUs = tuner.sensor_at < counters.captured_at_us ? tuner.sensor_at : counters.captured_at_us;
    input->sampleId = counters.sample_id;
    input->generation = tuner.generation;
    input->hashRateGHs = counters.useful_ghs;
    input->errorPercentage = input->worstErrorPercentage = counters.error_percentage;
    input->current = (fwt_point){state->POWER_MANAGEMENT_MODULE.frequency_value,
        state->POWER_MANAGEMENT_MODULE.applied_core_voltage_mv};
    input->miningActive = mining_state_work_allowed(state) && !state->SELF_TEST_MODULE.is_active &&
        !state->SYSTEM_MODULE.is_firmware_update;
    input->fault = state->SYSTEM_MODULE.hardware_fault || state->SYSTEM_MODULE.overheat_mode ||
        (tuner.sensor_ok && (tuner.strict.status_vout || tuner.strict.status_iout || tuner.strict.status_input ||
            tuner.strict.status_temperature || tuner.strict.status_cml || tuner.strict.iout >= 25.0));
    input->valid = tuner.sensor_ok && counters.valid && input->capturedAtUs && now >= input->capturedAtUs &&
        now-input->capturedAtUs <= TUNE_FRESH_US && tuner.strict.vin >= 4.5 && tuner.strict.vin <= 6.5 &&
        tuner.strict.iout > 0 &&
        isfinite(input->asicTempC) && input->asicTempC > 0 && input->asicTempC < 125;
}

static bool backup_matches_live(GlobalState *state,uint64_t now) {
    if (!tuner.backup_available || !fwt_input_valid(&tuner.input,now) ||
        !tuner.input.miningActive || tuner.input.fault || !tuner.input.fanOk ||
        fabs(tuner.input.current.frequencyMHz-tuner.backup[0].value.f) > 0.01 ||
        tuner.input.current.coreVoltageMv != tuner.backup[1].value.u16 ||
        !isfinite(state->POWER_MANAGEMENT_MODULE.actual_frequency) ||
        fabs(state->POWER_MANAGEMENT_MODULE.actual_frequency-tuner.backup[0].value.f) > 2) return false;
    NvsConfigMutation current[BACKUP_FIELDS]={0};
    if (!nvs_config_read_atomic(backup_keys,current,BACKUP_FIELDS)) return false;
    for (unsigned i=0;i<BACKUP_FIELDS;++i) {
        if (current[i].type != tuner.backup[i].type ||
            (i == 0 ? current[i].value.f != tuner.backup[i].value.f :
             i == 2 ? current[i].value.b != tuner.backup[i].value.b :
                      current[i].value.u16 != tuner.backup[i].value.u16)) return false;
    }
    return true;
}

bool autotune_gamma_select_target(GlobalState *state, uint16_t *mv, float *mhz) {
    if (!tuner.initialized || !tuner.supported || !mv || !mhz) return false;
    pthread_mutex_lock(&tuner_mutex);
    uint64_t now = esp_timer_get_time();
    tuner.last_owner_tick = now;
    const uint32_t epoch = atomic_load(&manual_epoch);
    if (atomic_load(&transition_live) && atomic_load(&transition_epoch) != epoch)
        autotune_gamma_owner_idle();
    if (tuner.frozen) {
        *mv=state->POWER_MANAGEMENT_MODULE.applied_core_voltage_mv;
        *mhz=state->POWER_MANAGEMENT_MODULE.frequency_value;
        pthread_mutex_unlock(&tuner_mutex); return true;
    }
    if ((tuner.core.owns || tuner.restore_pending) && epoch != tuner.owner_epoch) {
        fwt_cancel(&tuner.core,"manual-control");
        tuner.restore_pending = tuner.confirming = tuner.commanded = tuner.rail_wait = false;
        tuner.override_reason = "manual-control";
        atomic_store(&transition_live,false);
        /* The owner may have read its normal target before the manual
         * transaction completed and revoked this epoch.  Hand control back
         * using the latest atomic pair, never that older saved baseline. */
        if (!nvs_config_get_tuning_pair(mv,mhz)) {
            *mv=state->POWER_MANAGEMENT_MODULE.applied_core_voltage_mv;
            *mhz=state->POWER_MANAGEMENT_MODULE.frequency_value;
            tuner.override_reason="manual-settings-snapshot-unavailable";
            pthread_mutex_unlock(&tuner_mutex); return true;
        }
    }
    poll_input(state,now);
    now = esp_timer_get_time();
    if (!tuner.core.owns && !tuner.restore_pending) {
        if (!backup_matches_live(state,now)) {
            tuner.backup_matches=0;tuner.backup_restored=false;
        } else if (tuner.sensor_at != tuner.backup_match_at) {
            tuner.backup_match_at=tuner.sensor_at;
            if (tuner.backup_matches<2) ++tuner.backup_matches;
            if (tuner.backup_matches>=2) tuner.backup_restored=true;
        }
    }
    bool owned = tuner.core.owns;
    if (tuner.restore_pending) {
        tuner.core.state = FWT_DISABLED;
        if (tuner.restore_committed && (now < tuner.actuation_started ||
            now-tuner.actuation_started > FWT_APPLY_TIMEOUT_US)) {
            tuner.override_reason="restore-transition-timed-out";
            pthread_mutex_unlock(&tuner_mutex);
            POWER_MANAGEMENT_stop_for_fault(state,"Original tuning settings could not be verified within 30 seconds");
            return false;
        }
        if (!tuner.input.valid || !tuner.input.miningActive || tuner.input.fault || !tuner.input.fanOk ||
            tuner.input.asicTempC >= 70 || tuner.input.vrmTempC >= 95) {
            tuner.override_reason = "restore-pending-safe-operating-state";
            /* Retain the live point instead of falling through to the saved
             * baseline, which might now be unsafe to restore. */
            *mv = state->POWER_MANAGEMENT_MODULE.applied_core_voltage_mv;
            *mhz = state->POWER_MANAGEMENT_MODULE.frequency_value;
            atomic_store(&transition_live,false);
            pthread_mutex_unlock(&tuner_mutex); return true;
        }
        if (!tuner.restore_committed) {
            if (nvs_config_apply_autotune_restore(tuner.backup,BACKUP_FIELDS) != ESP_OK) {
                tuner.override_reason = "restore-storage-failed";
                *mv=state->POWER_MANAGEMENT_MODULE.applied_core_voltage_mv;
                *mhz=state->POWER_MANAGEMENT_MODULE.frequency_value;
                atomic_store(&transition_live,false);
                pthread_mutex_unlock(&tuner_mutex); return true;
            }
            tuner.restore_committed = true;
            tuner.target = (fwt_point){tuner.backup[0].value.f,tuner.backup[1].value.u16};
            tuner.commanded = false;
            tuner.tx_before = SERIAL_tx_failure_count();
            tuner.actuation_started=now;
            /* A coupled restore can raise both V and F. Keep the lower clock
             * until two new strict regulator readings confirm the higher rail;
             * a normally settling output is not an actuation failure. */
            tuner.rail_wait=tuner.target.frequencyMHz > state->POWER_MANAGEMENT_MODULE.actual_frequency+2 &&
                (tuner.target.coreVoltageMv > state->POWER_MANAGEMENT_MODULE.applied_core_voltage_mv ||
                 fabs(tuner.input.voltageReadbackMv-tuner.target.coreVoltageMv)>30);
            tuner.rail_commanded=false;tuner.rail_confirmations=0;
            tuner.rail_frequency=state->POWER_MANAGEMENT_MODULE.frequency_value;
            tuner.rail_sample_at=tuner.sensor_at;
        }
        owned = true;
    } else if (tuner.core.owns) {
        fwt_request request = fwt_tick(&tuner.core,&tuner.input,now);
        fwt_history_capture(&tuner.history,&tuner.core);
        if (request.kind == FWT_REQUEST_PAUSE) {
            tuner.override_reason = tuner.core.reason;
            pthread_mutex_unlock(&tuner_mutex);
            POWER_MANAGEMENT_stop_for_fault(state,"Auto-tuning stopped: sensor, cooling or operating-point fault");
            return false;
        }
        if (request.kind == FWT_REQUEST_APPLY) {
            tuner.target = request.point; tuner.commanded = false; tuner.confirming = false;
            tuner.tx_before = SERIAL_tx_failure_count();
        } else if (!tuner.core.awaitingApply) tuner.target = tuner.core.current;
        owned = tuner.core.owns;
    }
    if (owned && tuner.rail_wait) {
        if (tuner.rail_commanded && tuner.sensor_at > tuner.rail_sample_at) {
            const bool confirmed=tuner.sensor_ok && tuner.input.fanOk && !tuner.input.fault &&
                fabs(tuner.input.voltageReadbackMv-tuner.target.coreVoltageMv) <= 30;
            tuner.rail_confirmations=confirmed?tuner.rail_confirmations+1:0;
            tuner.rail_sample_at=tuner.sensor_at;
        }
        if (tuner.rail_confirmations < 2) {
            *mv=(uint16_t)lround(tuner.target.coreVoltageMv);*mhz=tuner.rail_frequency;
            atomic_store(&transition_epoch,tuner.owner_epoch);atomic_store(&transition_live,true);
            tuner.override_reason="waiting-for-original-voltage-readback";
            pthread_mutex_unlock(&tuner_mutex);return true;
        }
        tuner.rail_wait=false;tuner.commanded=tuner.confirming=false;
        tuner.override_reason="restoring-original-clock";
    }
    if (tuner.confirming && tuner.sensor_at > tuner.applied_at && tuner.sensor_ok) {
        const bool confirmed = SERIAL_tx_failure_count() == tuner.tx_before &&
            fabs(tuner.input.voltageReadbackMv-tuner.target.coreVoltageMv) <= 30 &&
            isfinite(state->POWER_MANAGEMENT_MODULE.actual_frequency) &&
            fabs(state->POWER_MANAGEMENT_MODULE.actual_frequency-tuner.target.frequencyMHz) <= 2.0;
        if (confirmed) ++tuner.confirmations; else tuner.confirmations = 0;
        tuner.applied_at = tuner.sensor_at;
        if (tuner.confirmations >= 2) {
            tuner.confirming = false;
            if (tuner.restore_pending) {
                NvsConfigMutation current[BACKUP_FIELDS] = {0};
                bool equal = nvs_config_read_atomic(backup_keys,current,BACKUP_FIELDS);
                for (unsigned i=0;equal && i<BACKUP_FIELDS;++i)
                    equal = current[i].type == tuner.backup[i].type &&
                        (i == 0 ? current[i].value.f == tuner.backup[i].value.f :
                         i == 2 ? current[i].value.b == tuner.backup[i].value.b :
                                  current[i].value.u16 == tuner.backup[i].value.u16);
                tuner.backup_restored = equal;
                tuner.restore_pending = !equal;
                tuner.override_reason = equal ? "backup-restored" : "restore-readback-failed";
                owned = false; atomic_store(&transition_live,false);
            } else {
                fwt_applied(&tuner.core,true,now);
                fwt_history_capture(&tuner.history,&tuner.core);
            }
        }
    }
    if (owned) {
        *mv = (uint16_t)lround(tuner.target.coreVoltageMv);
        *mhz = tuner.target.frequencyMHz;
        atomic_store(&transition_epoch,tuner.owner_epoch);
        atomic_store(&transition_live,true);
    } else atomic_store(&transition_live,false);
    pthread_mutex_unlock(&tuner_mutex);
    return owned;
}

bool autotune_gamma_transition_finished(GlobalState *state, bool success) {
    if (!tuner.initialized) return success;
    pthread_mutex_lock(&tuner_mutex);
    if (tuner.rail_wait) {
        success=success && !autotune_gamma_transition_cancelled() &&
            SERIAL_tx_failure_count()==tuner.tx_before &&
            uart_wait_tx_done(UART_NUM_1,pdMS_TO_TICKS(1000))==ESP_OK;
        if (success && !tuner.rail_commanded) {
            tuner.rail_commanded=true;tuner.rail_sample_at=tuner.sensor_at;
            ++tuner.generation;hashrate_monitor_reset_measurements(state);
        }
        pthread_mutex_unlock(&tuner_mutex);return success;
    }
    if (!tuner.commanded && (tuner.core.awaitingApply || (tuner.restore_pending && tuner.restore_committed))) {
        const uint64_t now = esp_timer_get_time();
        success = success && !autotune_gamma_transition_cancelled() &&
            SERIAL_tx_failure_count() == tuner.tx_before &&
            uart_wait_tx_done(UART_NUM_1,pdMS_TO_TICKS(1000)) == ESP_OK;
        if (!success) {
            if (tuner.core.awaitingApply) fwt_applied(&tuner.core,false,now);
            fwt_history_capture(&tuner.history,&tuner.core);
            tuner.override_reason = "transition-failed";
        } else {
            tuner.commanded = true; tuner.confirming = true;
            tuner.confirmations = 0; tuner.applied_at = now;
            ++tuner.generation;
            hashrate_monitor_reset_measurements(state);
        }
    }
    pthread_mutex_unlock(&tuner_mutex);
    return success && !autotune_gamma_transition_cancelled();
}
void autotune_gamma_interrupt(const char *reason) {
    if (!reason) reason="interrupted";
    autotune_gamma_manual_override();
    if (!tuner.initialized) return;
    pthread_mutex_lock(&tuner_mutex);
    fwt_cancel(&tuner.core,reason);
    if (!strcmp(reason,"firmware-update") || !strcmp(reason,"restart")) tuner.frozen=true;
    tuner.override_reason = reason;
    tuner.confirming = tuner.commanded = tuner.restore_pending = tuner.rail_wait = false;
    /* Keep the old wire epoch armed until the power owner acknowledges it.
     * Otherwise a preflight-approved old command could run after revocation. */
    pthread_mutex_unlock(&tuner_mutex);
}

static bool add_point(cJSON *object, const char *name, bool available, fwt_point point) {
    if (!available) return cJSON_AddNullToObject(object,name) != NULL;
    cJSON *item = cJSON_AddObjectToObject(object,name);
    return item && cJSON_AddNumberToObject(item,"frequencyMHz",point.frequencyMHz) &&
        cJSON_AddNumberToObject(item,"coreVoltageMv",point.coreVoltageMv);
}
static const char *inactive_fault_reason(void) {
    if (tuner.core.active) return NULL;
    if (tuner.device && (tuner.device->SYSTEM_MODULE.hardware_fault || tuner.device->SYSTEM_MODULE.overheat_mode))
        return "Miner hardware protection is active; tuning is disabled";
    if (!tuner.input.fault || !tuner.sensor_ok) return NULL;
    if (tuner.strict.status_vout) return "Voltage regulator reports an output-voltage warning or fault; tuning is disabled";
    if (tuner.strict.status_iout) return "Voltage regulator reports an output-current warning or fault; tuning is disabled";
    if (tuner.strict.status_input) return "Voltage regulator reports an input-power warning or fault; tuning is disabled";
    if (tuner.strict.status_temperature) return "Voltage regulator reports a temperature warning or fault; tuning is disabled";
    if (tuner.strict.status_cml) return "Voltage regulator reports a communication or internal fault; tuning is disabled";
    if (tuner.strict.iout >= 25.0) return "Voltage regulator output current reaches the 25 A tuning limit";
    return "Miner protection is active; tuning is disabled";
}
static bool live_mining(void) {
    return tuner.device && mining_state_work_allowed(tuner.device) &&
        !tuner.device->SELF_TEST_MODULE.is_active && !tuner.device->SYSTEM_MODULE.is_firmware_update;
}
static bool current_fault(void) {
    return tuner.input.fault || (tuner.device &&
        (tuner.device->SYSTEM_MODULE.hardware_fault || tuner.device->SYSTEM_MODULE.overheat_mode));
}
static bool fresh_input(uint64_t now) {
    return live_mining() && tuner.input.valid && tuner.input.capturedAtUs &&
        now >= tuner.input.capturedAtUs && now-tuner.input.capturedAtUs <= TUNE_FRESH_US;
}
static bool ownership_revoked(void) {
    return tuner.frozen || ((tuner.core.owns || tuner.restore_pending) &&
        tuner.owner_epoch != atomic_load(&manual_epoch));
}
static const char *presentation_reason(uint64_t now) {
    if (!tuner.storage_ok) return "storage-unavailable";
    if (ownership_revoked()) return tuner.frozen && tuner.override_reason ? tuner.override_reason : "manual-control";
    const char *reason = inactive_fault_reason();
    if (reason) return reason;
    reason = tuner.override_reason ? tuner.override_reason : tuner.core.reason;
    /* Boot/pause cancellation text is historical once this exact idle state
     * has fresh healthy inputs. Do not erase handoff/fault/restore evidence. */
    if (reason && !strcmp(reason,"mining-paused-or-faulted") && tuner.core.state == FWT_DISABLED && !tuner.core.active &&
        !tuner.core.owns && !tuner.restore_pending && !tuner.confirming && !tuner.commanded &&
        !tuner.rail_wait && !tuner.frozen && !tuner.core.awaitingApply &&
        fresh_input(now) && !current_fault() && tuner.input.fanOk) return "disabled";
    return reason ? reason : "disabled";
}
static bool obscured_search(void) {
    return tuner.restore_pending || ownership_revoked() || tuner.override_reason != NULL;
}
bool autotune_gamma_try_display_snapshot(GammaAutotuneDisplaySnapshot *out, uint64_t now) {
    if (!out) return false;
    memset(out,0,sizeof(*out));
    /* LVGL timer callbacks already hold the LVGL port lock. A trylock only,
     * with no stale fallback, avoids any blocking LVGL -> tuner lock order. */
    if (pthread_mutex_trylock(&tuner_mutex) != 0) return false;
    out->supported = tuner.initialized && tuner.supported;
    out->available = out->supported && tuner.storage_ok;
    out->telemetryFresh = fresh_input(now);
    if(out->telemetryFresh) {out->hashRateGHs=tuner.input.hashRateGHs;out->powerW=tuner.input.powerW;}
    out->fault = current_fault();
    out->active = !ownership_revoked() && tuner.core.active; out->ownsSettings = !ownership_revoked() && (tuner.core.owns || tuner.restore_pending);
    out->restoring = tuner.restore_pending; out->state = tuner.core.state; out->priority = tuner.core.config.priority;
    out->search = fwt_search_read(&tuner.core,now,obscured_search());
    fwt_history_capture(&tuner.history,&tuner.core);
    out->hasDecision = fwt_history_count(&tuner.history)>0;
    out->latestDecision = fwt_event_short_label(fwt_history_latest(&tuner.history));
    out->currentAvailable = out->telemetryFresh && fwt_positive(tuner.input.current.frequencyMHz) &&
        fwt_positive(tuner.input.current.coreVoltageMv);
    if (out->currentAvailable) out->current = tuner.input.current;
    snprintf(out->reason,sizeof(out->reason),"%s",presentation_reason(now));
    pthread_mutex_unlock(&tuner_mutex);
    return true;
}
_Static_assert(sizeof(GammaAutotuneDisplaySnapshot) <= 256,"Keep display snapshots small and on-stack");
static bool add_events(cJSON *status) {
    cJSON *events=cJSON_AddArrayToObject(status,"events");
    if(!events || !cJSON_AddNumberToObject(status,"eventCapacity",FWT_EVENT_CAPACITY)) return false;
    for(unsigned i=0;i<fwt_history_count(&tuner.history);++i) {
        const fwt_event *e=fwt_history_at(&tuner.history,i);
        cJSON *event=cJSON_CreateObject();
        if(!event) return false;
        if(!cJSON_AddItemToArray(events,event)) {cJSON_Delete(event);return false;}
        if(!cJSON_AddNumberToObject(event,"id",e->id) || !cJSON_AddNumberToObject(event,"atSeconds",e->atUs/1000000.0) ||
           !cJSON_AddStringToObject(event,"kind",fwt_event_kind_name(e->kind)) ||
           !cJSON_AddStringToObject(event,"reason",e->reason?e->reason:"unknown") ||
           !cJSON_AddStringToObject(event,"outcome",fwt_event_outcome_name(e->outcome)) ||
           !add_point(event,"from",fwt_positive(e->from.frequencyMHz)&&fwt_positive(e->from.coreVoltageMv),e->from) ||
           !add_point(event,"point",true,e->point)) return false;
        if(e->hasMeasurement) {
            cJSON *mean=cJSON_AddObjectToObject(event,"measurement");
            if(!mean || !cJSON_AddNumberToObject(mean,"hashRateGHs",e->hashRateGHs) ||
               !cJSON_AddNumberToObject(mean,"powerW",e->powerW) ||
               !cJSON_AddNumberToObject(mean,"errorPercentage",e->errorPercentage) ||
               !cJSON_AddNumberToObject(mean,"worstErrorPercentage",e->worstErrorPercentage)) return false;
        } else if(!cJSON_AddNullToObject(event,"measurement")) return false;
        if(e->hasScore) {
            if(!cJSON_AddNumberToObject(event,"score",e->score)||!cJSON_AddNumberToObject(event,"requiredScore",e->requiredScore)) return false;
        } else if(!cJSON_AddNullToObject(event,"score")||!cJSON_AddNullToObject(event,"requiredScore")) return false;
    }
    return true;
}
static cJSON *report(void) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;
    pthread_mutex_lock(&tuner_mutex);
    uint64_t now = esp_timer_get_time();
    tuner.observer_until = now + UINT64_C(15000000);
    fwt_history_capture(&tuner.history,&tuner.core);
    cJSON *hardware = cJSON_AddObjectToObject(root,"hardware");
    cJSON *limits = cJSON_AddObjectToObject(root,"limits");
    cJSON *guidance = cJSON_AddObjectToObject(root,"guidance");
    cJSON *status = cJSON_AddObjectToObject(root,"status");
    cJSON *backup = cJSON_AddObjectToObject(root,"backup");
    cJSON *diagnostic = cJSON_AddObjectToObject(root,"diagnostics");
    cJSON *config = config_json(&tuner.core.config);
    if (!hardware || !limits || !guidance || !status || !backup || !diagnostic || !config) {cJSON_Delete(config); goto failed;}
    if (!cJSON_AddItemToObject(root,"config",config)) {cJSON_Delete(config); goto failed;}
#define STR(o,k,v) do {if(!cJSON_AddStringToObject((o),(k),(v))) goto failed;} while(0)
#define NUM(o,k,v) do {if(!cJSON_AddNumberToObject((o),(k),(v))) goto failed;} while(0)
#define BOOL(o,k,v) do {if(!cJSON_AddBoolToObject((o),(k),(v))) goto failed;} while(0)
    NUM(root,"schemaVersion",1); BOOL(root,"supported",tuner.supported); NUM(root,"waiverVersion",1);
    char model[32]; snprintf(model,sizeof(model),"Bitaxe Gamma %s",tuner.board);
    STR(hardware,"model",model); NUM(hardware,"asicCount",1);
    STR(hardware,"asicSensor","EMC2101 external ASIC diode");
    STR(hardware,"vrmSensor","TPS546 internal regulator die");
    STR(hardware,"powerSource","Estimated device power: measured Vout x Iout + 5 W; not wall power");
    NUM(limits,"minFrequencyMHz",tuner.core.limits.minFrequencyMHz);
    NUM(limits,"maxFrequencyMHz",tuner.core.limits.maxFrequencyMHz);
    NUM(limits,"minVoltageMv",tuner.core.limits.minVoltageMv);
    NUM(limits,"maxVoltageMv",tuner.core.limits.maxVoltageMv);
    NUM(limits,"maxAsicTempC",tuner.core.limits.maxAsicTempC);
    NUM(limits,"maxVrmTempC",tuner.core.limits.maxVrmTempC);
    NUM(limits,"maxPowerW",tuner.core.limits.maxPowerW);
    NUM(limits,"frequencyStepMHz",tuner.core.limits.frequencyStepMHz);
    NUM(limits,"voltageStepMv",tuner.core.limits.voltageStepMv);
    STR(guidance,"asic","Recommended/default tuning ceiling: 60 C. Above 60 C needs caution; above 65 C is strongly discouraged. Higher configurable limits are not recommended operating targets or certified junction temperatures. Existing 75 C firmware protection remains active.");
    STR(guidance,"vrm","Recommended/default tuning ceiling: 70 C. Above 70 C needs caution; 80 C and higher is strongly discouraged. Protection cutoffs are not recommended operating targets. The TPS546 die-sensor setting can be raised up to the allowed 95 C limit; that limit is not a recommendation. Existing 105 C firmware protection and regulator faults remain authoritative.");
    STR(guidance,"power","Power is an output-derived estimate including a fixed 5 W allowance. It cannot certify PSU, cable or wall-power headroom. Frequency and voltage upper bounds initially match your saved settings; raising them is an explicit experimental choice. PLL frequency is computed from commands, not measured clock readback.");
    BOOL(guidance,"experimental",true);
    STR(status,"state",tuner.restore_pending ? "restoring" : fwt_state_name(tuner.core.state));
    STR(status,"reason",presentation_reason(now));
    BOOL(status,"active",!ownership_revoked() && tuner.core.active); BOOL(status,"ownsSettings",!ownership_revoked() && (tuner.core.owns || tuner.restore_pending));
    NUM(status,"runId",tuner.core.runId); NUM(status,"experiments",tuner.core.experiments);
    if(!add_events(status)) goto failed;
    if (!add_point(status,"baseline",tuner.core.baselineAvailable,tuner.core.baseline) ||
        !add_point(status,"current",tuner.input.current.frequencyMHz > 0,tuner.input.current) ||
        !add_point(status,"best",tuner.core.bestAvailable,tuner.core.best)) goto failed;
    cJSON *telemetry = cJSON_AddObjectToObject(status,"telemetry");
    if (!telemetry) goto failed;
    const bool mining_active = live_mining();
    const bool fault = current_fault();
    const bool fresh = fresh_input(now);
    BOOL(telemetry,"valid",fresh); BOOL(telemetry,"miningActive",mining_active);
    BOOL(telemetry,"fault",fault); BOOL(telemetry,"fanOk",fresh && tuner.input.fanOk);
    cJSON *search = cJSON_AddObjectToObject(status,"search");
    if (!search) goto failed;
    const fwt_search_view progress = fwt_search_read(&tuner.core,now,obscured_search());
    if (progress.timed) {
        NUM(search,"phaseElapsedSeconds",progress.phaseElapsedSeconds); NUM(search,"minimumSeconds",progress.minimumSeconds);
    } else if (!cJSON_AddNullToObject(search,"phaseElapsedSeconds") || !cJSON_AddNullToObject(search,"minimumSeconds")) goto failed;
    NUM(search,"samples",progress.samples); NUM(search,"minimumSamples",progress.minimumSamples);
    NUM(search,"roundExperiments",progress.roundExperiments); NUM(search,"roundExperimentLimit",progress.roundExperimentLimit);
    NUM(search,"rejectedWindows",progress.rejectedWindows);
    const char *metric_keys[] = {"reference","best"};
    for (unsigned i=0;i<2;++i) {
        if (i ? progress.bestAvailable : progress.referenceAvailable) {
            cJSON *metric = cJSON_AddObjectToObject(search,metric_keys[i]);
            if (!metric) goto failed;
            NUM(metric,"hashRateGHs",i ? progress.bestHash : progress.referenceHash);
            NUM(metric,"powerW",i ? progress.bestPower : progress.referencePower);
        } else if (!cJSON_AddNullToObject(search,metric_keys[i])) goto failed;
    }
    /* Cached evidence only. A GET neither polls hardware nor rejuvenates an
     * unsuccessful acquisition's previous successful regulator sample. */
    BOOL(diagnostic,"sensorReadSucceeded",tuner.sensor_ok);
    if (tuner.sensor_at && now >= tuner.sensor_at) NUM(diagnostic,"lastAttemptAgeSeconds",(now-tuner.sensor_at)/1000000.0);
    else if (!cJSON_AddNullToObject(diagnostic,"lastAttemptAgeSeconds")) goto failed;
    if (tuner.strict_success_at && now >= tuner.strict_success_at) NUM(diagnostic,"lastSuccessAgeSeconds",(now-tuner.strict_success_at)/1000000.0);
    else if (!cJSON_AddNullToObject(diagnostic,"lastSuccessAgeSeconds")) goto failed;
#define CACHED(k,v) do {if(tuner.strict_success_at) {NUM(diagnostic,(k),(v));} else if(!cJSON_AddNullToObject(diagnostic,(k))) goto failed;} while(0)
    CACHED("statusWord",tuner.strict.status_word);
    CACHED("statusVout",tuner.strict.status_vout); CACHED("statusIout",tuner.strict.status_iout);
    CACHED("statusInput",tuner.strict.status_input); CACHED("statusTemperature",tuner.strict.status_temperature);
    CACHED("statusCml",tuner.strict.status_cml);
    CACHED("vinV",tuner.strict.vin); CACHED("ioutA",tuner.strict.iout);
#undef CACHED
#define READING(k,v) do {if(fresh) {NUM(telemetry,(k),(v));} else if(!cJSON_AddNullToObject(telemetry,(k))) goto failed;} while(0)
    READING("hashRateGHs",tuner.input.hashRateGHs); READING("errorPercentage",tuner.input.errorPercentage);
    READING("worstErrorPercentage",tuner.input.worstErrorPercentage); READING("powerW",tuner.input.powerW);
    READING("asicTempC",tuner.input.asicTempC); READING("vrmTempC",tuner.input.vrmTempC);
    READING("voltageReadbackMv",tuner.input.voltageReadbackMv);
    READING("sampleAgeSeconds",(now-tuner.input.capturedAtUs)/1000000.0);
    BOOL(backup,"available",tuner.backup_available);
    BOOL(backup,"restored",tuner.backup_restored && !tuner.core.owns && !tuner.restore_pending &&
        tuner.device && backup_matches_live(tuner.device,now));
    if (tuner.backup_available) {
        NUM(backup,"frequencyMHz",tuner.backup[0].value.f); NUM(backup,"coreVoltageMv",tuner.backup[1].value.u16);
    } else if (!cJSON_AddNullToObject(backup,"frequencyMHz") || !cJSON_AddNullToObject(backup,"coreVoltageMv")) goto failed;
#undef STR
#undef NUM
#undef BOOL
#undef READING
    pthread_mutex_unlock(&tuner_mutex);
    return root;
failed:
    pthread_mutex_unlock(&tuner_mutex); cJSON_Delete(root); return NULL;
}

static const char *handle_action(const cJSON *json) {
    const cJSON *action = cJSON_GetObjectItemCaseSensitive(json,"action");
    if (!cJSON_IsString(action)) return "invalid-request";
    const char *error = NULL;
    pthread_mutex_lock(&tuner_mutex);
    if (!tuner.supported) {error="unsupported-hardware"; goto done;}
    if (!strcmp(action->valuestring,"configure")) {
        const char *const names[] = {"action","config"};
        fwt_config config;
        if (tuner.core.awaitingApply || tuner.confirming || tuner.restore_pending) {
            error="transition-in-progress"; goto done;
        }
        if (!exact_fields(json,names,2) || !parse_config(cJSON_GetObjectItemCaseSensitive(json,"config"),&config)) {
            error="invalid-configuration"; goto done;
        }
        fwt_controller candidate=tuner.core;
        if (!fwt_configure(&candidate,&config)) {error="invalid-configuration"; goto done;}
        cJSON *record = bound_json(); cJSON *settings = config_json(&config);
        if (!record || !settings) {cJSON_Delete(record); cJSON_Delete(settings); error="allocation-failed"; goto done;}
        if (!cJSON_AddItemToObject(record,"config",settings)) {
            cJSON_Delete(record); cJSON_Delete(settings); error="allocation-failed"; goto done;
        }
        if (write_record("config",record) != ESP_OK) error="storage-unavailable";
        else tuner.core=candidate;
        cJSON_Delete(record);
        if (!error) {tuner.storage_ok=true; tuner.override_reason=NULL;}
    } else if (!strcmp(action->valuestring,"start")) {
        const char *const names[] = {"action","waiverAccepted","waiverVersion"};
        const cJSON *waiver = cJSON_GetObjectItemCaseSensitive(json,"waiverAccepted");
        double version = 0;
        if (!exact_fields(json,names,3) || !cJSON_IsTrue(waiver) || !number(json,"waiverVersion",&version) || version != 1) {
            error="waiver-required"; goto done;
        }
        const uint64_t now = esp_timer_get_time();
        tuner.observer_until = now+TUNE_FRESH_US;
        if (!tuner.storage_ok || tuner.restore_pending || tuner.frozen) {error="storage-or-restore-unavailable"; goto done;}
        if (!fwt_input_valid(&tuner.input,now) || !tuner.input.miningActive || tuner.input.fault ||
            tuner.device->SYSTEM_MODULE.overheat_mode || tuner.core.owns) {error="miner-not-ready"; goto done;}
        const uint32_t epoch = atomic_load(&manual_epoch);
        NvsConfigMutation saved[BACKUP_FIELDS] = {0};
        if (!nvs_config_read_atomic(backup_keys,saved,BACKUP_FIELDS) ||
            fabs(saved[0].value.f-tuner.input.current.frequencyMHz) > 0.01 ||
            saved[1].value.u16 != tuner.input.current.coreVoltageMv ||
            fabs(tuner.input.voltageReadbackMv-saved[1].value.u16) > 30 ||
            fabs(tuner.device->POWER_MANAGEMENT_MODULE.actual_frequency-saved[0].value.f) > 2) {
            error="manual-settings-not-applied"; goto done;
        }
        if (!ensure_backup()) {error="backup-verification-failed"; goto done;}
        /* Activation shares the physical-write revocation fence, after all
         * storage I/O has completed. Never consume a manual change that raced
         * the snapshot as authorization for a new automatic owner. */
        pthread_mutex_lock(&wire_gate);
        if (epoch != atomic_load(&manual_epoch)) error="manual-control-changed";
        else if (!fwt_start(&tuner.core,&tuner.input,esp_timer_get_time())) error="miner-not-ready";
        else {
            tuner.owner_epoch=epoch; tuner.backup_restored=false; tuner.override_reason=NULL;
            tuner.backup_matches=0;tuner.backup_match_at=0;
            tuner.target=tuner.core.current; tuner.commanded=tuner.confirming=false;
        }
        pthread_mutex_unlock(&wire_gate);
    } else if (!strcmp(action->valuestring,"stop")) {
        const char *const names[] = {"action"};
        if (!exact_fields(json,names,1)) {error="invalid-request"; goto done;}
        if (!tuner.backup_available) {error="backup-unavailable"; goto done;}
        autotune_gamma_manual_override();
        fwt_cancel(&tuner.core,"restore-requested");
        tuner.restore_pending=true; tuner.restore_committed=false; tuner.backup_restored=false;
        tuner.confirming=tuner.commanded=tuner.rail_wait=false; tuner.owner_epoch=atomic_load(&manual_epoch);
        tuner.override_reason="restore-requested";
    } else error="invalid-request";
done:
    fwt_history_capture(&tuner.history,&tuner.core);
    pthread_mutex_unlock(&tuner_mutex);
    return error;
}

static esp_err_t response(httpd_req_t *req, const char *status, const char *error) {
    httpd_resp_set_status(req,status); httpd_resp_set_type(req,"application/json");
    char body[160];
    if (error) snprintf(body,sizeof(body),"{\"ok\":false,\"error\":\"%s\"}",error);
    else snprintf(body,sizeof(body),"{\"ok\":true,\"restartRequired\":false}");
    return httpd_resp_sendstr(req,body);
}
static bool allowed(httpd_req_t *req) {
    if (is_network_allowed(req) != ESP_OK) {response(req,"401 Unauthorized","unauthorized"); return false;}
    if (set_cors_headers(req) != ESP_OK) {response(req,"500 Internal Server Error","request-failed"); return false;}
    httpd_resp_set_hdr(req,"Cache-Control","no-store"); return true;
}
static esp_err_t get_tuner(httpd_req_t *req) {
    if (!allowed(req)) return ESP_OK;
    cJSON *json = report();
    if (!json) return response(req,"503 Service Unavailable","report-unavailable");
    httpd_resp_set_type(req,"application/json");
    int prebuffer=4096; esp_err_t err=HTTP_send_json(req,json,&prebuffer); cJSON_Delete(json); return err;
}
static esp_err_t get_activity(httpd_req_t *req) {
    if(!allowed(req)) return ESP_OK;
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    cJSON *json=cJSON_CreateObject();
    if(!json) return response(req,"503 Service Unavailable","report-unavailable");
    bool locked=pthread_mutex_trylock(&tuner_mutex)==0;
    bool ok=cJSON_AddNumberToObject(json,"schemaVersion",1) &&
        cJSON_AddBoolToObject(json,"available",locked&&tuner.initialized);
    if(locked&&tuner.initialized) {
        const uint64_t now=esp_timer_get_time();
        const bool revoked=ownership_revoked();
        ok=ok && cJSON_AddBoolToObject(json,"supported",tuner.supported) &&
            cJSON_AddBoolToObject(json,"active",!revoked&&tuner.core.active) &&
            cJSON_AddBoolToObject(json,"ownsSettings",!revoked&&(tuner.core.owns||tuner.restore_pending)) &&
            cJSON_AddBoolToObject(json,"fault",current_fault()||tuner.core.state==FWT_FAULTED) &&
            cJSON_AddStringToObject(json,"state",tuner.restore_pending?"restoring":fwt_state_name(tuner.core.state)) &&
            cJSON_AddStringToObject(json,"reason",presentation_reason(now)) &&
            cJSON_AddStringToObject(json,"priority",fwt_priority_name(tuner.core.config.priority)) &&
            cJSON_AddBoolToObject(json,"continuous",tuner.core.config.continuous) &&
            cJSON_AddNumberToObject(json,"runId",tuner.core.runId);
        if(tuner.last_owner_tick&&now>=tuner.last_owner_tick)
            ok=ok&&cJSON_AddNumberToObject(json,"controllerAgeSeconds",(now-tuner.last_owner_tick)/1000000.0);
        else ok=ok&&cJSON_AddNullToObject(json,"controllerAgeSeconds");
    }
    if(locked) pthread_mutex_unlock(&tuner_mutex);
    if(!ok) {cJSON_Delete(json);return response(req,"503 Service Unavailable","report-unavailable");}
    httpd_resp_set_type(req,"application/json");
    int prebuffer=1024;esp_err_t error=HTTP_send_json(req,json,&prebuffer);cJSON_Delete(json);return error;
}
static esp_err_t post_tuner(httpd_req_t *req) {
    if (!allowed(req)) return ESP_OK;
    if (!req->content_len || req->content_len >= TUNE_BODY_MAX) return response(req,"400 Bad Request","invalid-request");
    char *body=malloc(req->content_len+1);
    if (!body) return response(req,"503 Service Unavailable","allocation-failed");
    size_t received=0;
    while(received<req->content_len) {
        int count=httpd_req_recv(req,body+received,req->content_len-received);
        if(count<=0) break;
        received+=count;
    }
    body[received]=0;
    cJSON *json=received==req->content_len && strlen(body)==received && !strstr(body,"\\u0000") ?
        cJSON_ParseWithOpts(body,NULL,true):NULL;
    free(body);
    const char *error=json ? handle_action(json):"invalid-request";
    cJSON_Delete(json);
    return error ? response(req,"409 Conflict",error):response(req,"200 OK",NULL);
}
esp_err_t autotune_gamma_register(httpd_handle_t server) {
    const httpd_uri_t routes[] = {
        {.uri="/api/5tratum/autotune",.method=HTTP_GET,.handler=get_tuner},
        {.uri="/api/5tratum/autotune",.method=HTTP_POST,.handler=post_tuner},
        {.uri="/api/5tratum/autotune/activity",.method=HTTP_GET,.handler=get_activity}};
    for(unsigned i=0;i<sizeof(routes)/sizeof(routes[0]);++i) {
        esp_err_t error=httpd_register_uri_handler(server,&routes[i]);
        if(error!=ESP_OK) return error;
    }
    return ESP_OK;
}
