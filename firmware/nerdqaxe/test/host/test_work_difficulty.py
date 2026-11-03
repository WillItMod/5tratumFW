#!/usr/bin/env python3
"""Actual job-table/manager/HTTP difficulty projection with fake transport only."""
from pathlib import Path
import os,re,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
def method(text,signature):
    start=text.index(signature);at=text.index('{',start);depth=1;end=at+1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
with tempfile.TemporaryDirectory(prefix='5tfw-difficulty-') as directory:
    p=Path(directory)
    (p/'esp_heap_caps.h').write_text('#pragma once\n')
    (p/'macros.h').write_text('#pragma once\n#include <cstdlib>\n#define MALLOC malloc\nstruct PThreadGuard { pthread_mutex_t &m; PThreadGuard(pthread_mutex_t &v):m(v){pthread_mutex_lock(&m);} ~PThreadGuard(){pthread_mutex_unlock(&m);} };\n')
    (p/'mining.h').write_text('#pragma once\n#include <cstdint>\n#include <cstdlib>\nstruct bm_job {char *extranonce2,*jobid; uint32_t pool_diff,target; int pool_id;};\ninline void free_bm_job(bm_job *p){free(p->extranonce2);free(p->jobid);free(p);}\n')
    source=r'''
#include <cassert>
#include <thread>
#include <atomic>
#include <cstring>
#include <string>
#include <iostream>
#include "tasks/asic_jobs.h"
#include "http_server/work_difficulty_report.h"
static uint64_t now=10000000;
uint64_t esp_timer_get_time(){return now;}
AsicJobs asicJobs;
namespace FiveTratumMining {static bool enabled=true;static uint64_t generation=1;bool miningWritesAllowed(){return enabled;}uint64_t workGeneration(){return generation;}}
struct PeerStatus {bool live=true;bool connected(){return live;}};
struct Peer {pthread_mutex_t m_muxStatusMutex=PTHREAD_MUTEX_INITIALIZER;PeerStatus m_muxStatus;bool m_validNotify=true;};
struct StratumManager {enum{PRIMARY=0,SECONDARY=1};pthread_mutex_t m_mutex=PTHREAD_MUTEX_INITIALIZER;Peer peers[2];Peer *m_stratumTasks[2]{&peers[0],&peers[1]};bool dual=false;int active=0;int getCompatPingPoolIndex(){return active;}bool isDualPool(){return dual;}FiveTratumWork::Snapshot getWorkDifficulty();};
'''
    source+=r'''
struct MiningInfoBase {virtual ~MiningInfoBase()=default;};
struct MiningInfoV2Standard:MiningInfoBase {template<class... T>void updateJob(T...){ }void setDifficulty(uint32_t){} };
struct MiningInfoV2Extended:MiningInfoBase {template<class... T>void updateJob(T...){ }void setDifficulty(uint32_t){} };
struct mining_notify{};struct MiningInfoV1 {void create_job_mining_notify(mining_notify*){}};
MiningInfoV2Standard *s_v2_standard[2]{};MiningInfoV2Extended *s_v2_extended[2]{};MiningInfoBase *miningInfo[2]{};
struct sv2_ext_job_t {uint32_t job_id=0;};
pthread_mutex_t current_stratum_job_mutex=PTHREAD_MUTEX_INITIALIZER;
MiningInfoV1 *ensureV1(int){static MiningInfoV1 value;return &value;}void trigger_job_creation(){}
#define ESP_LOGI(...) ((void)0)
'''
    v1=(ROOT/'main/tasks/create_jobs_task.cpp').read_text();v2=(ROOT/'main/tasks/create_jobs_sv2.cpp').read_text()
    source+=method(v1,'\nvoid create_job_mining_notify(')
    for name in ('void create_job_sv2_standard(','void create_job_sv2_extended(','void create_job_sv2_set_difficulty('):source+=method(v2,name)
    source+=method((ROOT/'main/stratum/stratum_manager.cpp').read_text(),'FiveTratumWork::Snapshot StratumManager::getWorkDifficulty()')
    source+=r'''
static StratumManager manager;static StratumManager *STRATUM_MANAGER=&manager;
using esp_err_t=int;constexpr int ESP_OK=0,ESP_FAIL=-1,HTTPD_401_UNAUTHORIZED=401,HTTPD_500_INTERNAL_SERVER_ERROR=500;
struct httpd_req_t {std::string response;};int http_server=0;static bool allowed=true;
struct ConGuard{ConGuard(int,httpd_req_t*){}};
int is_network_allowed(httpd_req_t*){return allowed?ESP_OK:ESP_FAIL;}int set_cors_headers(httpd_req_t*){return ESP_OK;}
int httpd_resp_send_err(httpd_req_t*,int,const char*){return ESP_FAIL;}void httpd_resp_send_500(httpd_req_t*){}
void httpd_resp_set_type(httpd_req_t*,const char*){}void httpd_resp_set_hdr(httpd_req_t*,const char*,const char*){}
int sendJsonResponse(httpd_req_t *req,JsonDocument &doc){serializeJson(doc,req->response);return ESP_OK;}
'''
    handler=(ROOT/'main/http_server/handler_work_difficulty.cpp').read_text()
    source+=re.sub(r'^#include[^\n]*\n','',handler,flags=re.M)
    source+=r'''
bm_job *job(int pool,uint32_t share=8192,uint32_t bits=0x1d00ffff){auto *j=(bm_job*)malloc(sizeof(bm_job));*j={strdup("01"),strdup("job"),share,bits,pool};return j;}
JsonDocument read(){httpd_req_t req;assert(GET_five_tratum_work_difficulty(&req)==ESP_OK);JsonDocument d;assert(!deserializeJson(d,req.response));return d;}
void publish(int pool,int slot,uint32_t share=8192,uint32_t bits=0x1d00ffff){asicJobs.noteWorkReceived(pool,now);auto stamp=asicJobs.workStamp(pool);asicJobs.storeJob(job(pool,share,bits),slot,1,stamp);}
int main(){
 double diff=0;assert(FiveTratumWork::targetDifficulty(0x1d00ffff,diff)&&diff==1);assert(FiveTratumWork::targetDifficulty(0x1b0404cb,diff)&&std::fabs(diff-16307.420938523983)<1e-7);
 for(uint32_t bits:{0U,0x1d80ffffU,0x2300ffffU,0x22010000U,0x21010000U,0x01000001U})assert(!FiveTratumWork::targetDifficulty(bits,diff)&&diff==0);
 assert(FiveTratumWork::targetDifficulty(0x01010000,diff)&&diff>1e60);
 auto empty=read();assert(empty["pools"].size()==2&&empty["pools"][0]["shareDifficulty"].isNull());
 uint8_t block[32]{};create_job_sv2_standard(0,1,1,block,block,1,0x1d00ffff,0,8192,false);
 auto standardStamp=asicJobs.workStamp(0);assert(standardStamp.receivedAtUs==now);now+=1000000;create_job_sv2_set_difficulty(0,4096);assert(asicJobs.workStamp(0).receivedAtUs==standardStamp.receivedAtUs);
 sv2_ext_job_t ext;create_job_sv2_extended(1,&ext,block,0,0,0,4096,false);assert(asicJobs.workStamp(1).receivedAtUs==now);
 mining_notify notify;create_job_mining_notify(0,&notify,true);assert(asicJobs.workStamp(0).epoch!=standardStamp.epoch&&asicJobs.workStamp(0).receivedAtUs==now);

 publish(0,5);auto first=read();assert(first["normalization"]=="bitcoin-diff1"&&first["pools"][0]["available"]==true&&first["pools"][0]["shareDifficulty"]==8192&&first["pools"][0]["networkDifficulty"]==1);
 const auto original=asicJobs.workStamp(0);now+=600000000;asicJobs.storeJob(job(0),6,1,original);auto old=read();assert(old["pools"][0]["available"]==true&&old["pools"][0]["jobAgeSeconds"]==600); // Long valid work remains valid; nonce generation and GET cannot renew its age.
 for(int i=0;i<12;++i)assert(read()["pools"][0]["jobAgeSeconds"]==600);
 FiveTratumMining::generation=2;assert(read()["pools"][0]["available"]==false);FiveTratumMining::generation=1;
 publish(1,7,4096,0x1b0404cb);manager.dual=true;auto both=read();assert(both["pools"][0]["selected"]==true&&both["pools"][1]["selected"]==true&&both["pools"][1]["shareDifficulty"]==4096);
 manager.dual=false;manager.active=1;auto fallback=read();assert(fallback["pools"][0]["available"]==false&&fallback["pools"][1]["available"]==true);
 manager.peers[1].m_muxStatus.live=false;assert(read()["pools"][1]["available"]==false);asicJobs.cleanJobs(1);manager.peers[1].m_muxStatus.live=true;assert(read()["pools"][1]["available"]==false);
 manager.active=0;asicJobs.noteWorkReceived(0,now);auto obsolete=asicJobs.workStamp(0);++now;asicJobs.noteWorkReceived(0,now);asicJobs.storeJob(job(0),8,1,obsolete);assert(read()["pools"][0]["available"]==false); // An old build may still be stored for ordinary mining, but cannot relabel a new session/job.
 auto current=asicJobs.workStamp(0);asicJobs.storeJob(job(0),9,1,current);assert(read()["pools"][0]["available"]==true);
 FiveTratumMining::enabled=false;assert(read()["pools"][0]["available"]==false);FiveTratumMining::enabled=true;
 publish(0,10,0);assert(read()["pools"][0]["available"]==false);publish(0,10,1,0x1d80ffff);assert(read()["pools"][0]["networkDifficulty"].isNull());
 publish(0,11);manager.dual=true;publish(1,11);assert(read()["pools"][0]["available"]==false&&read()["pools"][1]["available"]==true); // Reused wire slot cannot keep another peer's metadata.
 const auto retainedStamp=asicJobs.workStamp(1);asicJobs.resetChainGeneration(2);FiveTratumMining::generation=2;assert(read()["pools"][1]["available"]==false);asicJobs.storeJob(job(0),12,1,current);assert(read()["pools"][0]["available"]==false);
 now+=5000000;asicJobs.storeJob(job(1),12,2,retainedStamp);assert(read()["pools"][1]["available"]==true&&read()["pools"][1]["jobAgeSeconds"]==5); // A protected clock change preserves original upstream age, not retired hardware jobs.
 allowed=false;httpd_req_t denied;assert(GET_five_tratum_work_difficulty(&denied)==ESP_FAIL&&denied.response.empty());allowed=true;
 STRATUM_MANAGER=nullptr;assert(read()["pools"][0]["connected"]==false);STRATUM_MANAGER=&manager;
 delete s_v2_standard[0];delete s_v2_standard[1];delete s_v2_extended[0];delete s_v2_extended[1];
 std::cout<<"Actual dispatched-job, session invalidation, compact-target, passive HTTP and JSON cases passed\n";
}
'''
    (p/'test.cpp').write_text(source)
    exe=p/'test';subprocess.run([os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++'),'-std=c++17','-Wall','-Wextra','-Werror','-pthread','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I',str(p),'-I',str(ROOT/'main'),'-I',str(ROOT/'components/arduinojson'),str(p/'test.cpp'),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
# Production hook order is important: capture under the job lock, confirm only after dispatch.
main=(ROOT/'main/tasks/create_jobs_task.cpp').read_text()
assert main.index('workStamp=asicJobs.workStamp(active_pool)')<main.index('next_job = mi->buildBmJob')<main.index('asics->sendWork(')<main.index('operation.generation(),workStamp')
assert 'asicJobs.noteWorkReceived(pool,esp_timer_get_time());' in method(main,'\nvoid create_job_mining_notify(')
v2=(ROOT/'main/tasks/create_jobs_sv2.cpp').read_text()
for entry in ('void create_job_sv2_standard(','void create_job_sv2_extended('):assert 'asicJobs.noteWorkReceived(pool,esp_timer_get_time());' in method(v2,entry)
assert 'noteWorkReceived' not in method(v2,'void create_job_sv2_set_difficulty(')
assert 'asics->transportOkay()&&operation.isCurrent()' in main
print('Actual SV1/SV2 activation and dispatch metadata wiring passed; no physical I/O')
