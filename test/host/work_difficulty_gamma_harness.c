#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <pthread.h>
#include "global_state.h"
#include "mining_state.h"
#include "work_difficulty.h"
#include "work_difficulty_api.h"
#include "stratum_api.h"
#include "sv2_protocol.h"
#include "mining.h"

static int64_t now_us=1000000;
static bool network_allowed=true, tx_fail=false;
static uint32_t tx_failures;
static unsigned sends;
static void (*during_send)(void);
static httpd_uri_t route;
static bm_job sent_job;
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define MAX_EXTRANONCE2_LEN 32
#define MAX_EXTRANONCE2_STR 65
#define MAX_ACTIVE_JOB_IDS 16

int64_t esp_timer_get_time(void){return now_us;}
esp_err_t is_network_allowed(httpd_req_t *req){(void)req;return network_allowed?ESP_OK:ESP_FAIL;}
esp_err_t set_cors_headers(httpd_req_t *req){(void)req;return ESP_OK;}
esp_err_t httpd_resp_set_type(httpd_req_t *req,const char *v){(void)req;(void)v;return ESP_OK;}
esp_err_t httpd_resp_set_hdr(httpd_req_t *req,const char *key,const char *v){(void)req;assert(!strcmp(key,"Cache-Control")&&!strcmp(v,"no-store"));return ESP_OK;}
esp_err_t httpd_resp_send_500(httpd_req_t *req){req->status=500;return ESP_FAIL;}
esp_err_t httpd_resp_send_err(httpd_req_t *req,int status,const char *text){(void)text;req->status=status;return ESP_FAIL;}
esp_err_t httpd_resp_sendstr(httpd_req_t *req,const char *text){free(req->body);req->body=strdup(text);req->status=200;return ESP_OK;}
esp_err_t httpd_register_uri_handler(httpd_handle_t server,const httpd_uri_t *value){(void)server;route=*value;return ESP_OK;}
uint32_t SERIAL_tx_failure_count(void){return tx_failures;}
void ASIC_send_work(GlobalState *g,bm_job *job){(void)g;sent_job=*job;sends++;if(during_send)during_send();if(tx_fail&&tx_failures<UINT32_MAX)tx_failures++;free(job->jobid);free(job->extranonce2);free(job);}
void extranonce_2_generate(uint64_t extranonce_2,uint32_t length,char dest[static length * 2 + 1]){(void)extranonce_2;memset(dest,'0',length*2);dest[length*2]=0;}
void calculate_coinbase_tx_hash(const char*a,const char*b,const char*c,const char*d,uint8_t out[32]){(void)a;(void)b;(void)c;(void)d;memset(out,0,32);}
void calculate_coinbase_tx_hash_bin(const uint8_t*a,size_t b,const uint8_t*c,size_t d,const uint8_t*e,size_t f,const uint8_t*g,size_t h,uint8_t out[32]){(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;memset(out,0,32);}
void calculate_merkle_root_hash(const uint8_t a[32],const uint8_t b[][32],const int n,uint8_t out[32]){(void)a;(void)b;(void)n;memset(out,0,32);}
void reverse_32bit_words(const uint8_t in[32],uint8_t out[32]){memcpy(out,in,32);}
void reverse_endianness_per_word(uint8_t in[32]){(void)in;}
void midstate_sha256_bin(const uint8_t *in,size_t len,uint8_t out[32]){(void)in;(void)len;memset(out,0,32);}
uint32_t increment_bitmask(uint32_t v,uint32_t mask){return v+mask;}
size_t hex2bin(const char *a,uint8_t*b,size_t n){(void)a;memset(b,0,n);return n;}
size_t bin2hex(const uint8_t*a,size_t n,char*b,size_t limit){(void)a;assert(limit>=2*n+1);memset(b,'0',n*2);b[n*2]=0;return n*2;}
void SYSTEM_notify_new_ntime(GlobalState*g,uint32_t n){(void)g;(void)n;}
void SYSTEM_clean_jobs_queue(GlobalState*g){g->stratum_queue.count=0;work_difficulty_invalidate_dispatch();}
void queue_enqueue(FakeQueue*q,void*p){q->last=p;q->count++;}
void *queue_dequeue(FakeQueue*q){void*p=q->last;q->last=NULL;q->count=0;return p;}
void sv2_ext_job_free(sv2_ext_job_t*p){free(p->coinbase_prefix);free(p->coinbase_suffix);free(p);}
void STRATUM_V1_free_mining_notify(mining_notify*p){free(p);}
void decode_mining_notification(GlobalState*g,const mining_notify*n){(void)g;(void)n;}
#include "real_work_functions.inc"

static WorkDifficultySnapshot read_snapshot(void){WorkDifficultySnapshot s;work_difficulty_snapshot(&s,(uint64_t)now_us,true);return s;}
static uint64_t session;
static work_source_stamp stamp;
static work_source_stamp new_stamp(unsigned pool,uint64_t time){work_difficulty_select(pool);session=work_difficulty_connect(pool);assert(session);stamp=work_difficulty_received(pool,session,time);assert(stamp.job_epoch);return stamp;}
static void compact_case(void){double d;
 assert(work_difficulty_target(0x1d00ffff,&d)&&d==1.0);assert(work_difficulty_target(0x1c00ffff,&d)&&d==256.0);assert(work_difficulty_target(0x1b00ffff,&d)&&d==65536.0);
 uint32_t bad[]={0,0x1d80ffff,0x23000001,0x22000100,0x21010000,0x00000001,0x01003456,0x1d000000};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){d=123;assert(!work_difficulty_target(bad[i],&d)&&d==0);}
 assert(work_difficulty_target(0x220000ff,&d)&&isfinite(d)&&d>0);assert(work_difficulty_target(0x2100ffff,&d)&&isfinite(d)&&d>0);
}
static void lifecycle_case(void){stamp=new_stamp(0,1000000);uint64_t generation=work_difficulty_prepare(&stamp);assert(generation);
 assert(work_difficulty_dispatched(&stamp,generation,4096.5,0x1d00ffff,true));now_us=601000000;WorkDifficultySnapshot s=read_snapshot();assert(s.pools[0].available&&s.pools[0].share_difficulty==4096.5&&s.pools[0].network_difficulty==1&&s.pools[0].received_at_us==1000000&&!s.pools[1].selected);
 assert(work_difficulty_dispatched(&stamp,work_difficulty_prepare(&stamp),8192.25,0x1d00ffff,true));s=read_snapshot();assert(s.pools[0].received_at_us==1000000&&s.pools[0].share_difficulty==8192.25);
 stamp=new_stamp(1,601000000);assert(work_difficulty_dispatched(&stamp,work_difficulty_prepare(&stamp),3.5,0x1c00ffff,true));s=read_snapshot();assert(!s.pools[0].available&&!s.pools[0].connected&&s.pools[1].available&&s.pools[1].selected&&s.pools[1].network_difficulty==256);
 work_difficulty_disconnect();s=read_snapshot();assert(!s.pools[1].available&&!s.pools[1].connected);assert(!work_difficulty_prepare(&stamp));
}
static void race_case(void){stamp=new_stamp(0,1000000);work_source_stamp old=stamp;uint64_t old_gen=work_difficulty_prepare(&old);now_us=2000000;stamp=work_difficulty_received(0,session,now_us);assert(!work_difficulty_dispatched(&old,old_gen,1,0x1d00ffff,true));
 uint64_t generation=work_difficulty_prepare(&stamp);work_difficulty_invalidate_dispatch();assert(!work_difficulty_dispatched(&stamp,generation,1,0x1d00ffff,true));
 generation=work_difficulty_prepare(&stamp);assert(work_difficulty_dispatched(&stamp,generation,2,0x1d00ffff,true));assert(read_snapshot().pools[0].received_at_us==2000000);
 work_difficulty_forget_job(0,session);assert(!work_difficulty_prepare(&stamp));assert(!read_snapshot().pools[0].available);
 uint64_t old_session=session;stamp=new_stamp(0,3000000);assert(!work_difficulty_received(0,old_session,4000000).job_epoch);assert(!work_difficulty_dispatched(&old,old_gen,1,0x1d00ffff,true));
}
static void invalid_case(void){stamp=new_stamp(0,1000000);uint64_t generation=work_difficulty_prepare(&stamp);assert(work_difficulty_dispatched(&stamp,generation,1,0x1d00ffff,true));
 assert(!work_difficulty_dispatched(&stamp,generation,NAN,0x1d00ffff,true));assert(!read_snapshot().pools[0].available);
 assert(!work_difficulty_dispatched(&stamp,generation,1,0x23000001,true));assert(!read_snapshot().pools[0].available);
 assert(!work_difficulty_dispatched(&stamp,generation,1,0x1d00ffff,false));
}
static void replace_during_send(void){now_us++;work_difficulty_received(0,session,(uint64_t)now_us);}
static void constructors_case(void){GlobalState g={.allowed=true,.extranonce_2_len=2,.extranonce_str="00"};
 stamp=new_stamp(0,1000000);mining_notify n={.job_id="1",.prev_block_hash="",.coinbase_1="",.coinbase_2="",.target=0x1d00ffff,.work_source=stamp};
 generate_work(&g,&n,0,12.5);assert(read_snapshot().pools[0].share_difficulty==12.5);now_us+=600000000;generate_work(&g,&n,1,12.5);assert(read_snapshot().pools[0].received_at_us==1000000);
 sv2_conn_t conn={.extranonce_size=2};g.sv2_conn=&conn;stamp=new_stamp(0,1000000);sv2_job_t j={.nbits=0x1c00ffff,.work_source=stamp};generate_work_sv2(&g,&j,22.25);assert(read_snapshot().pools[0].share_difficulty==22.25&&read_snapshot().pools[0].network_difficulty==256);
 stamp=new_stamp(0,1000000);sv2_ext_job_t e={.nbits=0x1d00ffff,.work_source=stamp};generate_work_sv2_ext(&g,&e,33.75,2);assert(read_snapshot().pools[0].share_difficulty==33.75);
 during_send=replace_during_send;generate_work_sv2_ext(&g,&e,44,3);during_send=NULL;assert(!read_snapshot().pools[0].available);
 stamp=new_stamp(0,1000000);j.work_source=stamp;tx_fail=true;generate_work_sv2(&g,&j,55);tx_fail=false;assert(!read_snapshot().pools[0].available);
 tx_failures=UINT32_MAX;generate_work_sv2(&g,&j,55);assert(!read_snapshot().pools[0].available);
}
static void enqueue_case(void){GlobalState g={.allowed=true};sv2_conn_t conn={0};stamp=new_stamp(0,1000000);conn.work_source_session=session;conn.work_source_pool=0;uint8_t zero[32]={0};
 now_us=123000000;stratum_v2_enqueue_job(&g,&conn,42,0,zero,zero,0,0x1d00ffff,true);sv2_job_t*j=g.stratum_queue.last;assert(j->work_source.received_at_us==123000000);generate_work_sv2(&g,j,6.25);assert(read_snapshot().pools[0].received_at_us==123000000);free(j);g.stratum_queue.last=NULL;g.stratum_queue.count=0;
 now_us=124000000;stratum_v2_enqueue_job(&g,&conn,42,0,zero,zero,0,0x1d00ffff,false);assert(g.stratum_queue.count==0&&read_snapshot().pools[0].received_at_us==123000000);
 sv2_ext_job_t*e=calloc(1,sizeof(*e));e->job_id=43;e->nbits=0x1d00ffff;e->clean_jobs=true;now_us=125000000;stratum_v2_enqueue_ext_job(&g,&conn,e);assert(e->work_source.received_at_us==125000000);sv2_ext_job_free(e);
 now_us=126000000;mining_notify*n=calloc(1,sizeof(*n));n->job_id="44";n->target=0x1d00ffff;accept_real_v1(&g,n,0,session,(uint64_t)now_us);assert(n->work_source.received_at_us==126000000);free(n);
}
static void http_case(void){GlobalState g={.allowed=true};assert(work_difficulty_register(NULL,&g)==ESP_OK);assert(!strcmp(route.uri,"/api/5tratum/work-difficulty")&&route.method==HTTP_GET);
 stamp=new_stamp(0,1000000);assert(work_difficulty_dispatched(&stamp,work_difficulty_prepare(&stamp),4096.5,0x1d00ffff,true));now_us=601000000;httpd_req_t req={0};unsigned count=sends;assert(route.handler(&req)==ESP_OK);assert(sends==count);cJSON*d=cJSON_Parse(req.body);assert(d);assert(!strcmp(cJSON_GetObjectItem(d,"normalization")->valuestring,"bitcoin-diff1"));cJSON*p=cJSON_GetObjectItem(d,"pools");assert(cJSON_GetArraySize(p)==2);cJSON*a=cJSON_GetArrayItem(p,0),*b=cJSON_GetArrayItem(p,1);assert(cJSON_GetObjectItem(a,"jobAgeSeconds")->valuedouble==600);assert(cJSON_IsNull(cJSON_GetObjectItem(b,"shareDifficulty")));cJSON_Delete(d);
 now_us+=1000000;route.handler(&req);d=cJSON_Parse(req.body);a=cJSON_GetArrayItem(cJSON_GetObjectItem(d,"pools"),0);assert(cJSON_GetObjectItem(a,"jobAgeSeconds")->valuedouble==601);cJSON_Delete(d);
 g.allowed=false;route.handler(&req);d=cJSON_Parse(req.body);a=cJSON_GetArrayItem(cJSON_GetObjectItem(d,"pools"),0);assert(cJSON_IsFalse(cJSON_GetObjectItem(a,"available"))&&cJSON_IsNull(cJSON_GetObjectItem(a,"networkDifficulty")));cJSON_Delete(d);
 network_allowed=false;assert(route.handler(&req)==ESP_FAIL&&req.status==401);free(req.body);
}
static atomic_bool writer_done;
static void *write_concurrently(void *arg){(void)arg;for(unsigned i=1;i<=10000;i++){work_source_stamp current=work_difficulty_received(0,session,1000000+i);work_difficulty_dispatched(&current,work_difficulty_prepare(&current),(double)i,i%2?0x1d00ffff:0x1c00ffff,true);}atomic_store(&writer_done,true);return NULL;}
static void threaded_case(void){new_stamp(0,1000000);atomic_store(&writer_done,false);pthread_t writer;assert(!pthread_create(&writer,NULL,write_concurrently,NULL));do{WorkDifficultySnapshot s;work_difficulty_snapshot(&s,2000000000,true);if(s.pools[0].available){unsigned i=(unsigned)s.pools[0].share_difficulty;assert(s.pools[0].received_at_us==1000000+i);assert(s.pools[0].network_difficulty==(i%2?1:256));}}while(!atomic_load(&writer_done));assert(!pthread_join(writer,NULL));}
int main(int argc,char**argv){assert(argc==2);if(!strcmp(argv[1],"compact"))compact_case();else if(!strcmp(argv[1],"lifecycle"))lifecycle_case();else if(!strcmp(argv[1],"race"))race_case();else if(!strcmp(argv[1],"invalid"))invalid_case();else if(!strcmp(argv[1],"constructors"))constructors_case();else if(!strcmp(argv[1],"enqueue"))enqueue_case();else if(!strcmp(argv[1],"http"))http_case();else if(!strcmp(argv[1],"threaded"))threaded_case();else return 2;puts("PASS");return 0;}
