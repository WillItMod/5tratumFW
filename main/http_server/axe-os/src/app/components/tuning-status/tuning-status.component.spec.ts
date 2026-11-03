import { TestBed, fakeAsync, tick } from '@angular/core/testing';
import { EMPTY, Subject, of, throwError } from 'rxjs';
import { provideRouter } from '@angular/router';
import { AutotuneActivityService } from '../../services/autotune-activity.service';
import { TuningStatusComponent } from './tuning-status.component';

function activity():any {return {schemaVersion:1,available:true,supported:true,active:true,ownsSettings:true,fault:false,state:'measuring',reason:'measuring-settled-operating-point',priority:'performance',continuous:false,runId:1,controllerAgeSeconds:0};}

describe('Global passive tuning activity',()=>{
  let component:TuningStatusComponent;let report:any;let now:number;
  let service:{get:jasmine.Spy;authorize:jasmine.Spy;command:jasmine.Spy};
  beforeEach(()=>{now=1000;spyOn(performance,'now').and.callFake(()=>now);report=activity();
    service={get:jasmine.createSpy('get').and.callFake(()=>of(structuredClone(report))),authorize:jasmine.createSpy('authorize'),command:jasmine.createSpy('command')};
    component=new TuningStatusComponent(service as any);
  });
  afterEach(()=>component.ngOnDestroy());
  function begin():void{component.ngOnInit();tick(0);}
  function advance(ms:number):void{now+=ms;tick(ms);}
  it('shows an unmistakable active phase and goal using reads only',fakeAsync(()=>{
    begin();expect(component.badge.label).toBe('Tuning active');expect(component.badge.phase).toBe('Measuring');expect(component.badge.goal).toBe('Performance');
    advance(10000);expect(service.get.calls.count()).toBe(3);expect(service.authorize).not.toHaveBeenCalled();expect(service.command).not.toHaveBeenCalled();component.ngOnDestroy();
  }));
  it('distinguishes monitoring, held ownership and a fresh disabled controller',fakeAsync(()=>{
    report.state='observing';begin();expect(component.badge.label).toBe('Tuning monitoring');expect(component.badge.tone).toBe('observing');
    report.state='holding';report.active=false;advance(5000);expect(component.badge.label).toBe('Tuning holding');expect(component.badge.phase).toBe('Control retained');
    report.state='disabled';report.ownsSettings=false;advance(5000);expect(component.badge.label).toBe('Tuning off');expect(component.badge.phase).toBe('Control released');component.ngOnDestroy();
  }));
  it('uses the authoritative fault flag even for an inactive supported controller',fakeAsync(()=>{
    report.active=false;report.ownsSettings=false;report.state='disabled';report.fault=true;begin();
    expect(component.badge.label).toBe('Tuning blocked');expect(component.badge.phase).toBe('Protection reported');expect(component.badge.tone).toBe('fault');
    report.fault=false;report.state='faulted';report.ownsSettings=true;advance(5000);expect(component.badge.label).toBe('Tuning faulted');component.ngOnDestroy();
  }));
  it('preserves last active ownership through busy, malformed, stale and missing activity',fakeAsync(()=>{
    begin();report={schemaVersion:1,available:false};advance(5000);expect(component.badge.label).toBe('Tuning last active');expect(component.badge.phase).toBe('Current state unknown');
    report={...activity(),active:false,ownsSettings:false,state:'disabled',controllerAgeSeconds:20};advance(5000);expect(component.badge.label).toBe('Tuning last active');
    report={schemaVersion:1,available:true,active:false};advance(5000);expect(component.badge.label).toBe('Tuning last active');
    service.get.and.returnValue(throwError(()=>({status:404})));advance(5000);expect(component.hiddenLegacy).toBeFalse();expect(component.badge.description).toContain('do not assume it stopped');component.ngOnDestroy();
  }));
  it('keeps a last held result visible when the API disappears',fakeAsync(()=>{
    report.active=false;report.state='holding';begin();service.get.and.returnValue(throwError(()=>({status:503})));advance(5000);
    expect(component.badge.label).toBe('Tuning last held');expect(component.hiddenLegacy).toBeFalse();component.ngOnDestroy();
  }));
  it('hides a legacy404 only before any active or owned report has been observed',fakeAsync(()=>{
    service.get.and.returnValue(throwError(()=>({status:404})));begin();expect(component.hiddenLegacy).toBeTrue();
    service.get.and.callFake(()=>of(structuredClone(report)));advance(5000);expect(component.hiddenLegacy).toBeFalse();
    report.active=false;report.ownsSettings=false;report.state='disabled';advance(5000);expect(component.badge.label).toBe('Tuning off');
    service.get.and.returnValue(throwError(()=>({status:404})));advance(5000);expect(component.hiddenLegacy).toBeFalse();expect(component.badge.label).toBe('Tuning checking');component.ngOnDestroy();
  }));
  it('counts network transit plus the cached controller age before calling a report current',fakeAsync(()=>{
    const response=new Subject<unknown>();service.get.and.returnValue(response);component.ngOnInit();tick(0);report.controllerAgeSeconds=12;now+=4000;response.next(report);response.complete();
    expect(component.fresh).toBeFalse();expect(component.badge.label).toBe('Tuning last active');component.ngOnDestroy();
  }));
  it('marks an old heartbeat unknown even if requests keep completing without observations',fakeAsync(()=>{
    begin();service.get.and.returnValue(EMPTY);advance(16000);expect(component.fresh).toBeFalse();expect(component.badge.label).toBe('Tuning last active');component.ngOnDestroy();
  }));
  it('rejects missing heartbeat, nonboolean state fields and malformed faults without claiming Off',fakeAsync(()=>{
    report.active=false;report.ownsSettings=false;report.state='disabled';report.controllerAgeSeconds=null;begin();expect(component.fresh).toBeFalse();expect(component.badge.label).toBe('Tuning checking');
    report.controllerAgeSeconds=0;report.active='false';advance(5000);expect(component.badge.label).toBe('Tuning checking');
    report.active=false;delete report.fault;advance(5000);expect(component.badge.label).toBe('Tuning checking');expect(component.hiddenLegacy).toBeFalse();component.ngOnDestroy();
  }));
  it('keeps restoring and suspended states distinct from a stopped controller',fakeAsync(()=>{
    report.active=false;report.state='restoring';begin();expect(component.badge.label).toBe('Tuning restoring');
    report.state='suspended';advance(5000);expect(component.badge.label).toBe('Tuning suspended');expect(component.badge.phase).toBe('Control retained');component.ngOnDestroy();
  }));
  it('cancels polling and in-flight reads when the header is destroyed',fakeAsync(()=>{
    const response=new Subject<unknown>();service.get.and.returnValue(response);component.ngOnInit();tick(0);component.ngOnDestroy();const reads=service.get.calls.count();
    advance(15000);response.next(report);expect(service.get.calls.count()).toBe(reads);expect(component.badge.label).toBe('Tuning checking');
  }));
  it('renders a navigation link with live status and completely hides a never-engaged legacy badge',fakeAsync(()=>{
    TestBed.configureTestingModule({imports:[TuningStatusComponent],providers:[provideRouter([]),{provide:AutotuneActivityService,useValue:service}]});
    const fixture=TestBed.createComponent(TuningStatusComponent);fixture.componentInstance.tuningRoute='/pages/tuning';fixture.detectChanges();tick(0);fixture.detectChanges();
    const link=fixture.nativeElement.querySelector('a');expect(link.getAttribute('href')).toBe('/pages/tuning');expect(link.textContent).toContain('Tuning active');
    expect(link.getAttribute('aria-label')).toContain('Open Tuning for details');fixture.destroy();
    service.get.and.returnValue(throwError(()=>({status:404})));
    const legacy=TestBed.createComponent(TuningStatusComponent);legacy.detectChanges();tick(0);legacy.detectChanges();expect(legacy.nativeElement.querySelector('a')).toBeNull();expect(getComputedStyle(legacy.nativeElement).display).toBe('none');legacy.destroy();
  }));

  it('retains only bounded flat activity fields rather than unexpected full reports',fakeAsync(()=>{
    report.reason='x'.repeat(500);report.unexpected={events:new Array(1000).fill('unused')};begin();
    expect((component as any).last.unexpected).toBeUndefined();expect((component as any).last.reason.length).toBe(256);component.ngOnDestroy();
  }));

  it('retains a last-reported inactive protection fault through a later404',fakeAsync(()=>{
    report.active=false;report.ownsSettings=false;report.state='disabled';report.fault=true;begin();
    service.get.and.returnValue(throwError(()=>({status:404})));advance(5000);
    expect(component.hiddenLegacy).toBeFalse();expect(component.badge.label).toBe('Tuning last blocked');expect(component.badge.phase).toBe('Current state unknown');component.ngOnDestroy();
  }));

  it('treats revoked ownership flags as authoritative even when a phase remains cached',fakeAsync(()=>{
    begin();report.active=false;report.ownsSettings=false;
    for(const state of ['measuring','restoring','suspended','faulted']){report.state=state;advance(5000);expect(component.badge.label).toBe('Tuning off');expect(component.badge.phase).toBe('Control released');}
    report.fault=true;advance(5000);expect(component.badge.label).toBe('Tuning blocked');component.ngOnDestroy();
  }));
  it('up-ranks stale positive evidence from Off while refusing stale negative evidence',fakeAsync(()=>{
    report.active=false;report.ownsSettings=false;report.state='disabled';begin();expect(component.badge.label).toBe('Tuning off');
    report={...activity(),controllerAgeSeconds:20};advance(5000);expect(component.badge.label).toBe('Tuning last active');
    report.active=false;report.ownsSettings=false;report.state='disabled';advance(5000);expect(component.badge.label).toBe('Tuning last active');
    report.fault=true;advance(5000);expect(component.badge.label).toBe('Tuning last blocked');component.ngOnDestroy();
  }));

  it('labels a released limit stop separately from active tuning and hardware protection',fakeAsync(()=>{
    report.active=false;report.ownsSettings=false;report.state='faulted';report.reason='cannot-reduce-load-within-limits';begin();
    expect(component.badge.label).toBe('Tuning stopped');expect(component.badge.phase).toContain('limits reached');expect(component.badge.tone).toBe('suspended');
    report.fault=true;advance(5000);expect(component.badge.label).toBe('Tuning blocked');
    report.fault=false;advance(5000);service.get.and.returnValue(throwError(()=>({status:404})));advance(5000);expect(component.hiddenLegacy).toBeFalse();expect(component.badge.label).toBe('Tuning last stopped');component.ngOnDestroy();
  }));

});
