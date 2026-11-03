import { fakeAsync, tick } from '@angular/core/testing';
import { EMPTY, Subject, of, throwError } from 'rxjs';
import { AutotuneComponent } from './autotune.component';
import { AutotuneReport } from '../../models/IAutotune';

function fixture(count = 4): AutotuneReport {
  return {schemaVersion:1,waiverVersion:1,supported:true,
    hardware:{model:count === 1 ? 'Gamma 601' : count === 8 ? 'NerdOCTAXE-γ' : 'NerdQAxe++',asicCount:count,asicSensor:'ASIC sensor',vrmSensor:'Regulator sensor',powerSource:'Estimated device power, not wall power'},
    config:{priority:'balanced',continuous:false,minFrequencyMHz:400,maxFrequencyMHz:525,minVoltageMv:1000,maxVoltageMv:1150,maxAsicTempC:65,maxVrmTempC:85,maxPowerW:250},
    limits:{minFrequencyMHz:50,maxFrequencyMHz:1000,minVoltageMv:900,maxVoltageMv:1400,maxAsicTempC:70,maxVrmTempC:95,maxPowerW:300,frequencyStepMHz:6.25,voltageStepMv:5},
    guidance:{asic:'Keep below the protected ASIC ceiling.',vrm:'This sensor measures regulator temperature.',power:'The power estimate is not a PSU rating.',experimental:true},
    status:{state:'disabled',reason:'Ready for explicit Start',active:false,ownsSettings:false,runId:0,experiments:0,
      baseline:null,current:{frequencyMHz:525,coreVoltageMv:1150},best:null,
      telemetry:{valid:true,miningActive:true,fault:false,fanOk:true,hashRateGHs:count * 1000,errorPercentage:0,worstErrorPercentage:0,powerW:count*20,asicTempC:55,vrmTempC:48,voltageReadbackMv:1150,sampleAgeSeconds:0}},
    backup:{available:false,restored:false,frequencyMHz:null,coreVoltageMv:null}};
}

describe('Dynamic tuning controls', () => {
  let component: AutotuneComponent;
  let report: AutotuneReport;
  let service: {get:jasmine.Spy;authorize:jasmine.Spy;command:jasmine.Spy};
  let now: number;
  beforeEach(() => {
    now = 1000;
    spyOn(performance,'now').and.callFake(() => now);
    report = fixture();
    service = {get:jasmine.createSpy('get').and.callFake(() => of(structuredClone(report))),
      authorize:jasmine.createSpy('authorize').and.returnValue(of({totp:'123456'})),
      command:jasmine.createSpy('command').and.returnValue(of({ok:true,restartRequired:false}))};
    component = new AutotuneComponent(service as any);
    component.load();
  });
  afterEach(() => component.ngOnDestroy());
  function acceptedStart(): void {
    service.command.and.callFake((body: any) => {
      if (body.action === 'start') {
        report.status = {...report.status,active:true,ownsSettings:true,state:'baseline',runId:report.status.runId+1};
        report.backup = {available:true,restored:false,frequencyMHz:525,coreVoltageMv:1150};
      }
      return of({ok:true,restartRequired:false});
    });
  }
  it('loads and polls status without enabling or writing anything', fakeAsync(() => {
    component.ngOnInit(); tick(15000);
    expect(service.get.calls.count()).toBeGreaterThan(1);
    expect(service.command).not.toHaveBeenCalled(); expect(service.authorize).not.toHaveBeenCalled();
    expect(component.waiverAccepted).toBeFalse();
    component.ngOnDestroy();const reads=service.get.calls.count();tick(10000);expect(service.get.calls.count()).toBe(reads);
  }));
  it('requires a fresh explicit waiver for every Start and sends the supported waiver version', fakeAsync(() => {
    acceptedStart();
    component.start(); expect(service.command).not.toHaveBeenCalled();
    component.waiverAccepted = true; component.start(); tick(0);
    expect(service.command.calls.count()).toBe(1);
    expect(service.command.calls.mostRecent().args).toEqual([{action:'start',waiverAccepted:true,waiverVersion:1},'123456']);
    expect(component.waiverAccepted).toBeFalse(); expect(component.message).toContain('Tuning started'); expect(component.busy).toBeFalse();
  }));
  it('does not send Start when authentication is cancelled and clears its waiver', () => {
    service.authorize.and.returnValue(EMPTY); component.waiverAccepted=true; component.start();
    expect(service.command).not.toHaveBeenCalled(); expect(component.busy).toBeFalse(); expect(component.waiverAccepted).toBeFalse();
  });
  it('does not write after the page is destroyed during authentication', () => {
    const auth = new Subject<{totp?:string}>(); service.authorize.and.returnValue(auth);
    component.waiverAccepted=true;component.start();component.ngOnDestroy();auth.next({totp:'654321'});
    expect(service.command).not.toHaveBeenCalled();
  });
  it('preserves local edits when status is refreshed', () => {
    component.edit!.priority='efficiency';report.status.state='observing';component.load();
    expect(component.edit!.priority).toBe('efficiency');expect(component.hasUnsavedChanges).toBeTrue();
    expect(component.report!.status.state).toBe('observing');
  });
  it('saves only complete tuning options, confirms readback and does not Start', fakeAsync(() => {
    component.edit!.priority='performance';component.edit!.continuous=true;
    service.command.and.callFake((body:any) => {report.config=structuredClone(body.config);return of({ok:true,restartRequired:false});});
    component.save();tick(0);
    const body=service.command.calls.mostRecent().args[0];
    expect(body.action).toBe('configure');expect(Object.keys(body.config).length).toBe(9);
    expect(body.config.priority).toBe('performance');expect(body.config.continuous).toBeTrue();
    expect(component.hasUnsavedChanges).toBeFalse();expect(component.message).toContain('options saved');expect(report.status.active).toBeFalse();
  }));
  it('does not save invalid or widened hardware bounds', () => {
    for (const [field,value] of [['maxAsicTempC',71],['maxVrmTempC',96],['maxPowerW',301],['maxVoltageMv',1500],['minFrequencyMHz',0],['maxFrequencyMHz',NaN]] as const) {
      component.edit={...report.config,[field]:value};expect(component.validationError).not.toBe('');component.save();
    }
    expect(service.command).not.toHaveBeenCalled();
  });
  it('matches each actuator’s bound units without confusing its search increment with the input grid', () => {
    component.edit!.minFrequencyMHz=400.5;
    if(component.wholeMHz) expect(component.validationError).toContain('whole MHz'); else expect(component.validationError).toBe('');
    component.edit=structuredClone(report.config);component.edit!.maxVoltageMv=1150.5;expect(component.validationError).toContain('whole millivolts');
    component.save();expect(service.command).not.toHaveBeenCalled();
  });
  it('does not start with unsaved options, paused/faulted mining or failed cooling', () => {
    component.waiverAccepted=true;component.edit!.continuous=true;expect(component.canStart).toBeFalse();
    component.edit=structuredClone(report.config);
    for (const key of ['miningActive','fanOk'] as const) {
      report.status.telemetry[key]=false;component.load();expect(component.canStart).toBeFalse();report.status.telemetry[key]=true;
    }
    report.status.telemetry.fault=true;component.load();expect(component.canStart).toBeFalse();
    component.start();expect(service.command).not.toHaveBeenCalled();
  });
  it('supports the same independent goals for one, four and eight ASICs', () => {
    for (const count of [1,4,8]) {
      report=fixture(count);component.load();component.waiverAccepted=true;
      expect(component.readError).toBe('');expect(component.canStart).toBeTrue();
      expect(component.priorities.map(item=>item.value)).toEqual(['efficiency','balanced','performance']);
      expect(component.metric(report.status.telemetry.errorPercentage,2)).toBe('0.00');
    }
  });
  it('marks stale status unavailable and disables Start without hiding explicit restore', () => {
    report.backup={available:true,restored:false,frequencyMHz:525,coreVoltageMv:1150};component.load();component.waiverAccepted=true;
    now+=13000;expect(component.canStart).toBeFalse();expect(component.metric(50)).toBe('—');expect(component.canRestore).toBeTrue();
  });
  it('counts complete network transit time before accepting hardware freshness', () => {
    const response = new Subject<AutotuneReport>();service.get.and.returnValue(response);
    component.load();now+=1000;report.status.telemetry.sampleAgeSeconds=14.5;response.next(report);response.complete();
    component.waiverAccepted=true;expect(component.isFresh).toBeTrue();expect(component.telemetryFresh).toBeFalse();expect(component.canStart).toBeFalse();
    expect(component.metric(0,2)).toBe('—');
  });
  it('rejects unknown API metadata without authorizing a request', () => {
    for (const mutate of [(value:any)=>value.waiverVersion=2,(value:any)=>value.status.state='mystery',(value:any)=>delete value.hardware.model,(value:any)=>value.backup={available:true,restored:false,frequencyMHz:null,coreVoltageMv:1150}]) {
      const invalid:any=fixture();mutate(invalid);service.get.and.returnValue(of(invalid));component.load();expect(component.readError).not.toBe('');
      component.waiverAccepted=true;expect(component.canStart).toBeFalse();
    }
    expect(service.command).not.toHaveBeenCalled();
  });
  it('never treats missing or malformed sensor data as safe zero readings', () => {
    for (const mutate of [(value:any)=>value.status.telemetry.valid='true',(value:any)=>value.status.telemetry.sampleAgeSeconds=undefined,
      (value:any)=>value.status.telemetry.voltageReadbackMv=-1,(value:any)=>value.status.telemetry.errorPercentage=101]) {
      const invalid:any=fixture();mutate(invalid);service.get.and.returnValue(of(invalid));component.load();
      component.waiverAccepted=true;expect(component.canStart).toBeFalse();expect(component.metric(0)).toBe('—');
    }
    expect(service.command).not.toHaveBeenCalled();
  });
  it('keeps the original-backup restore reachable with corrupt options and unusable current protection limits', fakeAsync(() => {
    report.backup={available:true,restored:false,frequencyMHz:525,coreVoltageMv:1150};
    report.supported=false;report.config=null as any;report.limits.maxAsicTempC=0;report.status.telemetry.valid=false;
    component.load();component.waiverAccepted=true;
    expect(component.readError).toBe('');expect(component.canStart).toBeFalse();expect(component.canSave).toBeFalse();expect(component.canRestore).toBeTrue();
    service.command.and.callFake(()=>{report.backup.restored=true;report.status.active=false;report.status.ownsSettings=false;return of({ok:true,restartRequired:false});});
    component.restore();tick(0);expect(component.message).toContain('backup restored');expect(service.command.calls.mostRecent().args[0]).toEqual({action:'stop'});
  }));
  it('keeps a lost Start acknowledgment unconfirmed, permits restore and never repeats Start', () => {
    service.command.and.returnValue(throwError(()=>new Error('network')));component.waiverAccepted=true;component.start();
    expect(component.actionError).toContain('could not be confirmed');expect(component.canRestore).toBeTrue();
    expect(service.command.calls.count()).toBe(1);expect(component.waiverAccepted).toBeFalse();
  });
  it('requires acknowledgment and never retries a state-changing request automatically', fakeAsync(() => {
    service.command.and.returnValue(of({ok:false,restartRequired:false}));component.edit!.continuous=true;component.save();tick(30000);
    expect(service.command.calls.count()).toBe(1);expect(component.actionError).toContain('could not be confirmed');expect(component.hasUnsavedChanges).toBeTrue();
  }));
  it('confirms an asynchronous original-backup restore through reads only', fakeAsync(() => {
    report.backup={available:true,restored:false,frequencyMHz:525,coreVoltageMv:1150};
    report.status={...report.status,state:'holding',active:true,ownsSettings:true,current:{frequencyMHz:500,coreVoltageMv:1100}};component.load();
    service.command.and.callFake(() => {report.status.state='restoring';report.status.active=false;return of({ok:true,restartRequired:false});});
    component.restore();tick(0);expect(component.busy).toBeTrue();expect(component.report!.status.state).toBe('restoring');
    tick(1000);expect(component.message).toBe('');report.status.ownsSettings=false;report.status.state='disabled';report.status.current={frequencyMHz:525,coreVoltageMv:1150};report.backup.restored=true;
    tick(1000);expect(component.busy).toBeFalse();expect(component.message).toContain('backup restored');
    expect(service.command.calls.count()).toBe(1);expect(service.command.calls.mostRecent().args[0]).toEqual({action:'stop'});
  }));
  it('does not call a restore successful when the target differs from its backup', fakeAsync(() => {
    report.backup={available:true,restored:true,frequencyMHz:525,coreVoltageMv:1150};
    report.status.current={frequencyMHz:500,coreVoltageMv:1100};component.load();component.restore();tick(30001);
    expect(component.message).toBe('');expect(component.actionError).toContain('could not be confirmed');expect(service.command.calls.count()).toBe(1);
  }));
  it('leaves a faulted restore unconfirmed without clearing its hardware fault', fakeAsync(() => {
    report.backup={available:true,restored:false,frequencyMHz:525,coreVoltageMv:1150};component.load();
    service.command.and.callFake(()=>{report.status.state='faulted';report.status.telemetry.fault=true;return of({ok:true,restartRequired:false});});
    component.restore();tick(0);expect(component.actionError).toContain('could not be confirmed');expect(report.status.telemetry.fault).toBeTrue();expect(component.message).toBe('');
  }));
  it('shows actual useful TH/s and J/TH, and never presents invalid live values as zero', () => {
    expect(component.ths(4000)).toBe('4.00'); expect(component.energy(4000,80)).toBe('20.00');
    expect(component.energy(0,80)).toBe('—'); expect(component.energy(4000,NaN)).toBe('—');
    now+=13000; expect(component.ths(4000)).toBe('—');expect(component.energy(4000,80)).toBe('—');
  });
  it('keeps a prospective goal separate from the saved goal and computes headroom from saved ceilings', () => {
    component.edit!.priority='performance'; component.edit!.maxAsicTempC=60;
    expect(component.runningGoal).toContain('Balanced');expect(component.headroom[0].value).toBe('10.0 °C below ceiling');
    expect(component.priorities.every(goal=>goal.description.length>50)).toBeTrue();
    expect(service.command).not.toHaveBeenCalled();
  });
  it('accepts optional completed round means independently of live sensors and shows a minimum rather than an ETA', () => {
    report.status.search={phaseElapsedSeconds:92,minimumSeconds:75,samples:7,minimumSamples:8,roundExperiments:2,roundExperimentLimit:24,rejectedWindows:1,reference:{hashRateGHs:4000,powerW:80},best:{hashRateGHs:4050,powerW:79}};
    report.status.telemetry.valid=false;component.load();
    expect(component.search!.reference!.hashRateGHs).toBe(4000);expect(component.phaseProgress).toBe(100);
    expect(component.ths(component.search!.reference!.hashRateGHs,false)).toBe('4.00');
    expect(component.metric(55)).toBe('—');expect(component.canStart).toBeFalse();
    now+=13000;expect(component.search).toBeNull();
  });
  it('hides missing or malformed additive search fields without disabling original-backup Restore', () => {
    report.backup={available:true,restored:false,frequencyMHz:525,coreVoltageMv:1150};
    for(const search of [undefined,{},[],{phaseElapsedSeconds:0,minimumSeconds:75,samples:8,minimumSamples:8,roundExperiments:2,roundExperimentLimit:24,rejectedWindows:0,reference:{hashRateGHs:4000,powerW:0},best:null}]) {
      report.status.search=search as any;component.load();expect(component.search).toBeNull();expect(component.readError).toBe('');expect(component.canRestore).toBeTrue();
    }
    expect(service.command).not.toHaveBeenCalled();
  });
  it('bounds tab history, breaks failed/stale/invalid observations and clears it on a new run', () => {
    for(let i=0;i<160;i++){now+=1000;component.load();}
    expect(component.history.length).toBe(component.historyLimit);
    service.get.and.returnValue(throwError(()=>new Error('offline')));component.load();
    now+=1000;service.get.and.callFake(()=>of(structuredClone(report)));component.load();
    expect(component.trends[0].path.split('M').length-1).toBe(2);
    now+=13000;component.load();expect(component.trends[0].path.split('M').length-1).toBe(3);
    report.status.telemetry.valid=false;component.load();expect(component.history[component.history.length-1].ths).toBeNull();
    report.status.telemetry.valid=true;report.status.runId++;component.load();
    expect(component.history.length).toBe(1);expect(component.trends[0].points.length).toBe(1);expect(service.command).not.toHaveBeenCalled();
  });
  it('uses each distinct controller state for its live explanation without claiming an improvement', () => {
    report.status.ownsSettings=true;report.status.state='holding';component.load();expect(component.activityText).toContain('does not guarantee an improvement');
    report.status.state='restoring';component.load();expect(component.activityText).toContain('confirms restoration');
    report.status.ownsSettings=false;report.status.state='disabled';component.load();expect(component.activityText).toContain('explicit consent');
  });

  it('rejects overflowing derived energy without losing a legitimate zero-hash observation', () => {
    expect(component.energy(Number.MIN_VALUE,Number.MAX_VALUE,false)).toBe('—');
    report.status.telemetry.hashRateGHs=0;component.load();
    expect(component.ths(0)).toBe('0.00');expect(component.history[component.history.length-1].ths).toBe(0);
    expect(component.history[component.history.length-1].joules).toBeNull();
    report.status.telemetry.hashRateGHs=1000;report.status.telemetry.powerW=Number.MAX_VALUE;component.load();
    expect(component.trends.every(trend=>!trend.path.includes('NaN')&&!trend.path.includes('Infinity'))).toBeTrue();
  });
  it('does not invite another Start when an inactive controller still owns the held setting', () => {
    report.status.state='holding';report.status.ownsSettings=true;component.load();
    expect(component.ownershipText).toContain('Disable and restore');expect(component.canStart).toBeFalse();
    report.status.telemetry.fault=true;component.load();expect(component.activityText).toContain('active protection fault');
    report.status.telemetry.fault=false;report.status.reason='temperature-limit-backoff';component.load();expect(component.activityText).toContain('temperature ceiling');
    report.status.reason='error-recovery-voltage-trial';component.load();expect(component.activityText).toContain('headroom');
    report.status.reason='Manual or scheduled settings took ownership';component.load();expect(component.activityText).toContain('does not overwrite');
  });
  it('hides unpaired or nonpositive phase timing without changing the core report', () => {
    const search={phaseElapsedSeconds:0,minimumSeconds:75,samples:0,minimumSamples:8,roundExperiments:0,roundExperimentLimit:24,rejectedWindows:0,reference:null,best:null};
    for(const change of [{phaseElapsedSeconds:null},{minimumSeconds:null},{minimumSeconds:0}]) {
      report.status.search={...search,...change};component.load();expect(component.search).toBeNull();expect(component.readError).toBe('');
    }
  });

  it('makes active, monitoring, held ownership, protection and stale status unambiguous', () => {
    report.status.active=true;report.status.ownsSettings=true;report.status.state='measuring';component.load();expect(component.activityBanner.kind).toBe('active');
    report.status.state='observing';component.load();expect(component.activityBanner.kind).toBe('observing');
    report.status.active=false;report.status.state='holding';component.load();expect(component.activityBanner.kind).toBe('holding');expect(component.activityBanner.label).toContain('STILL OWNS');
    report.status.telemetry.fault=true;component.load();expect(component.activityBanner.kind).toBe('fault');
    now+=13000;expect(component.activityBanner.kind).toBe('unknown');
  });
  it('plots all five requested sensor panels and efficiency with board and worst errors on one scale', () => {
    report.status.telemetry.errorPercentage=.05;report.status.telemetry.worstErrorPercentage=.08;component.load();
    expect(component.trends.map(chart=>chart.name)).toEqual(['Useful hashrate','ASIC temperature','VRM temperature','Reported power','ASIC errors','Energy per useful hash']);
    now+=5000;Object.assign(report.status.telemetry,{asicTempC:56,vrmTempC:50,powerW:82,hashRateGHs:4050,errorPercentage:.06,worstErrorPercentage:.09});component.load();
    const row=component.history[component.history.length-1];expect(row.asicTempC).toBe(56);expect(row.vrmTempC).toBe(50);expect(row.powerW).toBe(82);expect(row.boardErrors).toBe(.06);expect(row.worstErrors).toBe(.09);
    expect(component.trends[4].lines.map(line=>line.name)).toEqual(['Board','Worst ASIC']);expect(component.trends[4].lines[0].path).not.toBe(component.trends[4].lines[1].path);
    expect(component.trends.every(chart=>chart.lines.every(line=>!line.path.includes('NaN')&&!line.path.includes('Infinity')))).toBeTrue();
  });
  it('breaks every sensor trace on invalid reads and resets each panel for a new run', () => {
    report.status.telemetry.valid=false;now+=5000;component.load();
    report.status.telemetry.valid=true;now+=5000;component.load();
    expect(component.trends.every(chart=>chart.lines.every(line=>line.path.split('M').length-1===2))).toBeTrue();
    report.status.runId++;component.load();expect(component.history.length).toBe(1);expect(component.trends.every(chart=>chart.lines.every(line=>line.points.length===1))).toBeTrue();
  });
  function event(id:number,change:any={}):any {return {id,atSeconds:100+id,kind:'adjustment',reason:'evaluating-nearby-operating-point',outcome:'requested',from:{frequencyMHz:525,coreVoltageMv:1150},point:{frequencyMHz:515,coreVoltageMv:1150},measurement:null,score:null,requiredScore:null,...change};}
  it('keeps real requested and confirmed outcomes separate and upserts the same controller event ID', () => {
    report.status.eventCapacity=8;report.status.events=[event(1,{kind:'started',reason:'measuring-starting-point',outcome:'measured',from:null}),event(2)];component.load();
    expect(component.events.length).toBe(2);expect(component.eventOutcome(component.events[1])).toContain('awaiting confirmation');
    report.status.events=[event(1,{kind:'started',reason:'measuring-starting-point',outcome:'measured',from:null}),event(2,{outcome:'applied'})];component.load();
    expect(component.events.length).toBe(2);expect(component.eventOutcome(component.events[1])).toContain('miner confirmed');
    report.status.events.push(event(3,{kind:'rejected',outcome:'measured',reason:'improvement-within-noise-margin',score:1.001,requiredScore:1.004,measurement:{hashRateGHs:4050,powerW:82,errorPercentage:.05,worstErrorPercentage:.08}}));component.load();
    expect(component.eventTitle(component.events[2])).toBe('Comparison rejected');expect(component.eventScore(component.events[2].score)).toBe('1.001000');expect(component.eventReason(component.events[2])).toContain('did not clear');
    expect(service.command).not.toHaveBeenCalled();
  });
  it('records missed controller IDs, keeps at most100 events and clears run-scoped history', () => {
    report.status.eventCapacity=8;report.status.events=[event(1)];component.load();report.status.events=[event(10),event(11)];component.load();
    expect(component.eventGapBefore(component.events.find(row=>row.id===10)!)).toBe(8);
    report.status.runId++;report.status.events=[];component.load();expect(component.events).toEqual([]);expect(component.eventGaps).toEqual([]);
    for(let id=1;id<=130;id++){report.status.events=[event(id)];component.load();}
    expect(component.events.length).toBe(100);expect(component.events[0].id).toBe(31);expect(component.visibleEvents.length).toBe(12);component.showEarlierEvents();expect(component.visibleEvents.length).toBe(100);
  });
  it('keeps Restore reachable and cached decisions historical when optional feed data is missing or malformed', () => {
    report.status.eventCapacity=8;report.status.events=[event(1)];report.backup={available:true,restored:false,frequencyMHz:525,coreVoltageMv:1150};component.load();
    report.status.events=[event(2,{score:NaN})];report.config=null as any;component.load();
    expect(component.readError).toBe('');expect(component.eventFeedAvailable).toBeFalse();expect(component.events.length).toBe(1);expect(component.eventFeedNote).toContain('earlier observations');expect(component.canRestore).toBeTrue();
    report.status.events=undefined;component.load();expect(component.canRestore).toBeTrue();expect(service.command).not.toHaveBeenCalled();
  });
  it('renders future event kinds/outcomes as reported data without fabricating an accepted decision', () => {
    report.status.eventCapacity=8;report.status.events=[event(1,{kind:'future-kind',outcome:'future-outcome',reason:'future-controller-reason',unboundedExtra:'x'.repeat(10000)})];component.load();
    expect(Object.keys(component.events[0])).not.toContain('unboundedExtra');expect(component.eventTitle(component.events[0])).toBe('Controller event');expect(component.eventOutcome(component.events[0])).toContain('future-outcome');expect(component.eventReason(component.events[0])).toContain('future controller reason');
    now+=13000;expect(component.eventFeedNote).toContain('earlier observations');
  });
  it('clears graphs and events when a reused run ID reports a new boot clock', () => {
    report.status.eventCapacity=8;report.status.events=[event(1,{atSeconds:100})];component.load();now+=5000;component.load();expect(component.history.length).toBeGreaterThan(1);
    report.status.events=[event(1,{atSeconds:5})];component.load();expect(component.events.length).toBe(1);expect(component.events[0].atSeconds).toBe(5);expect(component.history.length).toBe(1);
  });

  it('warns on saved and edited ceilings at exact user-defined recommendation boundaries without changing settings', () => {
    expect(component.savedTemperatureWarnings.map(row=>[row.sensor,row.severity])).toEqual([['ASIC','warning'],['VRM','danger']]);
    for(const [asic,vrm,expected] of [[60,70,[]],[65,79.99,['warning','warning']],[65.01,80,['danger','danger']]] as any[]) {
      component.edit={...report.config,maxAsicTempC:asic,maxVrmTempC:vrm};expect(component.editedTemperatureWarnings.map(row=>row.severity)).toEqual(expected);
    }
    expect(report.config.maxAsicTempC).toBe(65);expect(report.config.maxVrmTempC).toBe(85);expect(service.command).not.toHaveBeenCalled();
  });

  it('edits an existing active tune without another waiver or Start and confirms saved feedback', fakeAsync(() => {
    report.status.active=true;report.status.ownsSettings=true;report.status.state='measuring';report.backup={available:true,restored:false,frequencyMHz:525,coreVoltageMv:1150};component.load();
    component.edit!.priority='performance';expect(component.waiverAccepted).toBeFalse();expect(component.canSave).toBeTrue();expect(component.saveActionLabel).toBe('Save changes to this tune');expect(component.editExplanation).toContain('No new waiver or Start');
    service.command.and.callFake((body:any)=>{report.config=structuredClone(body.config);return of({ok:true,restartRequired:false});});component.save();tick(0);
    expect(service.command.calls.count()).toBe(1);expect(service.command.calls.mostRecent().args[0].action).toBe('configure');expect(component.message).toContain('Changes saved for this tune');expect(component.waiverAccepted).toBeFalse();
  }));
  it('requires owned clock and voltage bounds to include the reported point but permits lower thermal and power ceilings', () => {
    report.status.ownsSettings=true;report.status.active=true;report.status.state='measuring';report.status.current={frequencyMHz:500,coreVoltageMv:1100};component.load();
    component.edit!.maxFrequencyMHz=499;expect(component.validationError).toContain('current reported point');component.save();expect(service.command).not.toHaveBeenCalled();
    component.edit={...report.config,maxVoltageMv:1099};expect(component.validationError).toContain('current reported point');
    component.edit={...report.config,maxAsicTempC:50,maxVrmTempC:40,maxPowerW:70};expect(component.validationError).toBe('');expect(component.canSave).toBeTrue();
  });
  it('explains applying and restoring locks without removing the recovery action', () => {
    report.status.ownsSettings=true;report.backup={available:true,restored:false,frequencyMHz:525,coreVoltageMv:1150};
    for(const state of ['applying','restoring']){report.status.state=state;component.load();component.edit!.priority='efficiency';expect(component.optionsLocked).toBeTrue();expect(component.canSave).toBeFalse();expect(component.validationError).not.toBe('');expect(component.canRestore).toBeTrue();}
  });
  it('opens an already-owned tune on its live dashboard and keeps an unsaved draft through polling', () => {
    component.ngOnDestroy();report.status.active=true;report.status.ownsSettings=true;report.status.state='measuring';component=new AutotuneComponent(service as any);component.load();
    expect(component.viewMode).toBe('live');component.viewMode='options';component.edit!.priority='performance';component.load();expect(component.edit!.priority).toBe('performance');expect(component.viewMode).toBe('options');expect(component.hasUnsavedChanges).toBeTrue();
  });

  it('uses released ownership ahead of cached phases without relaxing Start safety checks', () => {
    report.status.active=false;report.status.ownsSettings=false;report.status.reason='manual-control';
    report.backup={available:true,restored:false,frequencyMHz:525,coreVoltageMv:1150};
    for(const state of ['applying','restoring','suspended','faulted','measuring']) {
      report.status.state=state;component.load();component.edit!.priority='efficiency';
      expect(component.activityBanner.kind).toBe('off');expect(component.stateLabel).toBe('Inactive');
      expect(component.ownershipText).toBe('Explicit Start required');expect(component.optionsLocked).toBeFalse();
      expect(component.canSave).toBeTrue();expect(component.canRestore).toBeTrue();
      expect(component.activityText).toContain('does not overwrite');
      if(['applying','restoring','faulted'].includes(state)){component.waiverAccepted=true;expect(component.canStart).toBeFalse();}
    }
    report.status.telemetry.fault=true;component.load();expect(component.activityBanner.kind).toBe('fault');
    expect(service.command).not.toHaveBeenCalled();
  });
  it('clears old boot observations even when the reused event IDs have been evicted', () => {
    report.status.runId=1;report.status.eventCapacity=8;
    for(let id=1;id<=130;id++){report.status.events=[event(id,{atSeconds:1000+id})];now+=1000;component.load();}
    expect(component.events.length).toBe(100);expect(component.events[0].id).toBe(31);
    report.status.events=Array.from({length:8},(_,i)=>event(i+1,{atSeconds:5+i}));component.load();
    expect(component.events.map(event=>event.id)).toEqual([1,2,3,4,5,6,7,8]);
    expect(component.history.length).toBe(1);expect(component.eventGapBefore(component.events[0])).toBe(0);
    expect(service.command).not.toHaveBeenCalled();
  });

  function pausedRecovery(): void {
    report.status.recovery={requested:false,phase:'idle',reason:''};
    report.backup={available:true,restored:false,frequencyMHz:525,coreVoltageMv:1150};
    report.status.telemetry.miningActive=false;component.load();
  }
  it('requires the recovery capability and a fresh explicitly paused report without breaking ordinary restore', () => {
    report.backup={available:true,restored:false,frequencyMHz:525,coreVoltageMv:1150};component.load();
    expect(component.canRestore).toBeTrue();expect(component.canRestoreAndResume).toBeFalse();expect(component.recoveryText).toContain('matching updated');
    pausedRecovery();expect(component.canRestoreAndResume).toBeTrue();
    report.status.telemetry.miningActive=true;component.load();expect(component.canRestoreAndResume).toBeFalse();
    report.status.telemetry.miningActive=false;(report.status as any).recovery={requested:false,phase:'future-phase',reason:''};component.load();expect(component.canRestoreAndResume).toBeFalse();expect(component.canRestore).toBeTrue();
    expect(service.command).not.toHaveBeenCalled();
  });
  it('shows original settings and requires explicit confirmation before requesting resume', () => {
    pausedRecovery();spyOn(window,'confirm').and.returnValue(false);component.restoreAndResume();
    expect(window.confirm).toHaveBeenCalledWith(jasmine.stringMatching(/525 MHz.*1150 mV.*auto-tuning off/));
    expect(service.authorize).not.toHaveBeenCalled();expect(service.command).not.toHaveBeenCalled();
  });
  it('sends recovery once and waits for original restore plus fresh normal mining, without starting tuning', fakeAsync(() => {
    pausedRecovery();spyOn(window,'confirm').and.returnValue(true);
    service.command.and.callFake(()=>{report.status.recovery={requested:true,phase:'restoring',reason:'restoring-original'};report.status.state='restoring';report.status.ownsSettings=true;return of({ok:true,restartRequired:false});});
    component.restoreAndResume();tick(0);expect(component.busy).toBeTrue();expect(service.command.calls.count()).toBe(1);
    expect(service.command.calls.mostRecent().args[0]).toEqual({action:'restore-and-resume'});
    report.status.recovery={requested:false,phase:'complete',reason:''};report.status.state='disabled';report.status.ownsSettings=false;report.backup.restored=true;
    tick(1000);expect(component.busy).toBeTrue();
    report.status.telemetry.miningActive=true;tick(1000);expect(component.busy).toBeFalse();expect(component.message).toContain('normal mining verified');
    expect(service.command.calls.count()).toBe(1);expect(component.waiverAccepted).toBeFalse();expect(report.status.active).toBeFalse();
  }));
  it('shows blocked recovery as terminal and never automatically retries it', fakeAsync(() => {
    pausedRecovery();spyOn(window,'confirm').and.returnValue(true);
    service.command.and.callFake(()=>{report.status.recovery={requested:false,phase:'blocked',reason:'cooling-not-ready'};return of({ok:true,restartRequired:false});});
    component.restoreAndResume();tick(0);expect(component.busy).toBeFalse();expect(component.actionError).toContain('cooling not ready');
    tick(10000);expect(service.command.calls.count()).toBe(1);expect(component.canRestore).toBeTrue();
  }));
  it('does not resend a recovery after a lost acknowledgement and requires fresh readback before another request', fakeAsync(() => {
    pausedRecovery();spyOn(window,'confirm').and.returnValue(true);service.command.and.returnValue(throwError(()=>new Error('connection lost')));
    component.restoreAndResume();tick(0);expect(component.canRestoreAndResume).toBeFalse();expect(component.actionError).toContain('No repeat request');
    component.restoreAndResume();expect(service.command.calls.count()).toBe(1);
    report.status.recovery={requested:true,phase:'validating-off',reason:''};component.load();expect(component.canRestoreAndResume).toBeFalse();
    expect(component.canRestore).toBeTrue();expect(service.command.calls.count()).toBe(1);
  }));
  it('explains a limit-constrained stop without claiming active ownership or a hardware defect', () => {
    report.status.reason='cannot-reduce-load-within-limits';report.status.state='faulted';report.status.active=false;report.status.ownsSettings=false;report.config.minFrequencyMHz=690;report.config.maxFrequencyMHz=800;component.load();
    expect(component.activityBanner.label).toBe('STOPPED — LIMIT REACHED');expect(component.activityText).toContain('690 MHz');expect(component.activityText).toContain('not a diagnosis');
    expect(component.ownedSettingsText).toContain('released');expect(component.optionsLocked).toBeFalse();
    report.status.telemetry.fault=true;component.load();expect(component.activityBanner.kind).toBe('fault');
  });
  it('retains instantaneous rejection evidence separately from complete measurement windows and drops malformed extras', () => {
    const conditions={asicTempC:60.5,vrmTempC:58,powerW:86,hashRateGHs:4100,errorPercentage:.1,worstErrorPercentage:.2,extra:'discard'};
    report.status.eventCapacity=8;report.status.events=[event(1,{kind:'rejected',outcome:'failed',reason:'temperature-limit-backoff',measurement:null,conditions})];component.load();
    expect(component.eventTitle(component.events[0])).toBe('Point rejected by limits');expect(component.eventReason(component.events[0])).toContain('not marked as permanently defective');
    expect(component.events[0].measurement).toBeNull();expect(component.events[0].conditions!.asicTempC).toBe(60.5);expect((component.events[0].conditions as any).extra).toBeUndefined();
    conditions.asicTempC=99;expect(component.events[0].conditions!.asicTempC).toBe(60.5);
    report.status.events=[event(2,{conditions:{...conditions,powerW:Infinity}})];component.load();expect(component.eventFeedAvailable).toBeTrue();expect(component.events[1].conditions).toBeUndefined();
    report.status.events=[event(3)];component.load();expect(component.eventFeedAvailable).toBeTrue();expect(component.events[2].conditions).toBeUndefined();expect(component.canRestoreAndResume).toBeFalse();
  });

  it('lets explicit resume supersede only the acknowledged readback wait of an ordinary paused restore', fakeAsync(() => {
    pausedRecovery();spyOn(window,'confirm').and.returnValue(true);
    service.command.and.callFake((body:any)=>{
      report.status.state='restoring';report.status.ownsSettings=true;
      if(body.action==='restore-and-resume')report.status.recovery={requested:true,phase:'restoring',reason:''};
      return of({ok:true,restartRequired:false});
    });
    component.restore();tick(0);expect(component.busy).toBeTrue();expect(component.canRestoreAndResume).toBeTrue();
    component.restoreAndResume();tick(0);expect(component.busy).toBeTrue();expect(component.canRestoreAndResume).toBeFalse();
    expect(service.command.calls.allArgs().map(args=>args[0].action)).toEqual(['stop','restore-and-resume']);
    report.status.recovery={requested:false,phase:'complete',reason:''};report.status.state='disabled';report.status.ownsSettings=false;report.backup.restored=true;report.status.telemetry.miningActive=true;
    tick(1000);expect(component.busy).toBeFalse();expect(component.message).toContain('normal mining verified');expect(service.command.calls.count()).toBe(2);
  }));

});
