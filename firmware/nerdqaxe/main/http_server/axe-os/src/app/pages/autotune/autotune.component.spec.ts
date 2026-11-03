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
});
