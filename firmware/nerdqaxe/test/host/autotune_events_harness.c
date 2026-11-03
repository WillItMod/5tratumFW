#include "autotune/events.h"
#include <assert.h>
#include <stdio.h>

static fwt_input sample(uint64_t now,fwt_point point,uint64_t generation) {
 fwt_input value;memset(&value,0,sizeof(value));
 value.valid=value.miningActive=value.fanOk=true;value.current=point;
 value.capturedAtUs=now;value.sampleId=now;value.generation=generation;
 value.voltageReadbackMv=point.coreVoltageMv;value.hashRateGHs=1000;value.powerW=20;
 value.asicTempC=50;value.vrmTempC=55;return value;
}
int main(void) {
 const fwt_limits limits={200,800,950,1400,70,95,60,6.25,5};
 const fwt_config config={FWT_EFFICIENCY,false,300,600,1000,1200,68,90,35};
 fwt_controller core;fwt_history history;memset(&history,0,sizeof(history));
 uint64_t now=FWT_SECOND,generation=1;fwt_point point={500,1150};
 fwt_input input=sample(now,point,generation);fwt_init(&core,&limits,&config);
 assert(fwt_start(&core,&input,now));fwt_history_capture(&history,&core);
 assert(fwt_history_count(&history)==1&&fwt_history_latest(&history)->kind==FWT_EVENT_STARTED);
 bool baseline=false,rejection=false,retained=false,requested=false,applied=false;
 for(unsigned n=0;n<1000&&core.state!=FWT_HOLDING;++n) {
  now+=5*FWT_SECOND;input=sample(now,point,generation);
  fwt_request request=fwt_tick(&core,&input,now);assert(request.kind!=FWT_REQUEST_PAUSE);
  fwt_history_capture(&history,&core);
  for(unsigned i=0;i<fwt_history_count(&history);++i) {
   const fwt_event *event=fwt_history_at(&history,i);
   baseline|=event->kind==FWT_EVENT_BASELINE;retained|=event->kind==FWT_EVENT_RETAINED;
   if(event->kind==FWT_EVENT_REJECTED) {
    rejection=true;assert(event->hasMeasurement&&event->hasScore);
    assert(event->hashRateGHs==1000&&event->powerW==20&&event->score==1&&event->requiredScore>=1.002);
   }
  }
  if(request.kind==FWT_REQUEST_APPLY) {
   const uint32_t id=core.lastAdjustment.id;unsigned count=fwt_history_count(&history);
   assert(core.lastAdjustment.outcome==FWT_OUTCOME_REQUESTED);requested=true;
   point=request.point;++generation;fwt_applied(&core,true,now);fwt_history_capture(&history,&core);
   assert(core.lastAdjustment.id==id&&core.lastAdjustment.outcome==FWT_OUTCOME_APPLIED);
   assert(fwt_history_count(&history)==count);applied=true;
  }
  const fwt_controller before=core;const fwt_history before_history=history;
  for(unsigned i=0;i<10;++i) fwt_history_capture(&history,&core);
  assert(!memcmp(&before,&core,sizeof(core))&&!memcmp(&before_history,&history,sizeof(history)));
 }
 assert(core.state==FWT_HOLDING&&baseline&&rejection&&retained&&requested&&applied);
 assert(core.rejected==4&&fwt_history_count(&history)==FWT_EVENT_CAPACITY);
 uint32_t last=0;
 for(unsigned i=0;i<FWT_EVENT_CAPACITY;++i) {
  const fwt_event *event=fwt_history_at(&history,i);assert(event&&event->id>last);last=event->id;
 }
 assert(fwt_history_at(&history,FWT_EVENT_CAPACITY)==NULL);
 assert(fwt_history_at(&history,0)->id>1); /* Old events were evicted, not unbounded. */
 fwt_cancel(&core,"manual-control");now+=FWT_SECOND;input=sample(now,point,generation);
 assert(fwt_start(&core,&input,now));fwt_history_capture(&history,&core);
 assert(fwt_history_count(&history)==1&&history.runId==2&&fwt_history_latest(&history)->id==1);
 /* A failed physical transition updates the request rather than pretending it applied. */
 fwt_request request=fwt_none();
 for(unsigned n=0;n<30&&request.kind==FWT_REQUEST_NONE;++n) {
  now+=5*FWT_SECOND;input=sample(now,point,generation);request=fwt_tick(&core,&input,now);fwt_history_capture(&history,&core);
 }
 assert(request.kind==FWT_REQUEST_APPLY);const uint32_t failed_id=core.lastAdjustment.id;
 fwt_applied(&core,false,now);fwt_history_capture(&history,&core);
 assert(core.state==FWT_FAULTED&&fwt_history_latest(&history)->id==failed_id&&fwt_history_latest(&history)->outcome==FWT_OUTCOME_FAILED);
 assert(!strcmp(fwt_event_short_label(fwt_history_latest(&history)),"Change failed"));
 /* A minimum-bound thermal stop records the rejected point and true cause.
  * It is not misreported as a completed measurement window. */
 fwt_config bounded=config;bounded.minFrequencyMHz=500;
 point=(fwt_point){500,1150};now+=FWT_SECOND;input=sample(now,point,++generation);
 fwt_init(&core,&limits,&bounded);memset(&history,0,sizeof(history));
 assert(fwt_start(&core,&input,now));now+=FWT_SECOND;
 input=sample(now,point,generation);input.asicTempC=bounded.maxAsicTempC;
 request=fwt_tick(&core,&input,now);fwt_history_capture(&history,&core);
 assert(request.kind==FWT_REQUEST_PAUSE&&!core.active&&!core.owns);
 assert(!strcmp(core.reason,"cannot-reduce-load-within-limits"));
 const fwt_event *blocked=fwt_history_latest(&history);
 assert(blocked&&blocked->kind==FWT_EVENT_REJECTED&&blocked->outcome==FWT_OUTCOME_FAILED);
 assert(!strcmp(blocked->reason,"temperature-limit-backoff")&&!blocked->hasMeasurement&&!blocked->hasScore);
 assert(fwt_point_equal(blocked->point,point)&&!strcmp(fwt_event_short_label(blocked),"Limits reached"));
 assert(blocked->hasConditions&&blocked->asicTempC==bounded.maxAsicTempC&&blocked->vrmTempC==55&&blocked->powerW==20);
 /* Regression from the observed OctAxe limit stop. Power and errors are
  * intentionally within bounds here: thermal margin alone must explain it. */
 {
  const fwt_limits octLimits={100,800,1050,1400,80,85,200,10,5};
  const fwt_config octConfig={FWT_PERFORMANCE,true,690,800,1175,1200,60,70,200};
  fwt_controller oct;fwt_point minimum={690,1180};now+=FWT_SECOND;
  fwt_input octInput=sample(now,minimum,1);octInput.asicTempC=62.05;octInput.vrmTempC=56.8;
  octInput.powerW=190;octInput.hashRateGHs=11000;
  fwt_init(&oct,&octLimits,&octConfig);assert(fwt_start(&oct,&octInput,now));
  const fwt_request stop=fwt_tick(&oct,&octInput,now);
  assert(stop.kind==FWT_REQUEST_PAUSE && oct.state==FWT_FAULTED && !oct.active);
  assert(!strcmp(oct.reason,"cannot-reduce-load-within-limits"));
  assert(!strcmp(oct.lastDecision.reason,"temperature-limit-backoff"));
  assert(oct.lastDecision.hasConditions && oct.lastDecision.asicTempC==62.05);
 }
 /* Seven genuine completed error windows exhaust correction. Retain the
  * failed point's measured means, never invent a score or thermal condition. */
 {
  fwt_controller unstable;fwt_history unstableHistory;memset(&unstableHistory,0,sizeof(unstableHistory));
  point=(fwt_point){500,1150};now+=FWT_SECOND;input=sample(now,point,++generation);
  fwt_init(&unstable,&limits,&config);assert(fwt_start(&unstable,&input,now));
  for(unsigned n=0;n<1000 && unstable.owns;++n) {
   now+=5*FWT_SECOND;input=sample(now,point,generation);
   input.errorPercentage=input.worstErrorPercentage=2;
   request=fwt_tick(&unstable,&input,now);fwt_history_capture(&unstableHistory,&unstable);
   if(request.kind==FWT_REQUEST_APPLY) {point=request.point;++generation;fwt_applied(&unstable,true,now);}
  }
  assert(!unstable.owns && unstable.recoveryAttempts==7 && !strcmp(unstable.reason,"error-recovery-exhausted"));
  const fwt_event *failed=fwt_history_latest(&unstableHistory);
  assert(failed && failed->kind==FWT_EVENT_REJECTED && failed->outcome==FWT_OUTCOME_FAILED);
  assert(failed->hasMeasurement && !failed->hasScore && !failed->hasConditions && fwt_point_equal(failed->point,point));
  assert(failed->hashRateGHs==1000 && failed->powerW==20 && failed->errorPercentage==2 && failed->worstErrorPercentage==2);
 }
 printf("PASS: measured decisions, requested/applied/failed identity, bounded history, read-only capture, new-run reset; event=%zu history=%zu controller=%zu bytes\n",sizeof(fwt_event),sizeof(fwt_history),sizeof(fwt_controller));
 return 0;
}
