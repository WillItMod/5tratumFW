#include "autotune_gamma_stubs.h"
/* Reuse the existing fake storage/HTTP boundary; all NVS journal and profile
 * code linked by the runner is production, not a second implementation. */
#define main legacy_profiles_main
#include "profiles_runtime_test.c"
#undef main
static bool revoke_after_snapshot;
static bool interleaved_snapshot(const NvsConfigKey *,NvsConfigMutation *,size_t);
#define nvs_config_read_atomic interleaved_snapshot
#include "../../main/autotune_runtime.c"
#undef nvs_config_read_atomic
static bool interleaved_snapshot(const NvsConfigKey *keys,NvsConfigMutation *values,size_t count) {
    const bool result=nvs_config_read_atomic(keys,values,count);
    if (revoke_after_snapshot) {revoke_after_snapshot=false;autotune_gamma_manual_override();}
    return result;
}

static uint64_t host_now=UINT64_C(1000000);
static bool sensors_ok=true, counters_ok=true, host_fan_ok=true, host_current_ok=true;
static unsigned sensor_reads, counter_resets, fault_stops, point_changes;
static atomic_uint_least32_t tx_failures;
static double sensor_vout_delta;
static float host_asic_temp=50,host_vrm_temp=55;
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
    /* Deterministic output-power plant; it is not a claimed real miner curve. */
    double watts=12.0*(gs.POWER_MANAGEMENT_MODULE.frequency_value/625.125)*(v/1.173)*(v/1.173);
    *out=(TPS546_StrictTelemetry){.vin=5.1,.vout=v+sensor_vout_delta,.iout=host_current_ok?watts/v:0,.temperature_c=host_vrm_temp};
    return ESP_OK;
}
esp_err_t EMC2101_read_strict(float *temp,uint16_t *rpm) {
    if(!sensors_ok) return ESP_FAIL;
    *temp=host_asic_temp;*rpm=host_fan_ok?3000:0;return ESP_OK;
}
GammaAutotuneCounterSample hashrate_monitor_autotune_snapshot(void *state,uint64_t now) {
    (void)state;
    return (GammaAutotuneCounterSample){.valid=counters_ok,.captured_at_us=now-500000,
        .sample_id=now-500000,.useful_ghs=gs.POWER_MANAGEMENT_MODULE.frequency_value*2.04,.error_percentage=0};
}
void hashrate_monitor_reset_measurements(void *state) {(void)state;++counter_resets;}
void POWER_MANAGEMENT_stop_for_fault(void *state,const char *reason) {
    (void)reason;GlobalState *device=state;++fault_stops;device->SYSTEM_MODULE.hardware_fault=true;device->ASIC_initalized=false;
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
    assert(cJSON_AddItemToObject(json,"config",config_json(&config)));
    char *body=cJSON_PrintUnformatted(json);assert(body);
    httpd_req_t req=tune_request(true,body);free(body);cJSON_Delete(json);return req;
}
static void *wire_writer(void *unused) {
    (void)unused;uint8_t bytes[12]={0};
    return (void *)(uintptr_t)SERIAL_send(bytes,sizeof(bytes),false);
}
static void *wire_revoker(void *fault) {
    pthread_mutex_lock(&wire_test_mutex);
    atomic_store(&revocation_attempted,true);pthread_cond_broadcast(&wire_test_condition);
    pthread_mutex_unlock(&wire_test_mutex);
    if(fault) autotune_gamma_interrupt("hardware-fault");else autotune_gamma_manual_override();
    atomic_store(&revocation_completed,true);return NULL;
}
int main(int argc,char **argv) {
    assert(argc==2);init();const char *name=argv[1];
    if(!strcmp(name,"waiver")) {
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
        assert(cJSON_GetArraySize(limits)==9);
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
    } else if(!strcmp(name,"restore-storage")) {
        start();for(unsigned i=0;i<50&&!point_changes;++i) step(true);assert(point_changes);
        const float live=gs.POWER_MANAGEMENT_MODULE.frequency_value;
        const uint16_t voltage=gs.POWER_MANAGEMENT_MODULE.applied_core_voltage_mv;
        /* A manual fan change is durable v2 and cancels auto ownership. */
        nvs_config_set_u16(NVS_CONFIG_MANUAL_FAN_SPEED,80);drain_auth_write();step(true);
        assert(journal()->bytes[4]==2&&nvs_config_get_u16(NVS_CONFIG_MANUAL_FAN_SPEED)==80);
        assert(tune_request(true,"{\"action\":\"stop\"}").code==200);
        fail_set=true;assert(step(true));
        assert(tuner.restore_pending&&!tuner.backup_restored&&!tuner.restore_committed);
        assert(gs.POWER_MANAGEMENT_MODULE.frequency_value==live&&gs.POWER_MANAGEMENT_MODULE.applied_core_voltage_mv==voltage);
        fail_set=false;
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
