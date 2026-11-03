#include "autotune/presentation.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
 fwt_controller c; memset(&c,0,sizeof(c));
 fwt_search_view v=fwt_search_read(NULL,0,false);
 assert(!v.timed&&!v.referenceAvailable&&!v.bestAvailable&&v.minimumSamples==8&&v.roundExperimentLimit==24);
 c.owns=c.active=true;c.state=FWT_SETTLING;c.stateSinceUs=10*FWT_SECOND;
 c.samples=3;c.roundExperiments=2;c.rejected=1;
 fwt_controller before=c;
 v=fwt_search_read(&c,25*FWT_SECOND,false);
 assert(v.timed&&v.phaseElapsedSeconds==15&&v.minimumSeconds==30&&v.samples==3&&v.roundExperiments==2&&v.rejectedWindows==1);
 assert(!memcmp(&before,&c,sizeof(c)));
 v=fwt_search_read(&c,9*FWT_SECOND,false);assert(!v.timed);
 c.state=FWT_MEASURING;c.observationStartedUs=40*FWT_SECOND;
 v=fwt_search_read(&c,145*FWT_SECOND,false);
 assert(v.timed&&v.phaseElapsedSeconds==105&&v.minimumSeconds==75); /* Never invent a completion time. */
 c.state=FWT_BASELINE;v=fwt_search_read(&c,55*FWT_SECOND,false);
 assert(v.timed&&v.phaseElapsedSeconds==15&&v.minimumSeconds==75);
 c.bestAvailable=true;c.baselineHash=1000;c.baselinePower=20;c.bestHash=1050;c.bestPower=19;
 c.state=FWT_HOLDING;c.active=false;
 v=fwt_search_read(&c,150*FWT_SECOND,false);
 assert(!v.timed&&v.referenceAvailable&&v.bestAvailable&&v.referenceHash==1000&&v.bestPower==19);
 v=fwt_search_read(&c,150*FWT_SECOND,true);assert(!v.timed&&!v.referenceAvailable&&!v.bestAvailable);
 c.bestAvailable=false;v=fwt_search_read(&c,150*FWT_SECOND,false);
 assert(!v.referenceAvailable&&!v.bestAvailable); /* Old round means cannot masquerade as the new result. */
 c.bestAvailable=true;c.baselineHash=NAN;c.bestPower=0;v=fwt_search_read(&c,150*FWT_SECOND,false);
 assert(!v.referenceAvailable&&!v.bestAvailable);
 c.state=FWT_SETTLING;c.active=true;c.owns=false;v=fwt_search_read(&c,150*FWT_SECOND,false);assert(!v.timed);
 puts("PASS: cached search timing, finite measurements, round reset and read-only presentation");
 return 0;
}
