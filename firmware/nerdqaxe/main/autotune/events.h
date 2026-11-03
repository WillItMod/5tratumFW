/* Bounded RAM presentation history. GPL-3.0-or-later. No I/O or control. */
#ifndef FIVETRATUM_AUTOTUNE_EVENTS_H
#define FIVETRATUM_AUTOTUNE_EVENTS_H
#include "core.h"
#define FWT_EVENT_CAPACITY 8u
typedef struct {
 uint32_t runId, latestSequence;
 unsigned count, head;
 fwt_event events[FWT_EVENT_CAPACITY];
} fwt_history;
static inline const char *fwt_event_kind_name(fwt_event_kind kind) {
 switch(kind) {
 case FWT_EVENT_STARTED:return "started";case FWT_EVENT_BASELINE:return "baseline";
 case FWT_EVENT_ACCEPTED:return "accepted";case FWT_EVENT_REJECTED:return "rejected";
 case FWT_EVENT_RETAINED:return "retained";case FWT_EVENT_ERRORS:return "errors";
 case FWT_EVENT_ADJUSTMENT:return "adjustment";default:return "unknown";
 }
}
static inline const char *fwt_event_outcome_name(fwt_event_outcome outcome) {
 switch(outcome) {
 case FWT_OUTCOME_REQUESTED:return "requested";case FWT_OUTCOME_APPLIED:return "applied";
 case FWT_OUTCOME_FAILED:return "failed";default:return "measured";
 }
}
static inline const char *fwt_event_short_label(const fwt_event *event) {
 if(!event||!event->id) return "No decisions yet";
 switch(event->kind) {
 case FWT_EVENT_STARTED:return "Run started";case FWT_EVENT_BASELINE:return "Reference measured";
 case FWT_EVENT_ACCEPTED:return "Better result found";case FWT_EVENT_REJECTED:return "Trial not better";
 case FWT_EVENT_RETAINED:return "Best point checked";case FWT_EVENT_ERRORS:return "Errors too high";
 case FWT_EVENT_ADJUSTMENT:
  if(event->outcome==FWT_OUTCOME_FAILED) return "Change failed";
  if(event->outcome==FWT_OUTCOME_REQUESTED) return "Change requested";
  if(event->reason&&!strcmp(event->reason,"temperature-limit-backoff")) return "Cooling backoff";
  if(event->reason&&!strcmp(event->reason,"power-limit-backoff")) return "Power backoff";
  if(event->reason&&!strcmp(event->reason,"restoring-better-measured-point")) return "Returned to best";
  if(event->reason&&!strcmp(event->reason,"error-recovery-voltage-trial")) return "Error: voltage trial";
  return "Step applied";
 default:return "Decision unavailable";
 }
}
static inline unsigned fwt_history_count(const fwt_history *history) { return history?history->count:0; }
static inline const fwt_event *fwt_history_at(const fwt_history *history,unsigned index) {
 return history&&index<history->count?&history->events[(history->head+index)%FWT_EVENT_CAPACITY]:NULL;
}
static inline const fwt_event *fwt_history_latest(const fwt_history *history) {
 return history&&history->count?fwt_history_at(history,history->count-1):NULL;
}
static inline void fwt_history_add(fwt_history *history,const fwt_event *event) {
 if(!event->id) return;
 for(unsigned i=0;i<history->count;++i) {
  fwt_event *stored=&history->events[(history->head+i)%FWT_EVENT_CAPACITY];
  if(stored->id==event->id) {*stored=*event;return;} /* Confirm a request without duplicating it. */
 }
 if(event->id<=history->latestSequence) return; /* Never resurrect an evicted record. */
 unsigned slot;
 if(history->count<FWT_EVENT_CAPACITY) {slot=(history->head+history->count)%FWT_EVENT_CAPACITY;++history->count;}
 else {slot=history->head;history->head=(history->head+1)%FWT_EVENT_CAPACITY;}
 history->events[slot]=*event;history->latestSequence=event->id;
}
/* Call under the existing owner mutex after tick/applied/start and before a
 * cached read. Polling cannot create events or change a controller sample. */
static inline void fwt_history_capture(fwt_history *history,const fwt_controller *controller) {
 if(history->runId!=controller->runId) {memset(history,0,sizeof(*history));history->runId=controller->runId;}
 const fwt_event *a=&controller->lastDecision,*b=&controller->lastAdjustment;
 if(a->id<=b->id) {fwt_history_add(history,a);fwt_history_add(history,b);}
 else {fwt_history_add(history,b);fwt_history_add(history,a);}
}
#ifdef __cplusplus
static_assert(sizeof(fwt_event)<=128,"Keep decision records bounded");
static_assert(sizeof(fwt_history)<=1100,"Keep history outside controller copies");
#else
_Static_assert(sizeof(fwt_event)<=128,"Keep decision records bounded");
_Static_assert(sizeof(fwt_history)<=1100,"Keep history outside controller copies");
#endif
#endif
