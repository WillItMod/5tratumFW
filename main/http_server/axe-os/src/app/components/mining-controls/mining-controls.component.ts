import { Component, Input, OnDestroy, OnInit } from '@angular/core';
import { FormArray, FormBuilder, FormGroup } from '@angular/forms';
import { Subject, finalize, takeUntil, timer } from 'rxjs';
import { ToastrService } from 'ngx-toastr';
import { SystemApiService } from 'src/app/services/system.service';
import { LiveDataService } from 'src/app/services/live-data.service';
import { MINING_TIMEZONES, MiningControlsService, MiningSchedule, MiningScheduleStatus, miningStateLabel, validateMiningSchedule } from 'src/app/services/mining-controls.service';

@Component({ selector: 'app-mining-controls', templateUrl: './mining-controls.component.html', host: { class: 'fw-mining-controls' } })
export class MiningControlsComponent implements OnInit, OnDestroy {
  @Input() uri = '';
  readonly timezones = MINING_TIMEZONES;
  readonly weekdays = [{ name: 'Mon', bit: 2 }, { name: 'Tue', bit: 4 }, { name: 'Wed', bit: 8 }, { name: 'Thu', bit: 16 }, { name: 'Fri', bit: 32 }, { name: 'Sat', bit: 64 }, { name: 'Sun', bit: 1 }];
  readonly form: FormGroup;
  supported: boolean | null = null;
  status: MiningScheduleStatus | null = null;
  statusReceivedAt = 0;
  readError: string | null = null;
  busy = false;
  saved = false;
  savingSchedule = false;
  returningToSchedule = false;
  measuredPower: number | null = null;
  private powerReceivedAt = 0;
  private now = Date.now();
  private loadedSavedSchedule = false;
  savedSchedule: MiningSchedule | null = null;
  private pendingRequest: boolean | null = null;
  private requestAt = 0;
  private overrideRequestAt = 0;
  private destroy$ = new Subject<void>();

  constructor(private fb: FormBuilder, private mining: MiningControlsService, private system: SystemApiService, private liveData: LiveDataService, private toastr: ToastrService) {
    this.form = fb.group({ enabled: false, timezone: 'UTC', windows: fb.array([]) });
  }
  get windows(): FormArray { return this.form.get('windows') as FormArray; }
  get schedule(): MiningSchedule { return this.form.getRawValue() as MiningSchedule; }
  get validationErrors(): string[] { return validateMiningSchedule(this.schedule); }
  get statusFresh(): boolean { return this.supported === true && !!this.statusReceivedAt && !this.readError && this.now - this.statusReceivedAt < 15000; }
  get observedPower(): number | null { return this.now - this.powerReceivedAt < 15000 ? this.measuredPower : null; }
  get stateLabel(): string {
    if (this.supported === false) return 'Mining confirmation unavailable';
    if (this.statusFresh && this.status?.error) return 'Mining needs attention';
    if (this.pendingRequest !== null) return this.pendingRequest ? 'Pause requested' : 'Resume requested';
    return miningStateLabel(this.status, this.statusFresh);
  }
  get actionDisabled(): boolean { return this.supported !== true || this.busy || this.returningToSchedule || !this.statusFresh || !!this.status?.transitionPending || this.status?.requestedPaused !== this.status?.appliedPaused || this.pendingRequest !== null; }
  get controlSource(): string {
    if (!this.statusFresh || !this.status) return 'Waiting for fresh device status';
    if (this.returningToSchedule) return 'Returning to schedule control…';
    if (this.status.manualOverride !== 'none') return this.status.manualOverride === 'paused' ? 'Manual pause' : 'Manual resume';
    if (!this.savedSchedule?.enabled) return 'Schedule disabled';
    if (!this.status.clockValid) return 'Waiting paused for network time';
    return this.status.scheduledPause ? 'Scheduled pause window active' : 'Outside pause windows · mining allowed';
  }

  ngOnInit(): void {
    timer(0, 1000).pipe(takeUntil(this.destroy$)).subscribe(() => this.now = Date.now());
    this.mining.observe(this.uri).pipe(takeUntil(this.destroy$)).subscribe(observation => {
      this.now = Date.now();
      this.readError = observation.error;
      const response = observation.response;
      if (!response) return;
      this.supported = response.supported;
      if (!response.supported || !response.schedule || !response.status) return;
      this.status = response.status;
      this.savedSchedule = response.schedule;
      this.statusReceivedAt = observation.receivedAt;
      if (this.returningToSchedule && observation.receivedAt > this.overrideRequestAt && this.status.manualOverride === 'none') this.returningToSchedule = false;
      if (!this.loadedSavedSchedule) {
        if (!this.form.dirty) this.loadSchedule(response.schedule);
        this.loadedSavedSchedule = true;
      }
      if (this.pendingRequest !== null && observation.receivedAt > this.requestAt && (this.status.error || (!this.status.transitionPending && this.status.appliedPaused === this.pendingRequest))) this.pendingRequest = null;
    });
    if (!this.uri) this.liveData.info$.pipe(takeUntil(this.destroy$)).subscribe(info => {
      this.measuredPower = Number.isFinite(info.power) && info.power >= 0 ? info.power : null;
      this.powerReceivedAt = this.liveData.lastUpdateAt;
    });
  }

  private loadSchedule(schedule: MiningSchedule): void {
    this.form.patchValue({ enabled: schedule.enabled, timezone: schedule.timezone });
    this.windows.clear();
    schedule.windows.forEach(window => this.windows.push(this.fb.group(window)));
    this.form.markAsPristine();
  }
  addWindow(): void {
    if (this.windows.length >= 8) return;
    this.windows.push(this.fb.group({ days: 127, start: '23:00', end: '07:00' }));
    this.form.markAsDirty();
    this.saved = false;
  }
  removeWindow(index: number): void { this.windows.removeAt(index); this.form.markAsDirty(); this.saved = false; }
  hasDay(index: number, bit: number): boolean { return !!(this.windows.at(index).get('days')?.value & bit); }
  toggleDay(index: number, bit: number, checked: boolean): void {
    const control = this.windows.at(index).get('days')!;
    control.setValue(checked ? control.value | bit : control.value & ~bit);
    control.markAsDirty();
  }
  overnight(index: number): boolean {
    const window = this.windows.at(index).value;
    return /^\d\d:\d\d$/.test(window.start) && /^\d\d:\d\d$/.test(window.end) && window.end < window.start;
  }
  saveSchedule(): void {
    if (this.supported !== true || this.busy || this.validationErrors.length || !this.form.dirty) return;
    const schedule = this.schedule;
    this.busy = true;
    this.savingSchedule = true;
    this.form.disable({ emitEvent: false });
    this.mining.saveSchedule(schedule, this.uri).pipe(finalize(() => { this.busy = false; this.savingSchedule = false; this.form.enable({ emitEvent: false }); }), takeUntil(this.destroy$)).subscribe({
      next: () => { this.pendingRequest = null; this.returningToSchedule = false; this.form.markAsPristine(); this.saved = true; this.loadedSavedSchedule = false; this.mining.refresh(); this.toastr.success('Mining schedule saved on the device'); },
      error: () => this.toastr.error('Could not save the mining schedule. Your draft is unchanged.')
    });
  }
  setPaused(paused: boolean): void {
    if (this.actionDisabled) return;
    this.busy = true;
    this.pendingRequest = paused;
    this.requestAt = Date.now();
    const action = paused ? this.system.pauseMining(this.uri) : this.system.resumeMining(this.uri);
    action.pipe(finalize(() => this.busy = false), takeUntil(this.destroy$)).subscribe({
      next: () => { this.mining.refresh(); this.toastr.info(paused ? 'Pause requested. Waiting for ASIC confirmation.' : 'Resume requested. Waiting for ASIC confirmation.'); },
      error: () => { this.pendingRequest = null; this.toastr.error('Could not change the mining state'); }
    });
  }
  returnToSchedule(): void {
    if (this.supported !== true || this.busy || this.returningToSchedule || !this.statusFresh || this.status?.manualOverride === 'none') return;
    this.busy = true;
    this.returningToSchedule = true;
    this.overrideRequestAt = Date.now();
    this.mining.returnToSchedule(this.uri).pipe(finalize(() => this.busy = false), takeUntil(this.destroy$)).subscribe({
      next: () => { this.pendingRequest = null; this.mining.refresh(); this.toastr.info('Schedule control requested. Waiting for device status.'); },
      error: () => { this.returningToSchedule = false; this.toastr.error('Could not return to schedule control'); }
    });
  }
  ngOnDestroy(): void { this.destroy$.next(); this.destroy$.complete(); }
}
