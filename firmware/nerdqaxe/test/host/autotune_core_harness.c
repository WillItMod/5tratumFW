#include "autotune/core.h"
#include <assert.h>
#include <stdio.h>

static fwt_limits limits(void) {
 fwt_limits l={200,800,950,1400,70,95,60,6.25,5};return l;
}
static fwt_config config(fwt_priority priority,bool continuous) {
 fwt_config c={priority,continuous,300,600,1000,1200,68,90,35};return c;
}
static fwt_input input(uint64_t now) {
 fwt_input i;memset(&i,0,sizeof(i));i.valid=true;i.miningActive=true;i.fanOk=true;
 i.capturedAtUs=now;i.sampleId=now;i.generation=1;i.hashRateGHs=1000;
 i.powerW=20;i.asicTempC=50;i.vrmTempC=45;i.current.frequencyMHz=500;
 i.current.coreVoltageMv=1150;i.voltageReadbackMv=1150;return i;
}
static void safety_and_configuration(void) {
 fwt_limits l=limits();fwt_config cfg=config(FWT_BALANCED,false);fwt_controller c;
 fwt_init(&c,&l,&cfg);assert(fwt_config_valid(&cfg,&l));
 fwt_config bad=cfg;bad.maxAsicTempC=71;assert(!fwt_configure(&c,&bad));
 bad=cfg;bad.maxVoltageMv=NAN;assert(!fwt_configure(&c,&bad));
 bad=cfg;bad.minFrequencyMHz=700;assert(!fwt_configure(&c,&bad));
 bad=cfg;bad.priority=(fwt_priority)99;assert(!fwt_configure(&c,&bad));
 uint64_t now=FWT_SECOND;fwt_input i=input(now);
 i.valid=false;assert(!fwt_start(&c,&i,now));i.valid=true;
 i.fanOk=false;assert(!fwt_start(&c,&i,now));i.fanOk=true;
 i.fault=true;assert(!fwt_start(&c,&i,now));i.fault=false;
 i.errorPercentage=NAN;assert(!fwt_start(&c,&i,now));i.errorPercentage=0;
 assert(fwt_start(&c,&i,now));assert(c.owns&&c.active);assert(!fwt_start(&c,&i,now));
 /* A repeated HTTP observation is not another hardware interval. */
 for(unsigned n=0;n<100;++n) assert(fwt_tick(&c,&i,now).kind==FWT_REQUEST_NONE);
 assert(c.samples==0&&c.experiments==0);
 i.capturedAtUs=0;now+=FWT_SECOND;assert(fwt_tick(&c,&i,now).kind==FWT_REQUEST_NONE);
 now+=FWT_INVALID_TIMEOUT_US;assert(fwt_tick(&c,&i,now).kind==FWT_REQUEST_PAUSE);
 assert(c.state==FWT_FAULTED&&!c.owns);assert(fwt_tick(&c,&i,now).kind==FWT_REQUEST_NONE);
 fwt_init(&c,&l,&cfg);i=input(now);assert(fwt_start(&c,&i,now));
 i.generation=2;assert(fwt_tick(&c,&i,now).kind==FWT_REQUEST_NONE);assert(!c.owns);
 fwt_init(&c,&l,&cfg);i=input(now);assert(fwt_start(&c,&i,now));
 assert(fwt_tick(&c,&i,now-1).kind==FWT_REQUEST_PAUSE);
 fwt_init(&c,&l,&cfg);i=input(now);assert(fwt_start(&c,&i,now));
 i.miningActive=false;assert(fwt_tick(&c,&i,now).kind==FWT_REQUEST_NONE);assert(!c.owns);
 fwt_init(&c,&l,&cfg);i=input(now);assert(fwt_start(&c,&i,now));
 i.fault=true;assert(fwt_tick(&c,&i,now).kind==FWT_REQUEST_PAUSE);assert(!c.owns);
}
static void thermal_and_transaction_guards(void) {
 fwt_limits l=limits();fwt_config cfg=config(FWT_PERFORMANCE,true);fwt_controller c;
 uint64_t now=FWT_SECOND;fwt_input i=input(now);fwt_init(&c,&l,&cfg);assert(fwt_start(&c,&i,now));
 i.asicTempC=68;i.errorPercentage=5;i.worstErrorPercentage=5;
 fwt_request r=fwt_tick(&c,&i,now);assert(r.kind==FWT_REQUEST_APPLY);
 assert(r.point.frequencyMHz<i.current.frequencyMHz&&r.point.coreVoltageMv<=i.current.coreVoltageMv);
 /* The request is emitted exactly once and cannot be reissued by polling. */
 assert(fwt_tick(&c,&i,now).kind==FWT_REQUEST_NONE);
 now+=FWT_APPLY_TIMEOUT_US+1;assert(fwt_tick(&c,&i,now).kind==FWT_REQUEST_PAUSE);
 fwt_init(&c,&l,&cfg);i=input(now);assert(fwt_start(&c,&i,now));
 i.vrmTempC=90;r=fwt_tick(&c,&i,now);assert(r.kind==FWT_REQUEST_APPLY);
 fwt_applied(&c,false,now);assert(!c.owns&&c.state==FWT_FAULTED);
 fwt_init(&c,&l,&cfg);i=input(now);assert(fwt_start(&c,&i,now));
 cfg.maxFrequencyMHz=450;cfg.maxVoltageMv=1130;assert(!fwt_configure(&c,&cfg));
 cfg=config(FWT_PERFORMANCE,true);cfg.minFrequencyMHz=550;cfg.minVoltageMv=1160;
 assert(!fwt_configure(&c,&cfg)); /* New minima must never turn backoff upward. */
 cfg=config(FWT_PERFORMANCE,true);cfg.maxAsicTempC=49;assert(fwt_configure(&c,&cfg));
 r=fwt_tick(&c,&i,now);assert(r.kind==FWT_REQUEST_APPLY);
 assert(r.point.frequencyMHz<500&&r.point.coreVoltageMv<=1150);
 assert(!fwt_configure(&c,&cfg)); /* No mid-transition configuration write. */
 i.current=r.point;i.asicTempC=40;fwt_applied(&c,true,now);assert(c.generation==0);
 i.generation=2;i.valid=false;now+=5*FWT_SECOND;
 assert(fwt_tick(&c,&i,now).kind==FWT_REQUEST_NONE);assert(c.owns);
 i.valid=true;i.capturedAtUs=now;i.sampleId=now;
 assert(fwt_tick(&c,&i,now).kind==FWT_REQUEST_NONE);assert(c.generation==2);
 fwt_cancel(&c,"manual-change");r=fwt_tick(&c,&i,now);assert(r.kind==FWT_REQUEST_NONE&&!c.owns);
 /* No progress or inability to back off cannot authorize a voltage increase. */
 cfg=config(FWT_EFFICIENCY,false);i=input(now);i.current.frequencyMHz=300;
 fwt_init(&c,&l,&cfg);assert(fwt_start(&c,&i,now));i.asicTempC=68;
 assert(fwt_tick(&c,&i,now).kind==FWT_REQUEST_PAUSE);
}
static fwt_input plant(uint64_t now,fwt_point point,uint64_t generation,double ambient) {
 fwt_input i=input(now);i.current=point;i.voltageReadbackMv=point.coreVoltageMv;
 i.generation=generation;
 const double required=1000+point.frequencyMHz*0.20;
 const double error=fwt_clamp((required-point.coreVoltageMv)*0.2,0,15);
 i.errorPercentage=i.worstErrorPercentage=error;
 i.hashRateGHs=point.frequencyMHz*2*(1-error/100);
 i.powerW=3+0.025*point.frequencyMHz*pow(point.coreVoltageMv/1000,2);
 i.asicTempC=ambient+i.powerW*1.2;i.vrmTempC=ambient+i.powerW*0.8;
 return i;
}
static void adaptive_plant(fwt_priority priority,bool continuous) {
 fwt_limits l=limits();fwt_config cfg=config(priority,continuous);fwt_controller c;
 uint64_t now=FWT_SECOND,generation=1;fwt_point point={500,1150};
 fwt_input i=plant(now,point,generation,25);fwt_init(&c,&l,&cfg);assert(fwt_start(&c,&i,now));
 unsigned changes=0;bool completed=false;
 for(unsigned iteration=0;iteration<5000;++iteration) {
  now+=5*FWT_SECOND;i=plant(now,point,generation,25);
  fwt_request r=fwt_tick(&c,&i,now);
  assert(r.kind!=FWT_REQUEST_PAUSE);assert(c.owns);
  if(r.kind==FWT_REQUEST_APPLY) {
   assert(fwt_point_allowed(&c,r.point));
   if(r.point.coreVoltageMv>point.coreVoltageMv) assert(fwt_can_raise_voltage(&c,&i));
   point=r.point;++generation;++changes;fwt_applied(&c,true,now);
  }
  if((c.state==FWT_HOLDING||c.state==FWT_OBSERVING)&&changes>0) {completed=true;break;}
 }
 assert(completed&&changes>0&&c.bestAvailable);
 assert(c.experiments<=FWT_MAX_EXPERIMENTS);
 if(priority==FWT_PERFORMANCE) assert(c.best.frequencyMHz>500);
 if(priority==FWT_EFFICIENCY) assert(c.best.coreVoltageMv<1150);
 /* A stable optimum causes no repeated writes, in either mode. */
 const uint32_t decisions=c.decisions;
 for(unsigned n=0;n<10;++n) {
  now+=5*FWT_SECOND;i=plant(now,point,generation,25);
  assert(fwt_tick(&c,&i,now).kind==FWT_REQUEST_NONE);
 }
 assert(c.decisions==decisions);
 if(continuous) {
  /* Cooling/load changes initiate fresh evaluation; they are not a preset. */
  now+=5*FWT_SECOND;i=plant(now,point,generation,29);
  assert(fwt_tick(&c,&i,now).kind==FWT_REQUEST_NONE);
  assert(c.state==FWT_BASELINE&&c.active);
  cfg.continuous=false;assert(fwt_configure(&c,&cfg));
 } else {
  assert(!c.active&&c.owns&&c.state==FWT_HOLDING);
  /* Protection still removes load after a one-run result is held. */
  now+=5*FWT_SECOND;i=plant(now,point,generation,25);i.asicTempC=cfg.maxAsicTempC;
  fwt_request r=fwt_tick(&c,&i,now);
  assert(r.kind==FWT_REQUEST_APPLY&&r.point.frequencyMHz<point.frequencyMHz);
 }
 printf("priority %d continuous %d: %u bounded transitions, best %.2f MHz %.2f mV\n",
  (int)priority,(int)continuous,changes,c.best.frequencyMHz,c.best.coreVoltageMv);
}
static void error_recovery_branches(void) {
 fwt_limits l=limits();fwt_config cfg=config(FWT_BALANCED,false);fwt_controller c;
 for(unsigned headroom=0;headroom<2;++headroom) {
  uint64_t now=FWT_SECOND;fwt_input i=input(now);fwt_init(&c,&l,&cfg);assert(fwt_start(&c,&i,now));
  i.errorPercentage=i.worstErrorPercentage=2;
  if(!headroom) i.powerW=cfg.maxPowerW*0.95;
  fwt_request r=fwt_none();
  for(unsigned n=0;n<15&&r.kind==FWT_REQUEST_NONE;++n) {
   now+=5*FWT_SECOND;i.sampleId=i.capturedAtUs=now;r=fwt_tick(&c,&i,now);
  }
  assert(r.kind==FWT_REQUEST_APPLY);
  if(headroom) assert(r.point.coreVoltageMv>i.current.coreVoltageMv&&r.point.frequencyMHz==i.current.frequencyMHz);
  else assert(r.point.frequencyMHz<i.current.frequencyMHz&&r.point.coreVoltageMv<=i.current.coreVoltageMv);
 }
}
static void window_and_noise_guards(void) {
 fwt_limits l=limits();fwt_config cfg=config(FWT_BALANCED,false);fwt_controller c;
 uint64_t now=FWT_SECOND;fwt_input i=input(now);fwt_init(&c,&l,&cfg);assert(fwt_start(&c,&i,now));
 fwt_request r=fwt_none();
 for(unsigned n=0;n<20&&r.kind==FWT_REQUEST_NONE;++n) {
  now+=5*FWT_SECOND;i.capturedAtUs=i.sampleId=now;r=fwt_tick(&c,&i,now);
 }
 assert(r.kind==FWT_REQUEST_APPLY);i.current=r.point;
 const uint64_t applied=now;fwt_applied(&c,true,now);i.generation=2;
 now=applied+31*FWT_SECOND;i.capturedAtUs=i.sampleId=applied+25*FWT_SECOND;
 assert(fwt_tick(&c,&i,now).kind==FWT_REQUEST_NONE&&c.samples==0);
 now=applied+40*FWT_SECOND;i.capturedAtUs=i.sampleId=now;
 assert(fwt_tick(&c,&i,now).kind==FWT_REQUEST_NONE&&c.samples==0);
 now=applied+50*FWT_SECOND;i.capturedAtUs=i.sampleId=now;
 assert(fwt_tick(&c,&i,now).kind==FWT_REQUEST_NONE&&c.samples==1);
 /* A persistent error that never improves cannot trigger an endless voltage sweep. */
 now+=FWT_SECOND;i=input(now);fwt_init(&c,&l,&cfg);assert(fwt_start(&c,&i,now));
 unsigned recoveries=0;
 for(unsigned n=0;n<2000&&c.owns;++n) {
  now+=5*FWT_SECOND;i.capturedAtUs=i.sampleId=now;
  i.errorPercentage=0.1;i.worstErrorPercentage=2; /* One weak chip, good board average. */
  r=fwt_tick(&c,&i,now);
  if(r.kind==FWT_REQUEST_APPLY) {
   ++recoveries;i.current=r.point;i.voltageReadbackMv=r.point.coreVoltageMv;
   ++i.generation;fwt_applied(&c,true,now);
  }
 }
 assert(!c.owns&&c.state==FWT_FAULTED&&recoveries==6);
 /* Large observed noise must not win over a tiny apparent score improvement. */
 now+=FWT_SECOND;i=input(now);cfg=config(FWT_EFFICIENCY,false);
 fwt_init(&c,&l,&cfg);assert(fwt_start(&c,&i,now));
 for(unsigned n=0;n<3000&&c.state!=FWT_HOLDING;++n) {
  now+=5*FWT_SECOND;i.capturedAtUs=i.sampleId=now;
  i.hashRateGHs=1000*(n%2?0.85:1.15);i.powerW=20*(n%2?1.12:0.88);
  r=fwt_tick(&c,&i,now);assert(r.kind!=FWT_REQUEST_PAUSE);
  if(r.kind==FWT_REQUEST_APPLY) {
   i.current=r.point;i.voltageReadbackMv=r.point.coreVoltageMv;++i.generation;fwt_applied(&c,true,now);
  }
 }
 assert(c.state==FWT_HOLDING&&fwt_point_equal(c.best,c.baseline));
 /* Missing data after a one-run result cannot silently start another search. */
 now+=FWT_SECOND;i.valid=false;assert(fwt_tick(&c,&i,now).kind==FWT_REQUEST_NONE);
 i.valid=true;
 const uint32_t experiments=c.experiments;
 for(unsigned n=0;n<25;++n) {
  now+=5*FWT_SECOND;i.capturedAtUs=i.sampleId=now;i.hashRateGHs=1000;i.powerW=20;
  r=fwt_tick(&c,&i,now);assert(r.kind==FWT_REQUEST_NONE);
 }
 assert(c.state==FWT_HOLDING&&!c.active&&c.experiments==experiments);
}

int main(void) {
 safety_and_configuration();thermal_and_transaction_guards();error_recovery_branches();window_and_noise_guards();
 for(int priority=0;priority<3;++priority) for(unsigned continuous=0;continuous<2;++continuous)
  adaptive_plant((fwt_priority)priority,continuous!=0);
 puts("Actual shared controller: configuration, telemetry, ownership, clock regression, hot ASIC/VRM, transition failure/timeout, dynamic priorities, error recovery and continual/one-run trajectories passed");
 return 0;
}
