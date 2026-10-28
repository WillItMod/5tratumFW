#include "tuning_schedule.h"
#include "tuning_schedule_api.h"
#include <time.h>
#include <stdatomic.h>
static time_t clock_wall=1791374400;
static bool clock_synced,applied_paused;
time_t time(time_t *out){if(out)*out=clock_wall;return clock_wall;}
bool mining_schedule_owns_clock(void){return clock_synced;}
bool mining_schedule_applied_paused(void){return applied_paused;}
void vTaskDelay(unsigned ticks){(void)ticks;}
static const char *daily_schedule="{\"schemaVersion\":1,\"enabled\":true,\"utcOffsetMinutes\":0,\"events\":[{\"enabled\":true,\"dayMask\":127,\"timeMinutes\":540,\"slot\":0},{\"enabled\":true,\"dayMask\":127,\"timeMinutes\":1260,\"slot\":1},{\"enabled\":true,\"dayMask\":127,\"timeMinutes\":0,\"slot\":2}]}";
static void wall_at(const char *utc){struct tm local={0};assert(strptime(utc,"%Y-%m-%dT%H:%M:%S",&local));clock_wall=timegm(&local);}
static cJSON *tuning_report(void){httpd_req_t r=request(3,NULL);assert(r.code==200);cJSON *json=cJSON_Parse(r.reply);assert(json);return json;}
static void assert_status(const char *status,int selected,int applied)
{
    cJSON *json=tuning_report();
    assert(!strcmp(cJSON_GetObjectItem(json,"status")->valuestring,status));
    cJSON *s=cJSON_GetObjectItem(json,"selectedSlot"),*a=cJSON_GetObjectItem(json,"appliedSlot");
    assert(selected<0?cJSON_IsNull(s):cJSON_IsNumber(s)&&s->valueint==selected);
    assert(applied<0?cJSON_IsNull(a):cJSON_IsNumber(a)&&a->valueint==applied);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(json,"supported")));
    cJSON_Delete(json);
}
static void prepare_schedule(void)
{
    assert(tuning_schedule_init(&gs)==ESP_OK && register_tuning_schedule_api(NULL)==ESP_OK);
    gs.SYSTEM_MODULE.mining_runtime_ready=true;gs.ASIC_initalized=true;
    assert(request(1,"{\"type\":\"tuning\",\"slot\":0,\"name\":\"Day\",\"frequencyMHz\":600,\"coreVoltageMv\":1170}").code==200);
    assert(request(1,"{\"type\":\"tuning\",\"slot\":1,\"name\":\"Evening\",\"frequencyMHz\":550,\"coreVoltageMv\":1150}").code==200);
    assert(request(1,"{\"type\":\"tuning\",\"slot\":2,\"name\":\"Night\",\"frequencyMHz\":500,\"coreVoltageMv\":1100}").code==200);
}
static atomic_bool pair_writer_done;
static void *write_tuning_pairs(void *unused)
{
    (void)unused;
    for(unsigned i=0;i<300;i++) {
        NvsConfigMutation point[]={
            {.key=NVS_CONFIG_ASIC_FREQUENCY,.type=TYPE_FLOAT,.value.f=i%2?600:500},
            {.key=NVS_CONFIG_ASIC_VOLTAGE,.type=TYPE_U16,.value.u16=i%2?1170:1100}
        };
        assert(nvs_config_apply_atomic(point,2)==ESP_OK);
    }
    atomic_store(&pair_writer_done,true);
    return NULL;
}
static void tuning_runtime_case(const char *scenario)
{
    prepare_schedule();
    if(!strcmp(scenario,"tuning-pair-snapshot")) {
        uint16_t voltage;float frequency;
        assert(!nvs_config_get_tuning_pair(NULL,&frequency) && !nvs_config_get_tuning_pair(&voltage,NULL));
        pthread_t writer;assert(!pthread_create(&writer,NULL,write_tuning_pairs,NULL));
        unsigned reads=0;
        do {
            assert(nvs_config_get_tuning_pair(&voltage,&frequency));
            assert((frequency==625.125f && voltage==1173) || (frequency==600 && voltage==1170) || (frequency==500 && voltage==1100));
            reads++;
        } while(!atomic_load(&pair_writer_done) || reads<1000);
        assert(!pthread_join(writer,NULL));
    } else if(!strcmp(scenario,"tuning-read-save-auth")) {
        unsigned before=writes;
        for(unsigned i=0;i<2;i++){assert_status("disabled",-1,-1);tuning_schedule_tick();}
        assert(writes==before && nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==625.125f);
        authorized=false;assert(request(3,NULL).code==401);httpd_req_t denied=request(4,daily_schedule);assert(denied.code==401 && denied.received==0 && writes==before);authorized=true;
        httpd_req_t saved_request=request(4,daily_schedule);assert(saved_request.code==200 && strstr(saved_request.reply,"\"ok\":true"));
        before=writes;for(unsigned i=0;i<2;i++){assert_status("waiting-clock",-1,-1);tuning_schedule_tick();}
        assert(writes==before && nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==625.125f);
        assert(request(1,"{\"type\":\"tuning\",\"slot\":0,\"clear\":true}").code==409 && writes==before);
        assert(request(1,"{\"type\":\"tuning\",\"slot\":0,\"name\":\"Edited\",\"frequencyMHz\":525,\"coreVoltageMv\":1150}").code==409);
        assert(tuning_schedule_init(&gs)==ESP_OK && nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==625.125f && writes==before);
    } else if(!strcmp(scenario,"tuning-boundaries")) {
        assert(request(4,daily_schedule).code==200);clock_synced=true;
        wall_at("2026-10-07T08:59:59");assert_status("pending",2,-1);tuning_schedule_tick();assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==500);
        wall_at("2026-10-07T09:00:00");tuning_schedule_tick();assert_status("applied",0,0);assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==600);
        unsigned before=writes;tuning_schedule_tick();assert(writes==before);
        wall_at("2026-10-07T20:59:59");assert_status("applied",0,0);
        wall_at("2026-10-07T21:00:00");tuning_schedule_tick();assert_status("applied",1,1);assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==550);
        wall_at("2026-10-08T00:00:00");tuning_schedule_tick();assert_status("applied",2,2);assert(nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE)==1100);
        assert_string(NVS_CONFIG_STRATUM_PASS,"fixture-pass-A");assert_string(NVS_CONFIG_FALLBACK_STRATUM_PASS,"fixture-pass-B");assert(!reloads && !erases);
        assert(request(4,"{\"schemaVersion\":1,\"enabled\":true,\"utcOffsetMinutes\":60,\"events\":[{\"enabled\":true,\"dayMask\":64,\"timeMinutes\":1380,\"slot\":1},{\"enabled\":true,\"dayMask\":1,\"timeMinutes\":30,\"slot\":2}]}").code==200);
        wall_at("2026-10-10T23:29:59");assert_status("pending",1,-1);wall_at("2026-10-10T23:30:00");assert_status("pending",2,-1);
        wall_at("2026-10-17T21:59:59");assert_status("pending",2,-1);wall_at("2026-10-17T22:00:00");assert_status("pending",1,-1);
    } else if(!strcmp(scenario,"tuning-protection-clock")) {
        assert(request(4,daily_schedule).code==200);clock_synced=true;wall_at("2026-10-07T09:00:00");unsigned before=writes;
        gs.SYSTEM_MODULE.mining_paused=true;tuning_schedule_tick();assert_status("paused",0,-1);assert(writes==before);gs.SYSTEM_MODULE.mining_paused=false;
        applied_paused=true;tuning_schedule_tick();assert(writes==before);applied_paused=false;
        gs.SYSTEM_MODULE.hardware_fault=true;tuning_schedule_tick();assert(writes==before);gs.SYSTEM_MODULE.hardware_fault=false;
        gs.SYSTEM_MODULE.overheat_mode=true;tuning_schedule_tick();assert(writes==before);gs.SYSTEM_MODULE.overheat_mode=false;
        gs.SYSTEM_MODULE.mining_runtime_ready=false;tuning_schedule_tick();assert_status("waiting-runtime",0,-1);assert(writes==before);gs.SYSTEM_MODULE.mining_runtime_ready=true;
        gs.ASIC_initalized=false;tuning_schedule_tick();assert(writes==before);gs.ASIC_initalized=true;
        gs.SYSTEM_MODULE.pools_unavailable=true;tuning_schedule_tick();assert(writes==before);gs.SYSTEM_MODULE.pools_unavailable=false;
        clock_synced=false;tuning_schedule_tick();assert_status("waiting-clock",-1,-1);assert(writes==before);clock_synced=true;
        tuning_schedule_tick();assert_status("applied",0,0);assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==600);
        assert(request(2,"{\"type\":\"tuning\",\"frequencyMHz\":525,\"coreVoltageMv\":1150}").code==200);
        tuning_schedule_tick();assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==525);
        wall_at("2026-10-08T09:00:00");tuning_schedule_tick();assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==600);
        assert(request(4,"{\"schemaVersion\":1,\"enabled\":false,\"utcOffsetMinutes\":0,\"events\":[]}").code==200);
        before=writes;tuning_schedule_tick();assert_status("disabled",-1,-1);assert(writes==before);
    } else if(!strcmp(scenario,"tuning-validation-failure")) {
        const char *bad[]={"{\"schemaVersion\":1,\"enabled\":true,\"utcOffsetMinutes\":0,\"events\":[]}","{\"schemaVersion\":1,\"enabled\":false,\"utcOffsetMinutes\":15.5,\"events\":[]}","{\"schemaVersion\":1,\"schemaVersion\":1,\"enabled\":false,\"utcOffsetMinutes\":0,\"events\":[]}","{\"schemaVersion\":1,\"enabled\":true,\"utcOffsetMinutes\":0,\"events\":[{\"enabled\":true,\"dayMask\":127,\"timeMinutes\":0,\"slot\":true}]}","{\"schemaVersion\":1,\"enabled\":true,\"utcOffsetMinutes\":0,\"events\":[{\"enabled\":true,\"dayMask\":127,\"timeMinutes\":0,\"slot\":0},{\"enabled\":true,\"dayMask\":1,\"timeMinutes\":0,\"slot\":1}]}"};
        unsigned before=writes;for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++)assert(request(4,bad[i]).code==400 && writes==before);
        assert(request(4,"{\"schemaVersion\":1,\"enabled\":false,\"utcOffsetMinutes\":0,\"events\":[{\"enabled\":false,\"dayMask\":1,\"timeMinutes\":0,\"slot\":9}]}").code==409 && writes==before);
        assert(request(4,daily_schedule).code==200);clock_synced=true;wall_at("2026-10-07T09:00:00");
        fail_set=true;tuning_schedule_tick();assert_status("error",0,-1);assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==625.125f);fail_set=false;
        tuning_schedule_tick();assert_status("applied",0,0);
        Entry *old=find(2,"tuning_schedule");assert(old);char *raw=strdup((char *)old->bytes);assert(raw);
        fail_commit=true;assert(request(4,"{\"schemaVersion\":1,\"enabled\":false,\"utcOffsetMinutes\":0,\"events\":[]}").code==507);fail_commit=false;
        assert(!strcmp((char *)find(2,"tuning_schedule")->bytes,raw));free(raw);before=writes;tuning_schedule_tick();assert_status("applied",0,0);assert(writes==before);
        assert(tuning_schedule_init(&gs)==ESP_OK);before=writes;assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==600 && writes==before);tuning_schedule_tick();assert_status("applied",0,0);
        wall_at("2026-10-07T21:00:00");Entry *evening=find(2,"t1");assert(evening);char *evening_raw=strdup((char *)evening->bytes);assert(evening_raw);
        put(2,"t1",2,"{broken",0);before=writes;tuning_schedule_tick();assert_status("error",1,0);assert(writes==before && nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==600);
        put(2,"t1",2,evening_raw,0);free(evening_raw);tuning_schedule_tick();assert_status("applied",1,1);assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==550);
        put(2,"tuning_schedule",2,"{broken",0);assert(tuning_schedule_init(&gs)==ESP_OK);before=writes;tuning_schedule_tick();assert_status("error",-1,-1);assert(writes==before);
        assert(request(1,"{\"type\":\"tuning\",\"slot\":0,\"clear\":true}").code==409);
        assert(request(4,"{\"schemaVersion\":1,\"enabled\":false,\"utcOffsetMinutes\":0,\"events\":[]}").code==200);assert_status("disabled",-1,-1);
    } else if(!strcmp(scenario,"tuning-model-support")) {
        assert(request(4,daily_schedule).code==200);clock_synced=true;wall_at("2026-10-07T09:00:00");
        gs.DEVICE_CONFIG.board_version="602";assert_status("pending",0,-1);tuning_schedule_tick();assert_status("applied",0,0);
        gs.DEVICE_CONFIG.board_version="600";wall_at("2026-10-07T21:00:00");unsigned before=writes;tuning_schedule_tick();assert(writes==before);
        cJSON *json=tuning_report();assert(cJSON_IsFalse(cJSON_GetObjectItem(json,"supported")) && !strcmp(cJSON_GetObjectItem(json,"status")->valuestring,"waiting-runtime"));cJSON_Delete(json);
        gs.DEVICE_CONFIG.board_version="601";tuning_schedule_tick();assert_status("applied",1,1);
    } else assert(0);
}
