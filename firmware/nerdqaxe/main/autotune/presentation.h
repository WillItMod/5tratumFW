/* Read-only presentation of the measured controller state. GPL-3.0-or-later. */
#ifndef FIVETRATUM_AUTOTUNE_PRESENTATION_H
#define FIVETRATUM_AUTOTUNE_PRESENTATION_H
#include "core.h"

typedef struct {
 bool timed, referenceAvailable, bestAvailable;
 double phaseElapsedSeconds, minimumSeconds;
 unsigned samples, minimumSamples, roundExperiments, roundExperimentLimit;
 uint32_t rejectedWindows;
 double referenceHash, referencePower, bestHash, bestPower;
} fwt_search_view;

/* The caller holds its normal controller mutex. This function has no I/O,
 * clock reads, mutations, allocation or persistence. A minimum duration is
 * not a completion estimate: fresh unique samples may take longer to arrive.
 * The reference is the current search round, not the immutable user backup. */
static inline fwt_search_view fwt_search_read(const fwt_controller *c, uint64_t now, bool obscured)
{
 fwt_search_view v; memset(&v,0,sizeof(v));
 v.minimumSamples=FWT_MIN_SAMPLES; v.roundExperimentLimit=FWT_MAX_EXPERIMENTS;
 if(!c) return v;
 v.samples=c->samples; v.roundExperiments=c->roundExperiments; v.rejectedWindows=c->rejected;
 if(!obscured && c->owns && c->active) {
  uint64_t since=0, minimum=0;
  if(c->state==FWT_SETTLING) {since=c->stateSinceUs;minimum=FWT_SETTLE_US;}
  else if(c->state==FWT_BASELINE || c->state==FWT_MEASURING) {
   since=c->observationStartedUs; minimum=FWT_WINDOW_US+FWT_FRESH_US;
  }
  if(minimum && now>=since) {
   v.timed=true; v.phaseElapsedSeconds=(now-since)/(double)FWT_SECOND;
   v.minimumSeconds=minimum/(double)FWT_SECOND;
  }
 }
 if(!obscured && c->bestAvailable) {
  v.referenceAvailable=fwt_positive(c->baselineHash)&&fwt_positive(c->baselinePower);
  v.bestAvailable=fwt_positive(c->bestHash)&&fwt_positive(c->bestPower);
  if(v.referenceAvailable) {v.referenceHash=c->baselineHash;v.referencePower=c->baselinePower;}
  if(v.bestAvailable) {v.bestHash=c->bestHash;v.bestPower=c->bestPower;}
 }
 return v;
}
#endif
