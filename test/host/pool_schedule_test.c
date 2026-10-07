#include "pool_schedule_stubs.h"
#include "pool_schedule.h"
#include "pool_reload.h"
#include "nvs_config.h"
#include "http_server/operating_profiles.h"
#include "http_server/pool_schedule_api.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
// Include actual coordinator to exercise its internal handoff with fake tasks.
#include "../../main/tasks/protocol_coordinator.c"
#include "stratum_close_hooks.inc"

static time_t now = 1767571200; // 2026-01-05 Monday 00:00 UTC.
static bool synchronized, auto_exit = true, unauthorized, fail_apply;
static unsigned commits, writes, applies, protocol_starts, job_clears, closes;
static unsigned fail_commit, allocations, fail_allocation;
static uint16_t slots = 0x3ff;
static uint32_t current_hash = 100;
static char blob[2048], staged[2048];
static size_t blob_size, staged_size;
static coordinator_event_t events[32];
static unsigned queue_in, queue_out;
static httpd_uri_t routes[2];
static unsigned route_count;
static atomic_bool share_holding, close_started, close_done, release_share;
static unsigned json_allocations, json_fail_at;
static uint16_t configured_tls;
static void *json_allocate(size_t size) { if (++json_allocations == json_fail_at) return NULL; return malloc(size); }
time_t time(time_t *value) { if (value) *value = now; return now; }
bool mining_schedule_owns_clock(void) { return synchronized; }
const char *esp_err_to_name(esp_err_t error) { (void)error; return "test-error"; }
esp_err_t nvs_open(const char *ns, nvs_open_mode_t mode, nvs_handle_t *handle)
{ assert(!strcmp(ns, "5fw_profiles")); (void)mode; *handle = 1; return ESP_OK; }
void nvs_close(nvs_handle_t handle) { (void)handle; }
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *value, size_t *size)
{
    (void)h; assert(!strcmp(key, "pool_schedule"));
    if (!blob_size) return ESP_ERR_NVS_NOT_FOUND;
    if (!value) { *size = blob_size; return ESP_OK; }
    if (*size < blob_size) return ESP_ERR_NVS_INVALID_LENGTH;
    memcpy(value, blob, blob_size); *size = blob_size; return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *value, size_t size)
{ (void)h; assert(!strcmp(key,"pool_schedule") && size <= sizeof(staged)); memcpy(staged,value,size); staged_size=size; writes++; return ESP_OK; }
esp_err_t nvs_erase_key(nvs_handle_t h, const char *key)
{ (void)h; assert(!strcmp(key,"pool_schedule")); staged_size=0; return ESP_OK; }
esp_err_t nvs_commit(nvs_handle_t h)
{ (void)h; commits++; if (fail_commit) { fail_commit--; return ESP_FAIL; } memcpy(blob,staged,staged_size); blob_size=staged_size; return ESP_OK; }
bool operating_profile_pool_hash(unsigned slot, uint32_t *hash)
{ if (!(slots & (1u<<slot))) return false; *hash=100+slot; return true; }
bool operating_profile_current_pool_hash(bool fallback, uint32_t *hash)
{ assert(!fallback); *hash=current_hash; return true; }
esp_err_t operating_profiles_with_pool_slots(uint16_t mask, esp_err_t (*commit)(void *), void *context)
{ if (mask & ~slots) return ESP_ERR_NVS_NOT_FOUND; return commit(context); }
esp_err_t operating_profile_apply_pool_guarded(unsigned slot, bool fallback, bool (*allow)(void *), void *context)
{
    assert(!fallback);
    if (!allow(context)) return ESP_ERR_INVALID_STATE;
    if (fail_apply) return ESP_FAIL;
    if (!(slots & (1u<<slot))) return ESP_ERR_NVS_NOT_FOUND;
    current_hash=100+slot; applies++; return ESP_OK;
}
cJSON *operating_profiles_binding(void)
{ return cJSON_Parse("{\"identity\":{\"deviceId\":\"opaque-fixture\"},\"hardware\":{\"boardModel\":\"Gamma 601\"},\"firmware\":{\"product\":\"5tratumFW\"}}"); }
char *nvs_config_get_string(NvsConfigKey key)
{
    allocations++; if (allocations==fail_allocation) return NULL;
    const char *value = key==NVS_CONFIG_STRATUM_PROTOCOL ? "SV2" : key==NVS_CONFIG_STRATUM_URL ? "new-pool.invalid" :
                        key==NVS_CONFIG_STRATUM_USER ? "new-worker" : key==NVS_CONFIG_STRATUM_PASS ? "private-fixture" : "";
    return strdup(value);
}
uint16_t nvs_config_get_u16(NvsConfigKey key) { return key==NVS_CONFIG_STRATUM_PORT ? 4444 : key==NVS_CONFIG_STRATUM_DIFFICULTY ? 256 : key==NVS_CONFIG_STRATUM_TLS ? configured_tls : 0; }
bool nvs_config_get_bool(NvsConfigKey key) { return key==NVS_CONFIG_STRATUM_EXTRANONCE_SUBSCRIBE; }
QueueHandle_t xQueueCreate(unsigned length,unsigned size) { (void)length; assert(size==sizeof(coordinator_event_t)); return (void *)1; }
BaseType_t xQueueSend(QueueHandle_t queue,const void *event,unsigned wait)
{ (void)queue; (void)wait; assert(queue_in<32); events[queue_in++]=*(const coordinator_event_t*)event; return pdTRUE; }
BaseType_t xQueueReceive(QueueHandle_t queue,void *event,unsigned wait)
{
    (void)queue; (void)wait;
    if (auto_exit && atomic_load(&s_v1_active) && atomic_load(&s_v1_should_shutdown)) protocol_coordinator_v1_exited();
    if (auto_exit && atomic_load(&s_v2_active) && atomic_load(&s_v2_should_shutdown)) protocol_coordinator_v2_exited();
    if (queue_out==queue_in) return 0;
    *(coordinator_event_t*)event=events[queue_out++]; return pdTRUE;
}
BaseType_t xTaskCreate(void (*task)(void *),const char *name,unsigned stack,void *arg,unsigned priority,TaskHandle_t *handle)
{ (void)task;(void)stack;(void)arg;(void)priority;(void)handle; if (strcmp(name,"pool schedule")) protocol_starts++; return pdPASS; }
void stratum_v1_task(void *arg) { (void)arg; }
void stratum_v2_task(void *arg) { (void)arg; }
void vTaskDelay(unsigned value) { (void)value; }
void vTaskDelete(void *value) { (void)value; }
void queue_clear(void *value) { (void)value; job_clears++; }
void SYSTEM_clean_jobs_queue(GlobalState *state) { (void)state; job_clears++; }
int64_t esp_timer_get_time(void) { return 1000000; }
void mux_peer_status_disconnect(void) {}
bool wifi_is_connected(void) { return false; }
int esp_transport_close(esp_transport_handle_t value) { (void)value; closes++; return ESP_OK; }
int esp_transport_destroy(esp_transport_handle_t value) { (void)value; assert(!atomic_load(&share_holding)); return ESP_OK; }
void sv2_noise_destroy(void *value) { (void)value; assert(!atomic_load(&share_holding)); }
int esp_transport_connect(esp_transport_handle_t v,const char *h,int p,int timeout)
{ (void)v;(void)h;(void)p;(void)timeout; return ESP_FAIL; }
int esp_transport_read(esp_transport_handle_t v,char *b,int size,int timeout)
{ (void)v;(void)b;(void)size;(void)timeout; return -1; }
esp_transport_handle_t esp_transport_tcp_init(void) { return (void *)1; }
esp_transport_handle_t STRATUM_V1_transport_init(tls_mode mode,char *cert) { (void)mode;(void)cert; return (void *)1; }
int STRATUM_V1_subscribe(esp_transport_handle_t v,int uid,const char *model) { (void)v;(void)uid;(void)model; return 0; }
int STRATUM_V1_authorize(esp_transport_handle_t v,int uid,const char *u,const char *p) { (void)v;(void)uid;(void)u;(void)p; return 0; }
esp_err_t is_network_allowed(httpd_req_t *req) { (void)req; return unauthorized ? ESP_FAIL : ESP_OK; }
esp_err_t set_cors_headers(httpd_req_t *req) { (void)req; return ESP_OK; }
esp_err_t httpd_resp_set_hdr(httpd_req_t *r,const char *n,const char *v) { (void)r;(void)n;(void)v; return ESP_OK; }
esp_err_t httpd_resp_send_err(httpd_req_t *r,int code,const char *message) { r->code=code; snprintf(r->reply,sizeof(r->reply),"%s",message); return ESP_OK; }
esp_err_t httpd_resp_send_500(httpd_req_t *r) { return httpd_resp_send_err(r,500,"internal"); }
esp_err_t httpd_resp_set_status(httpd_req_t *r,const char *status) { r->code=atoi(status);return ESP_OK; }
esp_err_t httpd_resp_set_type(httpd_req_t *r,const char *type) { (void)r;assert(!strcmp(type,"application/json"));return ESP_OK; }
esp_err_t httpd_resp_sendstr(httpd_req_t *r,const char *body) { snprintf(r->reply,sizeof(r->reply),"%s",body);return ESP_OK; }
int httpd_req_recv(httpd_req_t *r,char *out,size_t size)
{ if (size>7) size=7; memcpy(out,r->body+r->received,size); r->received+=size; return size; }
esp_err_t httpd_register_uri_handler(httpd_handle_t s,const httpd_uri_t *r) { (void)s; assert(route_count<2); routes[route_count++]=*r; return ESP_OK; }
esp_err_t HTTP_send_json(httpd_req_t *r,const cJSON *json,int *prebuffer)
{ (void)prebuffer; char *raw=cJSON_PrintUnformatted(json); assert(raw); snprintf(r->reply,sizeof(r->reply),"%s",raw); free(raw); r->code=200; return ESP_OK; }

static PoolSchedule parse(const char *raw)
{ cJSON *json=cJSON_Parse(raw); PoolSchedule result; assert(pool_schedule_parse(json,&result)); cJSON_Delete(json); return result; }
static const char *weekly="{\"schemaVersion\":1,\"enabled\":true,\"utcOffsetMinutes\":0,\"events\":[{\"enabled\":true,\"dayMask\":2,\"timeMinutes\":0,\"slot\":1},{\"enabled\":true,\"dayMask\":2,\"timeMinutes\":600,\"slot\":2}]}";
static GlobalState make_state(void)
{
    GlobalState state={.configured_frequency=500,.configured_voltage=1130,.configured_fan=80,.ASIC_initalized=false,.stratum_protocol=STRATUM_PROTOCOL_V1,.transport=(void*)1};
    state.SYSTEM_MODULE=(SystemModule){.pool_url=strdup("old-pool.invalid"),.pool_user=strdup("old-worker"),.pool_pass=strdup("old-password"),.pool_cert=strdup(""),.pool_port=3333,
      .fallback_pool_url="fallback.invalid",.fallback_pool_user="fallback-worker",.fallback_pool_pass="fallback-password",.fallback_pool_cert="",.fallback_pool_port=5555,
      .fallback_pool_protocol=STRATUM_PROTOCOL_V1,.mining_paused=true,.pools_unavailable=true,.hardware_fault=true,.overheat_mode=true};
    return state;
}
static void unchanged_power(const GlobalState *state)
{ assert(state->configured_frequency==500 && state->configured_voltage==1130 && state->configured_fan==80 && !state->ASIC_initalized); assert(state->SYSTEM_MODULE.mining_paused && state->SYSTEM_MODULE.pools_unavailable && state->SYSTEM_MODULE.hardware_fault && state->SYSTEM_MODULE.overheat_mode); assert(!strcmp(state->SYSTEM_MODULE.fallback_pool_url,"fallback.invalid") && state->SYSTEM_MODULE.fallback_pool_port==5555); }
static void free_state(GlobalState *s) { free(s->SYSTEM_MODULE.pool_url);free(s->SYSTEM_MODULE.pool_user);free(s->SYSTEM_MODULE.pool_pass);free(s->SYSTEM_MODULE.pool_cert); }
static void *share_thread(void *unused)
{
    (void)unused;SYSTEM_stratum_io_lock();atomic_store(&share_holding,true);
    while(!atomic_load(&release_share)){struct timespec delay={.tv_nsec=1000000};nanosleep(&delay,NULL);}
    atomic_store(&share_holding,false);SYSTEM_stratum_io_unlock();return NULL;
}
static void *close_thread(void *state)
{ atomic_store(&close_started,true);stratum_v1_close_connection(state);atomic_store(&close_done,true);return NULL; }
static void *identity_reader(void *value)
{
    GlobalState *state=value;
    for(unsigned i=0;i<2000;i++){
        char host[254],user[513];uint16_t port;
        SYSTEM_copy_pool_identity(state,false,host,sizeof(host),user,sizeof(user),&port);
        if(!strcmp(host,"old-pool.invalid"))assert(!strcmp(user,"old-worker") && port==3333);
        else assert(!strcmp(host,"new-pool.invalid") && !strcmp(user,"new-worker") && port==4444);
    }
    return NULL;
}
int main(int argc,char **argv)
{
    assert(argc==2); const char *scenario=argv[1]; GlobalState state=make_state(); assert(pool_schedule_init(&state)==ESP_OK);
    if (!strcmp(scenario,"selector")) {
        PoolSchedule s=parse(weekly);
        assert(pool_schedule_select(&s,1,0)==1 && pool_schedule_select(&s,1,599)==1 && pool_schedule_select(&s,1,600)==2 && pool_schedule_select(&s,0,1439)==2);
        s.event_count=1; for(unsigned d=0;d<7;d++) for(unsigned m=0;m<1440;m++) assert(pool_schedule_select(&s,d,m)==1);
        s.enabled=false;assert(pool_schedule_select(&s,1,0)==-1);s.enabled=true;assert(pool_schedule_select(&s,7,0)==-1);
    } else if (!strcmp(scenario,"invalid")) {
        const char *bad[]={"{}","{\"schemaVersion\":1,\"enabled\":1,\"utcOffsetMinutes\":0,\"events\":[]}","{\"schemaVersion\":1,\"enabled\":true,\"utcOffsetMinutes\":1,\"events\":[]}","{\"schemaVersion\":1,\"enabled\":true,\"utcOffsetMinutes\":0,\"events\":[],\"enabled\":true}","{\"schemaVersion\":1,\"enabled\":true,\"utcOffsetMinutes\":0,\"events\":[{\"enabled\":true,\"dayMask\":0,\"timeMinutes\":0,\"slot\":1}]}","{\"schemaVersion\":1,\"enabled\":true,\"utcOffsetMinutes\":0,\"events\":[{\"enabled\":true,\"dayMask\":1,\"timeMinutes\":0,\"slot\":1.5}]}"};
        for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++){cJSON *j=cJSON_Parse(bad[i]);PoolSchedule s;assert(!pool_schedule_parse(j,&s));cJSON_Delete(j);}
        PoolSchedule s=parse(weekly);s.events[1].time_minutes=0;assert(pool_schedule_save(&s)==ESP_ERR_INVALID_ARG);s.event_count=17;assert(pool_schedule_save(&s)==ESP_ERR_INVALID_ARG);assert(writes==0);
        s=parse(weekly);s.utc_offset_minutes=-735;assert(pool_schedule_save(&s)==ESP_ERR_INVALID_ARG);s.utc_offset_minutes=855;assert(pool_schedule_save(&s)==ESP_ERR_INVALID_ARG);s.utc_offset_minutes=0;s.events[0].day_mask=128;assert(pool_schedule_save(&s)==ESP_ERR_INVALID_ARG);s.events[0].day_mask=2;s.events[0].slot=10;assert(pool_schedule_save(&s)==ESP_ERR_INVALID_ARG);s.events[0].slot=1;s.events[0].time_minutes=1440;assert(pool_schedule_save(&s)==ESP_ERR_INVALID_ARG);assert(writes==0);
    } else if (!strcmp(scenario,"json-oom")) {
        PoolSchedule s=parse(weekly);cJSON_Hooks hooks={.malloc_fn=json_allocate,.free_fn=free};
        for(unsigned fail=1;fail<=29;fail++){json_allocations=0;json_fail_at=fail;cJSON_InitHooks(&hooks);assert(pool_schedule_save(&s)==ESP_ERR_NO_MEM && writes==0);cJSON_InitHooks(NULL);}
        assert(pool_schedule_save(&s)==ESP_OK && writes==1);
    } else if (!strcmp(scenario,"clock")) {
        struct tm local;setenv("TZ","America/New_York",1);tzset();
        assert(!pool_schedule_clock(now,false,0,&local) && !pool_schedule_clock(100,true,0,&local));
        assert(pool_schedule_clock(1710052200,true,60,&local) && local.tm_hour==7 && local.tm_min==30); // DST transition still fixed +01.
        assert(pool_schedule_clock(1730615400,true,60,&local) && local.tm_hour==7 && local.tm_min==30);
        assert(!strcmp(getenv("TZ"),"America/New_York"));
        PoolSchedule s=parse(weekly);assert(pool_schedule_save(&s)==ESP_OK);pool_schedule_tick();assert(applies==0);synchronized=true;pool_schedule_tick();assert(applies==1);synchronized=false;now+=36000;pool_schedule_tick();assert(applies==1);unchanged_power(&state);
    } else if (!strcmp(scenario,"reconcile")) {
        PoolSchedule s=parse(weekly);assert(pool_schedule_save(&s)==ESP_OK);synchronized=true;pool_schedule_tick();assert(applies==1 && current_hash==101);pool_schedule_tick();assert(applies==1);current_hash=109;pool_schedule_tick();assert(applies==2 && current_hash==101);now+=600*60;pool_schedule_tick();assert(applies==3 && current_hash==102);now+=7*86400;pool_schedule_tick();assert(applies==3);unchanged_power(&state);
    } else if (!strcmp(scenario,"failure")) {
        PoolSchedule s=parse(weekly);slots=1;assert(pool_schedule_save(&s)==ESP_ERR_NVS_NOT_FOUND && writes==0);slots=0x3ff;assert(pool_schedule_save(&s)==ESP_OK);assert(operating_profile_pool_referenced(1));fail_commit=1;s.enabled=false;assert(pool_schedule_save(&s)==ESP_FAIL);assert(pool_schedule_init(&state)==ESP_OK);synchronized=true;fail_apply=true;pool_schedule_tick();assert(applies==0 && current_hash==100);fail_apply=false;pool_schedule_tick();assert(applies==1 && current_hash==101);assert(operating_profile_pool_referenced(1));unchanged_power(&state);
    } else if (!strcmp(scenario,"http")) {
        assert(register_pool_schedule_api(NULL)==ESP_OK && route_count==2 && routes[1].method==HTTP_POST);
        httpd_req_t r={.body=weekly,.content_len=strlen(weekly)};unauthorized=true;routes[1].handler(&r);assert(r.code==401 && writes==0);unauthorized=false;r.received=0;routes[1].handler(&r);assert(r.code==200 && writes==1);cJSON *j=cJSON_Parse(r.reply);assert(cJSON_IsNull(cJSON_GetObjectItem(j,"selectedSlot")) && cJSON_GetObjectItem(j,"identity"));cJSON_Delete(j);
        r=(httpd_req_t){.body="{} garbage",.content_len=10};routes[1].handler(&r);assert(r.code==400);r=(httpd_req_t){.body="{}\0x",.content_len=4};routes[1].handler(&r);assert(r.code==400);
        slots=0;r=(httpd_req_t){.body=weekly,.content_len=strlen(weekly)};routes[1].handler(&r);assert(r.code==409 && strstr(r.reply,"missing-profile"));slots=0x3ff;fail_commit=1;r=(httpd_req_t){.body=weekly,.content_len=strlen(weekly)};routes[1].handler(&r);assert(r.code==507 && strstr(r.reply,"storage-unavailable"));
    } else if (!strcmp(scenario,"corrupt")) {
        strcpy(blob,"malformed");blob_size=strlen(blob)+1;assert(pool_schedule_init(&state)==ESP_OK);assert(operating_profile_pool_referenced(9));synchronized=true;pool_schedule_tick();assert(applies==0);PoolSchedule s={0};assert(pool_schedule_save(&s)==ESP_OK);assert(!operating_profile_pool_referenced(9));unchanged_power(&state);
    } else if (!strcmp(scenario,"reload")) {
        stratum_protocol_t proto=STRATUM_PROTOCOL_V1;
        for(unsigned i=1;i<=5;i++){allocations=0;fail_allocation=i;assert(SYSTEM_reload_primary_pool(&state,&proto)==ESP_ERR_NO_MEM);assert(!strcmp(state.SYSTEM_MODULE.pool_url,"old-pool.invalid") && proto==STRATUM_PROTOCOL_V1);unchanged_power(&state);}
        fail_allocation=0;configured_tls=3;assert(SYSTEM_reload_primary_pool(&state,&proto)==ESP_ERR_INVALID_ARG && !strcmp(state.SYSTEM_MODULE.pool_url,"old-pool.invalid"));configured_tls=0;
        fail_allocation=0;assert(SYSTEM_reload_primary_pool(&state,&proto)==ESP_OK && proto==STRATUM_PROTOCOL_V2);char host[254],user[513];uint16_t port;SYSTEM_copy_pool_identity(&state,false,host,sizeof(host),user,sizeof(user),&port);assert(!strcmp(host,"new-pool.invalid") && !strcmp(user,"new-worker") && port==4444);unchanged_power(&state);
    } else if (!strcmp(scenario,"readers")) {
        pthread_t readers[3];for(unsigned i=0;i<3;i++)assert(!pthread_create(&readers[i],NULL,identity_reader,&state));
        stratum_protocol_t proto;for(unsigned i=0;i<500;i++)assert(SYSTEM_reload_primary_pool(&state,&proto)==ESP_OK);
        for(unsigned i=0;i<3;i++)assert(!pthread_join(readers[i],NULL));unchanged_power(&state);
        free(state.SYSTEM_MODULE.pool_user);state.SYSTEM_MODULE.pool_user=malloc(701);memset(state.SYSTEM_MODULE.pool_user,'x',700);state.SYSTEM_MODULE.pool_user[700]=0;
        char *copy=SYSTEM_duplicate_pool_user(&state,false);assert(copy && strlen(copy)==700 && !strcmp(copy,state.SYSTEM_MODULE.pool_user));free(copy);
    } else if (!strcmp(scenario,"close-hooks")) {
        unsigned generation=protocol_coordinator_work_generation();pthread_t share,closer;
        assert(!pthread_create(&share,NULL,share_thread,NULL));
        while(!atomic_load(&share_holding)){struct timespec delay={.tv_nsec=1000000};nanosleep(&delay,NULL);}
        assert(!pthread_create(&closer,NULL,close_thread,&state));
        while(!atomic_load(&close_started)){struct timespec delay={.tv_nsec=1000000};nanosleep(&delay,NULL);}
        struct timespec delay={.tv_nsec=20000000};nanosleep(&delay,NULL);assert(!atomic_load(&close_done) && closes==0);
        atomic_store(&release_share,true);assert(!pthread_join(share,NULL) && !pthread_join(closer,NULL));
        assert(state.transport==NULL && closes==1 && job_clears==1 && protocol_coordinator_work_generation()!=generation);unchanged_power(&state);
        state.transport=(void*)1;state.sv2_noise_ctx=(void*)1;state.sv2_conn=calloc(1,sizeof(sv2_conn_t));generation=protocol_coordinator_work_generation();
        stratum_v2_close_connection(&state);stratum_v2_release_state(&state,state.sv2_conn);
        assert(!state.transport && !state.sv2_noise_ctx && !state.sv2_conn && closes==2 && job_clears==2 && protocol_coordinator_work_generation()!=generation);unchanged_power(&state);
    } else if (!strcmp(scenario,"coordinator")) {
        protocol_coordinator_request_primary_reload(&state);protocol_coordinator_init(&state);s_primary_protocol=STRATUM_PROTOCOL_V1;assert(apply_primary_reload(&state));assert(protocol_starts==0 && !strcmp(state.SYSTEM_MODULE.pool_url,"new-pool.invalid"));unchanged_power(&state);
        s_state=COORD_STATE_RUNNING_PRIMARY;s_running_protocol=STRATUM_PROTOCOL_V1;start_protocol_task(&state,s_running_protocol);unsigned old_generation=atomic_load(&s_running_generation);protocol_coordinator_request_primary_reload(&state);assert(apply_primary_reload(&state) && protocol_starts==2 && job_clears==1 && s_running_protocol==STRATUM_PROTOCOL_V2);unchanged_power(&state);
        handle_event(&state,(coordinator_event_t){.type=COORD_EVENT_PROTOCOL_SUCCESS,.generation=old_generation});assert(state.SYSTEM_MODULE.pools_unavailable);
        unsigned failures=s_consecutive_pool_failures;handle_event(&state,(coordinator_event_t){.type=COORD_EVENT_PROTOCOL_FAILED,.generation=old_generation});assert(s_consecutive_pool_failures==failures && s_state==COORD_STATE_RUNNING_PRIMARY && protocol_starts==2);
        auto_exit=false;protocol_coordinator_request_primary_reload(&state);char *before=state.SYSTEM_MODULE.pool_url;assert(!apply_primary_reload(&state) && state.SYSTEM_MODULE.pool_url==before && protocol_starts==2);unchanged_power(&state);
        auto_exit=true;assert(apply_primary_reload(&state));unsigned starts=protocol_starts;s_state=COORD_STATE_RUNNING_FALLBACK;state.SYSTEM_MODULE.is_using_fallback=true;protocol_coordinator_request_primary_reload(&state);assert(apply_primary_reload(&state) && protocol_starts==starts && state.SYSTEM_MODULE.is_using_fallback);s_state=COORD_STATE_PAUSED;protocol_coordinator_request_primary_reload(&state);assert(apply_primary_reload(&state) && protocol_starts==starts);unchanged_power(&state);
    } else assert(!"unknown scenario");
    free_state(&state);puts("PASS");return 0;
}
