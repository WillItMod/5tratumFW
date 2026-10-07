#include "profiles_runtime_stubs.h"
#include "nvs_config.h"
#include "operating_profiles.h"
#include "cJSON.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct { unsigned ns; char key[32]; unsigned char *bytes; size_t size; int type; uint64_t number; } Entry;
static Entry store[100], pending;
static size_t stored_count;
static bool fail_set, fail_commit, authorized = true, reference_guard;
static unsigned writes, commits, erases, reloads;
static httpd_uri_t routes[3];
static unsigned route_count;
static int fail_journal_allocation = -1, journal_allocations;
void *profiles_test_malloc(size_t size) { if (fail_journal_allocation >= 0 && journal_allocations++ == fail_journal_allocation) return NULL; return malloc(size); }
static int fail_json_allocation = -1, json_allocations;
static void *json_malloc(size_t size) { if (fail_json_allocation >= 0 && json_allocations++ == fail_json_allocation) return NULL; return malloc(size); }
static Entry *find(unsigned ns, const char *key) { for (size_t i = 0; i < stored_count; i++) if (store[i].ns == ns && !strcmp(store[i].key, key)) return &store[i]; return NULL; }
static void put(unsigned ns, const char *key, int type, const char *value, uint64_t number) { Entry *entry = find(ns,key); if (!entry) entry = &store[stored_count++]; entry->ns=ns; snprintf(entry->key,32,"%s",key); entry->type=type; free(entry->bytes); entry->bytes=value?(unsigned char *)strdup(value):NULL; entry->size=value?strlen(value)+1:0; entry->number=number; }
const char *esp_err_to_name(esp_err_t error) { (void)error; return "host-test"; }
esp_err_t nvs_flash_init(void) { return ESP_OK; }
esp_err_t nvs_flash_erase(void) { erases++; return ESP_FAIL; }
esp_err_t nvs_open(const char *name,nvs_open_mode_t mode,nvs_handle_t *handle) { unsigned ns=!strcmp(name,"main")?1:2; if (mode==NVS_READONLY && ns==2 && !stored_count) return ESP_ERR_NVS_NOT_FOUND; *handle=ns; return ESP_OK; }
void nvs_close(nvs_handle_t handle) { (void)handle; free(pending.bytes); memset(&pending,0,sizeof(pending)); }
esp_err_t nvs_get_stats(const char *name,nvs_stats_t *stats) { (void)name; memset(stats,0,sizeof(*stats)); return ESP_OK; }
esp_err_t nvs_find_key(nvs_handle_t h,const char *key,void *type) { (void)type; return find(h,key)?ESP_OK:ESP_ERR_NVS_NOT_FOUND; }
static esp_err_t bytes_read(nvs_handle_t h,const char *key,void *out,size_t *size,int type) { Entry *entry=find(h,key); if(!entry)return ESP_ERR_NVS_NOT_FOUND; if(entry->type!=type)return ESP_ERR_NVS_TYPE_MISMATCH; if(!out){*size=entry->size;return ESP_OK;} if(*size<entry->size)return ESP_ERR_NVS_INVALID_LENGTH;memcpy(out,entry->bytes,entry->size);*size=entry->size;return ESP_OK; }
esp_err_t nvs_get_str(nvs_handle_t h,const char *key,char *out,size_t *size) {return bytes_read(h,key,out,size,1);}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size) {return bytes_read(h,key,out,size,2);}
static esp_err_t number_read(nvs_handle_t h,const char *key,uint64_t *out,int type){Entry *entry=find(h,key);if(!entry)return ESP_ERR_NVS_NOT_FOUND;if(entry->type!=type)return ESP_ERR_NVS_TYPE_MISMATCH;*out=entry->number;return ESP_OK;}
esp_err_t nvs_get_u16(nvs_handle_t h,const char *key,uint16_t *out){uint64_t n;esp_err_t e=number_read(h,key,&n,3);if(e==ESP_OK)*out=n;return e;}
esp_err_t nvs_get_i32(nvs_handle_t h,const char *key,int32_t *out){uint64_t n;esp_err_t e=number_read(h,key,&n,4);if(e==ESP_OK)*out=n;return e;}
esp_err_t nvs_get_u64(nvs_handle_t h,const char *key,uint64_t *out){return number_read(h,key,out,5);}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *bytes,size_t size){writes++;if(fail_set)return ESP_FAIL;free(pending.bytes);pending=(Entry){.ns=h,.type=2,.size=size,.bytes=malloc(size)};assert(pending.bytes);memcpy(pending.bytes,bytes,size);snprintf(pending.key,32,"%s",key);return ESP_OK;}
esp_err_t nvs_set_str(nvs_handle_t h,const char *key,const char *s){(void)h;(void)key;(void)s;writes++;return fail_set?ESP_FAIL:ESP_OK;}
esp_err_t nvs_set_u16(nvs_handle_t h,const char *key,uint16_t n){(void)h;(void)key;(void)n;writes++;return ESP_OK;}
esp_err_t nvs_set_i32(nvs_handle_t h,const char *key,int32_t n){(void)h;(void)key;(void)n;writes++;return ESP_OK;}
esp_err_t nvs_set_u64(nvs_handle_t h,const char *key,uint64_t n){(void)h;(void)key;(void)n;writes++;return ESP_OK;}
esp_err_t nvs_erase_key(nvs_handle_t h,const char *key){(void)h;(void)key;writes++;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h){(void)h;commits++;if(fail_commit)return ESP_FAIL;if(pending.bytes){Entry *entry=find(pending.ns,pending.key);if(!entry)entry=&store[stored_count++];free(entry->bytes);*entry=pending;memset(&pending,0,sizeof(pending));}return ESP_OK;}
QueueHandle_t xQueueCreate(unsigned count,unsigned size){(void)count;(void)size;return(void *)1;}
BaseType_t xQueueReceive(QueueHandle_t q,void *v,unsigned t){(void)q;(void)v;(void)t;return 0;}
BaseType_t xQueueSend(QueueHandle_t q,const void *v,unsigned t){(void)q;(void)v;(void)t;return pdPASS;}
SemaphoreHandle_t xSemaphoreCreateMutex(void){return(void *)1;}
BaseType_t xSemaphoreTake(SemaphoreHandle_t s,unsigned t){(void)s;(void)t;return pdPASS;}
BaseType_t xSemaphoreGive(SemaphoreHandle_t s){(void)s;return pdPASS;}
BaseType_t xTaskCreate(void(*fn)(void *),const char *name,unsigned size,void *arg,unsigned priority,TaskHandle_t *task){(void)fn;(void)name;(void)size;(void)arg;(void)priority;(void)task;return pdPASS;}
const esp_app_desc_t *esp_app_get_description(void){static esp_app_desc_t descriptor={.version="5tratumFW-0.1.0-beta.2"};return &descriptor;}
esp_err_t esp_read_mac(uint8_t *out,int kind){(void)kind;const uint8_t mac[]={2,0,0,0,0,1};memcpy(out,mac,6);return ESP_OK;}
int mbedtls_sha256(const unsigned char *bytes,size_t size,unsigned char *out,int mode){assert(size==24 && !memcmp(bytes,"5tratum/device/v1\0",18));(void)mode;for(unsigned i=0;i<32;i++)out[i]=i+1;return 0;}
void protocol_coordinator_request_primary_reload(GlobalState *global){assert(global);reloads++;}
bool operating_profile_pool_referenced(unsigned slot){return reference_guard && slot==0;}
esp_err_t is_network_allowed(httpd_req_t *req){(void)req;return authorized?ESP_OK:ESP_FAIL;}
esp_err_t set_cors_headers(httpd_req_t *req){(void)req;return ESP_OK;}
esp_err_t httpd_resp_set_hdr(httpd_req_t *req,const char *name,const char *value){(void)req;(void)name;(void)value;return ESP_OK;}
esp_err_t httpd_resp_set_type(httpd_req_t *req,const char *type){(void)req;(void)type;return ESP_OK;}
esp_err_t httpd_resp_set_status(httpd_req_t *req,const char *status){req->code=atoi(status);return ESP_OK;}
esp_err_t httpd_resp_sendstr(httpd_req_t *req,const char *text){snprintf(req->reply,sizeof(req->reply),"%s",text);return ESP_OK;}
esp_err_t httpd_resp_send_err(httpd_req_t *req,int code,const char *text){req->code=code;return httpd_resp_sendstr(req,text);}
void httpd_resp_send_500(httpd_req_t *req){req->code=500;}
int httpd_req_recv(httpd_req_t *req,char *out,size_t size){size_t count=req->content_len-req->received;if(count>size)count=size;if(count>17)count=17;memcpy(out,req->body+req->received,count);req->received+=count;return count;}
esp_err_t httpd_register_uri_handler(httpd_handle_t server,const httpd_uri_t *route){(void)server;assert(route_count<3);routes[route_count++]=*route;return ESP_OK;}
esp_err_t HTTP_send_json(httpd_req_t *req,const cJSON *root,int *prebuffer){(void)prebuffer;char *text=cJSON_PrintUnformatted(root);if(!text)return ESP_ERR_NO_MEM;esp_err_t error=httpd_resp_sendstr(req,text);free(text);return error;}
static httpd_req_t request(unsigned route,const char *body){httpd_req_t req={.code=200,.body=body,.content_len=body?strlen(body):0};routes[route].handler(&req);return req;}
static void assert_string(NvsConfigKey key,const char *value){char *actual=nvs_config_get_string(key);assert(actual&&!strcmp(actual,value));free(actual);}
static bool deny_apply(void *context){(void)context;return false;}
static esp_err_t observe_slots(void *context){(*(unsigned *)context)++;return ESP_OK;}
static void bootstrap(void){put(1,"boardversion",1,"601",0);put(1,"asicfrequency_f",1,"625.125",0);put(1,"asicvoltage",3,NULL,1173);put(1,"stratumprot",1,"SV1",0);put(1,"stratumurl",1,"pool.fixture.invalid",0);put(1,"stratumport",3,NULL,3333);put(1,"stratumuser",1,"fixture.worker",0);put(1,"stratumpass",1,"fixture-pass-A",0);put(1,"fbstratumurl",1,"backup.fixture.invalid",0);put(1,"fbstratumport",3,NULL,4444);put(1,"fbstratumpass",1,"fixture-pass-B",0);assert(nvs_config_init()==ESP_OK);static GlobalState gs;assert(device_config_init(&gs)==ESP_OK);assert(operating_profiles_register(NULL,&gs)==ESP_OK);assert(!writes&&!commits&&!erases);}
int main(int argc,char **argv){assert(argc==2);bootstrap();const char *scenario=argv[1];
 if(!strcmp(scenario,"redaction")){httpd_req_t req=request(0,NULL);assert(req.code==200);assert(!strstr(req.reply,"fixture-pass"));cJSON *json=cJSON_Parse(req.reply);assert(json);assert(cJSON_GetArraySize(cJSON_GetObjectItem(json,"tuning"))==10);assert(cJSON_GetArraySize(cJSON_GetObjectItem(json,"pools"))==10);assert(cJSON_GetObjectItem(cJSON_GetObjectItem(json,"identity"),"deviceId"));assert(cJSON_GetObjectItem(cJSON_GetObjectItem(json,"current"),"frequencyMHz")->valuedouble==625.125);cJSON_Delete(json);assert(!writes);}
 else if(!strcmp(scenario,"auth")){authorized=false;for(unsigned i=0;i<3;i++){httpd_req_t req=request(i,"{\"type\":\"tuning\",\"frequencyMHz\":525,\"coreVoltageMv\":1150}");assert(req.code==401&&req.received==0);}assert(!writes);}
 else if(!strcmp(scenario,"point")){httpd_req_t req=request(2,"{\"type\":\"tuning\",\"frequencyMHz\":525.25,\"coreVoltageMv\":1151}");assert(req.code==200);assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==525.25f);assert(nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE)==1151);assert_string(NVS_CONFIG_FALLBACK_STRATUM_PASS,"fixture-pass-B");assert(!erases);unsigned before=writes;assert(nvs_config_init()==ESP_OK);assert(writes==before);assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==525.25f);assert(nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE)==1151);}
 else if(!strcmp(scenario,"bounds")){const char *bad[]={"{\"type\":\"tuning\",\"frequencyMHz\":626,\"coreVoltageMv\":1150}","{\"type\":\"tuning\",\"frequencyMHz\":525.1,\"coreVoltageMv\":1150}","{\"type\":\"tuning\",\"frequencyMHz\":525,\"coreVoltageMv\":1251}","{\"type\":\"tuning\",\"frequencyMHz\":525,\"coreVoltageMv\":1150.5}","{\"type\":\"tuning\",\"frequencyMHz\":525,\"coreVoltageMv\":true}","{\"type\":\"tuning\",\"frequencyMHz\":525,\"frequencyMHz\":500,\"coreVoltageMv\":1150}"};for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++)assert(request(2,bad[i]).code==400);assert(!writes);assert(request(2,"{\"type\":\"tuning\",\"frequencyMHz\":625.125,\"coreVoltageMv\":1173}").code==200);}
 else if(!strcmp(scenario,"pool")){assert(request(1,"{\"type\":\"pool\",\"slot\":0,\"name\":\"Fixture pool\",\"captureCurrent\":\"primary\"}").code==200);httpd_req_t req=request(0,NULL);assert(req.code==200&&!strstr(req.reply,"fixture-pass"));assert(strstr(req.reply,"passwordConfigured\":true"));NvsConfigMutation change={.key=NVS_CONFIG_STRATUM_URL,.type=TYPE_STR,.value.str="other.fixture.invalid"};assert(nvs_config_apply_atomic(&change,1)==ESP_OK);assert(request(2,"{\"type\":\"pool\",\"slot\":0,\"poolTarget\":\"primary\"}").code==200);assert_string(NVS_CONFIG_STRATUM_URL,"pool.fixture.invalid");assert_string(NVS_CONFIG_STRATUM_PASS,"fixture-pass-A");assert_string(NVS_CONFIG_FALLBACK_STRATUM_PASS,"fixture-pass-B");assert(reloads==1);unsigned before_guard=writes;assert(operating_profile_apply_pool_guarded(0,false,deny_apply,NULL)==ESP_ERR_INVALID_STATE&&writes==before_guard);unsigned callbacks=0;assert(operating_profiles_with_pool_slots(1,observe_slots,&callbacks)==ESP_OK&&callbacks==1);assert(operating_profiles_with_pool_slots(2,observe_slots,&callbacks)!=ESP_OK&&callbacks==1);assert(request(1,"{\"type\":\"pool\",\"slot\":0,\"name\":\"Invalid TLS\",\"tls\":3}").code==400&&writes==before_guard);uint32_t slot_hash,current_hash;assert(operating_profile_pool_hash(0,&slot_hash)&&operating_profile_current_pool_hash(false,&current_hash)&&slot_hash==current_hash);reference_guard=true;unsigned before=writes;assert(request(1,"{\"type\":\"pool\",\"slot\":0,\"clear\":true}").code==409);assert(writes==before);}
 else if(!strcmp(scenario,"failure")){for(unsigned commit=0;commit<2;commit++){fail_set=!commit;fail_commit=commit;httpd_req_t req=request(2,"{\"type\":\"tuning\",\"frequencyMHz\":525,\"coreVoltageMv\":1150}");assert(req.code==507);assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==625.125f);assert(nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE)==1173);}fail_set=fail_commit=false;assert(nvs_config_init()==ESP_OK);assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==625.125f);assert(!erases);}
 else if(!strcmp(scenario,"journal-oom")){NvsConfigMutation changes[]={{.key=NVS_CONFIG_ASIC_FREQUENCY,.type=TYPE_FLOAT,.value.f=525.25f},{.key=NVS_CONFIG_ASIC_VOLTAGE,.type=TYPE_U16,.value.u16=1151}};unsigned failures=0;for(int i=0;i<32;i++){fail_journal_allocation=i;journal_allocations=0;unsigned before=writes;esp_err_t err=nvs_config_apply_atomic(changes,2);fail_journal_allocation=-1;if(err!=ESP_OK){failures++;assert(err==ESP_ERR_NO_MEM&&writes==before);assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==625.125f&&nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE)==1173);}else{NvsConfigMutation restore[]={{.key=NVS_CONFIG_ASIC_FREQUENCY,.type=TYPE_FLOAT,.value.f=625.125f},{.key=NVS_CONFIG_ASIC_VOLTAGE,.type=TYPE_U16,.value.u16=1173}};assert(nvs_config_apply_atomic(restore,2)==ESP_OK);}}assert(failures>10);assert(!erases);}
 else if(!strcmp(scenario,"private-slot")){const char *raw="{\"name\":null,\"password\":\"private-fixture\",\"unknown\":\"hidden\"}";assert(nvs_set_blob(2,"p0",raw,strlen(raw)+1)==ESP_OK&&nvs_commit(2)==ESP_OK);httpd_req_t req=request(0,NULL);assert(req.code==200&&!strstr(req.reply,"private-fixture")&&!strstr(req.reply,"hidden"));}
 else if(!strcmp(scenario,"corrupt-journal")){assert(request(2,"{\"type\":\"tuning\",\"frequencyMHz\":525.25,\"coreVoltageMv\":1151}").code==200);Entry *entry=find(1,"5fw_operating");assert(entry&&entry->size>12);entry->bytes[12]^=1;unsigned before=writes;assert(nvs_config_init()==ESP_ERR_INVALID_STATE);assert(writes==before&&!erases);}
 else if(!strcmp(scenario,"oom")){cJSON_Hooks hooks={.malloc_fn=json_malloc,.free_fn=free};cJSON_InitHooks(&hooks);for(int i=0;i<80;i++){fail_json_allocation=i;json_allocations=0;unsigned before=writes;httpd_req_t req=request(1,"{\"type\":\"pool\",\"slot\":0,\"name\":\"Fixture pool\",\"captureCurrent\":\"primary\"}");if(req.code!=200)assert(writes==before);assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY)==625.125f);}fail_json_allocation=-1;}
 else assert(0);
 puts("PASS");return 0;}
