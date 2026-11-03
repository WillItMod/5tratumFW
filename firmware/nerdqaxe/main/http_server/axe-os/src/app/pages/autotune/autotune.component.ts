import { Component, OnDestroy, OnInit } from '@angular/core';
import { EMPTY, Subject, catchError, exhaustMap, filter, finalize, map, switchMap, take, takeUntil, tap, throwError, timeout, timer } from 'rxjs';
import { AutotuneService } from '../../services/autotune.service';
import { AutotuneConfig, AutotunePoint, AutotuneReport, AutotuneSearch, AutotuneTelemetry } from '../../models/IAutotune';

@Component({ selector: 'app-autotune', templateUrl: './autotune.component.html', styleUrls: ['./autotune.component.scss', './autotune-goals.scss', './autotune-observations.scss', './autotune-readings.scss'] })
export class AutotuneComponent implements OnInit, OnDestroy {
  report: AutotuneReport | null = null;
  edit: AutotuneConfig | null = null;
  busy = false;
  loading = false;
  readError = '';
  actionError = '';
  message = '';
  waiverAccepted = false;
  readonly wholeMHz = true;
  readonly priorities = [
    {value:'efficiency', name:'Standard Glazed', level:'Level 1', goal:'Efficiency', description:'Seek lower energy per useful hash. A slower clock can win when the reduction in power outweighs the lost hashrate.'},
    {value:'balanced', name:'Jam Filled', level:'Level 2', goal:'Balanced', description:'Weigh useful hashrate against energy use. Explore nearby clock and voltage points without choosing a fixed power preset.'},
    {value:'performance', name:'Cream Filled, Cherry on Top', level:'Level 3', goal:'Performance', description:'Seek the highest sustained useful hashrate that passes measurement checks and stays within your limits, including your saved clock ceiling. It cannot search above that ceiling.'},
  ];
  private readonly destroyed = new Subject<void>();
  private requestedAt = -Infinity;
  private readEpoch = 0;
  private baseline = '';
  private startUnconfirmed = false;
  // These are tab observations, not the controller's unique counter samples.
  readonly historyLimit = 120;
  history: {at: number; ths: number | null; joules: number | null}[] = [];
  private historyRun: number | null = null;
  private historyGap = false;


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
        if (epoch === this.readEpoch) { this.historyGap = true; this.readError = 'Live tuning status is unavailable. Check the connection and refresh before changing tuning.'; }
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
      error: error => { this.historyGap = true; this.readError = [404,501].includes(error?.status)
        ? 'Auto-tuning requires a matching 5tratumFW application and web interface.'
        : 'Could not read tuning status. Check the connection and refresh.'; },
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
    if (!this.validReport(report)) { this.historyGap = true; this.readError = 'This application returned an unrecognised tuning report. Install its matching application and web interface.'; return false; }
    const dirty = this.hasUnsavedChanges;
    this.report = report;
    this.requestedAt = requestedAt;
    this.readError = '';
    this.observe();
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
  get runningGoal(): string {
    const priority = this.priorities.find(item => item.value === this.report?.config?.priority);
    return this.isFresh && priority ? `${priority.goal} · ${priority.name}` : 'Unavailable';
  }
  get ownershipText(): string {
    if (!this.isFresh) return 'Status unavailable';
    if (this.report!.status.state === 'restoring') return 'Restoration in progress';
    if (this.report!.status.active) return 'Run in progress';
    if (this.report!.status.ownsSettings) return 'Holding result · Disable and restore to release control';
    return 'Explicit Start required';
  }
  get activityText(): string {
    if (!this.isFresh) return 'Waiting for a fresh report. Earlier observations are kept below; they do not show the miner’s current condition.';
    const report = this.report!;
    if (report.status.telemetry.fault === true) return 'The miner reports an active protection fault. Tuning cannot clear it or force settings through it; check the miner’s reason below.';
    if (!report.supported) return 'Automatic tuning is unavailable for this hardware or operating state. Restore remains available when the miner has a backup or owns settings.';
    const reasons: Record<string,string> = {
      'temperature-limit-backoff':'Backing off because a saved temperature ceiling is close or exceeded. Safety can require a larger step than a normal experiment.',
      'power-limit-backoff':'Backing off because the reported power ceiling is close or exceeded. This is the miner’s power reading, not wall power.',
      'new-user-limits':'Adjusting to newly saved search limits. The controller must stay inside those bounds.',
      'error-recovery-voltage-trial':'Trying a small voltage increase to reduce ASIC timing errors, after checking temperature and power headroom. It must still settle and pass measurement checks.',
      'error-recovery-clock-backoff':'Reducing clock to recover from excessive ASIC errors. Voltage recovery is not allowed without sufficient headroom.',
      'restoring-better-measured-point':'Returning to a previously measured better point after a comparison failed its acceptance checks. This is separate from restoring the original backup.',
      'rollback-lacks-headroom':'Backing off because returning to the measured best point would lack temperature or power headroom.',
      'held-point-became-unstable':'The held result no longer meets error or stability requirements. Reducing load before taking new measurements.',
      'waiting-for-valid-telemetry':'Waiting for fresh, valid counters and sensors. No valid comparison window can be completed yet.',
      'conditions-changed-remeasuring':'Conditions changed, so the controller is measuring a new round reference before further comparisons.',
      'manual-control':'Manual controls took ownership. Tuning has yielded; it does not overwrite the manual change.',
      'manual-settings-changed':'Settings changed outside the tuner. Tuning has yielded to the other control.',
      'Manual or scheduled settings took ownership':'Manual or scheduled settings took ownership. Tuning has yielded and does not overwrite that change.',
      'External control took ownership':'Another control took ownership. Tuning has yielded and does not overwrite that change.',
      'Firmware update took ownership':'A firmware update took ownership. Tuning is interrupted; the update does not authorize another tuning run.',
      'Restart requested':'A restart took ownership. Restarting does not automatically start another tuning run.',
      'Shutdown requested':'Shutdown took ownership. Tuning cannot force settings through shutdown protection.',
      'An ASIC reports too little work for reliable tuning':'An ASIC reports too little useful work for a reliable comparison. The controller needs valid work from the required chain before tuning.',
      'Original settings restored':'The miner confirms the original pre-tuning settings were restored. Saved tuning options do not start another run.',
      'backup-restored':'The miner confirms the original pre-tuning backup was restored. Saved tuning options do not start another run.',
    };
    if (reasons[report.status.reason]) return reasons[report.status.reason];
    if (report.status.state !== 'restoring' && !this.telemetryFresh) return 'Waiting for fresh, valid counters and sensor readings. Current readings and safe tuning readiness cannot be confirmed.';
    const messages: Record<string,string> = {
      disabled:'The controller is off. Saved priorities and Continuous On do not start a run; Start always needs your explicit consent.',
      baseline:'Measuring a reference for this search round before comparing nearby settings. The original settings backup stays separate.',
      settling:'Letting the requested clock and voltage settle before collecting a comparison window.',
      measuring:'Collecting useful hashrate, reported power, ASIC errors and temperatures at the current request. A noisy or unsafe window can be rejected.',
      applying:'Requesting an adjustment within the saved bounds. A request is not a measurement of the ASIC rail or clock.',
      holding:'Holding the result of a completed bounded round. This does not guarantee an improvement over its reference.',
      observing:'Watching for a meaningful change in conditions before another round. Continuous tuning can keep the same setting when conditions stay stable.',
      suspended:'Adjustments are suspended. Check the miner’s reason and cooling, temperature, mining and sensor status.',
      faulted:'The miner reports a fault. Tuning cannot clear a protection fault or force a restore through unsafe conditions.',
      restoring:'Restoring the retained original settings. The request is complete only after the miner confirms restoration and releases control.',
    };
    return messages[this.report!.status.state] || 'Reading the controller’s current state.';
  }
  get search(): AutotuneSearch | null {
    const value = this.report?.status.search;
    if (!this.isFresh || !value || typeof value !== 'object' || Array.isArray(value)) return null;
    const seconds = (n: unknown) => n === null || this.finite(n) && n >= 0;
    const count = (n: unknown) => this.finite(n) && Number.isInteger(n) && n >= 0;
    const means = (n: unknown) => n === null || !!n && typeof n === 'object' && !Array.isArray(n)
      && this.finite((n as any).hashRateGHs) && (n as any).hashRateGHs > 0 && this.finite((n as any).powerW) && (n as any).powerW > 0 && this.energyValue((n as any).hashRateGHs,(n as any).powerW) !== null;
    return seconds(value.phaseElapsedSeconds) && seconds(value.minimumSeconds)
      && ((value.phaseElapsedSeconds === null && value.minimumSeconds === null) || (value.phaseElapsedSeconds !== null && value.minimumSeconds !== null && value.minimumSeconds > 0))
      && [value.samples,value.minimumSamples,value.roundExperiments,value.roundExperimentLimit,value.rejectedWindows].every(count)
      && value.minimumSamples > 0 && value.roundExperimentLimit > 0 && value.roundExperiments <= value.roundExperimentLimit
      && means(value.reference) && means(value.best) ? value : null;
  }
  get phaseProgress(): number {
    const value = this.search;
    return value?.minimumSeconds && value.phaseElapsedSeconds !== null ? Math.min(100,100*(value.phaseElapsedSeconds/value.minimumSeconds)) : 0;
  }
  duration(value: number | null): string { return this.finite(value) ? `${Math.floor(value)} s` : '—'; }
  ths(value: unknown, live = true): string { return (!live || this.telemetryFresh) && this.finite(value) ? (value/1000).toFixed(2) : '—'; }
  private energyValue(hashRateGHs: unknown, powerW: unknown): number | null {
    if (!this.finite(hashRateGHs) || hashRateGHs <= 0 || !this.finite(powerW) || powerW <= 0) return null;
    const value = powerW / (hashRateGHs / 1000);
    return Number.isFinite(value) && value > 0 ? value : null;
  }
  energy(hashRateGHs: unknown, powerW: unknown, live = true): string {
    const value = this.energyValue(hashRateGHs,powerW);
    return (!live || this.telemetryFresh) && value !== null ? value.toFixed(2) : '—';
  }
  get headroom(): {name: string; value: string; used: number; over: boolean}[] {
    if (!this.optionsUsable || !this.telemetryFresh) return [];
    const config = this.report!.config, t = this.report!.status.telemetry;
    return [{name:'ASIC',current:t.asicTempC!,limit:config.maxAsicTempC,unit:'°C'},
      {name:'VRM',current:t.vrmTempC!,limit:config.maxVrmTempC,unit:'°C'},
      {name:'Reported power',current:t.powerW!,limit:config.maxPowerW,unit:'W'}].map(row => ({name:row.name,
        value:`${Math.abs(row.limit-row.current).toFixed(1)} ${row.unit} ${row.current > row.limit ? 'over ceiling' : 'below ceiling'}`,
        used:Math.max(0,Math.min(100,100*row.current/row.limit)),over:row.current > row.limit}));
  }
  private observe(): void {
    const at = performance.now(), run = this.report!.status.runId;
    if (this.historyRun !== run) { this.history = []; this.historyRun = run; this.historyGap = false; }
    const last = this.history[this.history.length-1];
    if (last && (this.historyGap || at-last.at > 12000)) this.history.push({at:at-1,ths:null,joules:null});
    const t = this.report!.status.telemetry;
    const valid = this.telemetryFresh;
    this.history.push({at,ths:valid ? t.hashRateGHs!/1000 : null,joules:valid ? this.energyValue(t.hashRateGHs,t.powerW) : null});
    this.history = this.history.filter(point => at-point.at <= 600000).slice(-this.historyLimit);
    this.historyGap = !valid;
  }
  get trends(): {name: string; unit: string; path: string; points: {x:number;y:number}[]; minimum:string; maximum:string}[] {
    return ([{name:'Useful hashrate',unit:'TH/s',key:'ths'},{name:'Energy per useful hash',unit:'J/TH',key:'joules'}] as const).map(series => {
      const values = this.history.map(point => point[series.key]).filter((n): n is number => n !== null);
      const min = values.length ? Math.min(...values) : 0, max = values.length ? Math.max(...values) : 0;
      const padding = Math.max((max-min)*.15,max*.01,.01), low=Math.max(0,min-padding), high=Number.isFinite(max+padding) ? max+padding : max;
      const first = this.history[0]?.at || 0, last = this.history[this.history.length-1]?.at || first;
      let connected = false; const points: {x:number;y:number}[] = [];
      const path = this.history.map(point => {
        const value = point[series.key];
        if (value === null) { connected=false; return ''; }
        const x=8+584*(point.at-first)/Math.max(last-first,1000), y=88-72*((value-low)/(high-low));
        const command=connected ? 'L' : 'M'; connected=true; points.push({x,y}); return `${command}${x.toFixed(2)},${y.toFixed(2)}`;
      }).join(' ');
      return {name:series.name,unit:series.unit,path,points,minimum:values.length ? low.toFixed(2) : '—',maximum:values.length ? high.toFixed(2) : '—'};
    });
  }
  get historySpan(): string {
    if (this.history.length < 2) return 'Waiting for more tab observations';
    const clock = (at: number) => new Date(performance.timeOrigin+at).toLocaleTimeString([], {hour:'2-digit',minute:'2-digit',second:'2-digit'});
    return `${clock(this.history[0].at)} – ${clock(this.history[this.history.length-1].at)} · run ${this.historyRun}`;
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
