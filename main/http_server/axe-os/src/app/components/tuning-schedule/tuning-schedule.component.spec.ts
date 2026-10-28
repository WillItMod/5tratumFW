import { ComponentFixture, TestBed, fakeAsync, tick } from '@angular/core/testing';
import { FormsModule } from '@angular/forms';
import { CommonModule } from '@angular/common';
import { NoopAnimationsModule } from '@angular/platform-browser/animations';
import { ToastrService } from 'ngx-toastr';
import { NEVER, Subject, of, throwError } from 'rxjs';
import { TuningScheduleComponent } from './tuning-schedule.component';
import { TuningSchedule, TuningScheduleReport } from 'src/models/ITuningSchedule';
import { OperatingProfilesService } from '../../services/operating-profiles.service';

function configuration(): TuningSchedule {
  return {schemaVersion:1,enabled:true,utcOffsetMinutes:0,events:[
    {enabled:true,dayMask:127,timeMinutes:540,slot:0},
    {enabled:true,dayMask:127,timeMinutes:1260,slot:1},
    {enabled:true,dayMask:127,timeMinutes:0,slot:2},
  ]};
}

function reportFixture(): TuningScheduleReport {
  return {schemaVersion:1,enabled:false,utcOffsetMinutes:0,events:[],supported:true,timezoneMode:'fixed-utc-offset',clockValid:true,maxEvents:16,
    selectedSlot:null,appliedSlot:null,status:'disabled',error:null};
}

function profileFixture(): any {
  return {schemaVersion:1,tuning:Array.from({length:10},(_,slot) => ({slot,configured:slot < 3,
    name:slot < 3 ? ['Daytime','Evening','Overnight'][slot] : null,frequencyMHz:500 + slot * 10,coreVoltageMv:1100 + slot * 10})),pools:[]};
}

describe('Saved tuning profile scheduler', () => {
  let component: TuningScheduleComponent;
  let report: TuningScheduleReport;
  let profiles: any;
  let system: any;
  let success: jasmine.Spy;

  beforeEach(() => {
    report = reportFixture(); profiles = profileFixture();
    system = {
      getTuningSchedule:jasmine.createSpy('getTuningSchedule').and.callFake(() => of(structuredClone(report))),
      get:jasmine.createSpy('get').and.callFake(() => of(structuredClone(profiles))),
      saveTuningSchedule:jasmine.createSpy('saveTuningSchedule').and.callFake((payload:TuningSchedule, _uri:string) => {
        report = {...report,...structuredClone(payload)}; return of({ok:true});
      }),
    };
    success = jasmine.createSpy('success');
    component = new TuningScheduleComponent(system,{success} as any);
    component.uri = '/selected-miner';
  });
  afterEach(() => component.ngOnDestroy());

  it('only reads the selected miner on entry and while polling, retaining unsaved time points', fakeAsync(() => {
    component.ngOnInit();
    component.schedule = configuration();
    report.status = 'paused';report.selectedSlot = 1;
    tick(5000);
    expect(system.getTuningSchedule).toHaveBeenCalledWith('/selected-miner');
    expect(system.getTuningSchedule.calls.count()).toBe(2);
    expect(system.get).toHaveBeenCalledWith('/selected-miner');
    expect(component.schedule.events.map(event => event.timeMinutes)).toEqual([540,1260,0]);
    expect(component.report!.status).toBe('paused');
    expect(component.statusText).toContain('paused');
    expect(component.hasUnsavedChanges).toBeTrue();
    expect(system.saveTuningSchedule).not.toHaveBeenCalled();
    component.ngOnDestroy();
  }));

  it('saves 09:00 slot 1, 21:00 slot 2 and midnight slot 3 and confirms persisted readback', () => {
    component.ngOnInit(); component.schedule = configuration(); component.save();
    expect(system.saveTuningSchedule.calls.mostRecent().args).toEqual([configuration(),'/selected-miner']);
    expect(success).toHaveBeenCalledTimes(1);
    expect(system.getTuningSchedule.calls.count()).toBe(2);
    expect(component.hasUnsavedChanges).toBeFalse();expect(component.busy).toBeFalse();
    expect(component.report!.appliedSlot).toBeNull();
    expect(component.savedMessage).toContain('saved on miner');
  });

  it('does not save an unchanged schedule or mutate profiles while adding a time point', () => {
    component.ngOnInit(); component.save();
    expect(system.saveTuningSchedule).not.toHaveBeenCalled();
    const before = structuredClone(component.profiles);
    component.addEvent();
    expect(component.schedule!.events[0]).toEqual({enabled:true,dayMask:127,timeMinutes:540,slot:0});
    expect(component.profiles).toEqual(before);expect(component.profileLabel(1)).toBe('Slot 2 · Evening');
  });

  it('rejects empty enabled schedules, missing slots and overlapping time points', () => {
    component.ngOnInit();component.schedule = configuration();
    component.schedule.events[1].timeMinutes = 540;
    expect(component.validationError).toContain('same time');component.save();
    component.schedule.events[1].dayMask = 64;component.schedule.events[0].dayMask = 63;
    expect(component.validationError).toBe('');
    component.schedule.events[0].slot = 9;
    expect(component.validationError).toContain('saved tuning profile');component.save();
    component.schedule.events[0].enabled = false;
    expect(component.validationError).toContain('saved tuning profile');
    component.schedule.events = [];
    expect(component.validationError).toContain('at least one time point');component.save();
    expect(system.saveTuningSchedule).not.toHaveBeenCalled();
  });

  it('requires days and a valid time, including when a time input is cleared', () => {
    component.ngOnInit();component.schedule = configuration();
    component.schedule.events[0].dayMask = 0;
    expect(component.validationError).toContain('at least one day');
    component.schedule.events[0].dayMask = 127;component.changeTime(component.schedule.events[0],'');
    expect(component.timeText(component.schedule.events[0].timeMinutes)).toBe('');
    expect(component.validationError).toContain('valid time');
    component.changeTime(component.schedule.events[0],'00:00');
    expect(component.schedule.events[0].timeMinutes).toBe(0);
    component.schedule.utcOffsetMinutes = 11;
    expect(component.validationError).toContain('time zone offset');
  });


  for (const response of [{}, {ok:false}, {ok:'true'}]) {
    it(`retains edits when the write acknowledgment is ${JSON.stringify(response)}`, () => {
      component.ngOnInit();component.schedule = configuration();system.saveTuningSchedule.and.returnValue(of(response));component.save();
      expect(component.saveError).toContain('could not be confirmed');expect(component.hasUnsavedChanges).toBeTrue();
      expect(success).not.toHaveBeenCalled();expect(system.saveTuningSchedule).toHaveBeenCalledTimes(1);
    });
  }

  it('does not confirm an acknowledged write whose readback still contains the old policy', () => {
    component.ngOnInit();component.schedule = configuration();system.saveTuningSchedule.and.returnValue(of({ok:true}));component.save();
    expect(component.saveError).toContain('could not be confirmed');expect(component.hasUnsavedChanges).toBeTrue();expect(success).not.toHaveBeenCalled();
  });

  it('ignores an older in-flight status response once a saved policy has been confirmed', fakeAsync(() => {
    component.ngOnInit();const stale = new Subject<TuningScheduleReport>();
    system.getTuningSchedule.and.returnValue(stale);tick(5000);
    component.schedule = configuration();system.getTuningSchedule.and.callFake(() => of(structuredClone(report)));component.save();
    expect(component.report!.enabled).toBeTrue();
    stale.next(reportFixture());stale.complete();expect(component.report!.enabled).toBeTrue();
    expect(component.hasUnsavedChanges).toBeFalse();expect(system.saveTuningSchedule).toHaveBeenCalledTimes(1);component.ngOnDestroy();
  }));

  it('can confirm a lost acknowledgment with an explicit read without retrying the write', () => {
    component.ngOnInit();component.schedule = configuration();
    system.saveTuningSchedule.and.callFake((payload:TuningSchedule, _uri:string) => {
      report = {...report,...structuredClone(payload)};return throwError(() => new Error('connection lost'));
    });
    component.save();expect(component.saveError).toContain('could not be confirmed');expect(success).not.toHaveBeenCalled();
    component.load();
    expect(component.saveError).toBe('');expect(component.hasUnsavedChanges).toBeFalse();expect(success).toHaveBeenCalledTimes(1);
    expect(system.saveTuningSchedule).toHaveBeenCalledTimes(1);
  });

  it('leaves a failed readback unconfirmed and times out a stalled write without automatic retry', fakeAsync(() => {
    component.ngOnInit();component.schedule = configuration();system.saveTuningSchedule.and.returnValue(NEVER);component.save();
    tick(10000);
    expect(component.busy).toBeFalse();expect(component.saveError).toContain('could not be confirmed');expect(system.saveTuningSchedule).toHaveBeenCalledTimes(1);
    expect(success).not.toHaveBeenCalled();component.ngOnDestroy();
  }));

  it('blocks an older firmware endpoint and permits read-only retry after updating', () => {
    system.getTuningSchedule.and.returnValue(throwError(() => ({status:404})));component.ngOnInit();
    expect(component.schedule).toBeNull();expect(component.readError).toContain('matching application and web update');
    component.save();expect(system.saveTuningSchedule).not.toHaveBeenCalled();
    system.getTuningSchedule.and.returnValue(of(report));component.load();
    expect(component.schedule).not.toBeNull();expect(component.readError).toBe('');
  });

  it('requires an explicit supported scheduling capability rather than guessing from a model', () => {
    system.getTuningSchedule.and.returnValue(of({...report,supported:false}));component.ngOnInit();
    expect(component.schedule).toBeNull();expect(component.readError).toContain('matching 5tratumFW');
  });

  it('rejects malformed status, time points and saved profile lists before rendering or enabling saves', () => {
    const malformed = [{events:[null]},{events:[{}]},{events:[{enabled:true,dayMask:127,timeMinutes:540,slot:10}]},
      {events:configuration().events.map(event => ({...event,enabled:'true'}))},{utcOffsetMinutes:11},
      {selectedSlot:'0'},{appliedSlot:-1},{selectedSlot:undefined},{error:{}},{enabled:true,events:[]},
      {events:[configuration().events[0],configuration().events[0]]},{maxEvents:64},{clockValid:1},{storageValid:'false'}];
    for (const override of malformed) {
      system.getTuningSchedule.and.returnValue(of({...report,...override}));component.load();
      expect(component.schedule).withContext(JSON.stringify(override)).toBeNull();
      expect(component.readError).toContain('matching 5tratumFW');
    }
    system.getTuningSchedule.and.returnValue(of(report));
    for (const invalidProfiles of [{schemaVersion:1,tuning:[null]},{schemaVersion:1,tuning:Array(10).fill({slot:0,configured:false,name:null})}]) {
      system.get.and.returnValue(of(invalidProfiles));component.load();expect(component.schedule).toBeNull();
    }
    expect(system.saveTuningSchedule).not.toHaveBeenCalled();
  });

  it('allows replacing a corrupt stored schedule with an empty disabled schedule and requires valid persisted readback', () => {
    report.storageValid = false;report.status = 'error';report.error = 'Stored schedule is invalid';component.ngOnInit();
    expect(component.hasUnsavedChanges).toBeTrue();expect(component.validationError).toBe('');
    component.save();expect(component.saveError).toContain('could not be confirmed');expect(success).not.toHaveBeenCalled();
    report.storageValid = true;report.error = null;report.status = 'disabled';component.load();
    expect(component.saveError).toBe('');expect(component.hasUnsavedChanges).toBeFalse();expect(success).toHaveBeenCalledTimes(1);
    expect(system.saveTuningSchedule).toHaveBeenCalledTimes(1);
  });

  it('continues polling after a failed status read and never replaces local edits', fakeAsync(() => {
    component.ngOnInit();component.schedule = configuration();
    system.getTuningSchedule.and.returnValue(throwError(() => ({status:503})));tick(5000);
    expect(component.readError).toContain('Could not refresh');
    report.status = 'waiting-clock';report.clockValid = false;system.getTuningSchedule.and.returnValue(of(report));tick(5000);
    expect(component.readError).toBe('');expect(component.statusText).toContain('network time');
    expect(component.schedule.events).toEqual(configuration().events);expect(system.saveTuningSchedule).not.toHaveBeenCalled();component.ngOnDestroy();
  }));
});

describe('Tuning schedule production controls', () => {
  let fixture: ComponentFixture<TuningScheduleComponent>;
  let save: jasmine.Spy;
  beforeEach(async () => {
    save = jasmine.createSpy('saveTuningSchedule').and.returnValue(of({ok:true}));
    await TestBed.configureTestingModule({
      declarations:[TuningScheduleComponent],
      imports:[CommonModule,FormsModule,NoopAnimationsModule],
      providers:[
        {provide:OperatingProfilesService,useValue:{getTuningSchedule:() => of({...reportFixture(),...configuration(),status:'paused',selectedSlot:1,appliedSlot:0}),get:() => of(profileFixture()),saveTuningSchedule:save}},
        {provide:ToastrService,useValue:{success:() => {}}},
      ],
    }).compileComponents();
    fixture = TestBed.createComponent(TuningScheduleComponent);fixture.detectChanges();
  });
  afterEach(() => fixture.destroy());

  it('shows saved tuning names, three distinct daily time points, pause status and fixed-offset limitations', () => {
    const element: HTMLElement = fixture.nativeElement;
    expect(element.querySelectorAll('[data-tuning-event]').length).toBe(3);
    expect(Array.from(element.querySelectorAll<HTMLInputElement>('input[type=time]')).map(input => input.value)).toEqual(['09:00','21:00','00:00']);
    const text = element.textContent!;
    expect(text).toContain('Daytime');expect(text).toContain('Evening');expect(text).toContain('Overnight');
    expect(text).toContain('Mining is paused');expect(text).toContain('Last activated profile: Slot 1 · Daytime');
    expect(text).toContain('do not adjust for daylight saving');expect(text).toContain('pauses are preserved');
    expect(save).not.toHaveBeenCalled();
  });

  it('keeps cleared times invalid and exposes an understandable validation error', () => {
    const input = fixture.nativeElement.querySelector('input[type=time]') as HTMLInputElement;
    input.value = '';input.dispatchEvent(new Event('change'));fixture.detectChanges();
    expect(fixture.nativeElement.textContent).toContain('Choose a valid time');
    const buttons = Array.from(fixture.nativeElement.querySelectorAll('button')) as HTMLButtonElement[];
    expect(buttons.find(button => button.textContent?.trim() === 'Save tuning schedule')!.disabled).toBeTrue();
    expect(save).not.toHaveBeenCalled();
  });
});
