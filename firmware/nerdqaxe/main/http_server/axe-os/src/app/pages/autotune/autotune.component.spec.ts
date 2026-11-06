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
    report.status.current={frequencyMHz:500,coreVoltageMv:1100};component.load();component.restore();tick(195001);
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
    component.restoreAndResume();tick(0);expect(component.busy).toBeFalse();expect(component.actionError).toContain('cooling-not-ready');
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

  it('shows automatic cooling and recovery while tuning itself remains off', () => {
    report.status.active=false;report.status.ownsSettings=true;report.status.state='restoring';
    report.status.recovery={requested:true,phase:'validating-off',reason:'Automatic soft-limit recovery waiting for cooling'};component.load();
    expect(component.activityBanner.label).toBe('RECOVERING MINING · AUTO-TUNING OFF');expect(component.activityBanner.detail).toContain('waiting for cooling');
    expect(component.canRestoreAndResume).toBeFalse();expect(report.status.active).toBeFalse();expect(service.command).not.toHaveBeenCalled();
  });

  it('labels early error rejection from instantaneous evidence without inventing a full-window result or a clock-floor failure', () => {
    for (const count of [1,4,8]) {
      report=fixture(count);report.status.active=true;report.status.ownsSettings=true;report.status.state='settling';report.status.eventCapacity=8;
      for (const [index,reason] of ['early-error-limit','early-severe-error-limit'].entries()) {
        report.status.reason=reason;
        report.status.events=[event(index+1,{kind:'rejected',outcome:'failed',reason,measurement:null,conditions:{asicTempC:55,vrmTempC:48,powerW:80,hashRateGHs:4000,errorPercentage:3,worstErrorPercentage:5}})];
        component.load();const decision=component.events.find(value=>value.reason===reason)!;
        expect(component.activityText).toContain('Rejected this operating point early');
        expect(component.eventTitle(decision)).toBe('Point rejected early');
        expect(component.eventReason(decision)).toContain('full comparison window was not completed');
        expect(component.eventReason(decision)).not.toContain('could not reduce load further');
        expect(decision.measurement).toBeNull();expect(decision.score).toBeNull();expect(decision.requiredScore).toBeNull();
        expect(decision.conditions!.worstErrorPercentage).toBe(5);
      }
    }
    expect(service.command).not.toHaveBeenCalled();expect(service.authorize).not.toHaveBeenCalled();
    now+=13000;expect(component.activityText).toContain('Waiting for a fresh report');
  });
  it('explains a full error-exclusion history and exhausted correction without claiming a hardware fault or sending recovery', () => {
    report.status.state='faulted';report.status.reason='error-recovery-exhausted';report.status.active=false;report.status.ownsSettings=false;
    report.backup={available:true,restored:false,frequencyMHz:525,coreVoltageMv:1150};
    report.status.eventCapacity=8;report.status.events=[event(1,{kind:'rejected',outcome:'failed',reason:'error-point-exclusion-full',measurement:null})];component.load();
    const decision=component.events[0];expect(component.eventTitle(decision)).toBe('Error-rejection history full');
    expect(component.eventReason(decision)).toContain('No further experiments');expect(component.eventReason(decision)).not.toContain('could not reduce load further');
    expect(component.activityText).toContain('Bounded error correction has been exhausted');expect(component.activityText).toContain('can block it');
    expect(component.canRestore).toBeTrue();expect(report.status.telemetry.fault).toBeFalse();expect(service.command).not.toHaveBeenCalled();
    report.status.telemetry.fault=true;component.load();expect(component.activityText).toContain('active protection fault');
  });
  it('identifies completed-window, held-point and exhausted error rejections before the generic clock-floor explanation', () => {
    const mean={hashRateGHs:4000,powerW:80,errorPercentage:.7,worstErrorPercentage:1.2};
    const cases=[
      {reason:'error-window-limit',title:'Point rejected by measured errors',text:'completed measurement window',measurement:mean},
      {reason:'held-point-became-unstable',title:'Held point rejected by errors',text:'immediately reduces clock',measurement:null},
      {reason:'error-recovery-exhausted',title:'Error correction exhausted',text:'Bounded error correction has been exhausted',measurement:mean}
    ];
    report.status.active=true;report.status.ownsSettings=true;report.status.state='measuring';report.status.eventCapacity=8;
    for (const [index,value] of cases.entries()) {
      report.status.reason=value.reason;report.status.events=[event(index+1,{kind:'rejected',outcome:'failed',reason:value.reason,measurement:value.measurement})];component.load();
      const decision=component.events.find(entry=>entry.reason===value.reason)!;
      expect(component.eventTitle(decision)).toBe(value.title);expect(component.eventReason(decision)).toContain(value.text);
      expect(component.eventReason(decision)).not.toContain('could not reduce load further');expect(component.activityText).toContain(value.text);
      expect(decision.measurement).toEqual(value.measurement);expect(decision.score).toBeNull();expect(decision.requiredScore).toBeNull();
    }
    expect(service.command).not.toHaveBeenCalled();expect(service.authorize).not.toHaveBeenCalled();
  });
  it('distinguishes a requested return to the measured stable point from original-backup restoration', () => {
    report.status.state='applying';report.status.reason='restoring-last-stable-point';report.status.active=true;report.status.ownsSettings=true;
    report.backup={available:true,restored:false,frequencyMHz:525,coreVoltageMv:1150};
    report.status.eventCapacity=8;report.status.events=[event(1,{reason:'restoring-last-stable-point',point:{frequencyMHz:515,coreVoltageMv:1150}})];component.load();
    const decision=component.events[0];expect(component.activityText).toContain('measured stable point');expect(component.activityText).toContain('separate from restoring the original');
    expect(component.eventOutcome(decision)).toContain('awaiting confirmation');expect(component.eventReason(decision)).toContain('temperature and power headroom');
    expect(component.report!.backup.restored).toBeFalse();expect(component.report!.status.current!.frequencyMHz).toBe(525);
    expect(service.command).not.toHaveBeenCalled();expect(service.authorize).not.toHaveBeenCalled();
  });

  function advancedFixture(count=4,standard=800,enabled=false): void {
    component.ngOnDestroy();report=fixture(count);report.config.advancedClockOverride=enabled;
    report.limits.maxFrequencyMHz=standard;report.limits.advancedMaxFrequencyMHz=1200;
    component=new AutotuneComponent(service as any);component.load();
  }
  it('offers only the reported advanced range and keeps warning acceptance local across Gamma, QAxe, rev7 and OctAxe', () => {
    for (const [count,standard] of [[1,690],[4,800],[4,1000],[8,800]]) {
      advancedFixture(count,standard);
      expect(component.advancedClockAvailable).toBeTrue();expect(component.advancedClockWarningAccepted).toBeFalse();
      expect(component.maximumClockMHz).toBe(standard);expect(component.report!.config.advancedClockOverride).toBeFalse();
      component.setAdvancedClockAcknowledgement(true);
      expect(component.maximumClockMHz).toBe(1200);expect(component.edit!.advancedClockOverride).toBeTrue();
      expect(component.report!.config.advancedClockOverride).toBeFalse();expect(component.hasUnsavedChanges).toBeTrue();
      expect(component.waiverAccepted).toBeFalse();expect(component.canStart).toBeFalse();
    }
    expect(service.command).not.toHaveBeenCalled();expect(service.authorize).not.toHaveBeenCalled();
  });
  it('saves the advanced permission with explicit warning fields and confirms it before a separately consented Start', fakeAsync(() => {
    advancedFixture();component.setAdvancedClockAcknowledgement(true);component.edit!.maxFrequencyMHz=1000;
    service.command.and.callFake((body:any)=>{report.config=structuredClone(body.config);return of({ok:true,restartRequired:false});});
    component.save();tick(0);
    const saved=service.command.calls.mostRecent().args[0];
    expect(Object.keys(saved).sort()).toEqual(['action','advancedClockWarningAccepted','advancedClockWarningVersion','config']);
    expect(saved.advancedClockWarningAccepted).toBeTrue();expect(saved.advancedClockWarningVersion).toBe(1);
    expect(Object.keys(saved.config).length).toBe(10);expect(saved.config.advancedClockOverride).toBeTrue();
    expect(component.report!.config.maxFrequencyMHz).toBe(1000);expect(component.hasUnsavedChanges).toBeFalse();
    expect(component.advancedClockWarningAccepted).toBeTrue();expect(component.canStart).toBeFalse();expect(report.status.active).toBeFalse();
    acceptedStart();component.waiverAccepted=true;component.start();tick(0);
    expect(service.command.calls.mostRecent().args[0]).toEqual({action:'start',waiverAccepted:true,waiverVersion:1,advancedClockAccepted:true});
    expect(component.waiverAccepted).toBeFalse();expect(component.advancedClockWarningAccepted).toBeFalse();
    expect(service.command.calls.count()).toBe(2);
  }));
  it('does not infer fresh advanced consent from an already enabled saved mode or a higher original point', () => {
    advancedFixture(1,690,true);report.config.maxFrequencyMHz=1000;report.status.current={frequencyMHz:750,coreVoltageMv:1150};
    report.backup={available:true,restored:false,frequencyMHz:750,coreVoltageMv:1150};component.load();
    component.waiverAccepted=true;component.start();expect(service.command).not.toHaveBeenCalled();
    expect(component.advancedClockWarningAccepted).toBeFalse();expect(component.validationError).toContain('explicitly accept');
    component.setAdvancedClockAcknowledgement(true);expect(component.waiverAccepted).toBeFalse();expect(component.canStart).toBeFalse();
    component.waiverAccepted=true;expect(component.canStart).toBeTrue();expect(component.canRestore).toBeTrue();
    report.limits.advancedMaxFrequencyMHz=1100;component.load();
    expect(component.advancedClockWarningAccepted).toBeFalse();expect(component.canStart).toBeFalse();
  });
  it('uses the canonical false flag without advanced warning fields and retains the normal Start contract', fakeAsync(() => {
    advancedFixture();component.edit!.priority='performance';
    service.command.and.callFake((body:any)=>{report.config=structuredClone(body.config);return of({ok:true,restartRequired:false});});
    component.save();tick(0);const saved=service.command.calls.mostRecent().args[0];
    expect(Object.keys(saved).sort()).toEqual(['action','config']);expect(saved.config.advancedClockOverride).toBeFalse();
    expect(Object.keys(saved.config).length).toBe(10);
    acceptedStart();component.waiverAccepted=true;component.start();tick(0);
    expect(service.command.calls.mostRecent().args[0]).toEqual({action:'start',waiverAccepted:true,waiverVersion:1});
  }));
  it('keeps legacy false edits in the nine-key payload and never unlocks an advanced range from a model name or current point', fakeAsync(() => {
    component.edit!.advancedClockOverride=false;component.edit!.priority='performance';
    service.command.and.callFake((body:any)=>{report.config=structuredClone(body.config);return of({ok:true,restartRequired:false});});
    component.save();tick(0);const saved=service.command.calls.mostRecent().args[0];
    expect(Object.keys(saved.config).length).toBe(9);expect(saved.config.advancedClockOverride).toBeUndefined();
    expect(component.advancedClockAvailable).toBeFalse();expect(component.maximumClockMHz).toBe(report.limits.maxFrequencyMHz);
    component.setAdvancedClockAcknowledgement(true);expect(component.advancedClockWarningAccepted).toBeFalse();
    report.status.current={frequencyMHz:1100,coreVoltageMv:1150};report.backup={available:true,restored:false,frequencyMHz:1100,coreVoltageMv:1150};component.load();
    component.waiverAccepted=true;expect(component.canStart).toBeFalse();expect(component.canRestore).toBeTrue();
  }));
  it('fails closed on partial or malformed advanced metadata while preserving an original backup above the standard cap', () => {
    for (const change of [
      (value:AutotuneReport)=>delete value.limits.advancedMaxFrequencyMHz,
      (value:AutotuneReport)=>delete value.config.advancedClockOverride,
      (value:AutotuneReport)=>value.limits.advancedMaxFrequencyMHz=NaN,
      (value:AutotuneReport)=>value.limits.advancedMaxFrequencyMHz=Infinity,
      (value:AutotuneReport)=>value.limits.advancedMaxFrequencyMHz=799,
      (value:AutotuneReport)=>(value.config as any).advancedClockOverride='true'
    ]) {
      advancedFixture();change(report);report.backup={available:true,restored:false,frequencyMHz:1100,coreVoltageMv:1150};component.load();
      component.waiverAccepted=true;expect(component.advancedClockAvailable).toBeFalse();expect(component.optionsUsable).toBeFalse();
      expect(component.canSave).toBeFalse();expect(component.canStart).toBeFalse();expect(component.canRestore).toBeTrue();
      component.setAdvancedClockAcknowledgement(true);expect(service.command).not.toHaveBeenCalled();
    }
  });
  it('retains the experimental boundary and all independent electrical, thermal and actuator-unit limits', () => {
    advancedFixture();component.setAdvancedClockAcknowledgement(true);component.edit!.maxFrequencyMHz=1201;
    expect(component.validationError).toContain('experimental project maximum');component.edit!.maxFrequencyMHz=1200;
    expect(component.validationError).toBe('');
    const valid=structuredClone(component.edit!);
    for (const [field,value] of [['maxAsicTempC',71],['maxVrmTempC',96],['maxPowerW',301],['maxVoltageMv',1500]] as const) {
      component.edit={...valid,[field]:value};expect(component.validationError).not.toBe('');component.save();
    }
    component.edit={...valid,maxFrequencyMHz:900.5};
    if(component.wholeMHz)expect(component.validationError).toContain('whole MHz');else expect(component.validationError).toBe('');
    expect(service.command).not.toHaveBeenCalled();
  });
  it('requires an owned high point to be lowered or restored before disabling the advanced permission', fakeAsync(() => {
    advancedFixture(4,800,true);report.config.maxFrequencyMHz=1000;report.status.current={frequencyMHz:900,coreVoltageMv:1150};
    report.status.active=true;report.status.ownsSettings=true;report.status.state='measuring';report.backup={available:true,restored:false,frequencyMHz:1100,coreVoltageMv:1150};component.load();
    component.setAdvancedClockAcknowledgement(false);expect(component.edit!.advancedClockOverride).toBeFalse();
    expect(component.validationError).toContain('disable the override');component.edit!.maxFrequencyMHz=800;
    expect(component.validationError).toContain('include the current');expect(component.canSave).toBeFalse();expect(component.canRestore).toBeTrue();
    expect(service.command).not.toHaveBeenCalled();report.status.current.frequencyMHz=700;component.load();
    expect(component.validationError).toBe('');
    service.command.and.callFake((body:any)=>{report.config=structuredClone(body.config);return of({ok:true,restartRequired:false});});
    component.save();tick(0);const saved=service.command.calls.mostRecent().args[0];
    expect(saved.config.advancedClockOverride).toBeFalse();expect(saved.advancedClockWarningAccepted).toBeUndefined();
    expect(component.report!.backup.frequencyMHz).toBe(1100);
  }));
  it('allows idle override disable without touching a higher manual point and blocks only the later Start', fakeAsync(() => {
    advancedFixture(1,690,true);report.config.maxFrequencyMHz=1000;report.status.current={frequencyMHz:750,coreVoltageMv:1150};
    report.backup={available:true,restored:false,frequencyMHz:750,coreVoltageMv:1150};component.load();
    component.setAdvancedClockAcknowledgement(false);component.edit!.maxFrequencyMHz=690;
    expect(component.validationError).toBe('');expect(component.canSave).toBeTrue();
    service.command.and.callFake((body:any)=>{report.config=structuredClone(body.config);return of({ok:true,restartRequired:false});});
    component.save();tick(0);component.waiverAccepted=true;
    expect(component.report!.status.current!.frequencyMHz).toBe(750);expect(component.report!.backup.frequencyMHz).toBe(750);
    expect(component.canStart).toBeFalse();expect(component.startExplanation).toContain('enable the advanced override');
    expect(component.canRestore).toBeTrue();expect(service.command.calls.count()).toBe(1);
  }));
  it('does not confirm a saved advanced mode when readback drops its flag and never retries the mutation', fakeAsync(() => {
    advancedFixture();report.backup={available:true,restored:false,frequencyMHz:1100,coreVoltageMv:1150};component.load();
    component.setAdvancedClockAcknowledgement(true);component.edit!.maxFrequencyMHz=1000;
    component.save();tick(30001);
    expect(service.command.calls.count()).toBe(1);expect(component.message).toBe('');expect(component.actionError).toContain('could not be confirmed');
    expect(component.advancedClockWarningAccepted).toBeFalse();expect(component.canRestore).toBeTrue();
  }));
  it('explains backend advanced-clock rejection without retrying or preserving an accepted warning', fakeAsync(() => {
    advancedFixture();component.setAdvancedClockAcknowledgement(true);component.edit!.maxFrequencyMHz=1000;
    service.command.and.returnValue(throwError(()=>({error:{error:'advanced-clock-warning-required'}})));
    component.save();tick(0);expect(component.actionError).toContain('explicit advanced clock warning');
    expect(component.actionError).toContain('No repeat request');expect(component.advancedClockWarningAccepted).toBeFalse();
    tick(10000);expect(service.command.calls.count()).toBe(1);
  }));

  it('clears an earlier save confirmation when 900 is edited and saves that exact ceiling without the run waiver', fakeAsync(() => {
    advancedFixture(8,800);component.message='Tuning options saved: maximum clock 800 MHz.';
    component.edit!.maxFrequencyMHz=900;component.onOptionsEdit();
    expect(component.message).toBe('');expect(component.clockSaveProblem).toContain('900 MHz is not saved');
    expect(component.clockSaveProblem).toContain('run waiver is only for Start');expect(component.canSave).toBeFalse();component.save();
    expect(service.command).not.toHaveBeenCalled();component.setAdvancedClockAcknowledgement(true);
    expect(component.waiverAccepted).toBeFalse();expect(component.canSave).toBeTrue();
    service.command.and.callFake((body:any)=>{report.config=structuredClone(body.config);return of({ok:true,restartRequired:false});});
    component.save();tick(0);expect(service.command.calls.count()).toBe(1);
    expect(service.command.calls.mostRecent().args[0].config.maxFrequencyMHz).toBe(900);
    expect(component.report!.config.maxFrequencyMHz).toBe(900);expect(component.message).toContain('900 MHz');
    expect(component.message).toContain('has not started');expect(component.canStart).toBeFalse();expect(report.status.active).toBeFalse();
    acceptedStart();component.waiverAccepted=true;component.start();tick(0);expect(service.command.calls.count()).toBe(2);
  }));
  it('invalidates old success and run consent on any options edit without changing advanced consent or writing', () => {
    advancedFixture();component.setAdvancedClockAcknowledgement(true);component.message='Old success';component.actionError='Old error';component.waiverAccepted=true;
    component.edit!.priority='performance';component.onOptionsEdit();
    expect(component.message).toBe('');expect(component.actionError).toBe('');expect(component.waiverAccepted).toBeFalse();
    expect(component.advancedClockWarningAccepted).toBeTrue();expect(service.command).not.toHaveBeenCalled();
  });
  it('saves an active 900 MHz search ceiling without a new run or run waiver', fakeAsync(() => {
    advancedFixture(4,800);report.status.active=true;report.status.ownsSettings=true;report.status.state='measuring';report.status.runId=7;
    report.config.maxFrequencyMHz=800;report.status.current={frequencyMHz:750,coreVoltageMv:1150};component.load();
    component.edit!.maxFrequencyMHz=900;component.onOptionsEdit();component.setAdvancedClockAcknowledgement(true);
    service.command.and.callFake((body:any)=>{report.config=structuredClone(body.config);return of({ok:true,restartRequired:false});});
    component.save();tick(0);expect(service.command.calls.count()).toBe(1);expect(service.command.calls.mostRecent().args[0].action).toBe('configure');
    expect(component.report!.status.runId).toBe(7);expect(component.waiverAccepted).toBeFalse();expect(component.message).toContain('900 MHz');
    expect(component.canStart).toBeFalse();
  }));

  it('identifies the failed ASIC readiness check instead of suggesting another waiver', () => {
    report.status.telemetry.valid=false;
    report.status.readiness=[{key:'counters-3',label:'ASIC 3 counters',ready:false,detail:'ASIC 3 has no fresh error-counter interval.'}];
    component.load();component.waiverAccepted=true;
    expect(component.canStart).toBeFalse();
    expect(component.startExplanation).toContain('ASIC 3');
    expect(component.visibleStartChecks.some(row=>row.key==='counters-3' && !row.ready)).toBeTrue();
    expect(component.blockedStartChecks.some(row=>row.key==='waiver')).toBeFalse();
    component.start();expect(service.command).not.toHaveBeenCalled();
  });
  it('explains legacy cooling and mining gates without claiming individual sensors passed', () => {
    report.status.telemetry.fanOk=false;report.status.telemetry.miningActive=false;
    component.load();component.waiverAccepted=true;
    expect(component.startExplanation).toContain('Cooling feedback');
    expect(component.blockedStartChecks.map(row=>row.key)).toContain('mining');
    expect(component.canStart).toBeFalse();expect(service.command).not.toHaveBeenCalled();
  });
  it('does not treat stale positive readiness diagnostics as current healthy readings', () => {
    report.status.readiness=[{key:'fan-1',label:'Fan 1',ready:true,detail:'1500 RPM'}];component.load();component.waiverAccepted=true;
    now+=13000;
    expect(component.startChecks.find(row=>row.key==='fan-1')?.ready).toBeFalse();
    expect(component.startChecks.find(row=>row.key==='fan-1')?.detail).toContain('No current reading');
    expect(component.canStart).toBeFalse();
  });
  it('falls back safely when optional readiness diagnostics are malformed', () => {
    report.status.readiness=[{key:'fan-1',ready:'true'}] as any;report.status.telemetry.fanOk=false;
    component.load();component.waiverAccepted=true;
    expect(component.startExplanation).toContain('Cooling feedback');expect(component.canStart).toBeFalse();
  });
  it('accepts 65 C within the reported ASIC limit and explains limits above it', () => {
    report.config.maxAsicTempC=65;component.load();component.waiverAccepted=true;
    expect(component.validationError).toBe('');expect(component.canStart).toBeTrue();
    component.edit!.maxAsicTempC=report.limits.maxAsicTempC+1;
    expect(component.validationError).toContain('Temperature ceilings');expect(component.canStart).toBeFalse();
  });
  it('exports useful diagnostics without issuing a miner command', async () => {
    report.status.readiness=[{key:'fan-2',label:'Fan 2',ready:false,detail:'No fan RPM reading.'}];component.load();
    await component.copyDiagnostics();
    const exported=JSON.parse(component.diagnosticsText);
    expect(exported.report.status.readiness[0].key).toBe('fan-2');
    expect(exported.startChecks.some((row:any)=>row.key==='fan-2')).toBeTrue();
    expect(component.diagnosticsMessage).toContain('diagnostics');expect(service.command).not.toHaveBeenCalled();
  });

  it('retains mining and fault blockers when detailed readiness is only partial', () => {
    report.status.readiness=[{key:'fan-1',label:'Fan 1',ready:true,detail:'1500 RPM'}];
    report.status.telemetry.miningActive=false;report.status.telemetry.fault=true;
    component.load();component.waiverAccepted=true;
    expect(component.blockedStartChecks.map(row=>row.key)).toContain('mining');
    expect(component.blockedStartChecks.map(row=>row.key)).toContain('protection');
    expect(component.canStart).toBeFalse();
  });
  it('conservatively blocks contradictory failed diagnostics without bypassing aggregate guards', () => {
    report.status.readiness=[{key:'fan-2',label:'Fan 2',ready:false,detail:'Fan 2 has no valid RPM reading.'}];
    component.load();component.waiverAccepted=true;
    expect(component.canStart).toBeFalse();expect(component.startExplanation).toContain('Fan 2');
    component.start();expect(service.command).not.toHaveBeenCalled();
  });
  it('shows a pending request as a blocker while refreshing status', () => {
    const waiting=new Subject<AutotuneReport>();service.get.and.returnValue(waiting);
    component.waiverAccepted=true;component.load();
    expect(component.canStart).toBeFalse();
    expect(component.blockedStartChecks.some(row=>row.key==='request')).toBeTrue();
    waiting.next(report);waiting.complete();expect(component.canStart).toBeTrue();
  });

  function coolingFixture(valid=true): void {
    report.cooling={supportedMask:3,requiredMask:3,warningVersion:1,valid};component.load();
  }
  function acceptCoolingSave(): void {
    service.command.and.callFake((body:any)=>{
      if(body.action==='configure-cooling')report.cooling={...report.cooling!,requiredMask:body.requiredFanMask,valid:true};
      return of({ok:true,restartRequired:false});
    });
  }
  it('preserves legacy behavior and does not offer unsupported cooling writes', () => {
    expect(component.hasCoolingMetadata).toBeFalse();component.waiverAccepted=true;
    expect(component.coolingReady).toBeTrue();expect(component.canStart).toBeTrue();
    component.saveCooling();expect(service.command).not.toHaveBeenCalled();
  });
  it('shows both fitted fans by default and requires a separate acknowledgement to change them', () => {
    coolingFixture();expect(component.coolingDraftMask).toBe(3);expect(component.coolingSelectionText(3)).toBe('Both fans');
    expect(component.coolingChoices.map(c=>c.mask)).toEqual([3,1,2]);
    component.waiverAccepted=true;component.selectCooling(1);
    expect(component.waiverAccepted).toBeFalse();expect(component.canSaveCooling).toBeFalse();
    expect(component.canStart).toBeFalse();expect(component.startExplanation).toContain('cooling setup');
    component.saveCooling();expect(service.command).not.toHaveBeenCalled();
  });
  it('saves single-fan monitoring independently before the run waiver and does not start tuning', fakeAsync(() => {
    coolingFixture();acceptCoolingSave();component.selectCooling(1);component.coolingWarningAccepted=true;
    expect(component.canSaveCooling).toBeTrue();expect(component.waiverAccepted).toBeFalse();component.saveCooling();tick(0);
    expect(service.command.calls.count()).toBe(1);
    expect(service.command.calls.mostRecent().args).toEqual([{action:'configure-cooling',requiredFanMask:1,coolingWarningAccepted:true,coolingWarningVersion:1},'123456']);
    expect(component.report!.cooling!.requiredMask).toBe(1);expect(component.coolingMessage).toContain('confirmed');
    expect(component.coolingWarningAccepted).toBeFalse();expect(component.waiverAccepted).toBeFalse();
    expect(report.status.active).toBeFalse();expect(report.status.runId).toBe(0);expect(component.canStart).toBeFalse();
    component.waiverAccepted=true;expect(component.canStart).toBeTrue();
  }));
  it('preserves unsaved operating options and the original backup when saving cooling', fakeAsync(() => {
    coolingFixture();acceptCoolingSave();const before=structuredClone(report.config),backup=structuredClone(report.backup);
    component.edit!.maxVoltageMv=1140;component.selectCooling(2);component.coolingWarningAccepted=true;component.saveCooling();tick(0);
    expect(component.edit!.maxVoltageMv).toBe(1140);expect(component.hasUnsavedChanges).toBeTrue();
    expect(report.config).toEqual(before);expect(report.backup).toEqual(backup);expect(service.command.calls.count()).toBe(1);
  }));
  it('allows a corrupt stored cooling selection to be explicitly repaired without changing operating limits', fakeAsync(() => {
    coolingFixture(false);acceptCoolingSave();expect(component.coolingReady).toBeFalse();
    component.waiverAccepted=true;expect(component.canStart).toBeFalse();expect(component.startExplanation).toContain('needs repair');
    component.coolingWarningAccepted=true;expect(component.canSaveCooling).toBeTrue();component.saveCooling();tick(0);
    expect(component.report!.cooling!.valid).toBeTrue();expect(report.cooling!.requiredMask).toBe(3);expect(report.config.maxFrequencyMHz).toBe(525);
  }));
  it('never changes fitted-fan requirements while a tune or recovery owns the miner', () => {
    coolingFixture();component.selectCooling(1);component.coolingWarningAccepted=true;
    for(const state of ['baseline','measuring','holding','restoring']){
      report.status.state=state;report.status.ownsSettings=true;component.load();
      expect(component.coolingLocked).toBeTrue();expect(component.canSaveCooling).toBeFalse();
      component.selectCooling(2);expect(component.coolingDraftMask).toBe(1);component.saveCooling();
    }
    expect(service.command).not.toHaveBeenCalled();
  });
  it('keeps each rendered fitted-fan choice stable across change detection', () => {
    coolingFixture();const first=component.coolingChoices;
    for(let cycle=0;cycle<10;++cycle) component.coolingChoices.forEach((choice,index)=>expect(choice).toBe(first[index]));
    report.cooling!.supportedMask=1;report.cooling!.requiredMask=1;component.load();
    expect(component.coolingChoices.length).toBe(1);expect(component.coolingChoices[0]).toBe(first[1]);
  });
  it('keeps failed cooling readback blocked until a fresh matching report is received', fakeAsync(() => {
    coolingFixture();component.selectCooling(1);component.coolingWarningAccepted=true;component.saveCooling();tick(30001);
    expect(component.coolingError).toContain('could not be confirmed');expect(component.coolingReady).toBeFalse();
    component.waiverAccepted=true;expect(component.canStart).toBeFalse();expect(service.command.calls.count()).toBe(1);
    report.cooling!.requiredMask=1;now+=100;component.load();
    expect(component.coolingMessage).toContain('confirmed');expect(component.coolingReady).toBeTrue();
    expect(component.waiverAccepted).toBeFalse();expect(service.command.calls.count()).toBe(1);
  }));
  it('refuses stale or malformed cooling metadata without silently treating it as the legacy interface', () => {
    coolingFixture();component.selectCooling(1);component.coolingWarningAccepted=true;now+=13000;
    expect(component.canSaveCooling).toBeFalse();component.saveCooling();expect(service.command).not.toHaveBeenCalled();
    now=1000;
    for(const value of [{supportedMask:3,requiredMask:4,warningVersion:1,valid:true},{supportedMask:3,requiredMask:4294967297,warningVersion:1,valid:true},{supportedMask:3,requiredMask:1,warningVersion:2,valid:true},undefined]){
      report.cooling=value as any;component.load();component.waiverAccepted=true;
      expect(component.coolingReady).toBeFalse();expect(component.canStart).toBeFalse();
    }
  });
  it('resets cooling acknowledgement on edits and run acknowledgement on external cooling changes', () => {
    coolingFixture();component.selectCooling(1);component.coolingWarningAccepted=true;component.selectCooling(2);
    expect(component.coolingWarningAccepted).toBeFalse();component.useSavedCooling();expect(component.coolingDraftMask).toBe(3);
    component.waiverAccepted=true;component.coolingMessage='Previous saved choice';component.coolingError='Previous problem';report.cooling!.requiredMask=1;component.load();
    expect(component.coolingDraftMask).toBe(1);expect(component.waiverAccepted).toBeFalse();
    expect(component.coolingMessage).toBe('');expect(component.coolingError).toBe('');
  });

  it('can discard a cancelled cooling repair request without enabling an invalid setup', () => {
    coolingFixture(false);service.authorize.and.returnValue(EMPTY);
    component.coolingWarningAccepted=true;component.saveCooling();
    expect(service.command).not.toHaveBeenCalled();expect(component.coolingSaveUnconfirmed).toBeFalse();
    component.useSavedCooling();expect(component.coolingSaveUnconfirmed).toBeFalse();
    expect(component.coolingReady).toBeFalse();expect(component.canStart).toBeFalse();
  });
  it('confirms a cooling declaration without implying an existing hardware fault has cleared', fakeAsync(() => {
    coolingFixture(false);report.status.state='faulted';report.status.telemetry.fault=true;component.load();
    acceptCoolingSave();component.coolingWarningAccepted=true;component.saveCooling();tick(0);
    expect(component.coolingMessage).toContain('confirmed');expect(component.coolingError).toBe('');
    expect(component.canStart).toBeFalse();expect(component.startExplanation).toContain('fault');
  }));
  it('requires a fresh post-failure read before discarding an uncertain cooling write', () => {
    coolingFixture();component.selectCooling(1);component.coolingWarningAccepted=true;
    service.command.and.returnValue(throwError(()=>new Error('Lost acknowledgement')));
    component.saveCooling();expect(component.coolingSaveUnconfirmed).toBeTrue();
    expect(component.canUseSavedCooling).toBeFalse();component.useSavedCooling();
    expect(component.coolingSaveUnconfirmed).toBeTrue();expect(component.coolingDraftMask).toBe(1);
    now+=100;component.load();expect(component.canUseSavedCooling).toBeTrue();
    component.useSavedCooling();expect(component.coolingSaveUnconfirmed).toBeFalse();
    expect(component.coolingDraftMask).toBe(3);expect(service.command.calls.count()).toBe(1);
  });

  it('shows old blocked recovery instead of an endless restoring banner', () => {
    report.status.state='restoring';report.status.ownsSettings=true;
    report.status.recovery={requested:false,phase:'blocked',reason:'Original voltage did not verify'};component.load();
    expect(component.activityBanner.label).toBe('RESTORATION BLOCKED');expect(component.activityBanner.detail).toContain('Original voltage');
    expect(component.canRestore).toBeTrue();expect(service.command).not.toHaveBeenCalled();
  });
  it('shows a timed-out restoration with its exact last gate and attempt budget', () => {
    report.status.restoration={phase:'blocked',reason:'Waiting for fan 1',elapsedSeconds:180,timeoutSeconds:180,qualityWarning:null};component.load();
    expect(component.restorationHeading).toContain('time limit reached');expect(component.restorationReason).toBe('Waiting for fan 1');
    expect(component.restorationTiming).toBe('180 s elapsed · 180 s limit for this attempt');
  });
  it('confirms a physically restored backup with a mining quality warning and missing counters', fakeAsync(() => {
    report.backup={available:true,restored:false,frequencyMHz:525,coreVoltageMv:1150};component.load();
    service.command.and.callFake(()=>{report.status.active=false;report.status.ownsSettings=false;report.backup.restored=true;
      report.status.telemetry.valid=false;report.status.restoration={phase:'complete',reason:'Physical settings verified',elapsedSeconds:4,timeoutSeconds:180,qualityWarning:'ASIC measurements unavailable; tuning remains off'};return of({ok:true,restartRequired:false});});
    component.restore();tick(0);expect(component.busy).toBeFalse();expect(component.message).toContain('ASIC measurements unavailable');expect(component.activityBanner.label).toBe('ORIGINAL SETTINGS RESTORED');
    expect(service.command.calls.count()).toBe(1);
  }));
  it('confirms an explicitly completed paused save without claiming live restoration', fakeAsync(() => {
    report.backup={available:true,restored:false,frequencyMHz:525,coreVoltageMv:1150};component.load();
    service.command.and.callFake(()=>{report.status.active=false;report.status.ownsSettings=false;report.status.telemetry.miningActive=false;
      report.status.restoration={phase:'complete',reason:'Saved originals',elapsedSeconds:2,timeoutSeconds:180,qualityWarning:'Mining remains paused; live voltage not verified'};return of({ok:true,restartRequired:false});});
    component.restore();tick(0);expect(component.busy).toBeFalse();expect(component.message).toContain('Original settings saved; mining remains paused');expect(component.report!.backup.restored).toBeFalse();
  }));
  it('gates Start on controller heartbeat but retains Stop and manual refresh', fakeAsync(() => {
    const activity={get:jasmine.createSpy().and.returnValue(of({schemaVersion:1,available:true,runId:0,active:false,ownsSettings:false,controllerAgeSeconds:30}))};
    component.ngOnDestroy();component=new AutotuneComponent(service as any,activity as any);component.ngOnInit();tick(0);component.waiverAccepted=true;
    report.backup={available:true,restored:false,frequencyMHz:525,coreVoltageMv:1150};component.load();expect(component.canStart).toBeFalse();expect(component.canRestore).toBeTrue();expect(component.activityBanner.label).toContain('UNAVAILABLE');
    activity.get.and.returnValue(of({schemaVersion:1,available:true,runId:0,active:false,ownsSettings:false,controllerAgeSeconds:0}));tick(5000);
    expect(component.controllerFresh).toBeTrue();expect(component.canStart).toBeTrue();expect(service.command).not.toHaveBeenCalled();component.ngOnDestroy();
  }));
  it('supports a legacy missing activity endpoint but never treats a later lost heartbeat as current', fakeAsync(() => {
    const activity={get:jasmine.createSpy().and.returnValue(throwError(()=>({status:404})))};
    component.ngOnDestroy();component=new AutotuneComponent(service as any,activity as any);component.ngOnInit();tick(0);expect(component.controllerFresh).toBeTrue();
    activity.get.and.returnValue(of({schemaVersion:1,available:true,runId:0,active:false,ownsSettings:false,controllerAgeSeconds:0}));tick(5000);expect(component.controllerFresh).toBeTrue();
    activity.get.and.returnValue(throwError(()=>({status:404})));tick(5000);expect(component.controllerFresh).toBeFalse();expect(service.command).not.toHaveBeenCalled();component.ngOnDestroy();
  }));
  it('rejects malformed restoration metadata without changing an unsaved edit', () => {
    component.edit!.maxVoltageMv=1140;report.status.restoration={phase:'complete',reason:'bad',elapsedSeconds:-1,timeoutSeconds:180,qualityWarning:null};component.load();
    expect(component.restoration).toBeNull();expect(component.restorationMalformed).toBeTrue();expect(component.canStart).toBeFalse();expect(component.activityBanner.label).toBe('RESTORATION STATUS UNCONFIRMED');expect(component.hasUnsavedChanges).toBeTrue();expect(component.edit!.maxVoltageMv).toBe(1140);
  });

});
