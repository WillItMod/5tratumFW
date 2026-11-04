/* Original 5tratumFW adaptive controller. GPL-3.0-or-later.
 * C/C++ compatible, platform-free, bounded state; no persistence or I/O. */
#ifndef FIVETRATUM_AUTOTUNE_CORE_H
#define FIVETRATUM_AUTOTUNE_CORE_H
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Config is parsed from storage/user data: an integer can be range-checked
 * without C++'s undefined behavior on a corrupted out-of-enum value. */
typedef int fwt_priority;
enum { FWT_EFFICIENCY=0, FWT_BALANCED=1, FWT_PERFORMANCE=2 };
typedef enum { FWT_DISABLED=0, FWT_BASELINE, FWT_SETTLING, FWT_MEASURING,
 FWT_HOLDING, FWT_OBSERVING, FWT_SUSPENDED, FWT_FAULTED, FWT_APPLYING } fwt_state;
typedef struct { double frequencyMHz, coreVoltageMv; } fwt_point;
typedef struct {
 fwt_priority priority; bool continuous;
 double minFrequencyMHz, maxFrequencyMHz, minVoltageMv, maxVoltageMv;
 double maxAsicTempC, maxVrmTempC, maxPowerW;
} fwt_config;
typedef struct {
 double minFrequencyMHz, maxFrequencyMHz, minVoltageMv, maxVoltageMv;
 double maxAsicTempC, maxVrmTempC, maxPowerW;
 double frequencyStepMHz, voltageStepMv;
} fwt_limits;
typedef struct {
 bool valid, miningActive, fault, fanOk;
 uint64_t capturedAtUs, sampleId, generation;
 double hashRateGHs, errorPercentage, worstErrorPercentage, powerW;
 double asicTempC, vrmTempC;
 fwt_point current;
 double voltageReadbackMv;
} fwt_input;
typedef enum { FWT_REQUEST_NONE=0, FWT_REQUEST_APPLY, FWT_REQUEST_PAUSE } fwt_request_kind;
typedef struct { fwt_request_kind kind; fwt_point point; } fwt_request;
/* Observational records only: never consulted by the control policy. */
typedef enum { FWT_EVENT_NONE=0, FWT_EVENT_STARTED, FWT_EVENT_BASELINE,
 FWT_EVENT_ACCEPTED, FWT_EVENT_REJECTED, FWT_EVENT_RETAINED, FWT_EVENT_ERRORS,
 FWT_EVENT_ADJUSTMENT } fwt_event_kind;
typedef enum { FWT_OUTCOME_REQUESTED=0, FWT_OUTCOME_APPLIED,
 FWT_OUTCOME_FAILED, FWT_OUTCOME_MEASURED } fwt_event_outcome;
typedef struct {
 uint32_t id; fwt_event_kind kind; fwt_event_outcome outcome;
 uint64_t atUs; const char *reason;
 fwt_point from, point;
 double hashRateGHs, powerW, errorPercentage, worstErrorPercentage, score, requiredScore;
 double asicTempC, vrmTempC;
 bool hasMeasurement, hasScore, hasConditions;
} fwt_event;
#define FWT_ERROR_POINT_CAPACITY 16u
typedef struct {
 fwt_limits limits; fwt_config config;
 fwt_state state; const char *reason;
 bool active, owns, bestAvailable, baselineAvailable, awaitingApply;
 uint32_t runId, experiments, decisions, rejected;
 fwt_point baseline, current, best, pending, trialOrigin;
 uint64_t stateSinceUs, lastNowUs, lastSampleId, generation, invalidSinceUs;
 uint64_t observationStartedUs, lastSearchUs;
 unsigned samples, searchIndex, noImprovement, readbackSamples, roundExperiments, recoveryAttempts;
 double sumHash, sumPower, sumHashSquared, sumPowerSquared, sumError, worstError, maxAsic, maxVrm;
 double baselineHash, baselinePower, bestHash, bestPower, bestScore, bestUncertainty;
 double previousAsicTemp, previousVrmTemp;
 bool trial, recovering, rebaseline, holdAfterRecovery;
 uint32_t eventSequence;
 fwt_event lastDecision, lastAdjustment;
 /* Run-local safety evidence is separate from the current round's score. */
 bool stableAvailable;
 fwt_point stable, failedPoints[FWT_ERROR_POINT_CAPACITY];
 double stablePower;
 unsigned failedCount, errorSamples;
 uint64_t errorEpochUs, errorSampleId, errorCapturedAtUs, errorStartedUs;
} fwt_controller;

static inline bool fwt_config_valid(const fwt_config *cfg,const fwt_limits *limits);
static inline void fwt_init(fwt_controller *c,const fwt_limits *limits,const fwt_config *cfg);
static inline bool fwt_configure(fwt_controller *c,const fwt_config *cfg);
static inline bool fwt_start(fwt_controller *c,const fwt_input *input,uint64_t nowUs);
static inline fwt_request fwt_tick(fwt_controller *c,const fwt_input *input,uint64_t nowUs);
static inline void fwt_applied(fwt_controller *c,bool success,uint64_t nowUs);
static inline void fwt_cancel(fwt_controller *c,const char *reason);
static inline const char *fwt_state_name(fwt_state state);
static inline const char *fwt_priority_name(fwt_priority priority);

/* Engineering starting policy, requiring separate thermal bench qualification.
 * Times are monotonic, samples are unique hardware intervals, never HTTP polls. */
#define FWT_SECOND UINT64_C(1000000)
#define FWT_FRESH_US (15 * FWT_SECOND)
#define FWT_SETTLE_US (30 * FWT_SECOND)
#define FWT_WINDOW_US (60 * FWT_SECOND)
#define FWT_REASSESS_US (300 * FWT_SECOND)
#define FWT_APPLY_TIMEOUT_US (30 * FWT_SECOND)
#define FWT_INVALID_TIMEOUT_US (10 * FWT_SECOND)
#define FWT_MIN_SAMPLES 8u
#define FWT_MAX_EXPERIMENTS 24u
#define FWT_BOARD_ERROR_LIMIT 0.5
#define FWT_CHIP_ERROR_LIMIT 1.0
/* Experimental rejection policy, not manufacturer-certified safety limits. */
#define FWT_BOARD_SEVERE_ERROR_LIMIT 3.0
#define FWT_CHIP_SEVERE_ERROR_LIMIT 5.0
#define FWT_ERROR_MIN_SAMPLES 3u
#define FWT_ERROR_CONFIRM_US (10 * FWT_SECOND)

static inline double fwt_clamp(double n,double lo,double hi) {
 return n < lo ? lo : (n > hi ? hi : n);
}
static inline bool fwt_positive(double n) { return isfinite(n) && n > 0; }
static inline bool fwt_point_equal(fwt_point a,fwt_point b) {
 return fabs(a.frequencyMHz-b.frequencyMHz)<0.01 && fabs(a.coreVoltageMv-b.coreVoltageMv)<0.01;
}
static inline bool fwt_point_failed(const fwt_controller *c,fwt_point p) {
 for(unsigned n=0;n<c->failedCount;++n) if(fwt_point_equal(c->failedPoints[n],p)) return true;
 return false;
}
static inline void fwt_error_streak_reset(fwt_controller *c) {
 c->errorSamples=0;c->errorStartedUs=0;
}
static inline void fwt_event_record(fwt_controller *c,fwt_event *event,fwt_event_kind kind,
 fwt_event_outcome outcome,fwt_point from,fwt_point point,const char *reason,uint64_t now) {
 memset(event,0,sizeof(*event));
 if(c->eventSequence==UINT32_MAX) return; /* Metadata exhaustion cannot change control. */
 event->id=++c->eventSequence;event->kind=kind;event->outcome=outcome;
 event->atUs=now;event->reason=reason;event->from=from;event->point=point;
}
static inline void fwt_event_measure(fwt_controller *c,fwt_event_kind kind,const char *reason,
 uint64_t now,double hash,double power,bool scored,double score,double required) {
 const fwt_point none={0,0};
 fwt_event_record(c,&c->lastDecision,kind,FWT_OUTCOME_MEASURED,
  c->bestAvailable?c->best:none,c->current,reason,now);
 fwt_event *event=&c->lastDecision;
 if(!event->id) return;
 event->hasMeasurement=fwt_positive(hash)&&fwt_positive(power)&&c->samples>0;
 if(event->hasMeasurement) {
  event->hashRateGHs=hash;event->powerW=power;
  event->errorPercentage=c->sumError/c->samples;event->worstErrorPercentage=c->worstError;
 }
 event->hasScore=scored&&fwt_positive(score)&&fwt_positive(required);
 if(event->hasScore) {event->score=score;event->requiredScore=required;}
}
static inline bool fwt_config_contains(const fwt_config *cfg,fwt_point p) {
 return fwt_positive(p.frequencyMHz)&&fwt_positive(p.coreVoltageMv)&&
  p.frequencyMHz>=cfg->minFrequencyMHz&&p.frequencyMHz<=cfg->maxFrequencyMHz&&
  p.coreVoltageMv>=cfg->minVoltageMv&&p.coreVoltageMv<=cfg->maxVoltageMv;
}
static inline const char *fwt_state_name(fwt_state s) {
 switch(s) {
 case FWT_BASELINE:return "baseline"; case FWT_SETTLING:return "settling";
 case FWT_MEASURING:return "measuring"; case FWT_HOLDING:return "holding";
 case FWT_OBSERVING:return "observing"; case FWT_SUSPENDED:return "suspended";
 case FWT_FAULTED:return "faulted"; case FWT_APPLYING:return "applying";
 default:return "disabled";
 }
}
static inline const char *fwt_priority_name(fwt_priority p) {
 return p==FWT_PERFORMANCE?"performance":p==FWT_BALANCED?"balanced":"efficiency";
}
static inline bool fwt_config_valid(const fwt_config *c,const fwt_limits *l) {
 if(!c||!l||c->priority<FWT_EFFICIENCY||c->priority>FWT_PERFORMANCE) return false;
 if(!fwt_positive(l->minFrequencyMHz)||!fwt_positive(l->maxFrequencyMHz)||
    !fwt_positive(l->minVoltageMv)||!fwt_positive(l->maxVoltageMv)||
    !fwt_positive(l->maxAsicTempC)||!fwt_positive(l->maxVrmTempC)||
    !fwt_positive(l->maxPowerW)||!fwt_positive(l->frequencyStepMHz)||
    !fwt_positive(l->voltageStepMv)) return false;
 return fwt_positive(c->minFrequencyMHz)&&fwt_positive(c->maxFrequencyMHz)&&
  fwt_positive(c->minVoltageMv)&&fwt_positive(c->maxVoltageMv)&&
  fwt_positive(c->maxAsicTempC)&&fwt_positive(c->maxVrmTempC)&&fwt_positive(c->maxPowerW)&&
  c->minFrequencyMHz>=l->minFrequencyMHz&&c->maxFrequencyMHz<=l->maxFrequencyMHz&&
  c->minFrequencyMHz<=c->maxFrequencyMHz&&c->minVoltageMv>=l->minVoltageMv&&
  c->maxVoltageMv<=l->maxVoltageMv&&c->minVoltageMv<=c->maxVoltageMv&&
  c->maxAsicTempC<=l->maxAsicTempC&&c->maxVrmTempC<=l->maxVrmTempC&&
  c->maxPowerW<=l->maxPowerW;
}
static inline void fwt_clear_window(fwt_controller *c,uint64_t now) {
 c->samples=0;c->sumHash=0;c->sumPower=0;c->sumError=0;c->worstError=0;
 c->sumHashSquared=0;c->sumPowerSquared=0;
 c->maxAsic=0;c->maxVrm=0;c->observationStartedUs=now;
}
static inline void fwt_init(fwt_controller *c,const fwt_limits *l,const fwt_config *cfg) {
 memset(c,0,sizeof(*c));c->limits=*l;c->config=*cfg;
 c->state=FWT_DISABLED;c->reason="disabled";
}
static inline void fwt_cancel(fwt_controller *c,const char *reason) {
 if(c->awaitingApply&&c->lastAdjustment.id) {
  c->lastAdjustment.outcome=FWT_OUTCOME_FAILED;c->lastAdjustment.reason=reason?reason:"disabled";
 }
 c->active=false;c->owns=false;c->awaitingApply=false;c->trial=false;
 c->state=FWT_DISABLED;c->reason=reason?reason:"disabled";
}
static inline bool fwt_configure(fwt_controller *c,const fwt_config *cfg) {
 if(c->awaitingApply||!fwt_config_valid(cfg,&c->limits)||
    (c->owns&&!fwt_config_contains(cfg,c->current))) return false;
 const fwt_config old=c->config;
 const bool changed=old.priority!=cfg->priority||old.minFrequencyMHz!=cfg->minFrequencyMHz||
  old.maxFrequencyMHz!=cfg->maxFrequencyMHz||old.minVoltageMv!=cfg->minVoltageMv||
  old.maxVoltageMv!=cfg->maxVoltageMv||old.maxAsicTempC!=cfg->maxAsicTempC||
  old.maxVrmTempC!=cfg->maxVrmTempC||old.maxPowerW!=cfg->maxPowerW;
 c->config=*cfg;
 if(c->owns && changed) {c->rebaseline=true;c->active=true;c->bestAvailable=false;
  c->roundExperiments=0;c->searchIndex=0;c->noImprovement=0;fwt_error_streak_reset(c);}
 if(c->owns && !changed && old.continuous!=cfg->continuous) {
  if(!cfg->continuous && c->state==FWT_OBSERVING) {
   c->state=FWT_HOLDING;c->active=false;c->reason="holding-best-result";
  } else if(cfg->continuous && c->state==FWT_HOLDING) {
   c->state=FWT_OBSERVING;c->active=true;c->reason="observing-conditions";
   c->lastSearchUs=c->lastNowUs;
  }
 }
 return true;
}
static inline bool fwt_input_valid(const fwt_input *i,uint64_t now) {
 return i&&i->valid&&i->fanOk&&i->capturedAtUs>0&&now>=i->capturedAtUs&&
  now-i->capturedAtUs<=FWT_FRESH_US&&i->sampleId>0&&i->generation>0&&
  isfinite(i->hashRateGHs)&&i->hashRateGHs>=0&&
  isfinite(i->errorPercentage)&&i->errorPercentage>=0&&i->errorPercentage<=100&&
  isfinite(i->worstErrorPercentage)&&i->worstErrorPercentage>=i->errorPercentage-0.001&&
  i->worstErrorPercentage<=100&&fwt_positive(i->powerW)&&
  fwt_positive(i->asicTempC)&&fwt_positive(i->vrmTempC)&&
  fwt_positive(i->current.frequencyMHz)&&fwt_positive(i->current.coreVoltageMv)&&
  fwt_positive(i->voltageReadbackMv)&&fabs(i->voltageReadbackMv-i->current.coreVoltageMv)<=30;
}
static inline bool fwt_start(fwt_controller *c,const fwt_input *i,uint64_t now) {
 if(c->owns||c->runId==UINT32_MAX||!fwt_config_valid(&c->config,&c->limits)||
    !fwt_input_valid(i,now)||!i->miningActive||i->fault||!fwt_config_contains(&c->config,i->current)) return false;
 if(i->current.frequencyMHz<c->limits.minFrequencyMHz||i->current.frequencyMHz>c->limits.maxFrequencyMHz||
    i->current.coreVoltageMv<c->limits.minVoltageMv||i->current.coreVoltageMv>c->limits.maxVoltageMv) return false;
 const uint32_t id=c->runId+1;const fwt_limits l=c->limits;const fwt_config cfg=c->config;
 fwt_init(c,&l,&cfg);c->runId=id;c->active=true;c->owns=true;
 c->baseline=c->current=i->current;c->baselineAvailable=true;
 c->generation=i->generation;c->lastNowUs=now;c->stateSinceUs=now;
 c->errorEpochUs=now;
 c->state=FWT_BASELINE;c->reason="measuring-starting-point";
 const fwt_point none={0,0};
 fwt_event_record(c,&c->lastDecision,FWT_EVENT_STARTED,FWT_OUTCOME_MEASURED,none,c->current,c->reason,now);
 fwt_clear_window(c,now);return true;
}
static inline fwt_request fwt_none(void) {
 fwt_request r;memset(&r,0,sizeof(r));return r;
}
static inline fwt_request fwt_pause(fwt_controller *c,const char *reason) {
 fwt_request r=fwt_none();r.kind=FWT_REQUEST_PAUSE;
 if(c->awaitingApply&&c->lastAdjustment.id) {
  c->lastAdjustment.outcome=FWT_OUTCOME_FAILED;c->lastAdjustment.reason=reason;
 }
 c->active=false;c->owns=false;c->awaitingApply=false;c->trial=false;
 c->state=FWT_FAULTED;c->reason=reason;return r;
}
static inline bool fwt_point_allowed(const fwt_controller *c,fwt_point p) {
 return fwt_config_contains(&c->config,p);
}
static inline fwt_request fwt_apply(fwt_controller *c,fwt_point p,uint64_t now,const char *reason) {
 if(!fwt_point_allowed(c,p)) return fwt_pause(c,"candidate-outside-limits");
 if(fwt_point_failed(c,p)) return fwt_pause(c,"error-recovery-exhausted");
 fwt_request r=fwt_none();r.kind=FWT_REQUEST_APPLY;r.point=p;
 c->pending=p;c->awaitingApply=true;c->state=FWT_APPLYING;c->stateSinceUs=now;
 c->reason=reason;if(c->decisions<UINT32_MAX) ++c->decisions;
 fwt_event_record(c,&c->lastAdjustment,FWT_EVENT_ADJUSTMENT,FWT_OUTCOME_REQUESTED,c->current,p,reason,now);
 return r;
}
static inline void fwt_applied(fwt_controller *c,bool success,uint64_t now) {
 if(!c->owns||!c->awaitingApply) return;
 if(!success||now<c->lastNowUs) {
  if(c->lastAdjustment.id) {c->lastAdjustment.outcome=FWT_OUTCOME_FAILED;c->lastAdjustment.reason="transition-not-verified";}
  c->active=false;c->owns=false;c->awaitingApply=false;
  c->state=FWT_FAULTED;c->reason="transition-not-verified";return;
 }
 c->current=c->pending;c->awaitingApply=false;c->state=FWT_SETTLING;
 if(c->lastAdjustment.id) c->lastAdjustment.outcome=FWT_OUTCOME_APPLIED;
 c->reason="waiting-for-stable-measurements";c->stateSinceUs=now;c->lastNowUs=now;
 c->errorEpochUs=now;c->errorSampleId=0;c->errorCapturedAtUs=0;fwt_error_streak_reset(c);
 c->generation=0;c->lastSampleId=0;c->invalidSinceUs=0;fwt_clear_window(c,now);
}
static inline double fwt_score(const fwt_controller *c,double hash,double power) {
 if(!fwt_positive(hash)||!fwt_positive(power)||!fwt_positive(c->baselineHash)||!fwt_positive(c->baselinePower)) return -1;
 const double rate=hash/c->baselineHash;
 const double efficiency=(hash/power)/(c->baselineHash/c->baselinePower);
 return c->config.priority==FWT_EFFICIENCY?efficiency:
  c->config.priority==FWT_PERFORMANCE?rate:(0.55*rate+0.45*efficiency);
}
static inline void fwt_hold(fwt_controller *c,uint64_t now) {
 c->active=c->config.continuous;c->state=c->config.continuous?FWT_OBSERVING:FWT_HOLDING;
 c->reason=c->config.continuous?"observing-conditions":"holding-best-result";
 c->lastSearchUs=now;c->previousAsicTemp=c->maxAsic;c->previousVrmTemp=c->maxVrm;
 c->trial=false;c->recovering=false;c->holdAfterRecovery=false;
}
static inline bool fwt_can_raise_voltage(const fwt_controller *c,const fwt_input *i) {
 return i->asicTempC+5<c->config.maxAsicTempC&&i->vrmTempC+5<c->config.maxVrmTempC&&
  i->powerW<c->config.maxPowerW*0.9&&c->current.coreVoltageMv<c->config.maxVoltageMv;
}
static inline fwt_request fwt_backoff(fwt_controller *c,const fwt_input *i,uint64_t now,const char *reason) {
 fwt_point p=c->current;
 c->holdAfterRecovery=(!c->active&&!c->config.continuous)||c->holdAfterRecovery;
 const double steps=ceil(fmax(c->limits.frequencyStepMHz,p.frequencyMHz*0.05)/c->limits.frequencyStepMHz);
 p.frequencyMHz=fwt_clamp(p.frequencyMHz-steps*c->limits.frequencyStepMHz,
  c->config.minFrequencyMHz,c->config.maxFrequencyMHz);
 p.coreVoltageMv=fwt_clamp(p.coreVoltageMv,c->config.minVoltageMv,c->config.maxVoltageMv);
 /* At most the bounded number of rejected points can obstruct this descent. */
 for(unsigned n=0;n<=FWT_ERROR_POINT_CAPACITY&&fwt_point_failed(c,p);++n) {
  const double next=fmax(c->config.minFrequencyMHz,p.frequencyMHz-c->limits.frequencyStepMHz);
  if(next>=p.frequencyMHz) return fwt_pause(c,"error-recovery-exhausted");
  p.frequencyMHz=next;
 }
 if(fwt_point_failed(c,p)) return fwt_pause(c,"error-recovery-exhausted");
 if(fwt_point_equal(p,c->current)) {
  /* The current point failed this run's constraints, not necessarily the
   * silicon. Retain the actual initiating cause without inventing a mean. */
  fwt_event_record(c,&c->lastDecision,FWT_EVENT_REJECTED,FWT_OUTCOME_FAILED,
   c->current,c->current,reason,now);
  if(c->lastDecision.id && fwt_input_valid(i,now)) {
   fwt_event *event=&c->lastDecision;event->hasConditions=true;
   event->asicTempC=i->asicTempC;event->vrmTempC=i->vrmTempC;
   event->powerW=i->powerW;event->hashRateGHs=i->hashRateGHs;
   event->errorPercentage=i->errorPercentage;event->worstErrorPercentage=i->worstErrorPercentage;
  }
  return fwt_pause(c,"cannot-reduce-load-within-limits");
 }
 (void)i;c->bestAvailable=false;c->trial=false;c->recovering=true;c->active=true;
 c->noImprovement=0;c->searchIndex=0;
 return fwt_apply(c,p,now,reason);
}
static inline void fwt_event_error_conditions(fwt_controller *c,const fwt_input *i,uint64_t now,const char *reason) {
 fwt_event_record(c,&c->lastDecision,FWT_EVENT_REJECTED,FWT_OUTCOME_FAILED,c->current,c->current,reason,now);
 if(c->lastDecision.id) {
  fwt_event *event=&c->lastDecision;event->hasConditions=true;
  event->asicTempC=i->asicTempC;event->vrmTempC=i->vrmTempC;
  event->powerW=i->powerW;event->hashRateGHs=i->hashRateGHs;
  event->errorPercentage=i->errorPercentage;event->worstErrorPercentage=i->worstErrorPercentage;
 }
}
static inline bool fwt_reject_error_point(fwt_controller *c) {
 if(c->stableAvailable&&fwt_point_equal(c->stable,c->current)) c->stableAvailable=false;
 c->bestAvailable=false;
 if(fwt_point_failed(c,c->current)) return true;
 if(c->failedCount>=FWT_ERROR_POINT_CAPACITY) return false;
 c->failedPoints[c->failedCount++]=c->current;return true;
}
static inline bool fwt_can_return_stable(const fwt_controller *c,const fwt_input *i) {
 if(!c->stableAvailable||!fwt_point_allowed(c,c->stable)||fwt_point_failed(c,c->stable)||
    fwt_point_equal(c->stable,c->current)||!fwt_positive(c->stablePower)||c->stablePower>=c->config.maxPowerW) return false;
 const bool frequencyChanged=fabs(c->stable.frequencyMHz-c->current.frequencyMHz)>=0.01;
 const bool voltageChanged=fabs(c->stable.coreVoltageMv-c->current.coreVoltageMv)>=0.01;
 /* Keep rollback on the existing one-coordinate actuator path. A coupled
  * restore belongs to the separate, rail-confirmed original-backup owner. */
 if(frequencyChanged&&voltageChanged) return false;
 if(c->stable.frequencyMHz>c->current.frequencyMHz||c->stable.coreVoltageMv>c->current.coreVoltageMv)
  return i->asicTempC+5<c->config.maxAsicTempC&&i->vrmTempC+5<c->config.maxVrmTempC&&
   i->powerW<c->config.maxPowerW*0.9&&c->stablePower<c->config.maxPowerW*0.9;
 return true;
}
static inline fwt_request fwt_error_recover(fwt_controller *c,const fwt_input *i,uint64_t now,bool voltageAllowed) {
 if(!fwt_reject_error_point(c)) {
  fwt_event_error_conditions(c,i,now,"error-point-exclusion-full");
  return fwt_pause(c,"error-recovery-exhausted");
 }
 if(c->rejected<UINT32_MAX) ++c->rejected;
 if(++c->recoveryAttempts>6) {
  c->lastDecision.reason="error-recovery-exhausted";
  return fwt_pause(c,"error-recovery-exhausted");
 }
 c->holdAfterRecovery=(!c->active&&!c->config.continuous)||c->holdAfterRecovery;
 c->trial=false;c->recovering=true;c->active=true;
 if(fwt_can_return_stable(c,i)) return fwt_apply(c,c->stable,now,"restoring-last-stable-point");
 if(voltageAllowed&&fwt_can_raise_voltage(c,i)) {
  fwt_point p=c->current;p.coreVoltageMv=fmin(c->config.maxVoltageMv,p.coreVoltageMv+c->limits.voltageStepMv);
  if(!fwt_point_failed(c,p)) return fwt_apply(c,p,now,"error-recovery-voltage-trial");
 }
 return fwt_backoff(c,i,now,"error-recovery-clock-backoff");
}
/* Independent of score windows and browser polling. Old counter intervals
 * cannot condemn a newly applied point, including while it is settling. */
static inline const char *fwt_early_error(fwt_controller *c,const fwt_input *i) {
 if(i->sampleId<=c->errorSampleId||i->capturedAtUs<=c->errorCapturedAtUs) return NULL;
 if(i->capturedAtUs<c->errorEpochUs||i->capturedAtUs-c->errorEpochUs<FWT_FRESH_US) return NULL;
 if(c->errorCapturedAtUs&&i->capturedAtUs-c->errorCapturedAtUs>FWT_FRESH_US) fwt_error_streak_reset(c);
 c->errorSampleId=i->sampleId;c->errorCapturedAtUs=i->capturedAtUs;
 if(i->errorPercentage>=FWT_BOARD_SEVERE_ERROR_LIMIT||i->worstErrorPercentage>=FWT_CHIP_SEVERE_ERROR_LIMIT)
  return "early-severe-error-limit";
 if(i->errorPercentage<=FWT_BOARD_ERROR_LIMIT&&i->worstErrorPercentage<=FWT_CHIP_ERROR_LIMIT) {
  fwt_error_streak_reset(c);return NULL;
 }
 if(!c->errorSamples) c->errorStartedUs=i->capturedAtUs;
 if(c->errorSamples<UINT32_MAX) ++c->errorSamples;
 return c->errorSamples>=FWT_ERROR_MIN_SAMPLES&&i->capturedAtUs-c->errorStartedUs>=FWT_ERROR_CONFIRM_US?
  "early-error-limit":NULL;
}
static inline fwt_request fwt_next_candidate(fwt_controller *c,const fwt_input *i,uint64_t now) {
 if(c->roundExperiments>=FWT_MAX_EXPERIMENTS||c->noImprovement>=4||c->experiments==UINT32_MAX) {
  fwt_hold(c,now);return fwt_none();
 }
 /* Coordinate search prioritizes goals; actual points are learned per miner. */
 const unsigned efficiencyOrder[4]={0,1,2,3}; /* V down, F down, F up, V up */
 const unsigned balancedOrder[4]={2,0,1,3};
 const unsigned performanceOrder[4]={2,3,0,1};
 const unsigned *order=c->config.priority==FWT_EFFICIENCY?efficiencyOrder:
  c->config.priority==FWT_PERFORMANCE?performanceOrder:balancedOrder;
 for(unsigned attempt=0;attempt<4;++attempt) {
  const unsigned direction=order[c->searchIndex++%4];fwt_point p=c->current;
  if(direction==0) p.coreVoltageMv-=c->limits.voltageStepMv;
  if(direction==1) p.frequencyMHz-=c->limits.frequencyStepMHz;
  if(direction==2) {
   if(i->asicTempC+3>=c->config.maxAsicTempC||i->vrmTempC+3>=c->config.maxVrmTempC||i->powerW>=c->config.maxPowerW*0.95) continue;
   p.frequencyMHz+=c->limits.frequencyStepMHz;
  }
  if(direction==3) {if(!fwt_can_raise_voltage(c,i)) continue;p.coreVoltageMv+=c->limits.voltageStepMv;}
  if(!fwt_point_allowed(c,p)||fwt_point_equal(p,c->current)||fwt_point_failed(c,p)) continue;
  c->trialOrigin=c->current;c->trial=true;c->recovering=false;
  ++c->experiments;++c->roundExperiments;
  return fwt_apply(c,p,now,"evaluating-nearby-operating-point");
 }
 fwt_hold(c,now);return fwt_none();
}
static inline fwt_request fwt_tick(fwt_controller *c,const fwt_input *i,uint64_t now) {
 if(!c->owns) return fwt_none();
 if(now<c->lastNowUs) return fwt_pause(c,"monotonic-clock-regressed");
 c->lastNowUs=now;
 if(!fwt_config_valid(&c->config,&c->limits)) return fwt_pause(c,"invalid-configuration");
 if(!i||i->fault) return fwt_pause(c,"hardware-protection-fault");
 if(c->awaitingApply) {
  if(now-c->stateSinceUs>FWT_APPLY_TIMEOUT_US) return fwt_pause(c,"transition-timed-out");
  return fwt_none();
 }
 if(!i->miningActive) {fwt_cancel(c,"mining-stopped-or-owned-elsewhere");return fwt_none();}
 if(!fwt_input_valid(i,now)) {
  fwt_error_streak_reset(c);
  /* Counter warmup is expected after the protected actuator retires work. */
  if(c->state==FWT_SETTLING&&now-c->stateSinceUs<=FWT_SETTLE_US) return fwt_none();
  if(!c->invalidSinceUs) c->invalidSinceUs=now;
  if(now-c->invalidSinceUs>=FWT_INVALID_TIMEOUT_US) return fwt_pause(c,"required-telemetry-unavailable");
  if(c->state==FWT_HOLDING&&!c->config.continuous) c->holdAfterRecovery=true;
  c->state=FWT_SUSPENDED;c->reason="waiting-for-valid-telemetry";return fwt_none();
 }
 if(c->generation&&i->generation!=c->generation) {
  fwt_cancel(c,"mining-generation-changed");return fwt_none();
 }
 c->generation=i->generation;
 if(c->invalidSinceUs) {c->invalidSinceUs=0;c->rebaseline=true;}
 if(!fwt_point_equal(i->current,c->current)) {
  fwt_cancel(c,"operating-point-changed");return fwt_none();
 }
 const bool thermal=i->asicTempC>=c->config.maxAsicTempC-1||i->vrmTempC>=c->config.maxVrmTempC-1;
 const bool electrical=i->powerW>c->config.maxPowerW;
 const bool caps=!fwt_point_allowed(c,c->current);
 if(thermal||electrical||caps) return fwt_backoff(c,i,now,thermal?"temperature-limit-backoff":electrical?"power-limit-backoff":"new-user-limits");
 if(c->rebaseline) {
  fwt_error_streak_reset(c);
  c->rebaseline=false;c->trial=false;c->bestAvailable=false;c->state=FWT_BASELINE;
  c->stateSinceUs=now;c->lastSampleId=0;fwt_clear_window(c,now);
 }
 if(c->state==FWT_HOLDING||c->state==FWT_OBSERVING) {
  if(i->worstErrorPercentage>FWT_CHIP_ERROR_LIMIT||i->errorPercentage>FWT_BOARD_ERROR_LIMIT) {
   fwt_event_error_conditions(c,i,now,"held-point-became-unstable");
   return fwt_error_recover(c,i,now,false);
  }
  if(!c->config.continuous) return fwt_none();
  const bool changed=fabs(i->asicTempC-c->previousAsicTemp)>=3||fabs(i->vrmTempC-c->previousVrmTemp)>=3||
   (c->bestHash>0&&fabs(i->hashRateGHs-c->bestHash)/c->bestHash>0.10)||
   (c->bestPower>0&&fabs(i->powerW-c->bestPower)/c->bestPower>0.10);
  if(!changed&&now-c->lastSearchUs<FWT_REASSESS_US) return fwt_none();
  c->active=true;c->bestAvailable=false;c->roundExperiments=0;c->noImprovement=0;c->searchIndex=0;
  c->state=FWT_BASELINE;c->reason="conditions-changed-remeasuring";c->lastSampleId=0;fwt_clear_window(c,now);
 }
 const char *earlyError=fwt_early_error(c,i);
 if(earlyError) {
  fwt_event_error_conditions(c,i,now,earlyError);
  return fwt_error_recover(c,i,now,true);
 }
 if(c->state==FWT_SETTLING) {
  if(now-c->stateSinceUs<FWT_SETTLE_US) return fwt_none();
  c->state=FWT_MEASURING;c->reason="measuring-settled-operating-point";fwt_clear_window(c,now);
 }
 /* Discard a whole maximum permitted counter interval after the window starts.
  * Merely receiving a still-TTL-fresh pre-settle sample is not new evidence. */
 if(i->capturedAtUs<c->observationStartedUs||
    i->capturedAtUs-c->observationStartedUs<FWT_FRESH_US||i->sampleId<=c->lastSampleId) return fwt_none();
 c->lastSampleId=i->sampleId;++c->samples;c->sumHash+=i->hashRateGHs;
 c->sumPower+=i->powerW;c->sumError+=i->errorPercentage;
 c->sumHashSquared+=i->hashRateGHs*i->hashRateGHs;c->sumPowerSquared+=i->powerW*i->powerW;
 c->worstError=fmax(c->worstError,i->worstErrorPercentage);
 c->maxAsic=fmax(c->maxAsic,i->asicTempC);c->maxVrm=fmax(c->maxVrm,i->vrmTempC);
 if(now-c->observationStartedUs<FWT_WINDOW_US+FWT_FRESH_US||c->samples<FWT_MIN_SAMPLES) return fwt_none();
 const double hash=c->sumHash/c->samples,power=c->sumPower/c->samples;
 if(!fwt_positive(hash)||!fwt_positive(power)) return fwt_pause(c,"no-useful-work-measured");
 /* Use observed window variability to avoid rewarding measurement noise.
  * This is an engineering comparison margin, not a distribution/coverage claim. */
 const double varianceHash=fmax(0,c->sumHashSquared/c->samples-hash*hash);
 const double variancePower=fmax(0,c->sumPowerSquared/c->samples-power*power);
 const double uncertainty=2*(sqrt(varianceHash/c->samples)/hash+
  (c->config.priority==FWT_PERFORMANCE?0:sqrt(variancePower/c->samples)/power));
 if(!isfinite(uncertainty)) return fwt_pause(c,"measurement-uncertainty-invalid");
 const bool errors=c->worstError>FWT_CHIP_ERROR_LIMIT||c->sumError/c->samples>FWT_BOARD_ERROR_LIMIT;
 if(errors) {
  fwt_event_measure(c,FWT_EVENT_REJECTED,"error-window-limit",now,hash,power,false,0,0);
  c->lastDecision.outcome=FWT_OUTCOME_FAILED;
  return fwt_error_recover(c,i,now,true);
 }
 c->recoveryAttempts=0;
 c->stable=c->current;c->stablePower=power;c->stableAvailable=!fwt_point_failed(c,c->current);
 if(!c->bestAvailable) {
  fwt_event_measure(c,FWT_EVENT_BASELINE,"round-reference-measured",now,hash,power,false,0,0);
  c->baselineHash=hash;c->baselinePower=power;c->bestHash=hash;c->bestPower=power;
  c->best=c->current;c->bestScore=1;c->bestUncertainty=uncertainty;c->bestAvailable=true;
  if(c->holdAfterRecovery) {fwt_hold(c,now);return fwt_none();}
 } else {
  const double score=fwt_score(c,hash,power);
  if(!isfinite(score)||score<=0) return fwt_pause(c,"measurement-score-invalid");
  const double required=c->bestScore*(1+fmax(0.002,uncertainty+c->bestUncertainty));
  fwt_event_measure(c,score>required?FWT_EVENT_ACCEPTED:c->trial?FWT_EVENT_REJECTED:FWT_EVENT_RETAINED,
   score>required?"score-beat-required-margin":c->trial?"improvement-within-noise-margin":"best-point-remeasured",
   now,hash,power,true,score,required);
  if(score>c->bestScore*(1+fmax(0.002,uncertainty+c->bestUncertainty))) {
   c->bestScore=score;c->best=c->current;c->bestHash=hash;c->bestPower=power;c->bestUncertainty=uncertainty;c->noImprovement=0;
  } else if(c->trial) {
   ++c->rejected;++c->noImprovement;c->trial=false;
   if(!fwt_point_equal(c->current,c->best)) {
    /* A rollback that would raise power is prohibited near a thermal limit. */
    if((c->best.frequencyMHz>c->current.frequencyMHz||c->best.coreVoltageMv>c->current.coreVoltageMv)&&
       (i->asicTempC+3>=c->config.maxAsicTempC||i->vrmTempC+3>=c->config.maxVrmTempC||c->bestPower>c->config.maxPowerW))
     return fwt_backoff(c,i,now,"rollback-lacks-headroom");
    c->recovering=true;return fwt_apply(c,c->best,now,"restoring-better-measured-point");
   }
  }
 }
 c->trial=false;c->recovering=false;
 return fwt_next_candidate(c,i,now);
}
#ifdef __cplusplus
static_assert(sizeof(fwt_controller)<=1536,"Bound controller copies on configuration task stacks");
#else
_Static_assert(sizeof(fwt_controller)<=1536,"Bound controller copies on configuration task stacks");
#endif
#ifdef __cplusplus
}
#endif
#endif
