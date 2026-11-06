#include "autotune_gamma_stubs.h"
/* Reuse the existing fake storage/HTTP boundary; all NVS journal and profile
 * code linked by the runner is production, not a second implementation. */
#define main legacy_profiles_main
#define nvs_get_blob stored_nvs_get_blob
#include "profiles_runtime_test.c"
#undef nvs_get_blob
#undef main
static bool fail_recovery_journal_read, revoke_on_backup_read;
void autotune_gamma_manual_override(void);
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size) {
    if (fail_recovery_journal_read && !strcmp(key,"5fw_operating")) return ESP_FAIL;
    if (revoke_on_backup_read && !strcmp(key,"backup")) {revoke_on_backup_read=false;autotune_gamma_manual_override();}
    return stored_nvs_get_blob(h,key,out,size);
}
static bool revoke_after_snapshot,fail_tuning_snapshot;
static bool interleaved_snapshot(const NvsConfigKey *,NvsConfigMutation *,size_t);
#define nvs_config_read_atomic interleaved_snapshot
#include "../../main/autotune_runtime.c"
#undef nvs_config_read_atomic
void mining_schedule_update(void) {}
#include "gamma_stop_guard.inc"
static bool interleaved_snapshot(const NvsConfigKey *keys,NvsConfigMutation *values,size_t count) {
    if(fail_tuning_snapshot)return false;
    const bool result=nvs_config_read_atomic(keys,values,count);
    if (revoke_after_snapshot) {revoke_after_snapshot=false;autotune_gamma_manual_override();}
    return result;
}

static uint64_t host_now=UINT64_C(1000000);
static bool off_diode_unavailable;
static bool sensors_ok=true, counters_ok=true, host_fan_ok=true, host_current_ok=true;
static unsigned sensor_reads, counter_resets, fault_stops, point_changes;
static atomic_uint_least32_t tx_failures;
static double sensor_vout_delta;
static float host_asic_temp=50,host_vrm_temp=55;
static double host_error_percentage;
static uint8_t host_status_cml;
static char last_fault_reason[160];
static bool host_applied_paused,host_internal_stack=true,host_resume_refused;
static uint32_t host_schedule_revision=1;
static unsigned host_resume_requests,host_pause_requests;
static double off_vout,off_iout;
static uint16_t off_status_word=TPS546_STATUS_OFF|TPS546_STATUS_PGOOD;
static uint8_t off_status_vout;
bool nvs_config_recovery_stack_internal(void) {return host_internal_stack;}
bool mining_schedule_applied_paused(void) {return host_applied_paused;}
uint32_t mining_schedule_control_revision(void) {return host_schedule_revision;}
void mining_schedule_manual_override(bool paused) {
    ++host_schedule_revision;autotune_gamma_manual_override();gs.SYSTEM_MODULE.mining_paused=paused;
}
bool mining_schedule_resume_if_unchanged(uint32_t revision) {
    if (revision!=host_schedule_revision || host_resume_refused) return false;
    ++host_resume_requests;gs.SYSTEM_MODULE.mining_paused=false;return true;
}
bool mining_schedule_pause_if_unchanged(uint32_t revision) {
    if (revision!=host_schedule_revision) return false;
    ++host_pause_requests;gs.SYSTEM_MODULE.mining_paused=true;return true;
}
int64_t esp_timer_get_time(void) {return host_now;}
void vTaskDelay(unsigned ticks) {(void)ticks;}
esp_err_t uart_wait_tx_done(int uart,unsigned ticks) {(void)uart;(void)ticks;return ESP_OK;}
static unsigned physical_uart_frames,physical_i2c_frames;
static pthread_mutex_t wire_test_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wire_test_condition=PTHREAD_COND_INITIALIZER;
static bool block_uart,uart_entered,release_uart;
static atomic_bool revocation_attempted,revocation_completed;
static int tps546_i2c_handle;
#define BUF_SIZE 1024
#include "pmbus_commands.h"
static void prettyHex(unsigned char *bytes,int length) {(void)bytes;(void)length;}
static int uart_write_bytes(int uart,const char *bytes,int length) {
    (void)uart;(void)bytes;
    pthread_mutex_lock(&wire_test_mutex);
    if(block_uart) {
        uart_entered=true;pthread_cond_broadcast(&wire_test_condition);
        while(!release_uart) pthread_cond_wait(&wire_test_condition,&wire_test_mutex);
    }
    ++physical_uart_frames;
    pthread_mutex_unlock(&wire_test_mutex);return length;
}
static esp_err_t i2c_bitaxe_register_write_byte(int device,uint8_t reg,uint8_t value) {
    (void)device;(void)reg;(void)value;++physical_i2c_frames;return ESP_OK;
}
static esp_err_t i2c_bitaxe_register_write_word(int device,uint8_t reg,uint16_t value) {
    (void)device;(void)reg;(void)value;++physical_i2c_frames;return ESP_OK;
}
/* Exact production wire functions, copied by the runner without rewriting;
 * these call the strong production runtime lease hooks above. */
#include "gamma_wire_methods.inc"
esp_err_t TPS546_read_strict(TPS546_StrictTelemetry *out) {
    ++sensor_reads;
    if(!sensors_ok) return ESP_FAIL;
    double v=gs.POWER_MANAGEMENT_MODULE.applied_core_voltage_mv/1000.0;
    if(v<=0) return ESP_FAIL; /* Running strict acquisition rejects a powered-off rail. */
    /* Deterministic output-power plant; it is not a claimed real miner curve. */
    double watts=12.0*(gs.POWER_MANAGEMENT_MODULE.frequency_value/625.125)*(v/1.173)*(v/1.173);
    *out=(TPS546_StrictTelemetry){.vin=5.1,.vout=v+sensor_vout_delta,.iout=host_current_ok?watts/v:0,.temperature_c=host_vrm_temp,
        .status_word=host_status_cml?TPS546_STATUS_CML:0,.status_cml=host_status_cml};
    return ESP_OK;
}
esp_err_t TPS546_read_strict_off(TPS546_StrictTelemetry *out) {
    ++sensor_reads;if (!sensors_ok) return ESP_FAIL;
    *out=(TPS546_StrictTelemetry){.vin=5.1,.vout=off_vout,.iout=off_iout,.temperature_c=host_vrm_temp,
        .status_word=gs.ASIC_initalized?0:off_status_word,.status_vout=off_status_vout,
        .operation=gs.ASIC_initalized?OPERATION_ON:OPERATION_OFF};
    if(gs.ASIC_initalized) out->vout=gs.POWER_MANAGEMENT_MODULE.applied_core_voltage_mv/1000.0+sensor_vout_delta;
    return ESP_OK;
}
esp_err_t EMC2101_read_strict(float *temp,uint16_t *rpm) {
    if(!sensors_ok || (off_diode_unavailable && !gs.ASIC_initalized)) return ESP_FAIL;
    *temp=host_asic_temp;*rpm=host_fan_ok?3000:0;return ESP_OK;
}
esp_err_t EMC2101_read_fan_strict(uint16_t *rpm) {
    if(!sensors_ok) return ESP_FAIL;
    *rpm=host_fan_ok?3000:0;return ESP_OK;
}
GammaAutotuneCounterSample hashrate_monitor_autotune_snapshot(void *state,uint64_t now) {
    (void)state;
    return (GammaAutotuneCounterSample){.valid=counters_ok,.captured_at_us=now-500000,
        .sample_id=now-500000,.useful_ghs=gs.POWER_MANAGEMENT_MODULE.frequency_value*2.04,.error_percentage=host_error_percentage,
        .readiness_reason=counters_ok?NULL:"Work domain has no counter sample",.blocked_domain=counters_ok?0:3};
}
void hashrate_monitor_reset_measurements(void *state) {(void)state;++counter_resets;}
void POWER_MANAGEMENT_stop_for_fault(void *state,const char *reason) {
    snprintf(last_fault_reason,sizeof(last_fault_reason),"%s",reason);
    GlobalState *device=state;++fault_stops;device->SYSTEM_MODULE.hardware_fault=true;device->ASIC_initalized=false;
}
static httpd_req_t tune_request(bool write,const char *body) {return request(write?4:3,body);}
static void observe(void) {
    httpd_req_t req=tune_request(false,NULL);assert(req.code==200);
    uint16_t mv=nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE);float mhz=nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY);
    assert(!autotune_gamma_select_target(&gs,&mv,&mhz));
}
static void init(void) {
    bootstrap();
    gs.SYSTEM_MODULE.mining_runtime_ready=true;gs.ASIC_initalized=true;
    gs.POWER_MANAGEMENT_MODULE.frequency_value=625.125f;
    gs.POWER_MANAGEMENT_MODULE.actual_frequency=625.125f;
    gs.POWER_MANAGEMENT_MODULE.applied_core_voltage_mv=1173;
    autotune_gamma_init(&gs);assert(tuner.supported&&tuner.generation==1);
    assert(autotune_gamma_register(NULL)==ESP_OK&&route_count==6);
    observe();assert(tuner.input.valid);
}
static void start(void) {
    httpd_req_t req=tune_request(true,"{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}");
    if(req.code!=200) fprintf(stderr,"start failed: %s\n",req.reply);
    assert(req.code==200&&tuner.core.owns&&tuner.backup_available);
}
static bool step(bool callback) {
    host_now+=TUNE_POLL_US;
    uint16_t mv=nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE);float mhz=nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY);
    bool owned=autotune_gamma_select_target(&gs,&mv,&mhz);
    if(owned && callback) {
        if(mv!=gs.POWER_MANAGEMENT_MODULE.applied_core_voltage_mv || mhz!=gs.POWER_MANAGEMENT_MODULE.frequency_value) ++point_changes;
        gs.POWER_MANAGEMENT_MODULE.applied_core_voltage_mv=mv;
        gs.POWER_MANAGEMENT_MODULE.frequency_value=mhz;gs.POWER_MANAGEMENT_MODULE.actual_frequency=mhz;
        assert(autotune_gamma_transition_finished(&gs,true));
    }
    return owned;
}
static void reboot_runtime(void) {
    memset(&tuner,0,sizeof(tuner));atomic_store(&transition_live,false);atomic_store(&manual_epoch,0);
    autotune_gamma_init(&gs);
}
static void expect_error(const char *body,const char *error) {
    httpd_req_t req=tune_request(true,body);assert(req.code!=200&&strstr(req.reply,error));
}
static httpd_req_t configure_request(fwt_config config) {
    cJSON *json=cJSON_CreateObject();assert(json&&cJSON_AddStringToObject(json,"action","configure"));
    assert(cJSON_AddItemToObject(json,"config",config_json(&config,false)));
    char *body=cJSON_PrintUnformatted(json);assert(body);
    httpd_req_t req=tune_request(true,body);free(body);cJSON_Delete(json);return req;
}
static httpd_req_t configure_clock_request(fwt_config config,bool advanced,int warning) {
    cJSON *json=cJSON_CreateObject();assert(json&&cJSON_AddStringToObject(json,"action","configure"));
    assert(cJSON_AddItemToObject(json,"config",config_json(&config,advanced)));
    if(warning) {
        if(warning==3) assert(cJSON_AddNumberToObject(json,"advancedClockWarningAccepted",1));
        else assert(cJSON_AddBoolToObject(json,"advancedClockWarningAccepted",warning!=2));
        assert(cJSON_AddNumberToObject(json,"advancedClockWarningVersion",warning==4?2:1));
    }
    char *body=cJSON_PrintUnformatted(json);assert(body);
    httpd_req_t req=tune_request(true,body);free(body);cJSON_Delete(json);return req;
}
static void manual_clock(float frequency) {
    nvs_config_set_float(NVS_CONFIG_ASIC_FREQUENCY,frequency);drain_auth_write();
    gs.POWER_MANAGEMENT_MODULE.frequency_value=frequency;
    gs.POWER_MANAGEMENT_MODULE.actual_frequency=frequency;
    host_now+=TUNE_POLL_US;observe();
}
static void advanced_start(void) {
    httpd_req_t req=tune_request(true,"{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1,\"advancedClockAccepted\":true}");
    assert(req.code==200&&tuner.core.owns&&tuner.backup_available);
}
static void *wire_writer(void *unused) {
    (void)unused;uint8_t bytes[12]={0};
    return (void *)(uintptr_t)SERIAL_send(bytes,sizeof(bytes),false);
}
static void confirmed_off(void) {
    gs.SYSTEM_MODULE.mining_paused=true;gs.ASIC_initalized=false;host_applied_paused=true;
    gs.POWER_MANAGEMENT_MODULE.applied_core_voltage_mv=0;
    gs.POWER_MANAGEMENT_MODULE.frequency_value=50;
}
static void normal_resume_completed(void) {
    assert(host_resume_requests==1);
    assert(nvs_config_verify_autotune_restore(tuner.backup,BACKUP_FIELDS)==ESP_OK);
    gs.POWER_MANAGEMENT_MODULE.applied_core_voltage_mv=nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE);
    gs.POWER_MANAGEMENT_MODULE.frequency_value=nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY);
    gs.POWER_MANAGEMENT_MODULE.actual_frequency=gs.POWER_MANAGEMENT_MODULE.frequency_value;
    gs.ASIC_initalized=true;host_applied_paused=false;
}
static void recovery_finish(void) {
    normal_resume_completed();
    for (unsigned i=0;i<4 && tuner.recovery_requested;++i) {host_now+=TUNE_POLL_US;autotune_gamma_recovery_tick(&gs);}
    assert(!tuner.recovery_requested && tuner.recovery_phase==RECOVERY_COMPLETE && tuner.backup_restored);
    assert(!tuner.core.active && !tuner.core.owns && !gs.SYSTEM_MODULE.hardware_fault);
}
static void *wire_revoker(void *fault) {
    pthread_mutex_lock(&wire_test_mutex);
    atomic_store(&revocation_attempted,true);pthread_cond_broadcast(&wire_test_condition);
    pthread_mutex_unlock(&wire_test_mutex);
    if(fault) autotune_gamma_interrupt("hardware-fault");else autotune_gamma_manual_override();
    atomic_store(&revocation_completed,true);return NULL;
}
static void expect_readiness(const char *key,bool ready,const char *detail) {
    const unsigned before=writes,reads=sensor_reads;
    cJSON *json=report(),*status=cJSON_GetObjectItem(json,"status");
    cJSON *rows=cJSON_GetObjectItem(status,"readiness"),*found=NULL;
    assert(cJSON_IsArray(rows)&&cJSON_GetArraySize(rows)==8);
    for(int i=0;i<cJSON_GetArraySize(rows);++i) {
        cJSON *row=cJSON_GetArrayItem(rows,i);
        assert(cJSON_IsString(cJSON_GetObjectItem(row,"key"))&&cJSON_IsString(cJSON_GetObjectItem(row,"label"))&&
            cJSON_IsString(cJSON_GetObjectItem(row,"detail"))&&cJSON_IsBool(cJSON_GetObjectItem(row,"ready")));
        if(!strcmp(cJSON_GetObjectItem(row,"key")->valuestring,key))found=row;
    }
    assert(found&&cJSON_IsTrue(cJSON_GetObjectItem(found,"ready"))==ready);
    assert(strstr(cJSON_GetObjectItem(found,"detail")->valuestring,detail));
    cJSON_Delete(json);assert(writes==before&&sensor_reads==reads);
}
int main(int argc,char **argv) {
    assert(argc==2);init();const char *name=argv[1];
    if(!strcmp(name,"readiness")) {
        expect_readiness("fan-1",true,"3000 RPM");
        expect_readiness("counters-1",true,"all four");
        expect_readiness("freshness",true,"maximum 15 s");
        host_fan_ok=false;host_now+=TUNE_POLL_US;observe();expect_readiness("fan-1",false,"0 RPM");
        host_fan_ok=true;sensors_ok=false;host_now+=TUNE_POLL_US;observe();
        expect_readiness("regulator",false,"Strict regulator read failed");expect_readiness("asic-sensor",false,"No successful");
        sensors_ok=true;counters_ok=false;host_now+=TUNE_POLL_US;observe();expect_readiness("counters-1",false,"Domain 3: Work domain has no counter sample");
        counters_ok=true;host_current_ok=false;host_now+=TUNE_POLL_US;observe();expect_readiness("regulator",false,"0.00 A");
        host_current_ok=true;host_now+=TUNE_POLL_US;observe();
        gs.SYSTEM_MODULE.hardware_fault=true;expect_readiness("fault",false,"fault is active");gs.SYSTEM_MODULE.hardware_fault=false;
        gs.SYSTEM_MODULE.mining_paused=true;expect_readiness("mining",false,"paused");gs.SYSTEM_MODULE.mining_paused=false;
        host_now+=TUNE_FRESH_US+1;expect_readiness("freshness",false,"maximum 15 s");
    } else if(!strcmp(name,"advanced-migration")) {
        fwt_config config=tuner.core.config;
        cJSON *record=bound_json(),*legacy=config_json(&config,false);assert(record&&legacy);
        cJSON_DeleteItemFromObjectCaseSensitive(legacy,"advancedClockOverride");
        assert(cJSON_AddItemToObject(record,"config",legacy));
        assert(write_record("config",record)==ESP_OK);cJSON_Delete(record);
        unsigned before=writes;reboot_runtime();
        assert(writes==before&&tuner.storage_ok&&!tuner.advanced_clock_override&&!tuner.core.owns);
        assert(tuner.core.limits.maxFrequencyMHz==1200&&tuner.core.config.maxFrequencyMHz==625.125);
        cJSON *json=report();assert(json);
        const cJSON *saved=cJSON_GetObjectItemCaseSensitive(json,"config");
        assert(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(saved,"advancedClockOverride")));
        const cJSON *limits=cJSON_GetObjectItemCaseSensitive(json,"limits");
        assert(cJSON_GetObjectItemCaseSensitive(limits,"maxFrequencyMHz")->valuedouble==690);
        assert(cJSON_GetObjectItemCaseSensitive(limits,"advancedMaxFrequencyMHz")->valuedouble==1200);
        cJSON_Delete(json);
        manual_clock(750);reboot_runtime();
        assert(!tuner.advanced_clock_override&&gs.POWER_MANAGEMENT_MODULE.frequency_value==750);
        /* Existing valid saved configuration is retained, not silently raised. */
        assert(tuner.core.config.maxFrequencyMHz==625.125);
        find(2,"config")->ns=99;reboot_runtime();
        assert(tuner.core.config.maxFrequencyMHz==690&&gs.POWER_MANAGEMENT_MODULE.frequency_value==750);
        assert(!tuner.core.owns&&!tuner.backup_available&&!point_changes);
    } else if(!strcmp(name,"advanced-consent")) {
        fwt_config config=tuner.core.config;config.maxFrequencyMHz=800;
        unsigned before=writes;
        httpd_req_t req=configure_clock_request(config,false,0);assert(req.code!=200);
        req=configure_clock_request(config,true,0);assert(req.code!=200&&strstr(req.reply,"advanced-clock-warning-required"));
        for(int warning=2;warning<=4;++warning) assert(configure_clock_request(config,true,warning).code!=200);
        assert(writes==before&&!tuner.advanced_clock_override);
        assert(configure_clock_request(config,true,1).code==200&&tuner.advanced_clock_override);
        const Entry *saved=find(2,"config");assert(saved&&strstr((char *)saved->bytes,"\"advancedClockOverride\":true"));
        before=writes;
        expect_error("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}","advanced-clock-acceptance-required");
        expect_error("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1,\"advancedClockAccepted\":false}","advanced-clock-acceptance-required");
        expect_error("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1,\"advancedClockAccepted\":1}","waiver-required");
        expect_error("{\"action\":\"start\",\"waiverAccepted\":false,\"waiverVersion\":1,\"advancedClockAccepted\":true}","waiver-required");
        assert(writes==before&&!tuner.backup_available&&!tuner.core.owns);
        advanced_start();assert(tuner.advanced_clock_override);
        autotune_gamma_interrupt("manual-control");host_now+=TUNE_POLL_US;observe();
        reboot_runtime();assert(tuner.storage_ok&&tuner.advanced_clock_override&&!tuner.core.owns);
        host_now+=TUNE_POLL_US;observe();before=writes;
        expect_error("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}","advanced-clock-acceptance-required");
        assert(writes==before&&!tuner.core.owns);advanced_start();
    } else if(!strcmp(name,"advanced-strict")) {
        fwt_config config=tuner.core.config;unsigned before=writes;
        for(unsigned variant=0;variant<6;++variant) {
            cJSON *json=cJSON_CreateObject(),*options=config_json(&config,false);assert(json&&options);
            assert(cJSON_AddStringToObject(json,"action","configure")&&cJSON_AddItemToObject(json,"config",options));
            if(variant==0) assert(cJSON_ReplaceItemInObjectCaseSensitive(options,"advancedClockOverride",cJSON_CreateNumber(1)));
            if(variant==1) assert(cJSON_AddBoolToObject(options,"advancedClockOverride",true));
            if(variant==2) assert(cJSON_AddBoolToObject(options,"unknownClockOverride",true));
            if(variant==3) assert(cJSON_AddBoolToObject(json,"advancedClockWarningAccepted",true));
            if(variant==4) assert(cJSON_AddNumberToObject(json,"advancedClockWarningVersion",1));
            if(variant==5) {
                assert(cJSON_AddBoolToObject(json,"advancedClockWarningAccepted",false));
                assert(cJSON_AddNumberToObject(json,"advancedClockWarningVersion",2));
            }
            char *body=cJSON_PrintUnformatted(json);assert(body);
            assert(tune_request(true,body).code!=200);free(body);cJSON_Delete(json);
        }
        assert(writes==before&&!tuner.advanced_clock_override&&!tuner.core.owns);
        assert(configure_clock_request(config,false,2).code==200);
        cJSON *legacy=cJSON_CreateObject(),*options=config_json(&config,false);assert(legacy&&options);
        cJSON_DeleteItemFromObjectCaseSensitive(options,"advancedClockOverride");
        assert(cJSON_AddStringToObject(legacy,"action","configure")&&cJSON_AddItemToObject(legacy,"config",options));
        char *body=cJSON_PrintUnformatted(legacy);assert(body);
        assert(tune_request(true,body).code==200);free(body);cJSON_Delete(legacy);
        assert(strstr((char *)find(2,"config")->bytes,"\"advancedClockOverride\":false"));
    } else if(!strcmp(name,"advanced-pll-candidates")) {
        manual_clock(996.25f);reboot_runtime();host_now+=TUNE_POLL_US;observe();
        fwt_config config=tuner.core.config;config.priority=FWT_PERFORMANCE;config.maxFrequencyMHz=1197.5;config.maxPowerW=40;
        assert(configure_clock_request(config,true,1).code==200);advanced_start();
        assert(tuner.core.frequencySupported==clock_command_valid);
        fwt_controller unchecked=tuner.core;unchecked.frequencySupported=NULL;
        const unsigned before=writes,reads=sensor_reads;
        fwt_input input=tuner.input;input.powerW=18;
        fwt_request hypothetical=fwt_next_candidate(&unchecked,&input,host_now);
        assert(hypothetical.kind==FWT_REQUEST_APPLY&&hypothetical.point.frequencyMHz==1002.5);
        fwt_request checked=fwt_next_candidate(&tuner.core,&input,host_now);
        assert(checked.kind==FWT_REQUEST_APPLY&&checked.point.frequencyMHz==996.25&&checked.point.coreVoltageMv==1168);
        assert(tuner.core.state==FWT_APPLYING&&tuner.core.owns&&tuner.core.roundExperiments==1);
        assert(writes==before&&sensor_reads==reads&&!point_changes&&!physical_uart_frames);
    } else if(!strcmp(name,"advanced-active-policy")) {
        start();fwt_config config=tuner.core.config;config.maxFrequencyMHz=800;
        unsigned before=writes;
        assert(configure_clock_request(config,true,0).code!=200&&writes==before&&!tuner.advanced_clock_override);
        assert(configure_clock_request(config,true,1).code==200&&tuner.core.owns&&tuner.advanced_clock_override);
        config.maxFrequencyMHz=690;
        assert(configure_clock_request(config,false,2).code==200&&tuner.core.owns&&!tuner.advanced_clock_override);
        config.maxFrequencyMHz=1200.25;before=writes;
        assert(configure_clock_request(config,true,1).code!=200&&writes==before&&!tuner.advanced_clock_override);
        config.maxFrequencyMHz=800;fail_commit=true;
        assert(configure_clock_request(config,true,1).code!=200&&!tuner.advanced_clock_override&&tuner.core.config.maxFrequencyMHz==690);
        fail_commit=false;reboot_runtime();assert(!tuner.advanced_clock_override&&tuner.core.config.maxFrequencyMHz==690);
    } else if(!strcmp(name,"advanced-backup-restore")) {
        manual_clock(750);reboot_runtime();host_now+=TUNE_POLL_US;observe();unsigned before=writes;
        expect_error("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}","advanced-clock-override-required");
        assert(writes==before&&!tuner.backup_available&&gs.POWER_MANAGEMENT_MODULE.frequency_value==750);
        fwt_config config=tuner.core.config;config.maxFrequencyMHz=800;
        assert(configure_clock_request(config,true,1).code==200);advanced_start();
        Entry *backup=find(2,"backup");assert(backup);char *original=strdup((char *)backup->bytes);assert(original);
        assert(tuner.backup[0].value.f==750);
        config.maxFrequencyMHz=690;before=writes;
        assert(configure_clock_request(config,false,0).code!=200&&writes==before&&tuner.advanced_clock_override);
        /* A probe is RAM-only: simulate its applied lower clock, then require
         * Disable to command the original 750 rather than merely matching NVS. */
        gs.POWER_MANAGEMENT_MODULE.frequency_value=650;gs.POWER_MANAGEMENT_MODULE.actual_frequency=650;
        assert(tune_request(true,"{\"action\":\"stop\"}").code==200);
        for(unsigned i=0;i<12&&tuner.restore_pending;++i) step(true);
        assert(tuner.backup_restored&&!tuner.core.owns&&gs.POWER_MANAGEMENT_MODULE.frequency_value==750&&point_changes>0);
        assert(configure_clock_request(config,false,0).code==200&&!tuner.advanced_clock_override);
        reboot_runtime();assert(tuner.storage_ok&&tuner.backup_available&&!tuner.advanced_clock_override&&!tuner.core.owns);
        assert(!strcmp((char *)find(2,"backup")->bytes,original));free(original);
        assert(tune_request(true,"{\"action\":\"stop\"}").code==200);
        for(unsigned i=0;i<12&&tuner.restore_pending;++i) step(true);
        assert(tuner.backup_restored&&gs.POWER_MANAGEMENT_MODULE.frequency_value==750&&!tuner.core.owns);
    } else if(!strcmp(name,"advanced-stale-manual")) {
        unsigned before=writes;
        nvs_config_set_float(NVS_CONFIG_ASIC_FREQUENCY,750);drain_auth_write();before=writes;
        expect_error("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}","manual-settings-not-applied");
        assert(writes==before&&!tuner.backup_available&&!tuner.core.owns);
        gs.POWER_MANAGEMENT_MODULE.frequency_value=750;gs.POWER_MANAGEMENT_MODULE.actual_frequency=750;
        host_now+=TUNE_POLL_US;observe();
        expect_error("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1,\"advancedClockAccepted\":true}","advanced-clock-override-required");
        assert(writes==before&&!tuner.backup_available&&!tuner.core.owns);
    } else if(!strcmp(name,"recovery-running-denied")) {
        start();unsigned before=writes;
        expect_error("{\"action\":\"restore-and-resume\"}","recovery-requires-confirmed-asic-off");
        assert(writes==before && !host_resume_requests && !tuner.recovery_requested);
    } else if(!strcmp(name,"recovery-success")) {
        start();confirmed_off();unsigned before=writes;
        assert(tune_request(true,"{\"action\":\"stop\"}").code==200);
        assert(step(false) && tuner.restore_held && !tuner.restore_pending && !tuner.restore_committed && !host_resume_requests);
        assert(tuner.restoration_phase==RESTORE_BLOCKED&&strstr(tuner.restoration_reason,"Mining is paused"));
        assert(tune_request(true,"{\"action\":\"restore-and-resume\"}").code==200);
        assert(writes==before && tuner.recovery_requested && !host_resume_requests);
        expect_error("{\"action\":\"restore-and-resume\"}","recovery-already-requested");
        cJSON *json=report(),*recovery=cJSON_GetObjectItem(cJSON_GetObjectItem(json,"status"),"recovery");
        assert(cJSON_IsTrue(cJSON_GetObjectItem(recovery,"requested")));cJSON_Delete(json);
        autotune_gamma_recovery_tick(&gs);assert(host_resume_requests==1 && !tuner.backup_restored);
        recovery_finish();assert(!erases);
    } else if(!strcmp(name,"recovery-stop-and-revocation")) {
        start();confirmed_off();assert(tune_request(true,"{\"action\":\"restore-and-resume\"}").code==200);
        const uint32_t epoch=tuner.owner_epoch;const uint64_t started=tuner.restoration_started;const unsigned before=writes;
        assert(tune_request(true,"{\"action\":\"stop\"}").code==200);
        assert(tune_request(true,"{\"action\":\"stop\"}").code==200);
        assert(tuner.recovery_requested&&tuner.owner_epoch==epoch&&tuner.restoration_started==started&&writes==before);
        mining_schedule_manual_override(true);autotune_gamma_recovery_tick(&gs);
        assert(!host_resume_requests && tuner.recovery_phase==RECOVERY_BLOCKED);
        assert(tune_request(true,"{\"action\":\"restore-and-resume\"}").code==200);
        autotune_gamma_interrupt("firmware-update");autotune_gamma_recovery_tick(&gs);
        assert(!host_resume_requests && tuner.recovery_phase==RECOVERY_BLOCKED);
    } else if(!strcmp(name,"recovery-off-faults")) {
        start();confirmed_off();unsigned before=writes;
        gs.SYSTEM_MODULE.hardware_fault=true;
        assert(tune_request(true,"{\"action\":\"restore-and-resume\"}").code==200);
        autotune_gamma_recovery_tick(&gs);assert(!host_resume_requests && writes==before && gs.SYSTEM_MODULE.hardware_fault);
        gs.SYSTEM_MODULE.hardware_fault=false;off_status_vout=1;
        assert(tune_request(true,"{\"action\":\"restore-and-resume\"}").code==200);
        autotune_gamma_recovery_tick(&gs);assert(!host_resume_requests && writes==before);
        off_status_vout=0;off_vout=.2;
        assert(tune_request(true,"{\"action\":\"restore-and-resume\"}").code==200);
        autotune_gamma_recovery_tick(&gs);assert(tuner.recovery_requested && !host_resume_requests && writes==before);
        host_now+=FWT_APPLY_TIMEOUT_US;autotune_gamma_recovery_tick(&gs);
        assert(!tuner.recovery_requested && tuner.recovery_phase==RECOVERY_BLOCKED && !host_resume_requests);
        off_vout=0;host_internal_stack=false;
        assert(tune_request(true,"{\"action\":\"restore-and-resume\"}").code==200);
        autotune_gamma_recovery_tick(&gs);assert(!host_resume_requests && writes==before);
    } else if(!strcmp(name,"recovery-cooling-and-storage")) {
        start();confirmed_off();host_asic_temp=72;unsigned before=writes;
        assert(tune_request(true,"{\"action\":\"restore-and-resume\"}").code==200);
        autotune_gamma_recovery_tick(&gs);assert(tuner.recovery_requested && !host_resume_requests && writes==before);
        host_asic_temp=50;host_now+=TUNE_POLL_US;fail_recovery_journal_read=true;
        autotune_gamma_recovery_tick(&gs);assert(!tuner.recovery_requested && !host_resume_requests && !tuner.backup_restored);
        fail_recovery_journal_read=false;
        assert(tune_request(true,"{\"action\":\"restore-and-resume\"}").code==200);
        autotune_gamma_recovery_tick(&gs);recovery_finish();
    } else if(!strcmp(name,"recovery-off-settling")) {
        start();confirmed_off();off_vout=.15;off_iout=.3;host_fan_ok=false;unsigned before=writes;
        assert(tune_request(true,"{\"action\":\"restore-and-resume\"}").code==200);
        autotune_gamma_recovery_tick(&gs);assert(tuner.recovery_requested && !host_resume_requests && writes==before);
        cJSON *json=report(),*d=cJSON_GetObjectItem(json,"diagnostics");
        assert(cJSON_GetObjectItem(d,"recoveryOffVoutV")->valuedouble>.1 && cJSON_GetObjectItem(d,"recoveryOffFanRpm")->valuedouble==0);cJSON_Delete(json);
        off_vout=0;off_iout=-.01;host_fan_ok=true;host_now+=TUNE_POLL_US;
        autotune_gamma_recovery_tick(&gs);recovery_finish();
    } else if(!strcmp(name,"recovery-unpowered-diode")) {
        tuner.core.config.minFrequencyMHz=tuner.core.config.maxFrequencyMHz;
        start();host_asic_temp=61;step(false);confirmed_off();host_asic_temp=127.75f;off_vout=.0234;off_iout=-.32373;
        unsigned before=writes;autotune_gamma_recovery_tick(&gs);
        httpd_req_t activity={.code=200};assert(get_activity(&activity)==ESP_OK && strstr(activity.reply,"\"fault\":false"));
        assert(tuner.recovery_requested && !host_resume_requests && writes==before);
        assert(gs.SYSTEM_MODULE.tuning_recovery_work_blocked && !mining_state_work_allowed(&gs));
        cJSON *json=report(),*d=cJSON_GetObjectItem(json,"diagnostics");
        assert(cJSON_IsNull(cJSON_GetObjectItem(d,"recoveryOffAsicTempC")));
        assert(cJSON_GetObjectItem(d,"recoveryOffIoutA")->valuedouble<-.3);cJSON_Delete(json);
        host_now+=UINT64_C(30000000);autotune_gamma_recovery_tick(&gs);
        assert(host_resume_requests==1);normal_resume_completed();
        assert(!mining_state_work_allowed(&gs) && autotune_gamma_transition_allowed(&gs));
        host_asic_temp=59;host_now+=TUNE_POLL_US;autotune_gamma_recovery_tick(&gs);
        assert(!mining_state_work_allowed(&gs) && tuner.recovery_requested);
        host_asic_temp=54;host_vrm_temp=50;host_now+=TUNE_POLL_US;autotune_gamma_recovery_tick(&gs);
        assert(mining_state_work_allowed(&gs));
        for(unsigned i=0;i<4 && tuner.recovery_requested;++i) {host_now+=TUNE_POLL_US;autotune_gamma_recovery_tick(&gs);}
        assert(tuner.recovery_phase==RECOVERY_COMPLETE && tuner.backup_restored && !tuner.core.active);
    } else if(!strcmp(name,"recovery-prework-cancel")) {
        start();confirmed_off();assert(tune_request(true,"{\"action\":\"restore-and-resume\"}").code==200);
        autotune_gamma_recovery_tick(&gs);normal_resume_completed();
        assert(!mining_state_work_allowed(&gs));
        mining_schedule_manual_override(false);autotune_gamma_recovery_tick(&gs);
        assert(!tuner.recovery_requested && gs.SYSTEM_MODULE.tuning_recovery_work_blocked && !mining_state_work_allowed(&gs));
        autotune_gamma_recovery_tick(&gs);assert(!gs.SYSTEM_MODULE.mining_paused && !mining_state_work_allowed(&gs));
        confirmed_off();autotune_gamma_recovery_tick(&gs);assert(!gs.SYSTEM_MODULE.tuning_recovery_work_blocked && host_resume_requests==1);
    } else if(!strcmp(name,"recovery-schedule-revision")) {
        start();confirmed_off();assert(tune_request(true,"{\"action\":\"restore-and-resume\"}").code==200);
        autotune_gamma_recovery_tick(&gs);normal_resume_completed();
        const uint32_t epoch=atomic_load(&manual_epoch);const unsigned pauses=host_pause_requests,stored=writes;
        ++host_schedule_revision;gs.SYSTEM_MODULE.mining_paused=false;
        autotune_gamma_recovery_tick(&gs);autotune_gamma_recovery_tick(&gs);
        assert(atomic_load(&manual_epoch)>epoch&&host_pause_requests==pauses&&writes==stored);
        assert(!tuner.recovery_requested&&!tuner.restore_held&&!gs.SYSTEM_MODULE.mining_paused);
        assert(gs.SYSTEM_MODULE.tuning_recovery_work_blocked&&!mining_state_work_allowed(&gs));
        assert(tuner.restoration_phase==RESTORE_BLOCKED&&strstr(tuner.restoration_reason,"newer schedule")&&strstr(tuner.restoration_reason,"confirmed off"));
        cJSON *json=report(),*status=cJSON_GetObjectItem(json,"status");
        assert(cJSON_IsFalse(cJSON_GetObjectItem(status,"ownsSettings"))&&cJSON_IsFalse(cJSON_GetObjectItem(status,"active")));cJSON_Delete(json);
        uint16_t mv=0;float mhz=0;assert(!autotune_gamma_select_target(&gs,&mv,&mhz));
        assert(current_stop_request(&gs)&&autotune_gamma_transition_cancelled());
        autotune_gamma_interrupt("mining-paused-or-faulted");
        /* The normal power owner completes its stop without changing the new
         * schedule, then reports idle only after power-off confirmation. */
        gs.ASIC_initalized=false;host_applied_paused=true;
        gs.POWER_MANAGEMENT_MODULE.applied_core_voltage_mv=0;gs.POWER_MANAGEMENT_MODULE.frequency_value=50;
        autotune_gamma_owner_idle();autotune_gamma_recovery_tick(&gs);
        assert(!gs.SYSTEM_MODULE.tuning_recovery_work_blocked&&host_pause_requests==pauses&&host_resume_requests==1);
        assert(!gs.SYSTEM_MODULE.mining_paused&&!current_stop_request(&gs));
        normal_resume_completed();assert(mining_state_work_allowed(&gs));
    } else if(!strcmp(name,"recovery-prework-timeout")) {
        start();confirmed_off();assert(tune_request(true,"{\"action\":\"restore-and-resume\"}").code==200);
        autotune_gamma_recovery_tick(&gs);normal_resume_completed();host_asic_temp=75;
        host_now+=TUNE_POLL_US;autotune_gamma_recovery_tick(&gs);assert(!mining_state_work_allowed(&gs));
        host_now+=FWT_APPLY_TIMEOUT_US;autotune_gamma_recovery_tick(&gs);
        assert(!tuner.recovery_requested && tuner.recovery_phase==RECOVERY_BLOCKED && gs.SYSTEM_MODULE.mining_paused);
        assert(gs.SYSTEM_MODULE.tuning_recovery_work_blocked && !mining_state_work_allowed(&gs));
        confirmed_off();autotune_gamma_recovery_tick(&gs);assert(!gs.SYSTEM_MODULE.tuning_recovery_work_blocked && host_resume_requests==1);
    } else if(!strcmp(name,"recovery-commit-race")) {
        start();confirmed_off();unsigned before=writes;revoke_on_backup_read=true;
        assert(tune_request(true,"{\"action\":\"restore-and-resume\"}").code==200);
        autotune_gamma_recovery_tick(&gs);assert(!host_resume_requests && writes==before && !tuner.recovery_requested);
    } else if(!strcmp(name,"recovery-live-failure")) {
        start();confirmed_off();assert(tune_request(true,"{\"action\":\"restore-and-resume\"}").code==200);
        autotune_gamma_recovery_tick(&gs);normal_resume_completed();sensor_vout_delta=.08;
        for (unsigned i=0;i<20 && tuner.recovery_requested;++i) {host_now+=TUNE_POLL_US;autotune_gamma_recovery_tick(&gs);}
        assert(tuner.recovery_phase==RECOVERY_BLOCKED && !tuner.recovery_requested && !tuner.backup_restored);
        assert(gs.SYSTEM_MODULE.mining_paused && host_resume_requests==1);
        autotune_gamma_recovery_tick(&gs);assert(host_resume_requests==1);
    } else if(!strcmp(name,"recovery-floor-event")) {
        tuner.core.config.minFrequencyMHz=tuner.core.config.maxFrequencyMHz;
        start();host_asic_temp=59.5f;step(false);
        assert(fault_stops==0 && gs.SYSTEM_MODULE.mining_paused && !gs.SYSTEM_MODULE.hardware_fault);
        assert(fwt_history_latest(&tuner.history)->kind==FWT_EVENT_REJECTED);
        const fwt_event rejected=*fwt_history_latest(&tuner.history);
        assert(rejected.outcome==FWT_OUTCOME_FAILED && rejected.hasConditions && !rejected.hasMeasurement && !rejected.hasScore);
        /* Ordinary power-owner stop preserves the soft tuner limit reason. */
        autotune_gamma_interrupt("mining-paused-or-faulted");
        assert(tuner.core.state==FWT_FAULTED && !strcmp(tuner.core.reason,"cannot-reduce-load-within-limits"));
        confirmed_off();unsigned before=writes;
        autotune_gamma_recovery_tick(&gs);
        assert(tuner.recovery_requested && tuner.recovery_automatic && !host_resume_requests && writes==before);
        assert(!strcmp(tuner.recovery_reason,"automatic-waiting-for-cooling"));
        host_asic_temp=54;host_now+=TUNE_POLL_US;
        autotune_gamma_recovery_tick(&gs);recovery_finish();
        assert(tuner.core.state==FWT_DISABLED && tuner.automatic_recovery_attempted);
        assert(fwt_history_latest(&tuner.history)->id==rejected.id);
        cJSON *json=report(),*events=cJSON_GetObjectItem(cJSON_GetObjectItem(json,"status"),"events");
        cJSON *last=cJSON_GetArrayItem(events,cJSON_GetArraySize(events)-1);
        assert(cJSON_IsObject(cJSON_GetObjectItem(last,"conditions")) && cJSON_IsNull(cJSON_GetObjectItem(last,"measurement")));
        cJSON_Delete(json);
    } else if(!strcmp(name,"automatic-errors-exhausted")) {
        start();host_error_percentage=2;
        for(unsigned n=0;n<1000 && !tuner.recovery_requested;++n) step(true);
        assert(tuner.recovery_requested && tuner.recovery_automatic && tuner.core.recoveryAttempts==7);
        assert(!strcmp(tuner.core.reason,"error-recovery-exhausted") && !fault_stops && !gs.SYSTEM_MODULE.hardware_fault);
        const fwt_event rejected=*fwt_history_latest(&tuner.history);
        assert(rejected.kind==FWT_EVENT_REJECTED && rejected.outcome==FWT_OUTCOME_FAILED);
        assert(!strcmp(rejected.reason,"error-recovery-exhausted") && rejected.hasConditions && !rejected.hasMeasurement && !rejected.hasScore);
        assert(rejected.errorPercentage==2 && rejected.worstErrorPercentage==2 && rejected.hashRateGHs>0 && rejected.powerW>0);
        autotune_gamma_interrupt("mining-paused-or-faulted");
        assert(tuner.core.state==FWT_FAULTED && !strcmp(tuner.core.reason,"error-recovery-exhausted"));
        confirmed_off();host_error_percentage=0;host_asic_temp=50;host_vrm_temp=50;
        autotune_gamma_recovery_tick(&gs);recovery_finish();
        assert(host_resume_requests==1 && !fault_stops && tuner.automatic_recovery_attempted);
        assert(fwt_history_latest(&tuner.history)->id==rejected.id);
        autotune_gamma_recovery_tick(&gs);assert(host_resume_requests==1 && !tuner.recovery_requested);
    } else if(!strcmp(name,"automatic-floor-cancel")) {
        tuner.core.config.minFrequencyMHz=tuner.core.config.maxFrequencyMHz;
        start();host_asic_temp=59.5f;step(false);confirmed_off();
        assert(tuner.recovery_requested && tuner.recovery_automatic);
        mining_schedule_manual_override(true);host_asic_temp=50;
        autotune_gamma_recovery_tick(&gs);
        assert(!tuner.recovery_requested && !host_resume_requests && tuner.recovery_phase==RECOVERY_BLOCKED);
        autotune_gamma_recovery_tick(&gs);assert(!host_resume_requests);
    } else if(!strcmp(name,"automatic-floor-errors")) {
        tuner.core.config.minFrequencyMHz=tuner.core.config.maxFrequencyMHz;
        start();host_asic_temp=59.5f;step(false);confirmed_off();host_asic_temp=50;
        autotune_gamma_recovery_tick(&gs);normal_resume_completed();host_error_percentage=2;
        for (unsigned i=0;i<20 && tuner.recovery_requested;++i) {host_now+=TUNE_POLL_US;autotune_gamma_recovery_tick(&gs);}
        assert(!tuner.recovery_requested && tuner.recovery_phase==RECOVERY_COMPLETE && tuner.backup_restored);
        assert(!gs.SYSTEM_MODULE.mining_paused && host_resume_requests==1 && !fault_stops);
        assert(tuner.restoration_phase==RESTORE_COMPLETE&&tuner.restoration_quality_warning&&strstr(tuner.restoration_quality_warning,"ASIC errors"));
    } else if(!strcmp(name,"recovery-durable-formats")) {
        start();assert(nvs_config_apply_autotune_restore(tuner.backup,BACKUP_FIELDS)==ESP_OK);
        assert(journal()->bytes[4]==1 && nvs_config_verify_autotune_restore(tuner.backup,BACKUP_FIELDS)==ESP_OK);
        put(1,"minfanspeed",3,NULL,30);
        assert(nvs_config_verify_autotune_restore(tuner.backup,BACKUP_FIELDS)!=ESP_OK);
        put(1,"minfanspeed",1,"wrong-type",0);
        assert(nvs_config_verify_autotune_restore(tuner.backup,BACKUP_FIELDS)!=ESP_OK);
        put(1,"minfanspeed",3,NULL,25);
        NvsConfigMutation adjusted[BACKUP_FIELDS];memcpy(adjusted,tuner.backup,sizeof(adjusted));adjusted[3].value.u16=70;
        assert(nvs_config_apply_autotune_restore(adjusted,BACKUP_FIELDS)==ESP_OK);
        assert(journal()->bytes[4]==2 && nvs_config_verify_autotune_restore(adjusted,BACKUP_FIELDS)==ESP_OK);
        assert(nvs_config_verify_autotune_restore(tuner.backup,BACKUP_FIELDS)!=ESP_OK);
        journal()->bytes[12]^=1;assert(nvs_config_verify_autotune_restore(adjusted,BACKUP_FIELDS)!=ESP_OK);
        assert(!erases);
    } else if(!strcmp(name,"waiver")) {
        unsigned before=writes;
        expect_error("{\"action\":\"start\"}","waiver-required");
        expect_error("{\"action\":\"start\",\"waiverAccepted\":1,\"waiverVersion\":1}","waiver-required");
        expect_error("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":2}","waiver-required");
        assert(writes==before&&!tuner.core.owns&&!tuner.backup_available);
    } else if(!strcmp(name,"conservative-defaults")) {
        assert(tuner.core.config.maxAsicTempC==60 && tuner.core.config.maxVrmTempC==70);
        fwt_config config=tuner.core.config;config.maxAsicTempC=63;config.maxVrmTempC=75;
        assert(configure_request(config).code==200);reboot_runtime();
        assert(tuner.core.config.maxAsicTempC==63 && tuner.core.config.maxVrmTempC==75);
        assert(!tuner.core.active && !tuner.core.owns);
    } else if(!strcmp(name,"read-only")) {
        unsigned before=writes,reads=sensor_reads;
        cJSON *schema=report();assert(schema);
        const cJSON *guidance=cJSON_GetObjectItemCaseSensitive(schema,"guidance");
        const char *asic=cJSON_GetObjectItemCaseSensitive(guidance,"asic")->valuestring;
        const char *vrm=cJSON_GetObjectItemCaseSensitive(guidance,"vrm")->valuestring;
        assert(strstr(asic,"60 C") && strstr(vrm,"70 C") && !strstr(vrm,"Initial tuning ceiling 85"));
        const cJSON *limits=cJSON_GetObjectItemCaseSensitive(schema,"limits");
        const char *limit_keys[]={"minFrequencyMHz","maxFrequencyMHz","minVoltageMv","maxVoltageMv",
            "maxAsicTempC","maxVrmTempC","maxPowerW","frequencyStepMHz","voltageStepMv"};
        assert(cJSON_GetArraySize(limits)==10);
        assert(cJSON_GetObjectItemCaseSensitive(limits,"maxFrequencyMHz")->valuedouble==690);
        assert(cJSON_GetObjectItemCaseSensitive(limits,"advancedMaxFrequencyMHz")->valuedouble==1200);
        for(unsigned i=0;i<9;++i) {const cJSON *value=cJSON_GetObjectItemCaseSensitive(limits,limit_keys[i]);assert(cJSON_IsNumber(value)&&value->valuedouble>0);}
        cJSON_Delete(schema);
        for(unsigned i=0;i<10;++i) {httpd_req_t req=tune_request(false,NULL);assert(req.code==200&&!strstr(req.reply,"fixture-pass")&&!strstr(req.reply,"020000000001"));}
        assert(writes==before&&sensor_reads==reads&&!tuner.core.owns);
        authorized=false;
        httpd_req_t req=tune_request(true,"{\"action\":\"stop\"}");assert(req.code==401&&req.received==0&&writes==before);
    } else if(!strcmp(name,"passive-activity")) {
        const unsigned before=writes,reads=sensor_reads;
        const uint64_t observer=tuner.observer_until;
        const fwt_controller unchanged=tuner.core;
        tuner.last_owner_tick=host_now;
        for(unsigned i=0;i<4;++i) {
            httpd_req_t req={.code=200};assert(get_activity(&req)==ESP_OK&&req.code==200);
            cJSON *json=cJSON_Parse(req.reply);assert(json);
            assert(cJSON_IsTrue(cJSON_GetObjectItem(json,"available")));
            assert(cJSON_IsFalse(cJSON_GetObjectItem(json,"active")));
            assert(cJSON_GetObjectItem(json,"controllerAgeSeconds")->valuedouble==0);
            cJSON *restoration=cJSON_GetObjectItem(json,"restoration");
            assert(restoration&&cJSON_GetArraySize(restoration)==5);
            assert(!strcmp(cJSON_GetObjectItem(restoration,"phase")->valuestring,"idle"));
            assert(cJSON_IsNull(cJSON_GetObjectItem(restoration,"elapsedSeconds"))&&cJSON_IsNull(cJSON_GetObjectItem(restoration,"qualityWarning")));
            cJSON_Delete(json);
        }
        pthread_mutex_lock(&tuner_mutex);
        httpd_req_t busy={.code=200};assert(get_activity(&busy)==ESP_OK&&strstr(busy.reply,"\"available\":false"));
        pthread_mutex_unlock(&tuner_mutex);
        tuner.initialized=false;httpd_req_t cold={.code=200};assert(get_activity(&cold)==ESP_OK&&strstr(cold.reply,"\"available\":false"));tuner.initialized=true;
        host_now+=16*FWT_SECOND;httpd_req_t stale={.code=200};assert(get_activity(&stale)==ESP_OK);
        cJSON *json=cJSON_Parse(stale.reply);assert(json&&cJSON_GetObjectItem(json,"controllerAgeSeconds")->valuedouble==16);cJSON_Delete(json);
        authorized=false;httpd_req_t denied={.code=200};assert(get_activity(&denied)==ESP_OK&&denied.code==401);authorized=true;
        assert(writes==before&&sensor_reads==reads&&tuner.observer_until==observer&&!memcmp(&unchanged,&tuner.core,sizeof(unchanged)));
        for(unsigned phase=RESTORE_WAITING;phase<=RESTORE_BLOCKED;++phase) {
            tuner.restoration_phase=phase;tuner.restoration_started=host_now-10*FWT_SECOND;
            tuner.restoration_finished=phase>=RESTORE_COMPLETE?host_now-2*FWT_SECOND:0;
            tuner.restoration_reason="cached restoration reason";
            tuner.restoration_quality_warning=phase==RESTORE_COMPLETE?"cached ASIC quality warning":NULL;
            const GammaTuner cached=tuner;
            httpd_req_t activity={.code=200};assert(get_activity(&activity)==ESP_OK&&activity.code==200);
            cJSON *compact=cJSON_Parse(activity.reply),*restoration=cJSON_GetObjectItem(compact,"restoration");
            assert(restoration&&cJSON_GetArraySize(restoration)==5);
            assert(!strcmp(cJSON_GetObjectItem(restoration,"phase")->valuestring,restoration_phase_name(phase)));
            assert(!strcmp(cJSON_GetObjectItem(restoration,"reason")->valuestring,"cached restoration reason"));
            assert(cJSON_GetObjectItem(restoration,"elapsedSeconds")->valueint==(phase>=RESTORE_COMPLETE?8:10));
            assert(cJSON_GetObjectItem(restoration,"timeoutSeconds")->valueint==180);
            if(phase==RESTORE_COMPLETE)assert(!strcmp(cJSON_GetObjectItem(restoration,"qualityWarning")->valuestring,"cached ASIC quality warning"));
            else assert(cJSON_IsNull(cJSON_GetObjectItem(restoration,"qualityWarning")));
            cJSON_Delete(compact);
            assert(!memcmp(&cached,&tuner,sizeof(cached))&&writes==before&&sensor_reads==reads);
        }
    } else if(!strcmp(name,"passive-owned-fault")) {
        start();tuner.last_owner_tick=host_now;
        httpd_req_t req={.code=200};assert(get_activity(&req)==ESP_OK);
        cJSON *json=cJSON_Parse(req.reply);assert(cJSON_IsTrue(cJSON_GetObjectItem(json,"active"))&&cJSON_IsTrue(cJSON_GetObjectItem(json,"ownsSettings")));cJSON_Delete(json);
        gs.SYSTEM_MODULE.hardware_fault=true;req=(httpd_req_t){.code=200};assert(get_activity(&req)==ESP_OK);
        json=cJSON_Parse(req.reply);assert(cJSON_IsTrue(cJSON_GetObjectItem(json,"fault")));cJSON_Delete(json);
        gs.SYSTEM_MODULE.hardware_fault=false;autotune_gamma_manual_override();req=(httpd_req_t){.code=200};assert(get_activity(&req)==ESP_OK);
        json=cJSON_Parse(req.reply);assert(cJSON_IsFalse(cJSON_GetObjectItem(json,"active"))&&cJSON_IsFalse(cJSON_GetObjectItem(json,"ownsSettings")));cJSON_Delete(json);
        cJSON *full=report(),*status=cJSON_GetObjectItem(full,"status");
        assert(cJSON_IsFalse(cJSON_GetObjectItem(status,"active"))&&cJSON_IsFalse(cJSON_GetObjectItem(status,"ownsSettings")));
        cJSON_Delete(full);GammaAutotuneDisplaySnapshot snapshot;
        assert(autotune_gamma_try_display_snapshot(&snapshot,host_now)&&!snapshot.active&&!snapshot.ownsSettings);
        assert(tuner.core.active && tuner.core.owns); /* Projection did not mutate the owner. */
    } else if(!strcmp(name,"decision-history")) {
        start();assert(fwt_history_count(&tuner.history)==1);
        assert(fwt_history_latest(&tuner.history)->kind==FWT_EVENT_STARTED);
        for(unsigned i=0;i<500&&tuner.core.owns;++i) step(true);
        assert(fwt_history_count(&tuner.history)==FWT_EVENT_CAPACITY);
        uint32_t previous=0;bool measured=false,applied=false;
        for(unsigned i=0;i<fwt_history_count(&tuner.history);++i) {
            const fwt_event *e=fwt_history_at(&tuner.history,i);assert(e&&e->id>previous);previous=e->id;
            measured|=e->hasMeasurement;applied|=e->kind==FWT_EVENT_ADJUSTMENT&&e->outcome==FWT_OUTCOME_APPLIED;
        }
        assert(measured&&applied);
        const fwt_controller unchanged=tuner.core;const uint32_t latest=tuner.history.latestSequence;
        for(unsigned i=0;i<4;++i) {
            cJSON *schema=report();assert(schema);cJSON *status=cJSON_GetObjectItem(schema,"status"),*events=cJSON_GetObjectItem(status,"events");
            assert(cJSON_IsArray(events)&&cJSON_GetArraySize(events)==8&&cJSON_GetObjectItem(status,"eventCapacity")->valueint==8);
            cJSON *event=cJSON_GetArrayItem(events,7);assert(cJSON_GetObjectItem(event,"id")->valuedouble==latest&&cJSON_IsObject(cJSON_GetObjectItem(event,"point")));
            cJSON_Delete(schema);
        }
        assert(tuner.history.latestSequence==latest&&!memcmp(&unchanged,&tuner.core,sizeof(unchanged)));
        reboot_runtime();assert(!tuner.core.active&&!tuner.core.owns&&fwt_history_count(&tuner.history)==0);
    } else if(!strcmp(name,"search-presentation")) {
        const unsigned before=writes,reads=sensor_reads;
        cJSON *schema=report();assert(schema);
        cJSON *search=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(schema,"status"),"search");
        assert(search&&cJSON_GetArraySize(search)==9);
        assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(search,"phaseElapsedSeconds")));
        assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(search,"minimumSeconds")));
        assert(cJSON_GetObjectItemCaseSensitive(search,"minimumSamples")->valueint==8);
        assert(cJSON_GetObjectItemCaseSensitive(search,"roundExperimentLimit")->valueint==24);
        assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(search,"reference"))&&cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(search,"best")));
        cJSON_Delete(schema);
        host_now+=UINT64_C(90000000);
        tuner.core.state=FWT_BASELINE;tuner.core.owns=tuner.core.active=true;tuner.override_reason=NULL;
        tuner.core.observationStartedUs=host_now-UINT64_C(50000000);
        tuner.core.samples=7;tuner.core.roundExperiments=3;tuner.core.rejected=2;
        tuner.core.bestAvailable=true;tuner.core.baselineHash=1000;tuner.core.baselinePower=20;
        tuner.core.bestHash=1050;tuner.core.bestPower=18;
        fwt_controller unchanged=tuner.core;
        schema=report();search=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(schema,"status"),"search");
        assert(cJSON_GetObjectItemCaseSensitive(search,"phaseElapsedSeconds")->valuedouble==50);
        assert(cJSON_GetObjectItemCaseSensitive(search,"minimumSeconds")->valuedouble==75);
        assert(cJSON_GetObjectItemCaseSensitive(search,"samples")->valueint==7);
        assert(cJSON_GetObjectItemCaseSensitive(search,"roundExperiments")->valueint==3);
        assert(cJSON_GetObjectItemCaseSensitive(search,"rejectedWindows")->valueint==2);
        cJSON *reference=cJSON_GetObjectItemCaseSensitive(search,"reference"),*best=cJSON_GetObjectItemCaseSensitive(search,"best");
        assert(cJSON_GetObjectItemCaseSensitive(reference,"hashRateGHs")->valuedouble==1000);
        assert(cJSON_GetObjectItemCaseSensitive(reference,"powerW")->valuedouble==20);
        assert(cJSON_GetObjectItemCaseSensitive(best,"hashRateGHs")->valuedouble==1050);
        assert(cJSON_GetObjectItemCaseSensitive(best,"powerW")->valuedouble==18);
        cJSON_Delete(schema);assert(!memcmp(&unchanged,&tuner.core,sizeof(unchanged)));
        tuner.restore_pending=true;schema=report();search=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(schema,"status"),"search");
        assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(search,"phaseElapsedSeconds"))&&cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(search,"reference")));
        assert(cJSON_GetObjectItemCaseSensitive(search,"roundExperiments")->valueint==3);cJSON_Delete(schema);
        tuner.restore_pending=false;tuner.override_reason="manual-control";schema=report();search=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(schema,"status"),"search");
        assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(search,"minimumSeconds"))&&cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(search,"best")));cJSON_Delete(schema);
        assert(writes==before&&sensor_reads==reads);
    } else if(!strcmp(name,"display-snapshot")) {
        const unsigned before=writes,reads=sensor_reads;
        const uint64_t observer=tuner.observer_until;
        GammaAutotuneDisplaySnapshot snapshot;
        assert(sizeof(snapshot)<=256);
        pthread_mutex_lock(&tuner_mutex);
        memset(&snapshot,0x7f,sizeof(snapshot));
        assert(!autotune_gamma_try_display_snapshot(&snapshot,host_now));
        assert(!snapshot.available&&!snapshot.currentAvailable&&snapshot.current.frequencyMHz==0);
        pthread_mutex_unlock(&tuner_mutex);
        assert(autotune_gamma_try_display_snapshot(&snapshot,host_now));
        assert(snapshot.available&&snapshot.supported&&snapshot.telemetryFresh&&snapshot.currentAvailable);
        assert(snapshot.current.frequencyMHz==625.125&&snapshot.current.coreVoltageMv==1173);
        assert(!snapshot.active&&!snapshot.ownsSettings&&!snapshot.search.timed);
        assert(autotune_gamma_try_display_snapshot(&snapshot,host_now+TUNE_FRESH_US+1));
        assert(snapshot.available&&!snapshot.telemetryFresh&&!snapshot.currentAvailable&&snapshot.current.frequencyMHz==0);
        tuner.initialized=false;assert(autotune_gamma_try_display_snapshot(&snapshot,host_now));assert(!snapshot.available&&!snapshot.supported);
        assert(writes==before&&sensor_reads==reads&&tuner.observer_until==observer);
    } else if(!strcmp(name,"disabled-reason")) {
        const unsigned before=writes,reads=sensor_reads;
        tuner.override_reason="mining-paused-or-faulted";
        cJSON *schema=report();cJSON *status=cJSON_GetObjectItemCaseSensitive(schema,"status");
        assert(!strcmp(cJSON_GetObjectItemCaseSensitive(status,"reason")->valuestring,"disabled"));cJSON_Delete(schema);
        assert(!strcmp(tuner.override_reason,"mining-paused-or-faulted")&&!tuner.input.fault);
        host_now+=TUNE_FRESH_US+1;schema=report();status=cJSON_GetObjectItemCaseSensitive(schema,"status");
        assert(!strcmp(cJSON_GetObjectItemCaseSensitive(status,"reason")->valuestring,"mining-paused-or-faulted"));cJSON_Delete(schema);
        tuner.input.capturedAtUs=host_now;tuner.frozen=true;schema=report();status=cJSON_GetObjectItemCaseSensitive(schema,"status");
        assert(!strcmp(cJSON_GetObjectItemCaseSensitive(status,"reason")->valuestring,"mining-paused-or-faulted"));cJSON_Delete(schema);
        tuner.frozen=false;tuner.restore_pending=true;schema=report();status=cJSON_GetObjectItemCaseSensitive(schema,"status");
        assert(strcmp(cJSON_GetObjectItemCaseSensitive(status,"reason")->valuestring,"disabled"));cJSON_Delete(schema);
        tuner.restore_pending=false;gs.SYSTEM_MODULE.hardware_fault=true;schema=report();status=cJSON_GetObjectItemCaseSensitive(schema,"status");
        assert(strstr(cJSON_GetObjectItemCaseSensitive(status,"reason")->valuestring,"hardware protection"));
        assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(status,"telemetry"),"fault")));cJSON_Delete(schema);
        gs.SYSTEM_MODULE.hardware_fault=false;tuner.override_reason="manual-control";schema=report();status=cJSON_GetObjectItemCaseSensitive(schema,"status");
        assert(!strcmp(cJSON_GetObjectItemCaseSensitive(status,"reason")->valuestring,"manual-control"));cJSON_Delete(schema);
        assert(writes==before&&sensor_reads==reads);
    } else if(!strcmp(name,"strict-diagnostic")) {
        unsigned before=writes,reads=sensor_reads;
        tuner.strict.status_word=0x1234;tuner.strict.status_vout=0x10;
        tuner.strict.status_iout=0x20;tuner.strict.status_input=0x08;
        tuner.strict.status_temperature=0x40;tuner.strict.status_cml=0x80;
        tuner.input.fault=true;tuner.override_reason="mining-paused-or-faulted";
        const uint64_t success_at=tuner.strict_success_at;
        host_now+=UINT64_C(3000000);
        cJSON *schema=report();assert(schema);
        const cJSON *d=cJSON_GetObjectItemCaseSensitive(schema,"diagnostics");
        const char *keys[]={"statusWord","statusVout","statusIout","statusInput","statusTemperature","statusCml"};
        const unsigned values[]={0x1234,0x10,0x20,0x08,0x40,0x80};
        for(unsigned i=0;i<6;++i) assert(cJSON_GetObjectItemCaseSensitive(d,keys[i])->valueint==values[i]);
        assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(d,"sensorReadSucceeded")));
        assert(cJSON_GetObjectItemCaseSensitive(d,"vinV")->valuedouble==tuner.strict.vin);
        assert(cJSON_GetObjectItemCaseSensitive(d,"ioutA")->valuedouble==tuner.strict.iout);
        assert(cJSON_GetObjectItemCaseSensitive(d,"lastSuccessAgeSeconds")->valuedouble==3);
        assert(strstr(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(schema,"status"),"reason")->valuestring,"output-voltage warning or fault"));
        cJSON_Delete(schema);
        tuner.strict.status_vout=tuner.strict.status_iout=tuner.strict.status_input=tuner.strict.status_temperature=0;
        schema=report();assert(strstr(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(schema,"status"),"reason")->valuestring,"communication or internal fault"));cJSON_Delete(schema);
        assert(writes==before&&sensor_reads==reads&&tuner.input.fault&&tuner.strict.status_cml==0x80);
        sensors_ok=false;observe();assert(!tuner.sensor_ok&&tuner.strict_success_at==success_at);
        schema=report();d=cJSON_GetObjectItemCaseSensitive(schema,"diagnostics");
        assert(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(d,"sensorReadSucceeded")));
        assert(cJSON_GetObjectItemCaseSensitive(d,"lastSuccessAgeSeconds")->valuedouble==3);
        assert(cJSON_GetObjectItemCaseSensitive(d,"lastAttemptAgeSeconds")->valuedouble==0);
        assert(cJSON_GetObjectItemCaseSensitive(d,"statusCml")->valueint==0x80);
        cJSON_Delete(schema);assert(writes==before&&sensor_reads==reads+1);
        tuner.strict_success_at=0;schema=report();d=cJSON_GetObjectItemCaseSensitive(schema,"diagnostics");
        assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(d,"statusWord"))&&cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(d,"vinV"))&&cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(d,"lastSuccessAgeSeconds")));cJSON_Delete(schema);
    } else if(!strcmp(name,"backup-once")) {
        start();Entry *original=find(2,"backup");assert(original&&original->size);
        size_t length=original->size;unsigned char bytes[2048];memcpy(bytes,original->bytes,length);
        for(unsigned i=0;i<50&&!point_changes;++i) step(true);
        assert(point_changes>0&&counter_resets>0);
        autotune_gamma_manual_override();step(true);assert(!tuner.core.owns);
        gs.POWER_MANAGEMENT_MODULE.frequency_value=625.125f;gs.POWER_MANAGEMENT_MODULE.actual_frequency=625.125f;gs.POWER_MANAGEMENT_MODULE.applied_core_voltage_mv=1173;
        host_now+=TUNE_POLL_US;observe();start();
        assert(original->size==length&&!memcmp(bytes,original->bytes,length));
        reboot_runtime();assert(!tuner.core.owns&&tuner.backup_available);
        host_now+=TUNE_POLL_US;observe();assert(!tuner.backup_restored);
        host_now+=TUNE_POLL_US;observe();assert(tuner.backup_restored);
    } else if(!strcmp(name,"backup-failure")) {
        fail_set=true;expect_error("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}","backup-verification-failed");
        assert(!tuner.core.owns&&!tuner.backup_available&&!point_changes);fail_set=false;
        fail_commit=true;expect_error("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}","backup-verification-failed");
        assert(!tuner.core.owns&&!tuner.backup_available);fail_commit=false;start();
    } else if(!strcmp(name,"backup-binding")) {
        start();Entry *backup=find(2,"backup");assert(backup);
        cJSON *low=cJSON_ParseWithLength((const char *)backup->bytes,backup->size);assert(low);
        cJSON *settings=cJSON_GetObjectItemCaseSensitive(low,"settings");assert(settings);
        cJSON *frequency=cJSON_GetObjectItemCaseSensitive(settings,"frequencyMHz");assert(frequency);
        cJSON_SetNumberValue(frequency,50);
        const unsigned before=writes;
        assert(clock_command_valid(50)); /* Emergency encoding remains legal. */
        assert(!parse_backup(low)); /* It is not an admissible original tune. */
        assert(writes==before&&tuner.backup_available);
        cJSON_Delete(low);
        char *board=strstr((char *)backup->bytes,"601");assert(board);board[2]='2';
        reboot_runtime();assert(!tuner.backup_available&&!tuner.storage_ok&&!tuner.core.owns);
        expect_error("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}","storage-or-restore-unavailable");
        assert(!erases);
    } else if(!strcmp(name,"backup-invalid-snapshot")) {
        put(1,"manualfanspeed",3,NULL,101);assert(nvs_config_init()==ESP_OK);
        unsigned before=writes;
        expect_error("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}","backup-verification-failed");
        assert(writes==before&&!find(2,"backup")&&!tuner.backup_available);
        put(1,"manualfanspeed",3,NULL,100);assert(nvs_config_init()==ESP_OK);
        host_now+=TUNE_POLL_US;observe();start();
    } else if(!strcmp(name,"backup-cached-corruption") || !strcmp(name,"backup-cached-deletion")) {
        start();autotune_gamma_interrupt("manual-control");host_now+=TUNE_POLL_US;observe();
        Entry *record=find(2,"backup");assert(record);
        if(!strcmp(name,"backup-cached-deletion")) record->ns=99;
        else {char *voltage=strstr((char *)record->bytes,"1173");assert(voltage);voltage[3]='4';}
        unsigned before=writes;
        expect_error("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}","backup-verification-failed");
        assert(!tuner.core.owns&&writes==before&&tuner.backup[1].value.u16==1173);
    } else if(!strcmp(name,"wire-vout-drift")) {
        start();assert(step(false));assert(autotune_gamma_transition_allowed(&gs));
        sensor_vout_delta=-0.050;
        assert(!autotune_gamma_transition_allowed(&gs));
        sensor_vout_delta=0.050;
        assert(!autotune_gamma_transition_allowed(&gs));
        sensor_vout_delta=0;
        assert(autotune_gamma_transition_allowed(&gs));
        host_current_ok=false;assert(!autotune_gamma_transition_allowed(&gs));
    } else if(!strcmp(name,"manual-handoff")) {
        start();unsigned before=writes;
        autotune_gamma_manual_override();step(true);
        assert(!tuner.core.owns&&tuner.backup_available&&writes==before);
        for(unsigned i=0;i<20;++i) assert(!step(true));
    } else if(!strcmp(name,"hardware-policy-handoff")) {
        for(unsigned kind=0;kind<4;++kind) {
            start();assert(step(false));
            const uint32_t before=atomic_load(&manual_epoch);
            if(kind==0) nvs_config_set_i32(NVS_CONFIG_TEMP_OFFSET,nvs_config_get_i32(NVS_CONFIG_TEMP_OFFSET)+1);
            if(kind==1) nvs_config_set_bool(NVS_CONFIG_EMC_INTERNAL_TEMP,!nvs_config_get_bool(NVS_CONFIG_EMC_INTERNAL_TEMP));
            if(kind==2) nvs_config_set_u16(NVS_CONFIG_TPS546_FREQUENCY,nvs_config_get_u16(NVS_CONFIG_TPS546_FREQUENCY)+1);
            if(kind==3) nvs_config_set_float(NVS_CONFIG_TPS546_SCALE_LOOP,nvs_config_get_float(NVS_CONFIG_TPS546_SCALE_LOOP)+.001f);
            assert(atomic_load(&manual_epoch)!=before&&autotune_gamma_transition_cancelled());
            assert(!step(true)&&!tuner.core.owns&&tuner.backup_available);
            /* A Start racing a queued hardware change is revoked again when
             * that change reaches the actual cache/persistence boundary. */
            start();const uint32_t pending=atomic_load(&manual_epoch);
            drain_auth_write();assert(atomic_load(&manual_epoch)!=pending);
            assert(!step(true)&&!tuner.core.owns);
        }
        start();const uint32_t before=atomic_load(&manual_epoch);
        nvs_config_set_u64(NVS_CONFIG_BEST_DIFF,nvs_config_get_u64(NVS_CONFIG_BEST_DIFF)+1);
        drain_auth_write();assert(atomic_load(&manual_epoch)==before&&step(false)&&tuner.core.owns);
    } else if(!strcmp(name,"same-saved-intent")) {
        for(unsigned kind=0;kind<4;++kind) {
            start();assert(step(false));
            gs.POWER_MANAGEMENT_MODULE.frequency_value=618.875f;gs.POWER_MANAGEMENT_MODULE.actual_frequency=618.875f;
            host_now+=TUNE_POLL_US;
            unsigned before=writes;const uint32_t epoch=atomic_load(&manual_epoch);
            if(kind==0) nvs_config_set_float(NVS_CONFIG_ASIC_FREQUENCY,nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY));
            if(kind==1) nvs_config_set_u16(NVS_CONFIG_ASIC_VOLTAGE,nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE));
            if(kind==2) nvs_config_set_bool(NVS_CONFIG_AUTO_FAN_SPEED,nvs_config_get_bool(NVS_CONFIG_AUTO_FAN_SPEED));
            if(kind==3) {char *value=nvs_config_get_string(NVS_CONFIG_STRATUM_URL);nvs_config_set_string(NVS_CONFIG_STRATUM_URL,value);free(value);}
            assert(atomic_load(&manual_epoch)!=epoch&&!queue_pending&&writes==before);
            uint16_t voltage=1100;float frequency=618.875f;
            assert(!autotune_gamma_select_target(&gs,&voltage,&frequency)&&!tuner.core.owns);
            assert(voltage==1173&&frequency==625.125f&&tuner.backup_available&&writes==before);
            gs.POWER_MANAGEMENT_MODULE.frequency_value=gs.POWER_MANAGEMENT_MODULE.actual_frequency=625.125f;
            host_now+=TUNE_POLL_US;observe();
        }
        start();const uint32_t epoch=atomic_load(&manual_epoch);unsigned before=writes;
        nvs_config_set_u16(NVS_CONFIG_ASIC_FREQUENCY,580); /* Invalid key type. */
        nvs_config_set_u64(NVS_CONFIG_BEST_DIFF,nvs_config_get_u64(NVS_CONFIG_BEST_DIFF));
        assert(atomic_load(&manual_epoch)==epoch&&!queue_pending&&writes==before&&tuner.core.owns);
    } else if(!strcmp(name,"start-snapshot-interleave")) {
        start();autotune_gamma_interrupt("manual-control");step(false);
        revoke_after_snapshot=true;
        expect_error("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}","manual-control-changed");
        assert(!tuner.core.owns&&tuner.backup_available&&!point_changes);
        start();assert(tuner.core.owns);
    } else if(!strcmp(name,"manual-pair-interleave")) {
        start();uint16_t stale_mv=1173;float stale_mhz=625.125f;
        const NvsConfigMutation changes[]={
            {.key=NVS_CONFIG_ASIC_FREQUENCY,.type=TYPE_FLOAT,.value.f=575},
            {.key=NVS_CONFIG_ASIC_VOLTAGE,.type=TYPE_U16,.value.u16=1150}};
        assert(nvs_config_apply_atomic(changes,2)==ESP_OK);
        assert(!autotune_gamma_select_target(&gs,&stale_mv,&stale_mhz));
        assert(!tuner.core.owns&&stale_mv==1150&&stale_mhz==575);
    } else if(!strcmp(name,"restore-verify-only")) {
        start();nvs_config_set_bool(NVS_CONFIG_AUTO_FAN_SPEED,false);drain_auth_write();step(true);
        assert(!tuner.core.owns&&!nvs_config_get_bool(NVS_CONFIG_AUTO_FAN_SPEED));
        assert(tune_request(true,"{\"action\":\"stop\"}").code==200);
        const unsigned resets=counter_resets,frames=physical_uart_frames,rail=physical_i2c_frames;
        const uint64_t generation=tuner.generation;
        assert(step(true)&&tuner.restore_verify_only&&tuner.confirming);
        assert(nvs_config_get_bool(NVS_CONFIG_AUTO_FAN_SPEED)&&!tuner.backup_restored);
        assert(step(true)&&tuner.restore_pending&&!tuner.backup_restored);
        step(true);assert(!tuner.restore_pending&&tuner.backup_restored&&!tuner.core.owns);
        assert(counter_resets==resets&&tuner.generation==generation&&physical_uart_frames==frames&&physical_i2c_frames==rail&&!point_changes&&!fault_stops);
    } else if(!strcmp(name,"restore-verify-only-stale")) {
        start();assert(tune_request(true,"{\"action\":\"stop\"}").code==200);step(true);
        const unsigned resets=counter_resets;
        counters_ok=false;
        for(unsigned i=0;i<4&&tuner.restore_pending;++i) step(true);
        assert(!tuner.restore_pending&&tuner.backup_restored&&!fault_stops&&counter_resets==resets);
        assert(tuner.restoration_phase==RESTORE_COMPLETE&&strstr(tuner.restoration_quality_warning,"counters"));
        counters_ok=true;host_now+=TUNE_POLL_US;observe();start();
        assert(tune_request(true,"{\"action\":\"stop\"}").code==200);step(true);sensors_ok=false;
        for(unsigned i=0;i<20;++i)assert(step(true));
        assert(tuner.restore_pending&&!tuner.backup_restored&&!fault_stops);
        host_now=tuner.restoration_started+RESTORE_TIMEOUT_US;assert(step(true));
        assert(!tuner.restore_pending&&tuner.restore_held&&tuner.restoration_phase==RESTORE_BLOCKED&&!fault_stops);
        sensors_ok=true;assert(step(true));assert(tuner.restore_held&&!tuner.backup_restored);
        assert(tune_request(true,"{\"action\":\"stop\"}").code==200);
        for(unsigned i=0;i<4&&tuner.restore_pending;++i)step(true);
        assert(tuner.backup_restored&&!tuner.restore_held&&!fault_stops&&counter_resets==resets);
    } else if(!strcmp(name,"restore-no-counters")) {
        start();for(unsigned i=0;i<50&&!point_changes;++i)step(true);assert(point_changes);
        Entry *original=find(2,"backup");assert(original&&original->size<2048);
        unsigned char bytes[2048];const size_t size=original->size;memcpy(bytes,original->bytes,size);
        counters_ok=false;assert(tune_request(true,"{\"action\":\"stop\"}").code==200);
        for(unsigned i=0;i<12&&tuner.restore_pending;++i)step(true);
        assert(!tuner.restore_pending&&tuner.backup_restored&&!tuner.core.owns&&!fault_stops);
        assert(gs.POWER_MANAGEMENT_MODULE.frequency_value==625.125f&&gs.POWER_MANAGEMENT_MODULE.applied_core_voltage_mv==1173);
        assert(original->size==size&&!memcmp(original->bytes,bytes,size));
        assert(tuner.restoration_phase==RESTORE_COMPLETE&&strstr(tuner.restoration_quality_warning,"counters"));
        cJSON *json=report(),*status=cJSON_GetObjectItem(json,"status"),*restoration=cJSON_GetObjectItem(status,"restoration");
        assert(!strcmp(cJSON_GetObjectItem(restoration,"phase")->valuestring,"complete"));
        assert(cJSON_IsString(cJSON_GetObjectItem(restoration,"qualityWarning")));
        assert(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetObjectItem(json,"backup"),"restored")));
        cJSON_Delete(json);
    } else if(!strcmp(name,"restore-timeout-hold")) {
        start();for(unsigned i=0;i<50&&!point_changes;++i)step(true);assert(point_changes);
        const float live=gs.POWER_MANAGEMENT_MODULE.frequency_value;
        const uint16_t voltage=gs.POWER_MANAGEMENT_MODULE.applied_core_voltage_mv;
        assert(tune_request(true,"{\"action\":\"stop\"}").code==200);sensors_ok=false;
        assert(step(true)&&tuner.restore_pending&&!tuner.restore_committed);
        const unsigned before=writes;
        host_now=tuner.restoration_started+RESTORE_TIMEOUT_US;assert(step(true));
        assert(!tuner.restore_pending&&tuner.restore_held&&!tuner.backup_restored&&!fault_stops);
        assert(tuner.restoration_phase==RESTORE_BLOCKED&&strstr(tuner.restoration_reason,"180 seconds"));
        assert(strstr(tuner.restoration_reason,"Strict regulator read failed"));
        sensors_ok=true;
        for(unsigned i=0;i<4;++i)assert(step(true));
        assert(writes==before&&gs.POWER_MANAGEMENT_MODULE.frequency_value==live&&gs.POWER_MANAGEMENT_MODULE.applied_core_voltage_mv==voltage);
        assert(!tuner.core.owns&&!tuner.core.active);
        cJSON *json=report(),*restoration=cJSON_GetObjectItem(cJSON_GetObjectItem(json,"status"),"restoration");
        assert(!strcmp(cJSON_GetObjectItem(restoration,"phase")->valuestring,"blocked"));
        assert(cJSON_GetObjectItem(restoration,"elapsedSeconds")->valuedouble>=180);
        assert(cJSON_GetObjectItem(restoration,"timeoutSeconds")->valueint==180);
        assert(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetObjectItem(json,"status"),"ownsSettings")));
        assert(!strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem(json,"status"),"state")->valuestring,"suspended"));cJSON_Delete(json);
        httpd_req_t activity=request(5,NULL);assert(activity.code==200);
        json=cJSON_Parse(activity.reply);assert(json);
        assert(cJSON_IsTrue(cJSON_GetObjectItem(json,"ownsSettings"))&&!strcmp(cJSON_GetObjectItem(json,"state")->valuestring,"suspended"));cJSON_Delete(json);
        GammaAutotuneDisplaySnapshot display;assert(autotune_gamma_try_display_snapshot(&display,host_now));
        assert(display.ownsSettings&&!display.active&&!display.restoring&&display.state==FWT_SUSPENDED);
        assert(configure_request(tuner.core.config).code!=200);
        expect_error("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}","storage-or-restore-unavailable");
        const NvsConfigMutation changed[]={{.key=NVS_CONFIG_ASIC_FREQUENCY,.type=TYPE_FLOAT,.value.f=600},
            {.key=NVS_CONFIG_ASIC_VOLTAGE,.type=TYPE_U16,.value.u16=1150}};
        assert(nvs_config_apply_atomic(changed,2)==ESP_OK);
        uint16_t mv=0;float mhz=0;assert(!autotune_gamma_select_target(&gs,&mv,&mhz));
        assert(!tuner.restore_held&&mv==1150&&mhz==600);
    } else if(!strcmp(name,"restore-live-readback-failure")) {
        start();assert(tune_request(true,"{\"action\":\"stop\"}").code==200);
        assert(step(true)&&tuner.restore_verify_only);fail_tuning_snapshot=true;
        assert(step(true));assert(step(true));
        assert(tuner.restore_held&&!tuner.restore_pending&&!tuner.backup_restored);
        assert(tuner.restoration_phase==RESTORE_BLOCKED&&strstr(tuner.restoration_reason,"readback failed"));
        fail_tuning_snapshot=false;const unsigned stored=writes;
        for(unsigned i=0;i<3;++i)assert(step(true));
        assert(tuner.restore_held&&writes==stored);
        assert(tune_request(true,"{\"action\":\"stop\"}").code==200);
        for(unsigned i=0;i<4&&tuner.restore_pending;++i)step(true);
        assert(tuner.backup_restored&&!tuner.restore_held);
    } else if(!strcmp(name,"restore-physical-reasons")) {
        const GammaTuner clean=tuner;const char *reason=NULL;
        const char *expected[]={"Strict regulator","sensor read","Cooling fan","Input voltage","Core-voltage","output current","ASIC temperature","VRM temperature","stale","output-voltage"};
        for(unsigned kind=0;kind<10;++kind) {
            tuner=clean;
            if(kind==0){tuner.sensor_ok=false;tuner.regulator_read_ok=false;}
            if(kind==1){tuner.sensor_ok=false;tuner.regulator_read_ok=true;}
            if(kind==2)tuner.input.fanOk=false;
            if(kind==3)tuner.strict.vin=4.0;
            if(kind==4)tuner.input.voltageReadbackMv=0;
            if(kind==5)tuner.strict.iout=0;
            if(kind==6)tuner.input.asicTempC=70;
            if(kind==7)tuner.input.vrmTempC=95;
            if(kind==8)tuner.sensor_at=host_now-TUNE_FRESH_US-1;
            if(kind==9)tuner.strict.status_vout=1;
            assert(!restoration_physical_ready(&gs,host_now,&reason)&&reason&&strstr(reason,expected[kind]));
        }
        tuner=clean;assert(restoration_physical_ready(&gs,host_now,&reason)&&!reason);
    } else if(!strcmp(name,"recovery-counters-missing")) {
        start();confirmed_off();assert(tune_request(true,"{\"action\":\"restore-and-resume\"}").code==200);
        autotune_gamma_recovery_tick(&gs);counters_ok=false;recovery_finish();
        assert(tuner.restoration_phase==RESTORE_COMPLETE&&strstr(tuner.restoration_quality_warning,"counters"));
        assert(!gs.SYSTEM_MODULE.mining_paused&&!fault_stops&&host_resume_requests==1);
    } else if(!strcmp(name,"recovery-cooling-timeout")) {
        start();confirmed_off();host_vrm_temp=96;
        assert(tune_request(true,"{\"action\":\"restore-and-resume\"}").code==200);
        const unsigned before=writes;autotune_gamma_recovery_tick(&gs);
        assert(tuner.recovery_requested&&tuner.recovery_phase==RECOVERY_VALIDATING_OFF&&!host_resume_requests);
        host_now=tuner.restoration_started+RESTORE_TIMEOUT_US;autotune_gamma_recovery_tick(&gs);
        assert(!tuner.recovery_requested&&tuner.recovery_phase==RECOVERY_BLOCKED&&tuner.restoration_phase==RESTORE_BLOCKED);
        assert(writes==before&&!host_resume_requests&&gs.SYSTEM_MODULE.mining_paused);
        assert(strstr(tuner.restoration_reason,"recovery-waiting-for-cooling"));
        host_vrm_temp=50;autotune_gamma_recovery_tick(&gs);assert(!host_resume_requests);
        assert(tune_request(true,"{\"action\":\"restore-and-resume\"}").code==200);
        autotune_gamma_recovery_tick(&gs);recovery_finish();assert(host_resume_requests==1);
    } else if(!strcmp(name,"restore-verify-only-faults")) {
        const char *expected[]={"communication","fan tachometer","temperature","temperature","voltage readback"};
        for(unsigned kind=0;kind<5;++kind) {
            gs.SYSTEM_MODULE.hardware_fault=false;gs.ASIC_initalized=true;host_status_cml=0;
            host_fan_ok=true;host_asic_temp=50;host_vrm_temp=55;sensor_vout_delta=0;
            reboot_runtime();host_now+=TUNE_POLL_US;observe();start();
            assert(tune_request(true,"{\"action\":\"stop\"}").code==200);step(true);
            assert(tuner.restore_verify_only);
            const unsigned faults=fault_stops,resets=counter_resets,frames=physical_uart_frames,rail=physical_i2c_frames;
            if(kind==0) host_status_cml=2;
            if(kind==1) host_fan_ok=false;
            if(kind==2) host_asic_temp=70;
            if(kind==3) host_vrm_temp=95;
            if(kind==4) sensor_vout_delta=.08;
            step(true);
            assert(fault_stops==faults+1&&gs.SYSTEM_MODULE.hardware_fault&&!gs.ASIC_initalized&&!tuner.backup_restored);
            assert(strstr(last_fault_reason,expected[kind])&&counter_resets==resets&&physical_uart_frames==frames&&physical_i2c_frames==rail);
        }
    } else if(!strcmp(name,"restore-storage")) {
        start();for(unsigned i=0;i<50&&!point_changes;++i) step(true);assert(point_changes);
        const float live=gs.POWER_MANAGEMENT_MODULE.frequency_value;
        const uint16_t voltage=gs.POWER_MANAGEMENT_MODULE.applied_core_voltage_mv;
        /* A manual fan change is durable v2 and cancels auto ownership. */
        nvs_config_set_u16(NVS_CONFIG_MANUAL_FAN_SPEED,80);drain_auth_write();step(true);
        assert(journal()->bytes[4]==2&&nvs_config_get_u16(NVS_CONFIG_MANUAL_FAN_SPEED)==80);
        assert(tune_request(true,"{\"action\":\"stop\"}").code==200);
        fail_set=true;assert(step(true));
        assert(!tuner.restore_pending&&tuner.restore_held&&!tuner.backup_restored&&!tuner.restore_committed);
        assert(tuner.restoration_phase==RESTORE_BLOCKED&&strstr(tuner.restoration_reason,"storage or readback failed"));
        assert(gs.POWER_MANAGEMENT_MODULE.frequency_value==live&&gs.POWER_MANAGEMENT_MODULE.applied_core_voltage_mv==voltage);
        fail_set=false;
        const unsigned after_failure=writes;
        for(unsigned i=0;i<4;++i)assert(step(true));
        assert(tuner.restore_held&&writes==after_failure);
        assert(tune_request(true,"{\"action\":\"stop\"}").code==200);
        for(unsigned i=0;i<12&&tuner.restore_pending;++i) step(true);
        assert(tuner.backup_restored&&!tuner.restore_pending&&tuner.backup_available);
        assert(nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE)==1173&&nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==625.125f);
        assert(nvs_config_get_bool(NVS_CONFIG_AUTO_FAN_SPEED)&&nvs_config_get_u16(NVS_CONFIG_MANUAL_FAN_SPEED)==100&&nvs_config_get_u16(NVS_CONFIG_MIN_FAN_SPEED)==25&&nvs_config_get_u16(NVS_CONFIG_TEMP_TARGET)==60);
        assert(journal()->bytes[4]==1);unsigned before=writes;assert(nvs_config_init()==ESP_OK&&writes==before);
        reboot_runtime();assert(tuner.backup_available&&!tuner.core.owns);
    } else if(!strcmp(name,"restore-fault")) {
        start();autotune_gamma_interrupt("fault");gs.SYSTEM_MODULE.hardware_fault=true;
        assert(tune_request(true,"{\"action\":\"stop\"}").code==200);unsigned before=writes;
        assert(step(true)&&tuner.restore_pending&&!tuner.backup_restored&&writes==before&&gs.SYSTEM_MODULE.hardware_fault);
        httpd_req_t req=tune_request(false,NULL);assert(strstr(req.reply,"\"asicTempC\":null"));
    } else if(!strcmp(name,"stale-sensors")) {
        sensors_ok=false;host_now+=TUNE_POLL_US;observe();assert(!tuner.input.valid);
        expect_error("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}","miner-not-ready");
        httpd_req_t req=tune_request(false,NULL);assert(strstr(req.reply,"\"asicTempC\":null")&&!point_changes);
    } else if(!strcmp(name,"zero-current")) {
        host_current_ok=false;host_now+=TUNE_POLL_US;observe();assert(!tuner.input.valid);
        expect_error("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}","miner-not-ready");
        assert(!tuner.backup_available&&!point_changes);
    } else if(!strcmp(name,"domains")) {
        counters_ok=false;host_now+=TUNE_POLL_US;observe();assert(!tuner.input.valid);
        expect_error("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}","miner-not-ready");
    } else if(!strcmp(name,"lost-apply")) {
        start();for(unsigned i=0;i<60&&!tuner.core.awaitingApply;++i) step(false);
        assert(tuner.core.awaitingApply);
        for(unsigned i=0;i<20&&!fault_stops;++i) step(false);
        assert(fault_stops&&gs.SYSTEM_MODULE.hardware_fault&&tuner.backup_available&&!point_changes);
    } else if(!strcmp(name,"readback-failure")) {
        start();for(unsigned i=0;i<60&&!tuner.core.awaitingApply;++i) step(true);
        assert(tuner.core.awaitingApply&&tuner.confirming);
        sensor_vout_delta=.08;
        for(unsigned i=0;i<20&&!fault_stops;++i) step(false);
        assert(fault_stops&&tuner.backup_available&&!tuner.backup_restored);
    } else if(!strcmp(name,"configure-busy")) {
        start();for(unsigned i=0;i<60&&!tuner.core.awaitingApply;++i) step(true);assert(tuner.core.awaitingApply);
        unsigned before=writes;
        expect_error("{\"action\":\"configure\",\"config\":{}}","transition-in-progress");assert(writes==before);
    } else if(!strcmp(name,"configure-preflight")) {
        start();fwt_config config=tuner.core.config;config.maxFrequencyMHz=600;
        unsigned before=writes;httpd_req_t req=configure_request(config);
        assert(req.code==409&&strstr(req.reply,"invalid-configuration")&&writes==before&&tuner.core.config.maxFrequencyMHz==625.125);
    } else if(!strcmp(name,"restore-slow-rail") || !strcmp(name,"restore-stuck-rail")) {
        start();gs.POWER_MANAGEMENT_MODULE.frequency_value=575;gs.POWER_MANAGEMENT_MODULE.actual_frequency=575;
        gs.POWER_MANAGEMENT_MODULE.applied_core_voltage_mv=1100;
        assert(tune_request(true,"{\"action\":\"stop\"}").code==200);
        assert(step(true)&&tuner.rail_wait&&gs.POWER_MANAGEMENT_MODULE.frequency_value==575);
        assert(gs.POWER_MANAGEMENT_MODULE.applied_core_voltage_mv==1173);
        sensor_vout_delta=-.073;
        for(unsigned i=0;i<2;++i) {
            assert(step(true)&&tuner.rail_wait&&!fault_stops&&gs.POWER_MANAGEMENT_MODULE.frequency_value==575);
        }
        /* Repeated Disable cannot skip an already commanded but unconfirmed rail. */
        assert(tune_request(true,"{\"action\":\"stop\"}").code==200);
        assert(step(true)&&tuner.rail_wait&&gs.POWER_MANAGEMENT_MODULE.frequency_value==575);
        if(!strcmp(name,"restore-stuck-rail")) {
            for(unsigned i=0;i<20&&!fault_stops;++i) step(true);
            assert(fault_stops&&gs.SYSTEM_MODULE.hardware_fault&&tuner.backup_available&&!tuner.backup_restored);
            assert(gs.POWER_MANAGEMENT_MODULE.frequency_value==575);
        } else {
            sensor_vout_delta=0;
            assert(step(true)&&tuner.rail_wait&&tuner.rail_confirmations==1&&gs.POWER_MANAGEMENT_MODULE.frequency_value==575);
            assert(step(true)&&!tuner.rail_wait&&gs.POWER_MANAGEMENT_MODULE.frequency_value==625.125f&&!fault_stops);
            for(unsigned i=0;i<8&&tuner.restore_pending;++i) step(true);
            assert(tuner.backup_restored&&!tuner.restore_pending&&!fault_stops);
        }
    } else if(!strcmp(name,"restore-low-user-limit")) {
        start();tuner.core.config.maxAsicTempC=40;
        gs.POWER_MANAGEMENT_MODULE.frequency_value=575;gs.POWER_MANAGEMENT_MODULE.actual_frequency=575;
        assert(tune_request(true,"{\"action\":\"stop\"}").code==200);
        step(true);assert(autotune_gamma_transition_allowed(&gs));
        for(unsigned i=0;i<8&&tuner.restore_pending;++i) step(true);
        assert(tuner.backup_restored&&!tuner.restore_pending&&host_asic_temp>tuner.core.config.maxAsicTempC);
    } else if(!strcmp(name,"wire-after-preflight")) {
        start();step(true);assert(autotune_gamma_transition_allowed(&gs));
        uint8_t bytes[12]={0};unsigned uart_before=physical_uart_frames,i2c_before=physical_i2c_frames;
        autotune_gamma_manual_override();
        assert(SERIAL_send(bytes,sizeof(bytes),false)==0&&physical_uart_frames==uart_before);
        assert(smb_write_word(PMBUS_VOUT_COMMAND,600)!=ESP_OK&&physical_i2c_frames==i2c_before);
        assert(smb_write_byte(PMBUS_OPERATION,OPERATION_OFF)==ESP_OK&&physical_i2c_frames==i2c_before+1);
        step(true);assert(!tuner.core.owns);
        assert(SERIAL_send(bytes,sizeof(bytes),false)==sizeof(bytes));
        assert(smb_write_word(PMBUS_VOUT_COMMAND,600)==ESP_OK);
    } else if(!strcmp(name,"wire-inflight") || !strcmp(name,"wire-fault")) {
        start();step(true);pthread_t writer,revoker;block_uart=true;
        assert(!pthread_create(&writer,NULL,wire_writer,NULL));
        pthread_mutex_lock(&wire_test_mutex);
        while(!uart_entered)pthread_cond_wait(&wire_test_condition,&wire_test_mutex);
        pthread_mutex_unlock(&wire_test_mutex);
        assert(!pthread_create(&revoker,NULL,wire_revoker,!strcmp(name,"wire-fault")?(void *)1:NULL));
        pthread_mutex_lock(&wire_test_mutex);
        while(!atomic_load(&revocation_attempted))pthread_cond_wait(&wire_test_condition,&wire_test_mutex);
        assert(!atomic_load(&revocation_completed));
        release_uart=true;pthread_cond_broadcast(&wire_test_condition);pthread_mutex_unlock(&wire_test_mutex);
        void *result;assert(!pthread_join(writer,&result)&&(uintptr_t)result==12);
        assert(!pthread_join(revoker,NULL)&&atomic_load(&revocation_completed));
        unsigned uart_before=physical_uart_frames,i2c_before=physical_i2c_frames;uint8_t bytes[12]={0};
        assert(SERIAL_send(bytes,sizeof(bytes),false)==0&&physical_uart_frames==uart_before);
        assert(smb_write_word(PMBUS_VOUT_COMMAND,600)!=ESP_OK&&physical_i2c_frames==i2c_before);
        assert(smb_write_byte(PMBUS_OPERATION,OPERATION_OFF)==ESP_OK);
        autotune_gamma_owner_idle();
        assert(smb_write_word(PMBUS_VOUT_COMMAND,600)==ESP_OK);
    } else if(!strcmp(name,"uart-failure")) {
        start();for(unsigned i=0;i<60&&!tuner.core.awaitingApply;++i) step(false);assert(tuner.core.awaitingApply);
        ++tx_failures;assert(!autotune_gamma_transition_finished(&gs,true));assert(!tuner.core.owns);
    } else if(!strcmp(name,"manual-pending")) {
        gs.POWER_MANAGEMENT_MODULE.applied_core_voltage_mv=1150;
        tuner.input.current.coreVoltageMv=1150;tuner.input.voltageReadbackMv=1150;
        unsigned before=writes;
        expect_error("{\"action\":\"start\",\"waiverAccepted\":true,\"waiverVersion\":1}","manual-settings-not-applied");assert(writes==before&&!tuner.backup_available);
    } else assert(!"unknown scenario");
    assert(!erases);puts("PASS");return 0;
}
