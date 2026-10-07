import { CommonModule } from '@angular/common';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { ReactiveFormsModule } from '@angular/forms';
import { BehaviorSubject, of } from 'rxjs';
import { ToastrService } from 'ngx-toastr';
import { MiningControlsComponent } from './mining-controls.component';
import { MiningControlsService, MiningObservation, MiningScheduleStatus } from 'src/app/services/mining-controls.service';
import { SystemApiService } from 'src/app/services/system.service';
import { LiveDataService } from 'src/app/services/live-data.service';

const running: MiningScheduleStatus = { clockValid: true, localTime: '2026-10-07 12:00:00', scheduledPause: false, manualOverride: 'none', requestedPaused: false, appliedPaused: false, transitionPending: false, error: null };

describe('Mining controls console', () => {
  let component: MiningControlsComponent;
  let fixture: ComponentFixture<MiningControlsComponent>;
  let observations: BehaviorSubject<MiningObservation>;
  let mining: jasmine.SpyObj<MiningControlsService>;
  let system: jasmine.SpyObj<SystemApiService>;
  beforeEach(() => {
    observations = new BehaviorSubject<MiningObservation>({ response: { supported: true, schedule: { enabled: false, timezone: 'UTC', windows: [] }, status: running }, receivedAt: Date.now(), error: null });
    mining = jasmine.createSpyObj('MiningControlsService', ['observe', 'saveSchedule', 'returnToSchedule', 'refresh']);
    mining.observe.and.returnValue(observations.asObservable());
    mining.saveSchedule.and.returnValue(of({ message: 'Saved' }));
    mining.returnToSchedule.and.returnValue(of({ message: 'Returned' }));
    system = jasmine.createSpyObj('SystemApiService', ['pauseMining', 'resumeMining', 'updateSystem']);
    system.pauseMining.and.returnValue(of({ message: 'Pause accepted' }));
    system.resumeMining.and.returnValue(of({ message: 'Resume accepted' }));
    TestBed.configureTestingModule({ declarations: [MiningControlsComponent], imports: [CommonModule, ReactiveFormsModule], providers: [
      { provide: MiningControlsService, useValue: mining }, { provide: SystemApiService, useValue: system },
      { provide: LiveDataService, useValue: { info$: of({ power: 17.3 }), lastUpdateAt: Date.now() } },
      { provide: ToastrService, useValue: jasmine.createSpyObj('ToastrService', ['success', 'error', 'info']) },
    ] });
    fixture = TestBed.createComponent(MiningControlsComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('starts disabled and performs no automatic writes', () => {
    expect(component.schedule.enabled).toBeFalse();
    expect(component.schedule.windows).toEqual([]);
    expect(mining.saveSchedule).not.toHaveBeenCalled();
    expect(system.updateSystem).not.toHaveBeenCalled();
    expect(system.pauseMining).not.toHaveBeenCalled();
  });

  it('shows a review-only draft on upstream404 and cannot save it', () => {
    observations.next({ response: { supported: false }, receivedAt: Date.now(), error: null });
    component.addWindow();
    component.saveSchedule();
    fixture.detectChanges();
    expect(component.stateLabel).toBe('Mining confirmation unavailable');
    expect(component.actionDisabled).toBeTrue();
    expect(fixture.nativeElement.querySelector('.fw-mining-unavailable').textContent).toContain('review a schedule draft');
    expect(mining.saveSchedule).not.toHaveBeenCalled();
  });

  it('saves weekly windows through the schedule service without hardware fields', () => {
    component.addWindow();
    component.form.get('enabled')!.setValue(true);
    component.toggleDay(0, 1, false);
    component.saveSchedule();
    expect(mining.saveSchedule).toHaveBeenCalledWith({ enabled: true, timezone: 'UTC', windows: [{ days: 126, start: '23:00', end: '07:00' }] }, '');
    expect(component.overnight(0)).toBeTrue();
    expect(system.updateSystem).not.toHaveBeenCalled();
  });

  it('keeps an acknowledged pause pending until fresh applied-state confirmation', () => {
    component.setPaused(true);
    expect(system.pauseMining).toHaveBeenCalled();
    expect(component.stateLabel).toBe('Pause requested');
    expect(component.status?.appliedPaused).toBeFalse();
    expect(component.actionDisabled).toBeTrue();
    observations.next({ response: { supported: true, schedule: { enabled: false, timezone: 'UTC', windows: [] }, status: { ...running, requestedPaused: true, transitionPending: true } }, receivedAt: Date.now() + 1, error: null });
    expect(component.status?.appliedPaused).toBeFalse();
    expect(component.actionDisabled).toBeTrue();
    observations.next({ response: { supported: true, schedule: { enabled: false, timezone: 'UTC', windows: [] }, status: { ...running, requestedPaused: true, appliedPaused: true, manualOverride: 'paused' } }, receivedAt: Date.now() + 2, error: null });
    expect(component.stateLabel).toBe('ASIC paused');
    expect(component.controlSource).toBe('Manual pause');
  });

  it('uses saved schedule control rather than an unsaved enable toggle and preserves drafts while polling', () => {
    component.addWindow();
    component.form.get('enabled')!.setValue(true);
    observations.next({ response: { supported: true, schedule: { enabled: false, timezone: 'UTC', windows: [] }, status: running }, receivedAt: Date.now(), error: null });
    expect(component.controlSource).toBe('Schedule disabled');
    expect(component.windows.length).toBe(1);
    expect(component.form.dirty).toBeTrue();
  });

  it('does not show unavailable device time as a synchronized clock', () => {
    observations.next({ response: { supported: true, schedule: { enabled: true, timezone: 'Europe/London', windows: [{ days: 127, start: '23:00', end: '07:00' }] }, status: { ...running, clockValid: false, localTime: null, requestedPaused: true, appliedPaused: true, scheduledPause: true } }, receivedAt: Date.now(), error: null });
    fixture.detectChanges();
    expect(component.controlSource).toBe('Waiting paused for network time');
    expect(fixture.nativeElement.querySelector('.fw-device-time strong').textContent).toBe('Not synchronized');
  });

  it('confirms Return to schedule only after fresh override status changes', () => {
    observations.next({ response: { supported: true, schedule: { enabled: false, timezone: 'UTC', windows: [] }, status: { ...running, manualOverride: 'paused', requestedPaused: true, appliedPaused: true } }, receivedAt: Date.now(), error: null });
    component.returnToSchedule();
    expect(mining.returnToSchedule).toHaveBeenCalled();
    expect(component.controlSource).toBe('Returning to schedule control…');
    observations.next({ response: { supported: true, schedule: { enabled: false, timezone: 'UTC', windows: [] }, status: running }, receivedAt: Date.now() + 1, error: null });
    expect(component.returningToSchedule).toBeFalse();
    expect(component.controlSource).toBe('Schedule disabled');
  });

  it('labels saved windows as pause windows and never inverts their meaning', () => {
    const schedule = { enabled: true, timezone: 'UTC', windows: [{ days: 127, start: '23:00', end: '07:00' }] };
    observations.next({ response: { supported: true, schedule, status: { ...running, scheduledPause: true, appliedPaused: true, requestedPaused: true } }, receivedAt: Date.now(), error: null });
    expect(component.controlSource).toBe('Scheduled pause window active');
    observations.next({ response: { supported: true, schedule, status: running }, receivedAt: Date.now(), error: null });
    expect(component.controlSource).toBe('Outside pause windows · mining allowed');
  });

  it('lets a newly saved pause schedule supersede an acknowledged resume request', () => {
    observations.next({ response: { supported: true, schedule: { enabled: false, timezone: 'UTC', windows: [] }, status: { ...running, appliedPaused: true, requestedPaused: true, manualOverride: 'paused' } }, receivedAt: Date.now(), error: null });
    component.setPaused(false);
    expect(component.stateLabel).toBe('Resume requested');
    component.addWindow();
    component.form.get('enabled')!.setValue(true);
    component.saveSchedule();
    observations.next({ response: { supported: true, schedule: component.schedule, status: { ...running, scheduledPause: true, requestedPaused: true, appliedPaused: true } }, receivedAt: Date.now() + 1, error: null });
    expect(component.stateLabel).toBe('ASIC paused');
    expect(component.controlSource).toBe('Scheduled pause window active');
    expect(component.actionDisabled).toBeFalse();
  });

  it('clears a pending Return to schedule confirmation when a new schedule is saved', () => {
    observations.next({ response: { supported: true, schedule: { enabled: false, timezone: 'UTC', windows: [] }, status: { ...running, appliedPaused: true, requestedPaused: true, manualOverride: 'paused' } }, receivedAt: Date.now(), error: null });
    component.returnToSchedule();
    expect(component.returningToSchedule).toBeTrue();
    component.addWindow();
    component.saveSchedule();
    expect(component.returningToSchedule).toBeFalse();
  });
  it('labels a stale or failed clock read unavailable rather than unsynchronized', () => {
    observations.next({ response: null, receivedAt: 0, error: 'Unable to read mining status.' });
    fixture.detectChanges();
    expect(fixture.nativeElement.querySelector('.fw-device-time strong').textContent).toBe('Unavailable');
    expect(component.actionDisabled).toBeTrue();
  });

});
