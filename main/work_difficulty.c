#include "work_difficulty.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include <math.h>
#include <limits.h>
#include <string.h>

typedef struct {
    WorkDifficultyPool view;
    uint64_t session, job;
} Pool;
static Pool pools[2];
static uint64_t serial, generation = 1;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

static void invalidate_locked(void)
{
    if (generation && generation < UINT64_MAX) ++generation;
    else generation = 0; /* Exhaustion fails closed; never wraps into a valid generation. */
    for (unsigned i=0;i<2;i++) pools[i].view.available=false;
}
void work_difficulty_select(unsigned pool)
{
    taskENTER_CRITICAL(&lock);
    invalidate_locked();
    for(unsigned i=0;i<2;i++) {
        pools[i].view.selected=i==pool;
        pools[i].view.connected=false;
        pools[i].view.available=false;
    }
    taskEXIT_CRITICAL(&lock);
}
uint64_t work_difficulty_connect(unsigned pool)
{
    uint64_t result=0;
    taskENTER_CRITICAL(&lock);
    if(pool<2&&pools[pool].view.selected&&serial<UINT64_MAX&&generation) {
        invalidate_locked();
        Pool *p=&pools[pool];p->session=++serial;p->job=0;
        p->view.connected=true;p->view.available=false;p->view.received_at_us=0;
        result=p->session;
    }
    taskEXIT_CRITICAL(&lock);return result;
}
void work_difficulty_disconnect(void)
{
    taskENTER_CRITICAL(&lock);invalidate_locked();
    for(unsigned i=0;i<2;i++){pools[i].view.connected=false;pools[i].view.available=false;}
    taskEXIT_CRITICAL(&lock);
}
void work_difficulty_invalidate_dispatch(void)
{
    taskENTER_CRITICAL(&lock);invalidate_locked();taskEXIT_CRITICAL(&lock);
}
void work_difficulty_forget_job(unsigned pool,uint64_t session)
{
    taskENTER_CRITICAL(&lock);
    if(pool<2&&pools[pool].session==session) {
        pools[pool].view.available=false;pools[pool].view.received_at_us=0;
        if(pools[pool].job<UINT64_MAX)++pools[pool].job;
        else pools[pool].view.connected=false;
    }
    taskEXIT_CRITICAL(&lock);
}
work_source_stamp work_difficulty_received(unsigned pool,uint64_t session,uint64_t received_at_us)
{
    work_source_stamp stamp={0};
    taskENTER_CRITICAL(&lock);
    if(pool<2&&session&&received_at_us&&generation) {
        Pool *p=&pools[pool];
        if(p->view.selected&&p->view.connected&&p->session==session&&p->job==UINT64_MAX){p->view.available=false;p->view.connected=false;}
        if(p->view.selected&&p->view.connected&&p->session==session&&p->job<UINT64_MAX) {
            ++p->job;p->view.received_at_us=received_at_us;p->view.available=false;
            stamp=(work_source_stamp){.session_epoch=session,.job_epoch=p->job,.received_at_us=received_at_us,.pool_index=pool};
        }
    }
    taskEXIT_CRITICAL(&lock);return stamp;
}
static bool current_locked(const work_source_stamp *stamp)
{
    if(!stamp||stamp->pool_index>=2||!stamp->session_epoch||!stamp->job_epoch||!stamp->received_at_us)return false;
    const Pool *p=&pools[stamp->pool_index];
    return p->view.selected&&p->view.connected&&p->session==stamp->session_epoch&&p->job==stamp->job_epoch&&p->view.received_at_us==stamp->received_at_us;
}
uint64_t work_difficulty_prepare(const work_source_stamp *stamp)
{
    taskENTER_CRITICAL(&lock);uint64_t result=current_locked(stamp)?generation:0;taskEXIT_CRITICAL(&lock);return result;
}
bool work_difficulty_target(uint32_t bits,double *out)
{
    if (!out) return false;
    *out = 0;
    const unsigned size=bits>>24;uint32_t word=bits&0x007fffffU;
    if(!word||(bits&0x00800000U)||size>34||(word>0xffU&&size>33)||(word>0xffffU&&size>32))return false;
    unsigned exponent=size;
    if(size<=3){word>>=8*(3-size);exponent=3;}
    if(!word)return false;
    const double value=ldexp(65535.0/(double)word,8*(29-(int)exponent));
    if(!isfinite(value)||value<=0)return false;
    *out=value;return true;
}
bool work_difficulty_dispatched(const work_source_stamp *stamp,uint64_t captured_generation,
                               double actual_pool_diff,uint32_t actual_target,bool work_allowed)
{
    double network=0;
    const bool values_valid=work_allowed&&isfinite(actual_pool_diff)&&actual_pool_diff>0&&work_difficulty_target(actual_target,&network);
    bool accepted=false;
    taskENTER_CRITICAL(&lock);
    if(captured_generation&&captured_generation==generation&&current_locked(stamp)) {
        Pool *p=&pools[stamp->pool_index];p->view.available=values_valid;
        if(values_valid){p->view.share_difficulty=actual_pool_diff;p->view.network_difficulty=network;accepted=true;}
    }
    taskEXIT_CRITICAL(&lock);return accepted;
}
void work_difficulty_snapshot(WorkDifficultySnapshot *out,uint64_t now_us,bool work_allowed)
{
    if(!out)return;
    memset(out,0,sizeof(*out));out->now_us=now_us;
    taskENTER_CRITICAL(&lock);
    for(unsigned i=0;i<2;i++) {
        out->pools[i]=pools[i].view;
        out->pools[i].available=work_allowed&&out->pools[i].available&&out->pools[i].selected&&out->pools[i].connected&&out->pools[i].received_at_us&&now_us>=out->pools[i].received_at_us;
    }
    taskEXIT_CRITICAL(&lock);
}
