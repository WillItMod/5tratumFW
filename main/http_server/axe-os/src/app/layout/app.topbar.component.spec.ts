import { BehaviorSubject, Subject, of } from 'rxjs';
import { ToastrService } from 'ngx-toastr';
import { AppTopBarComponent } from './app.topbar.component';
import { LayoutService } from './service/app.layout.service';
import { SystemApiService } from 'src/app/services/system.service';
import { LiveDataService } from 'src/app/services/live-data.service';
import { SensitiveData } from 'src/app/services/sensitive-data.service';
import { DashboardEditService } from 'src/app/services/dashboard-edit.service';
import { MiningControlsService, MiningObservation, MiningScheduleStatus } from 'src/app/services/mining-controls.service';
import { SystemInfo } from 'src/app/generated/models';

const running: MiningScheduleStatus = { clockValid: true, localTime: '2026-10-07 12:00', scheduledPause: false, manualOverride: 'none', requestedPaused: false, appliedPaused: false, transitionPending: false, error: null };

describe('Topbar mining state confirmation', () => {
  let component: AppTopBarComponent;
  let observations: BehaviorSubject<MiningObservation>;
  let system: jasmine.SpyObj<SystemApiService>;
  let mining: jasmine.SpyObj<MiningControlsService>;
  let live: LiveDataService;
  let reports: BehaviorSubject<SystemInfo>;
  let toast: jasmine.SpyObj<ToastrService>;
  beforeEach(() => {
    observations = new BehaviorSubject<MiningObservation>({ response: { supported: true, schedule: { enabled: false, timezone: 'UTC', windows: [] }, status: running }, receivedAt: Date.now(), error: null });
    system = jasmine.createSpyObj('SystemApiService', ['pauseMining', 'resumeMining', 'restart']);
    system.pauseMining.and.returnValue(of({ message: 'Pause accepted' }));
    system.resumeMining.and.returnValue(of({ message: 'Resume accepted' }));
    mining = jasmine.createSpyObj('MiningControlsService', ['observe', 'refresh']);
    mining.observe.and.returnValue(observations.asObservable());
    reports = new BehaviorSubject({ miningPaused: false } as SystemInfo);
    live = { info$: reports.asObservable(), lastUpdateAt: Date.now() } as unknown as LiveDataService;
    toast = jasmine.createSpyObj<ToastrService>('ToastrService', ['info', 'error', 'success']);
    component = new AppTopBarComponent({} as LayoutService, system, live, toast, { hidden: of(false) } as SensitiveData, {} as DashboardEditService, mining);
    component.ngOnInit();
  });
  afterEach(() => component.ngOnDestroy());

  it('does not flip applied pause state on an acknowledged request', () => {
    component.toggleMiningPaused();
    expect(system.pauseMining).toHaveBeenCalledTimes(1);
    expect(component.isMiningPaused).toBeFalse();
    expect(component.miningActionDisabled).toBeTrue();
    expect(component.miningActionText).toBe('Pausing…');
    component.toggleMiningPaused();
    expect(system.pauseMining).toHaveBeenCalledTimes(1);
    observations.next({ response: { supported: true, schedule: { enabled: false, timezone: 'UTC', windows: [] }, status: { ...running, requestedPaused: true, appliedPaused: true } }, receivedAt: Date.now() + 1, error: null });
    expect(component.isMiningPaused).toBeTrue();
    expect(component.miningActionText).toBe('Resume');
    expect(component.miningActionDisabled).toBeFalse();
  });

  it('disables commands while fresh device state reports a pending transition', () => {
    observations.next({ response: { supported: true, schedule: { enabled: false, timezone: 'UTC', windows: [] }, status: { ...running, requestedPaused: true, transitionPending: true } }, receivedAt: Date.now(), error: null });
    component.toggleMiningPaused();
    expect(system.pauseMining).not.toHaveBeenCalled();
    expect(component.isMiningPaused).toBeFalse();
    expect(component.miningActionText).toBe('Pausing…');
  });

  it('disables stale or failed observations instead of presenting them as current', () => {
    observations.next({ response: { supported: true, schedule: { enabled: false, timezone: 'UTC', windows: [] }, status: running }, receivedAt: Date.now() - 20000, error: null });
    expect(component.miningActionDisabled).toBeTrue();
    observations.next({ response: null, receivedAt: 0, error: 'Offline' });
    expect(component.miningActionDisabled).toBeTrue();
    expect(component.miningActionDescription).toContain('unavailable');
  });

  it('keeps legacy pause requests distinct from unavailable ASIC confirmation', () => {
    observations.next({ response: { supported: false }, receivedAt: Date.now(), error: null });
    expect(component.miningActionDisabled).toBeFalse();
    expect(component.miningActionDescription).toContain('ASIC state confirmation requires 5tratumFW');
    live.lastUpdateAt = Date.now() - 20000;
    expect(component.miningActionDisabled).toBeTrue();
  });

  it('keeps pre-ACK reads pending, then accepts stable schedule supersession without changing the toast intent', () => {
    const acknowledgement = new Subject<{ message: string }>();
    system.pauseMining.and.returnValue(acknowledgement);
    component.toggleMiningPaused();
    observations.next({ response: { supported: true, schedule: { enabled: false, timezone: 'UTC', windows: [] }, status: { ...running, requestedPaused: true, appliedPaused: true, manualOverride: 'paused' } }, receivedAt: Date.now() + 1, error: null });
    expect(component.miningRequestBusy).toBeTrue();
    expect(component.miningActionText).toBe('Pausing…');
    mining.refresh.and.callFake(() => observations.next({ response: { supported: true, schedule: { enabled: true, timezone: 'UTC', windows: [{ days: 127, start: '23:00', end: '07:00' }] }, status: running }, receivedAt: Date.now() + 2, error: null }));
    acknowledgement.next({ message: 'Pause accepted' });
    expect(component.miningActionText).toBe('Pause');
    expect(component.miningActionDisabled).toBeFalse();
    expect(toast.info).toHaveBeenCalledWith('Pause requested. Waiting for device confirmation.');
    acknowledgement.complete();
  });

  it('reconciles stock telemetry only after acknowledgement and a fresh later report', () => {
    observations.next({ response: { supported: false }, receivedAt: Date.now(), error: null });
    const acknowledgement = new Subject<{ message: string }>();
    system.pauseMining.and.returnValue(acknowledgement);
    component.toggleMiningPaused();
    live.lastUpdateAt = Date.now() + 1;
    reports.next({ miningPaused: true } as SystemInfo);
    expect(component.miningActionText).toBe('Pausing…');
    acknowledgement.next({ message: 'Pause accepted' });
    expect(component.miningActionDisabled).toBeTrue();
    live.lastUpdateAt = Date.now() + 2;
    reports.next({ miningPaused: true } as SystemInfo);
    expect(component.miningActionText).toBe('Resume');
    expect(component.miningActionDisabled).toBeFalse();
    acknowledgement.complete();
  });
});
