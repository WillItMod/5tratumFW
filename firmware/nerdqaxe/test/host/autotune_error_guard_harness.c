#include "autotune/core.h"
#include <assert.h>
#include <stdio.h>

/* Drive the actual shared controller with independent hardware intervals.
 * No adapter, NVS, hardware or HTTP service participates in these tests. */
typedef struct {
 fwt_controller core;
 fwt_input input;
 uint64_t now;
} fixture;

static fwt_limits hardware(void) {
 const fwt_limits limits={100,2000,950,1400,70,95,100,6.25,5};
 return limits;
}
static fwt_config options(void) {
 const fwt_config config={FWT_EFFICIENCY,false,300,600,1000,1200,68,90,35};
 return config;
}
static fixture begin(fwt_config config) {
 fixture f;memset(&f,0,sizeof(f));
 const fwt_limits limits=hardware();
 f.now=FWT_SECOND;
 f.input.valid=true;f.input.miningActive=true;f.input.fanOk=true;
 f.input.capturedAtUs=f.input.sampleId=f.now;f.input.generation=1;
 f.input.hashRateGHs=1000;f.input.powerW=20;
 f.input.asicTempC=50;f.input.vrmTempC=45;
 f.input.current.frequencyMHz=500;f.input.current.coreVoltageMv=1150;
 f.input.voltageReadbackMv=1150;
 fwt_init(&f.core,&limits,&config);
 assert(fwt_start(&f.core,&f.input,f.now));
 return f;
}
static fwt_request reading(fixture *f,unsigned seconds,double board,double worst) {
 f->now+=(uint64_t)seconds*FWT_SECOND;
 f->input.capturedAtUs=f->input.sampleId=f->now;
 f->input.errorPercentage=board;f->input.worstErrorPercentage=worst;
 return fwt_tick(&f->core,&f->input,f->now);
}
static void apply(fixture *f,fwt_request request) {
 assert(request.kind==FWT_REQUEST_APPLY);
 assert(fwt_config_contains(&f->core.config,request.point));
 f->input.current=request.point;f->input.voltageReadbackMv=request.point.coreVoltageMv;
 ++f->input.generation;
 fwt_applied(&f->core,true,f->now);
 assert(f->core.owns&&!f->core.awaitingApply);
}
static fwt_request measured_first_trial(fixture *f) {
 const uint64_t started=f->now;
 for(unsigned n=0;n<30;++n) {
  const fwt_request request=reading(f,5,0,0);
  assert(request.kind!=FWT_REQUEST_PAUSE);
  if(request.kind==FWT_REQUEST_APPLY) {
   assert(f->now-started>=75*FWT_SECOND);
   return request;
  }
 }
 assert(!"Clean starting point did not complete its measurement window");
 return fwt_none();
}
static void correction(fwt_request request,fwt_point before) {
 assert(request.kind==FWT_REQUEST_APPLY);
 assert(!fwt_point_equal(request.point,before));
 assert(request.point.frequencyMHz<=before.frequencyMHz);
 assert(request.point.coreVoltageMv<=before.coreVoltageMv+5.001);
}

static void severe_boundaries_and_counter_flush(void) {
 const double board[]={3.0,0.1};
 const double worst[]={3.0,5.0};
 for(unsigned scenario=0;scenario<2;++scenario) {
  fixture f=begin(options());
  assert(reading(&f,14,board[scenario],worst[scenario]).kind==FWT_REQUEST_NONE);
  const fwt_request request=reading(&f,1,board[scenario],worst[scenario]);
  correction(request,f.input.current);
  assert(f.core.lastDecision.kind==FWT_EVENT_REJECTED);
  assert(f.core.lastDecision.outcome==FWT_OUTCOME_FAILED);
  assert(!strcmp(f.core.lastDecision.reason,"early-severe-error-limit"));
  assert(f.core.lastDecision.hasConditions&&!f.core.lastDecision.hasMeasurement&&!f.core.lastDecision.hasScore);
  assert(f.core.lastDecision.asicTempC==f.input.asicTempC);
  assert(f.core.lastDecision.errorPercentage==board[scenario]);
  assert(f.core.lastDecision.worstErrorPercentage==worst[scenario]);
 }
 /* A value immediately below either severe boundary is not an instant stop. */
 const double almostBoard[]={2.999,0.1};
 const double almostWorst[]={2.999,4.999};
 for(unsigned scenario=0;scenario<2;++scenario) {
  fixture f=begin(options());
  assert(reading(&f,15,almostBoard[scenario],almostWorst[scenario]).kind==FWT_REQUEST_NONE);
  assert(reading(&f,5,almostBoard[scenario],almostWorst[scenario]).kind==FWT_REQUEST_NONE);
  correction(reading(&f,5,almostBoard[scenario],almostWorst[scenario]),f.input.current);
 }
 fixture f=begin(options());
 apply(&f,measured_first_trial(&f));
 const uint64_t changed=f.now;
 assert(reading(&f,14,3,3).kind==FWT_REQUEST_NONE);
 /* Even a TTL-fresh reply can contain work from before the new point. */
 f.now=changed+15*FWT_SECOND;
 f.input.capturedAtUs=f.input.sampleId=changed;
 assert(fwt_tick(&f.core,&f.input,f.now).kind==FWT_REQUEST_NONE);
 f.input.capturedAtUs=f.input.sampleId=changed+14*FWT_SECOND;
 assert(fwt_tick(&f.core,&f.input,f.now).kind==FWT_REQUEST_NONE);
 f.input.capturedAtUs=f.input.sampleId=f.now;
 correction(fwt_tick(&f.core,&f.input,f.now),f.input.current);
 assert(f.now-changed==15*FWT_SECOND); /* No 105-second severe-error wait. */
}

static void ordinary_streak_and_unique_intervals(void) {
 const double board[]={0.5001,0.1};
 const double worst[]={0.5001,1.0001};
 for(unsigned scenario=0;scenario<2;++scenario) {
  fixture f=begin(options());
  assert(reading(&f,15,board[scenario],worst[scenario]).kind==FWT_REQUEST_NONE);
  assert(reading(&f,4,board[scenario],worst[scenario]).kind==FWT_REQUEST_NONE);
  assert(reading(&f,4,board[scenario],worst[scenario]).kind==FWT_REQUEST_NONE);
  /* Three intervals spanning eight seconds are insufficient. */
  const fwt_request request=reading(&f,2,board[scenario],worst[scenario]);
  correction(request,f.input.current);
  assert(!strcmp(f.core.lastDecision.reason,"early-error-limit"));
  assert(f.core.lastDecision.hasConditions&&!f.core.lastDecision.hasMeasurement);
 }
 fixture f=begin(options());
 assert(reading(&f,15,0.6,0.6).kind==FWT_REQUEST_NONE);
 for(unsigned n=0;n<100;++n) assert(fwt_tick(&f.core,&f.input,f.now).kind==FWT_REQUEST_NONE);
 f.now+=5*FWT_SECOND; /* Same hardware sample, later browser/control polls. */
 assert(fwt_tick(&f.core,&f.input,f.now).kind==FWT_REQUEST_NONE);
 f.input.capturedAtUs=f.input.sampleId=f.now;
 assert(fwt_tick(&f.core,&f.input,f.now).kind==FWT_REQUEST_NONE);
 f.now+=5*FWT_SECOND;
 for(unsigned n=0;n<100;++n) assert(fwt_tick(&f.core,&f.input,f.now).kind==FWT_REQUEST_NONE);
 f.input.capturedAtUs=f.input.sampleId=f.now;
 correction(fwt_tick(&f.core,&f.input,f.now),f.input.current);

 /* The sample ID and interval timestamp must both advance independently. */
 f=begin(options());
 assert(reading(&f,15,0.6,0.6).kind==FWT_REQUEST_NONE);
 const uint64_t firstId=f.input.sampleId;
 f.now+=5*FWT_SECOND;f.input.capturedAtUs=f.now;f.input.sampleId=firstId;
 assert(fwt_tick(&f.core,&f.input,f.now).kind==FWT_REQUEST_NONE);
 f.now+=5*FWT_SECOND;f.input.capturedAtUs=f.input.sampleId=f.now;
 assert(fwt_tick(&f.core,&f.input,f.now).kind==FWT_REQUEST_NONE);
 f.now+=5*FWT_SECOND;f.input.sampleId=f.now; /* Timestamp still from second interval. */
 assert(fwt_tick(&f.core,&f.input,f.now).kind==FWT_REQUEST_NONE);
 f.now+=5*FWT_SECOND;f.input.capturedAtUs=f.input.sampleId=f.now;
 correction(fwt_tick(&f.core,&f.input,f.now),f.input.current);

 /* Exactly15 seconds is a permitted interval gap, rather than a reset. */
 f=begin(options());
 assert(reading(&f,15,0.6,0.6).kind==FWT_REQUEST_NONE);
 assert(reading(&f,15,0.6,0.6).kind==FWT_REQUEST_NONE);
 correction(reading(&f,5,0.6,0.6),f.input.current);

 /* The ordinary limits themselves are accepted: they are strict > tests. */
 f=begin(options());
 for(unsigned n=0;n<12;++n) assert(reading(&f,5,0.5,1.0).kind==FWT_REQUEST_NONE);
 assert(f.core.rejected==0);
}

static void clean_invalid_stale_and_gap_reset(void) {
 for(unsigned interrupted=0;interrupted<5;++interrupted) {
  fixture f=begin(options());
  assert(reading(&f,15,0.6,0.6).kind==FWT_REQUEST_NONE);
  assert(reading(&f,5,0.6,0.6).kind==FWT_REQUEST_NONE);
  if(interrupted==0) {
   assert(reading(&f,5,0,0).kind==FWT_REQUEST_NONE);
  } else if(interrupted==4) {
   /* Distinct fresh data after a >15-second gap begins a new streak. */
   assert(reading(&f,16,0.6,0.6).kind==FWT_REQUEST_NONE);
   assert(reading(&f,5,0.6,0.6).kind==FWT_REQUEST_NONE);
   correction(reading(&f,5,0.6,0.6),f.input.current);
   continue;
  } else {
   f.now+=FWT_SECOND;
   if(interrupted==1) f.input.valid=false;
   if(interrupted==2) f.input.capturedAtUs=f.now-16*FWT_SECOND;
   if(interrupted==3) f.input.errorPercentage=NAN;
   assert(fwt_tick(&f.core,&f.input,f.now).kind==FWT_REQUEST_NONE);
   f.input.valid=true;
  }
  assert(reading(&f,1,0.6,0.6).kind==FWT_REQUEST_NONE);
  assert(reading(&f,5,0.6,0.6).kind==FWT_REQUEST_NONE);
  correction(reading(&f,5,0.6,0.6),f.input.current);
 }
}

static void stable_rollback_and_safe_fallback(void) {
 fixture f=begin(options());
 const fwt_point stable=f.input.current;
 const fwt_request trial=measured_first_trial(&f);
 assert(trial.point.frequencyMHz==stable.frequencyMHz&&trial.point.coreVoltageMv==stable.coreVoltageMv-5);
 apply(&f,trial);
 fwt_request request=reading(&f,15,3,3);
 assert(request.kind==FWT_REQUEST_APPLY&&fwt_point_equal(request.point,stable));
 assert(!strcmp(f.core.reason,"restoring-last-stable-point"));

 f=begin(options());apply(&f,measured_first_trial(&f));
 f.input.asicTempC=66; /* Below the thermal backoff, too close for upward rollback. */
 request=reading(&f,15,3,3);
 assert(request.kind==FWT_REQUEST_APPLY);
 assert(request.point.frequencyMHz<f.input.current.frequencyMHz);
 assert(request.point.coreVoltageMv<=f.input.current.coreVoltageMv);

 /* With no measured stable point, voltage/power/temperature ceilings bind. */
 for(unsigned boundary=0;boundary<4;++boundary) {
  fwt_config config=options();
  if(boundary==0) config.maxVoltageMv=1150;
  f=begin(config);
  if(boundary==1) f.input.asicTempC=config.maxAsicTempC-5;
  if(boundary==2) f.input.vrmTempC=config.maxVrmTempC-5;
  if(boundary==3) f.input.powerW=config.maxPowerW*0.9;
  request=reading(&f,15,3,3);
  assert(request.kind==FWT_REQUEST_APPLY);
  assert(request.point.frequencyMHz<500&&request.point.coreVoltageMv<=1150);
 }
 f=begin(options());
 request=reading(&f,15,3,3);
 assert(request.kind==FWT_REQUEST_APPLY);
 assert(request.point.frequencyMHz==500&&request.point.coreVoltageMv==1155);

 /* An original point differing on both axes must not bypass the existing
  * one-axis Gamma transition contract after a protective clock change. */
 f=begin(options());apply(&f,measured_first_trial(&f));
 f.input.asicTempC=f.core.config.maxAsicTempC;
 request=reading(&f,1,0,0);apply(&f,request);
 f.input.asicTempC=50;
 request=reading(&f,15,3,3);
 assert(request.kind==FWT_REQUEST_APPLY);
 assert(request.point.frequencyMHz==f.input.current.frequencyMHz || request.point.coreVoltageMv==f.input.current.coreVoltageMv);
}

static void protection_precedes_error_recovery(void) {
 fixture f=begin(options());f.input.fault=true;
 fwt_request request=reading(&f,15,3,5);
 assert(request.kind==FWT_REQUEST_PAUSE&&!f.core.owns);
 assert(!strcmp(f.core.reason,"hardware-protection-fault"));
 for(unsigned condition=0;condition<2;++condition) {
  f=begin(options());
  if(condition==0) f.input.asicTempC=f.core.config.maxAsicTempC;
  else f.input.powerW=f.core.config.maxPowerW+1;
  request=reading(&f,1,3,5); /* Thermal/electrical protection need not wait15s. */
  assert(request.kind==FWT_REQUEST_APPLY);
  assert(request.point.frequencyMHz<500&&request.point.coreVoltageMv<=1150);
  assert(!strcmp(f.core.reason,condition==0?"temperature-limit-backoff":"power-limit-backoff"));
 }
 f=begin(options());++f.input.generation;
 assert(reading(&f,15,3,5).kind==FWT_REQUEST_NONE&&!f.core.owns);
 f=begin(options());f.input.valid=false;
 assert(reading(&f,15,3,5).kind==FWT_REQUEST_NONE);
 assert(reading(&f,10,3,5).kind==FWT_REQUEST_PAUSE);
}

static void fast_corrections_remain_bounded(void) {
 fixture f=begin(options());
 for(unsigned n=0;n<6;++n) {
  const fwt_request request=reading(&f,15,3,3);
  correction(request,f.input.current);apply(&f,request);
 }
 const fwt_request request=reading(&f,15,3,3);
 assert(request.kind==FWT_REQUEST_PAUSE&&!f.core.owns);
 assert(!strcmp(f.core.reason,"error-recovery-exhausted"));
 assert(f.core.rejected==7);
}

static void full_performance_window_is_unchanged(void) {
 fixture f=begin(options());
 apply(&f,measured_first_trial(&f));
 const uint64_t changed=f.now;
 for(unsigned n=0;n<20;++n) assert(reading(&f,5,0,0).kind==FWT_REQUEST_NONE);
 assert(f.now-changed==100*FWT_SECOND);
 const fwt_request request=reading(&f,5,0,0);
 assert(f.now-changed==105*FWT_SECOND);
 assert(request.kind==FWT_REQUEST_APPLY); /* Full clean trial rejected on its score. */
 assert(f.core.lastDecision.hasMeasurement&&f.core.lastDecision.hasScore);
}

static void failed_point_exclusion_survives_rounds_and_edits(void) {
 fwt_config config=options();
 config.minFrequencyMHz=config.maxFrequencyMHz=500;
 config.minVoltageMv=1145;config.maxVoltageMv=1150;
 fixture f=begin(config);
 const fwt_point original=f.input.current;
 const fwt_request trial=measured_first_trial(&f);
 const fwt_point failed=trial.point;apply(&f,trial);
 fwt_request request=reading(&f,15,3,3);
 assert(request.kind==FWT_REQUEST_APPLY&&fwt_point_equal(request.point,original));apply(&f,request);
 for(unsigned n=0;n<30;++n) {
  request=reading(&f,5,0,0);
  assert(request.kind==FWT_REQUEST_NONE);
 }
 assert(f.core.state==FWT_HOLDING&&f.core.owns);
 config.priority=FWT_PERFORMANCE;config.continuous=true;
 assert(fwt_configure(&f.core,&config));
 /* More than two continuous reassessment periods, with no remaining safe
  * neighbor: the failed pair stays excluded despite option/round changes. */
 for(unsigned n=0;n<180;++n) {
  request=reading(&f,5,0,0);
  assert(request.kind!=FWT_REQUEST_PAUSE);
  assert(request.kind!=FWT_REQUEST_APPLY||!fwt_point_equal(request.point,failed));
  if(request.kind==FWT_REQUEST_APPLY) apply(&f,request);
 }
 fwt_cancel(&f.core,"explicit-new-run");
 assert(fwt_start(&f.core,&f.input,f.now));
 request=measured_first_trial(&f);
 assert(fwt_point_equal(request.point,failed)); /* Exclusion is run-scoped. */
}

static void corrections_do_not_return_to_failed_points(void) {
 fwt_config config=options();config.priority=FWT_PERFORMANCE;
 config.maxFrequencyMHz=500;config.minVoltageMv=1150;config.maxVoltageMv=1155;
 fixture f=begin(config);
 const fwt_point stable=f.input.current;
 fwt_request request=measured_first_trial(&f);
 assert(request.point.coreVoltageMv==1155);const fwt_point failed=request.point;apply(&f,request);
 request=reading(&f,15,3,3);
 assert(request.kind==FWT_REQUEST_APPLY&&fwt_point_equal(request.point,stable));apply(&f,request);
 /* Conditions have changed: the original point now errors too. The normal
  * +5mV correction is a recorded failure, so lower clock instead. */
 request=reading(&f,15,3,3);
 assert(request.kind==FWT_REQUEST_APPLY&&!fwt_point_equal(request.point,failed));
 assert(request.point.frequencyMHz<stable.frequencyMHz&&request.point.coreVoltageMv<=stable.coreVoltageMv);
}

static bool contains(const fwt_point *points,unsigned count,fwt_point point) {
 for(unsigned n=0;n<count;++n) if(fwt_point_equal(points[n],point)) return true;
 return false;
}
static void bounded_failed_point_memory_exhausts_safely(void) {
 fwt_config config=options();config.priority=FWT_PERFORMANCE;config.continuous=true;
 config.minFrequencyMHz=300;config.maxFrequencyMHz=1000;
 config.minVoltageMv=1145;config.maxVoltageMv=1150;
 fixture f=begin(config);
 fwt_point failed[32];unsigned count=0;
 bool exhausted=false;
 for(unsigned n=0;n<10000&&f.core.owns;++n) {
  const bool unstable=f.input.current.coreVoltageMv<1150;
  f.input.hashRateGHs=f.input.current.frequencyMHz*2;
  const fwt_request request=reading(&f,5,unstable?3:0,unstable?3:0);
  if(unstable&&request.kind!=FWT_REQUEST_NONE&&!contains(failed,count,f.input.current)) {
   assert(count<32);failed[count++]=f.input.current;
  }
  if(request.kind==FWT_REQUEST_APPLY) {
   assert(!contains(failed,count,request.point));apply(&f,request);
  } else if(request.kind==FWT_REQUEST_PAUSE) {
   assert(!strcmp(f.core.reason,"error-recovery-exhausted"));
   assert(f.core.lastDecision.outcome==FWT_OUTCOME_FAILED);
   assert(!strcmp(f.core.lastDecision.reason,"error-point-exclusion-full"));
   exhausted=true;
  }
 }
 assert(exhausted&&!f.core.owns);
 assert(count==17); /* Sixteen stored exact pairs, then a fail-closed seventeenth. */
}

int main(void) {
 severe_boundaries_and_counter_flush();
 ordinary_streak_and_unique_intervals();
 clean_invalid_stale_and_gap_reset();
 stable_rollback_and_safe_fallback();
 protection_precedes_error_recovery();
 fast_corrections_remain_bounded();
 full_performance_window_is_unchanged();
 failed_point_exclusion_survives_rounds_and_edits();
 corrections_do_not_return_to_failed_points();
 bounded_failed_point_memory_exhausts_safely();
 puts("Actual shared controller: early ASIC-error boundaries/streaks, counter flush, stable rollback, protection priority, full windows and bounded run exclusions passed");
 return 0;
}
