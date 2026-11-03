import { Component, OnDestroy, OnInit } from '@angular/core';
import { EMPTY, Subject, catchError, exhaustMap, filter, finalize, map, switchMap, take, takeUntil, tap, throwError, timeout, timer } from 'rxjs';
import { AutotuneService } from '../../services/autotune.service';
import { AutotuneConfig, AutotunePoint, AutotuneReport, AutotuneTelemetry } from '../../models/IAutotune';

@Component({ selector: 'app-autotune', templateUrl: './autotune.component.html', styleUrls: ['./autotune.component.scss'] })
export class AutotuneComponent implements OnInit, OnDestroy {
  report: AutotuneReport | null = null;
  edit: AutotuneConfig | null = null;
  busy = false;
  loading = false;
  readError = '';
  actionError = '';
  message = '';
  waiverAccepted = false;
  readonly wholeMHz = false;
  readonly priorities = [
    {value:'efficiency', name:'Standard Glazed', level:'Level 1', goal:'Efficiency', description:'Favour useful hashes per watt and lower heat.'},
    {value:'balanced', name:'Jam Filled', level:'Level 2', goal:'Balanced', description:'Balance useful hashrate, efficiency and thermal headroom.'},
    {value:'performance', name:'Cream Filled, Cherry on Top', level:'Level 3', goal:'Performance', description:'Favour sustained useful hashrate within the same protection limits.'},
  ];
  private readonly destroyed = new Subject<void>();
  private requestedAt = -Infinity;
  private readEpoch = 0;
  private baseline = '';
  private startUnconfirmed = false;

  constructor(private service: AutotuneService) {}

  ngOnInit(): void {
    this.load();
    timer(5000,5000).pipe(filter(() => !this.busy && !this.loading), exhaustMap(() => {
      const epoch = this.readEpoch;
      const requestedAt = performance.now();
      return this.service.get().pipe(switchMap(report => {
        if (epoch === this.readEpoch) this.accept(report, requestedAt);
        return EMPTY;
      }), catchError(() => {
        if (epoch === this.readEpoch) this.readError = 'Live tuning status is unavailable. Check the connection and refresh before changing tuning.';
        return EMPTY;
      }));
    }), takeUntil(this.destroyed)).subscribe();
  }
  ngOnDestroy(): void { this.destroyed.next(); this.destroyed.complete(); }

  load(): void {
    if (this.busy || this.loading) return;
    this.loading = true;
    const epoch = ++this.readEpoch;
    const requestedAt = performance.now();
    this.service.get().pipe(takeUntil(this.destroyed),finalize(() => this.loading = false)).subscribe({
      next: report => { if (epoch === this.readEpoch) this.accept(report, requestedAt); },
      error: error => this.readError = [404,501].includes(error?.status)
        ? 'Auto-tuning requires a matching 5tratumFW application and web interface.'
        : 'Could not read tuning status. Check the connection and refresh.',
    });
  }
  private config(value: AutotuneConfig): AutotuneConfig {
    return {priority:value.priority,continuous:value.continuous,maxAsicTempC:value.maxAsicTempC,maxVrmTempC:value.maxVrmTempC,
      minFrequencyMHz:value.minFrequencyMHz,maxFrequencyMHz:value.maxFrequencyMHz,minVoltageMv:value.minVoltageMv,
      maxVoltageMv:value.maxVoltageMv,maxPowerW:value.maxPowerW};
  }
  private finite(value: unknown): value is number { return typeof value === 'number' && Number.isFinite(value); }
  private pointValid(value: AutotunePoint | null): boolean {
    return value === null || !!value && this.finite(value.frequencyMHz) && value.frequencyMHz > 0 && this.finite(value.coreVoltageMv) && value.coreVoltageMv > 0;
  }
  private validConfig(value: AutotuneConfig): boolean {
    return !!value && ['efficiency','balanced','performance'].includes(value.priority) && typeof value.continuous === 'boolean'
      && [value.maxAsicTempC,value.maxVrmTempC,value.minFrequencyMHz,value.maxFrequencyMHz,value.minVoltageMv,value.maxVoltageMv,value.maxPowerW].every(number => this.finite(number) && number > 0)
      && value.minFrequencyMHz <= value.maxFrequencyMHz && value.minVoltageMv <= value.maxVoltageMv;
  }
  private unitsValid(value: AutotuneConfig): boolean {
    return Number.isInteger(value.minVoltageMv) && Number.isInteger(value.maxVoltageMv)
      && (!this.wholeMHz || Number.isInteger(value.minFrequencyMHz) && Number.isInteger(value.maxFrequencyMHz));
  }
  private validReport(report: AutotuneReport): boolean {
    // Restore remains reachable when options or current protection limits are
    // unusable. Only known API identity, state and bound backup metadata admit
    // a report; separate checks authorize configuration and Start.
    return report?.schemaVersion === 1 && report.waiverVersion === 1 && typeof report.supported === 'boolean'
      && !!report.hardware && [report.hardware.model,report.hardware.asicSensor,report.hardware.vrmSensor,report.hardware.powerSource].every(value => typeof value === 'string')
      && Number.isInteger(report.hardware.asicCount) && report.hardware.asicCount >= 0 && report.hardware.asicCount <= 64
      && !!report.status && !!report.status.telemetry && typeof report.status.telemetry === 'object' && !Array.isArray(report.status.telemetry) && typeof report.status.active === 'boolean' && typeof report.status.ownsSettings === 'boolean'
      && ['disabled','baseline','settling','measuring','holding','observing','suspended','faulted','applying','restoring'].includes(report.status.state)
      && typeof report.status.reason === 'string' && Number.isInteger(report.status.runId) && report.status.runId >= 0
      && typeof report.backup?.available === 'boolean' && typeof report.backup.restored === 'boolean'
      && (!report.backup.available || [1,4,8].includes(report.hardware.asicCount)
        && this.finite(report.backup.frequencyMHz) && report.backup.frequencyMHz > 0
        && this.finite(report.backup.coreVoltageMv) && report.backup.coreVoltageMv > 0);
  }
  get optionsUsable(): boolean {
    const report = this.report; const limits = report?.limits;
    return !!report && report.supported && this.validConfig(report.config) && this.unitsValid(report.config) && !!limits
      && report.guidance?.experimental === true && [report.guidance.asic,report.guidance.vrm,report.guidance.power].every(value => typeof value === 'string')
      && [1,4,8].includes(report.hardware.asicCount)
      && [limits.minFrequencyMHz,limits.maxFrequencyMHz,limits.minVoltageMv,limits.maxVoltageMv,limits.maxAsicTempC,limits.maxVrmTempC,limits.maxPowerW,limits.frequencyStepMHz,limits.voltageStepMv].every(value => this.finite(value) && value > 0)
      && limits.minFrequencyMHz <= limits.maxFrequencyMHz && limits.minVoltageMv <= limits.maxVoltageMv;
  }
  private validTelemetry(telemetry: AutotuneTelemetry | undefined): boolean {
    return !!telemetry && telemetry.valid === true && typeof telemetry.fanOk === 'boolean' && typeof telemetry.miningActive === 'boolean' && typeof telemetry.fault === 'boolean'
      && [telemetry.asicTempC,telemetry.vrmTempC,telemetry.powerW,telemetry.hashRateGHs,telemetry.errorPercentage,telemetry.worstErrorPercentage,telemetry.voltageReadbackMv,telemetry.sampleAgeSeconds].every(value => this.finite(value) && value >= 0)
      && telemetry.errorPercentage! <= 100 && telemetry.worstErrorPercentage! <= 100;
  }
  private accept(report: AutotuneReport, requestedAt: number): boolean {
    if (!this.validReport(report)) { this.readError = 'This application returned an unrecognised tuning report. Install its matching application and web interface.'; return false; }
    const dirty = this.hasUnsavedChanges;
    this.report = report;
    this.requestedAt = requestedAt;
    this.readError = '';
    if (this.optionsUsable && (!this.edit || !dirty)) {
      this.edit = this.config(report.config);
      this.baseline = JSON.stringify(this.edit);
    }
    return true;
  }
  get isFresh(): boolean {
    const age = performance.now() - this.requestedAt;
    return !!this.report && !this.readError && Number.isFinite(age) && age >= 0 && age <= 12000;
  }
  get hasUnsavedChanges(): boolean { return !!this.edit && JSON.stringify(this.config(this.edit)) !== this.baseline; }
  get telemetryFresh(): boolean {
    const telemetry = this.report?.status?.telemetry;
    const elapsed = (performance.now() - this.requestedAt) / 1000;
    return this.isFresh && this.validTelemetry(telemetry) && this.finite(telemetry!.sampleAgeSeconds)
      && elapsed >= 0 && telemetry!.sampleAgeSeconds + elapsed <= 15;
  }
  get validationError(): string {
    const value = this.edit; const limits = this.report?.limits;
    if (!this.optionsUsable) return 'Tuning options or hardware limits are unavailable. Restore the backup or refresh after the miner has recovered.';
    if (value && this.wholeMHz && (!Number.isInteger(value.minFrequencyMHz) || !Number.isInteger(value.maxFrequencyMHz))) return 'Use whole MHz for this miner’s clock bounds.';
    if (value && (!Number.isInteger(value.minVoltageMv) || !Number.isInteger(value.maxVoltageMv))) return 'Use whole millivolts for core-voltage bounds.';
    if (!value || !limits || !this.validConfig(value)) return 'Enter valid positive limits and choose a tuning priority.';
    if (value.minFrequencyMHz < limits.minFrequencyMHz || value.maxFrequencyMHz > limits.maxFrequencyMHz) return 'Clock limits must stay inside the hardware limits shown below.';
    if (value.minVoltageMv < limits.minVoltageMv || value.maxVoltageMv > limits.maxVoltageMv) return 'Voltage limits must stay inside the hardware limits shown below.';
    if (value.maxAsicTempC > limits.maxAsicTempC || value.maxVrmTempC > limits.maxVrmTempC) return 'Temperature ceilings must stay below the protected hardware limits shown below.';
    if (value.maxPowerW > limits.maxPowerW) return 'The power ceiling exceeds this hardware limit.';
    return '';
  }
  get canSave(): boolean { return !this.busy && !this.loading && this.isFresh && this.optionsUsable && this.hasUnsavedChanges && !this.validationError; }
  get canStart(): boolean {
    const report = this.report;
    return !!report && !this.busy && !this.loading && this.isFresh && this.optionsUsable && !report.status.active
      && !['faulted','restoring','applying'].includes(report.status.state) && !report.status.ownsSettings && !this.hasUnsavedChanges
      && !this.validationError && this.telemetryFresh && report.status.telemetry.fanOk === true
      && report.status.telemetry.miningActive === true && report.status.telemetry.fault === false && this.waiverAccepted;
  }
  get canRestore(): boolean { return !this.busy && !this.loading && (this.report?.backup?.available === true || this.report?.status.ownsSettings === true || this.startUnconfirmed); }
  get stateLabel(): string {
    if (!this.isFresh) return this.report ? 'Status unavailable' : 'Connecting';
    return ({disabled:'Disabled',qualifying:'Checking miner',baseline:'Measuring baseline',evaluating:'Evaluating',applying:'Adjusting',restoring:'Restoring backup',settling:'Settling',
      measuring:'Measuring',holding:'Holding result',observing:'Monitoring conditions',suspended:'Suspended',faulted:'Faulted'} as Record<string,string>)[this.report?.status.state || ''] || 'Checking status';
  }
  get reasonText(): string { return this.report?.status?.reason?.replace(/[-_]/g,' ') || ''; }
  metric(value: unknown, digits = 1): string { return this.telemetryFresh && this.finite(value) ? value.toFixed(digits) : '—'; }
  pointText(value: AutotunePoint | null | undefined): string {
    return value && this.pointValid(value) ? `${value.frequencyMHz.toFixed(2).replace(/\.?0+$/,'')} MHz · ${value.coreVoltageMv.toFixed(0)} mV` : '—';
  }
  save(): void {
    if (!this.canSave) return;
    const payload = this.config(this.edit!);
    this.execute({action:'configure',config:payload}, report => JSON.stringify(this.config(report.config)) === JSON.stringify(payload),
      'Tuning options saved.');
  }
  start(): void {
    if (!this.canStart) return;
    const previousRun = this.report!.status.runId;
    this.waiverAccepted = false;
    this.execute({action:'start',waiverAccepted:true,waiverVersion:1}, report => report.status.active && report.status.runId > previousRun,
      'Tuning started on the miner. You can close this page; the miner runs the controller.');
  }
  restore(): void {
    if (!this.canRestore) return;
    this.waiverAccepted = false;
    this.execute({action:'stop'}, report => !report.status.active && !report.status.ownsSettings && report.backup.restored === true && this.backupMatches(report),
      'Auto-tuning disabled and the pre-tuning backup restored.');
  }
  private execute(body: Record<string,unknown>, confirms: (report: AutotuneReport) => boolean, success: string): void {
    this.busy = true;
    this.readEpoch++;
    this.actionError = this.message = '';
    this.service.authorize().pipe(take(1),switchMap(({totp}) => {
      if (body['action'] === 'start') this.startUnconfirmed = true;
      return this.service.command(body,totp);
    }),switchMap(ack => {
      if (ack?.ok !== true || ack.restartRequired !== false) return throwError(() => new Error('unconfirmed'));
      // Retry reads only while a bounded asynchronous transition completes.
      // The state-changing request is never repeated.
      return timer(0,1000).pipe(exhaustMap(() => {
        const requestedAt = performance.now();
        return this.service.get().pipe(map(report => ({report,requestedAt})));
      }),tap(({report,requestedAt}) => {
        if (!this.accept(report,requestedAt)) throw new Error('unconfirmed');
        if (report.status.state === 'faulted') throw new Error('transition-fault');
      }),filter(({report}) => confirms(report)),take(1),timeout({first:30000}));
    }),takeUntil(this.destroyed),finalize(() => this.busy = false)).subscribe({
      next: ({report}) => {
        this.startUnconfirmed = false;
        if (body['action'] === 'configure') { this.edit = this.config(report.config); this.baseline = JSON.stringify(this.edit); }
        this.message = success;
      },
      error: () => { this.actionError = 'The request could not be confirmed. Refresh status and check the miner before trying again.'; },
    });
  }
  private backupMatches(report: AutotuneReport): boolean {
    const point = report.status.current; const backup = report.backup;
    return backup.available === true && !!point && this.finite(backup.frequencyMHz) && this.finite(backup.coreVoltageMv)
      && Math.abs(point.frequencyMHz - backup.frequencyMHz) < .01 && Math.abs(point.coreVoltageMv - backup.coreVoltageMv) < .01;
  }
  canLeave(): boolean { return !this.hasUnsavedChanges || window.confirm('Leave this page and discard its unsaved tuning options?'); }
}
export function leaveAutotune(component: AutotuneComponent): boolean { return component.canLeave(); }
