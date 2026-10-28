import { CommonModule } from '@angular/common';
import { Component } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { ComponentFixture, TestBed, fakeAsync, tick } from '@angular/core/testing';
import { of } from 'rxjs';
import { ToastrService } from 'ngx-toastr';
import { SchedulerComponent, leaveScheduler } from './scheduler.component';
import { TuningScheduleComponent } from '../tuning-schedule/tuning-schedule.component';
import { OperatingProfilesService } from '../../services/operating-profiles.service';

@Component({selector:'app-mining-controls',template:'Existing mining controls'})
class MiningControlsStub {}
@Component({selector:'app-pool-schedule',template:'Existing pool schedule'})
class PoolScheduleStub {}

describe('Gamma Scheduler tuning navigation', () => {
  let fixture: ComponentFixture<SchedulerComponent>;
  let save: jasmine.Spy;
  beforeEach(async () => {
    save = jasmine.createSpy('saveTuningSchedule');
    await TestBed.configureTestingModule({
      declarations:[SchedulerComponent,TuningScheduleComponent,MiningControlsStub,PoolScheduleStub],
      imports:[CommonModule,FormsModule],
      providers:[
        {provide:OperatingProfilesService,useValue:{
          getTuningSchedule:() => of({schemaVersion:1,enabled:false,utcOffsetMinutes:0,events:[],supported:true,timezoneMode:'fixed-utc-offset',clockValid:true,maxEvents:16,selectedSlot:null,appliedSlot:null,status:'disabled',error:null}),
          get:() => of({schemaVersion:1,tuning:Array.from({length:10},(_,slot) => ({slot,configured:slot === 0,name:slot === 0 ? 'Daytime' : null,frequencyMHz:500,coreVoltageMv:1100}))}),
          saveTuningSchedule:save,
        }},
        {provide:ToastrService,useValue:{success:() => {}}},
      ],
    }).compileComponents();
    fixture = TestBed.createComponent(SchedulerComponent);fixture.detectChanges();
  });
  afterEach(() => fixture.destroy());

  it('keeps the existing default power section and adds tuning as a distinct section without writes', () => {
    expect(fixture.componentInstance.section).toBe('power');
    expect(fixture.nativeElement.textContent).toContain('Existing mining controls');
    fixture.componentInstance.selectSection('tuning');fixture.detectChanges();
    expect(fixture.nativeElement.querySelector('[data-tuning-schedule]')).not.toBeNull();
    expect(fixture.nativeElement.textContent).toContain('Tuning profile schedule');expect(save).not.toHaveBeenCalled();
  });

  it('protects an unsaved tuning schedule when changing sections or leaving the page', fakeAsync(() => {
    fixture.componentInstance.selectSection('tuning');fixture.detectChanges();tick();fixture.detectChanges();
    const child = fixture.componentInstance.tuningScheduler!;child.addEvent();fixture.detectChanges();
    expect(child.hasUnsavedChanges).toBeTrue();
    const confirm = spyOn(window,'confirm').and.returnValue(false);
    fixture.componentInstance.selectSection('pool');expect(fixture.componentInstance.section).toBe('tuning');
    expect(leaveScheduler(fixture.componentInstance)).toBeFalse();
    confirm.and.returnValue(true);fixture.componentInstance.selectSection('pool');fixture.detectChanges();
    expect(fixture.componentInstance.section).toBe('pool');expect(fixture.nativeElement.textContent).toContain('Existing pool schedule');
    expect(leaveScheduler(fixture.componentInstance)).toBeTrue();expect(save).not.toHaveBeenCalled();fixture.destroy();
  }));
});
