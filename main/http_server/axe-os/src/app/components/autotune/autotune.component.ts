import { Component, OnDestroy, OnInit, Optional } from '@angular/core';
import { EMPTY, Subject, catchError, exhaustMap, filter, finalize, map, switchMap, take, takeUntil, tap, throwError, timeout, timer } from 'rxjs';
import { AutotuneService } from '../../services/autotune.service';
import { AutotuneActivityService } from '../../services/autotune-activity.service';
import { AutotuneConfig, AutotuneRestoration, AutotuneConditions, AutotuneRecovery, AutotuneEvent, AutotunePoint, AutotuneReadiness, AutotuneReport, AutotuneSearch, AutotuneTelemetry } from '../../models/IAutotune';

type ObservedMetric = 'ths' | 'joules' | 'asicTempC' | 'vrmTempC' | 'powerW' | 'boardErrors' | 'worstErrors';
interface TabObservation { at: number; ths: number | null; joules: number | null; asicTempC: number | null; vrmTempC: number | null; powerW: number | null; boardErrors: number | null; worstErrors: number | null; }
interface ObservedLine { name: string; secondary: boolean; path: string; points: {x:number;y:number}[]; }

@Component({ selector: 'app-autotune', templateUrl: './autotune.component.html', styleUrls: ['./autotune.component.scss', './autotune-goals.scss', './autotune-observations.scss', './autotune-readings.scss', './autotune-activity.scss', './autotune-events.scss', './autotune-flow.scss'] })
export class AutotuneComponent implements OnInit, OnDestroy {
  report: AutotuneReport | null = null;
  edit: AutotuneConfig | null = null;
  busy = false;
  loading = false;
  readError = '';
  actionError = '';
  message = '';
  showAllReadiness = false;
  diagnosticsText = '';
  diagnosticsMessage = '';
  waiverAccepted = false;
  advancedClockWarningAccepted = false;
  viewMode: 'live' | 'options' = 'options';
  private viewInitialized = false;
  readonly wholeMHz = false;
  readonly priorities = [
    {value:'efficiency', name:'Standard Glazed', level:'Level 1', goal:'Efficiency', description:'Seek lower energy per useful hash. A slower clock can win when the reduction in power outweighs the lost hashrate.'},
    {value:'balanced', name:'Jam Filled', level:'Level 2', goal:'Balanced', description:'Weigh useful hashrate against energy use. Explore nearby clock and voltage points without choosing a fixed power preset.'},
    {value:'performance', name:'Cream Filled, Cherry on Top', level:'Level 3', goal:'Performance', description:'Seek the highest sustained useful hashrate that passes measurement checks and stays within your limits, including your saved clock ceiling. It cannot search above that ceiling.'},
  ];
  private readonly destroyed = new Subject<void>();
  private requestedAt = -Infinity;
  private heartbeat: {runId:number;active:boolean;ownsSettings:boolean;controllerAgeSeconds:number|null}|null=null;
  private heartbeatAt=-Infinity;
  private heartbeatAdvertised=false;
  private heartbeatLegacy=false;
  private heartbeatAvailable=false;
  private readEpoch = 0;
  private baseline = '';
  private startUnconfirmed = false;
  private recoveryUnconfirmed = false;
  private recoverySubmittedAt = Infinity;
  private readonly cancelActionReadback = new Subject<void>();
  private pendingAction = '';
  private actionAcknowledged = false;
  // These are tab observations, not the controller's unique counter samples.
  readonly historyLimit = 120;
  history: TabObservation[] = [];
  private historyRun: number | null = null;
  private historyGap = false;
  readonly eventHistoryLimit = 100;
  events: AutotuneEvent[] = [];
  eventGaps: {beforeId:number;missing:number}[] = [];
  eventFeedAvailable = false;
  private eventRun: number | null = null;
  visibleEventCount = 12;


  constructor(private service: AutotuneService, @Optional() private activityService?: AutotuneActivityService) {}

  ngOnInit(): void {
    this.load();
    if(this.activityService) timer(0,5000).pipe(exhaustMap(()=>{
      const at=performance.now();
      return this.activityService!.get().pipe(tap(value=>this.acceptHeartbeat(value,at)),catchError(error=>{
        this.heartbeatAvailable=false;
        this.heartbeatLegacy=error?.status===404&&!this.heartbeatAdvertised;
        return EMPTY;
      }));
    }),takeUntil(this.destroyed)).subscribe();
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
  ngOnDestroy(): void { this.destroyed.next(); this.destroyed.complete(); this.cancelActionReadback.complete(); }

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
      maxVoltageMv:value.maxVoltageMv,maxPowerW:value.maxPowerW,
      ...(this.advancedClockAvailable ? {advancedClockOverride:value.advancedClockOverride === true} : {})};
  }
  private finite(value: unknown): value is number { return typeof value === 'number' && Number.isFinite(value); }
  private pointValid(value: AutotunePoint | null): boolean {
    return value === null || !!value && this.finite(value.frequencyMHz) && value.frequencyMHz > 0 && this.finite(value.coreVoltageMv) && value.coreVoltageMv > 0;
  }
  private validConfig(value: AutotuneConfig): boolean {
    return !!value && ['efficiency','balanced','performance'].includes(value.priority) && typeof value.continuous === 'boolean'
      && (value.advancedClockOverride === undefined || typeof value.advancedClockOverride === 'boolean')
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
    return !!report && report.supported && this.clockMetadataValid() && this.validConfig(report.config) && this.unitsValid(report.config) && !!limits
      && report.guidance?.experimental === true && [report.guidance.asic,report.guidance.vrm,report.guidance.power].every(value => typeof value === 'string')
      && [1,4,8].includes(report.hardware.asicCount)
      && [limits.minFrequencyMHz,limits.maxFrequencyMHz,limits.minVoltageMv,limits.maxVoltageMv,limits.maxAsicTempC,limits.maxVrmTempC,limits.maxPowerW,limits.frequencyStepMHz,limits.voltageStepMv].every(value => this.finite(value) && value > 0)
      && limits.minFrequencyMHz <= limits.maxFrequencyMHz && limits.minVoltageMv <= limits.maxVoltageMv;
  }
  get advancedClockAvailable(): boolean {
    const report=this.report, limits=report?.limits;
    return !!report && typeof report.config?.advancedClockOverride==='boolean' && !!limits
      && this.finite(limits.maxFrequencyMHz) && limits.maxFrequencyMHz>0
      && this.finite(limits.advancedMaxFrequencyMHz) && limits.advancedMaxFrequencyMHz>=limits.maxFrequencyMHz;
  }
  private clockMetadataValid(): boolean {
    const report=this.report;
    return !!report && (report.config?.advancedClockOverride===undefined && report.limits?.advancedMaxFrequencyMHz===undefined
      || this.advancedClockAvailable);
  }
  get maximumClockMHz(): number {
    const limits=this.report?.limits;
    return this.advancedClockAvailable && this.edit?.advancedClockOverride===true && this.advancedClockWarningAccepted
      ? limits!.advancedMaxFrequencyMHz! : limits?.maxFrequencyMHz || 0;
  }
  setAdvancedClockAcknowledgement(accepted: boolean): void {
    if (!this.advancedClockAvailable || !this.edit || !this.isFresh || this.optionsLocked) return;
    this.onOptionsEdit();
    this.advancedClockWarningAccepted=accepted===true;
    this.edit.advancedClockOverride=accepted===true;
    this.waiverAccepted=false;
  }
  private clockWarningChanged(previous: AutotuneReport, next: AutotuneReport): boolean {
    return previous.hardware.model!==next.hardware.model || previous.hardware.asicCount!==next.hardware.asicCount
      || previous.limits?.maxFrequencyMHz!==next.limits?.maxFrequencyMHz
      || previous.limits?.advancedMaxFrequencyMHz!==next.limits?.advancedMaxFrequencyMHz
      || previous.config?.advancedClockOverride!==next.config?.advancedClockOverride
      || previous.config?.maxFrequencyMHz!==next.config?.maxFrequencyMHz;
  }
  private validTelemetry(telemetry: AutotuneTelemetry | undefined): boolean {
    return !!telemetry && telemetry.valid === true && typeof telemetry.fanOk === 'boolean' && typeof telemetry.miningActive === 'boolean' && typeof telemetry.fault === 'boolean'
      && [telemetry.asicTempC,telemetry.vrmTempC,telemetry.powerW,telemetry.hashRateGHs,telemetry.errorPercentage,telemetry.worstErrorPercentage,telemetry.voltageReadbackMv,telemetry.sampleAgeSeconds].every(value => this.finite(value) && value >= 0)
      && telemetry.errorPercentage! <= 100 && telemetry.worstErrorPercentage! <= 100;
  }
  private accept(report: AutotuneReport, requestedAt: number): boolean {
    if (!this.validReport(report)) { this.historyGap = true; this.readError = 'This application returned an unrecognised tuning report. Install its matching application and web interface.'; return false; }
    const dirty = this.hasUnsavedChanges;
    if (this.report && this.clockWarningChanged(this.report,report) && this.pendingAction!=='configure') this.advancedClockWarningAccepted=false;
    this.report = report;
    this.requestedAt = requestedAt;
    this.readError = '';
    if (this.recovery && requestedAt >= this.recoverySubmittedAt) this.recoveryUnconfirmed=false;
    if (!this.viewInitialized) {this.viewMode=report.status.active||report.status.ownsSettings?'live':'options';this.viewInitialized=true;}
    this.observeEvents();
    this.observe();
    if (this.optionsUsable && (!this.edit || !dirty)) {
      this.edit = this.config(report.config);
      this.baseline = JSON.stringify(this.edit);
    }
    return true;
  }
  private acceptHeartbeat(value:any,at:number): void {
    this.heartbeatAvailable=false;this.heartbeatLegacy=false;
    if(!value||value.schemaVersion!==1||value.available!==true||!Number.isInteger(value.runId)||value.runId<0
      ||typeof value.active!=='boolean'||typeof value.ownsSettings!=='boolean'
      ||!(value.controllerAgeSeconds===null||this.finite(value.controllerAgeSeconds)&&value.controllerAgeSeconds>=0)) return;
    this.heartbeatAdvertised=true;this.heartbeat={...value};
    this.heartbeatAt=at;this.heartbeatAvailable=true;
  }
  get controllerFresh(): boolean {
    if(!this.activityService||this.heartbeatLegacy) return true;
    const value=this.heartbeat,age=(performance.now()-this.heartbeatAt)/1000;
    return this.heartbeatAvailable&&!!value&&this.finite(value.controllerAgeSeconds)&&age>=0&&value.controllerAgeSeconds+age<=15
      &&value.runId===this.report?.status.runId&&value.active===this.report.status.active&&value.ownsSettings===this.report.status.ownsSettings;
  }
  get controllerStatusText(): string { return 'The controller status is not current or does not match this report. Start is blocked. '+(this.busy?'A request is awaiting readback; Refresh is available when that check finishes.':'Refresh status or use Disable and restore backup.'); }
  get restoration(): AutotuneRestoration|null {
    const value=this.report?.status.restoration;
    if(!value||typeof value!=='object'||Array.isArray(value)||!['idle','waiting','saving','applying','verifying','complete','blocked'].includes(value.phase)
      ||typeof value.reason!=='string'||value.reason.length>512||!(value.qualityWarning===null||typeof value.qualityWarning==='string'&&value.qualityWarning.length<=512)
      ||![value.elapsedSeconds,value.timeoutSeconds].every(n=>n===null||this.finite(n)&&n>=0)) return null;
    return value;
  }
  get restorationMalformed(): boolean { return this.report?.status.restoration!==undefined&&!this.restoration; }
  get restorationBlocked(): boolean { return this.restoration?.phase==='blocked'||this.recovery?.phase==='blocked'; }
  get restorationVisible(): boolean { return this.restorationMalformed||!!this.restoration&&this.restoration.phase!=='idle'||this.restorationBlocked; }
  get restorationHeading(): string {
    if(!this.isFresh||!this.controllerFresh||this.restorationMalformed)return 'Restoration status unconfirmed';
    if(this.restorationBlocked)return this.restorationTimedOut?'Restoration blocked · time limit reached':'Restoration blocked';
    if(this.restoration?.phase==='complete')return this.report!.backup.restored ? 'Original settings restored' : 'Original settings saved; mining remains paused';
    return 'Restoration in progress';
  }
  get restorationTimedOut(): boolean { const value=this.restoration;return !!value&&value.phase==='blocked'&&this.finite(value.elapsedSeconds)&&this.finite(value.timeoutSeconds)&&value.elapsedSeconds>=value.timeoutSeconds; }
  get restorationStage(): string { return this.restoration?.phase||this.recovery?.phase||''; }
  get restorationReason(): string { return this.restoration?.reason||this.recovery?.reason||this.report?.status.reason||''; }
  get restorationTiming(): string {
    const value=this.restoration||this.recovery;
    return value&&this.finite(value.elapsedSeconds)&&this.finite(value.timeoutSeconds) ? `${Math.floor(value.elapsedSeconds)} s elapsed · ${Math.floor(value.timeoutSeconds)} s limit for this attempt` : '';
  }
  private stopConfirmed(report:AutotuneReport): boolean {
    if(!this.controllerFresh||this.restorationMalformed||report.status.active||report.status.ownsSettings)return false;
    if(this.restoration?.phase==='complete')return report.backup.restored===true||report.status.telemetry.miningActive===false&&!!this.restoration.qualityWarning;
    return report.backup.restored===true&&this.backupMatches(report);
  }
  private resumeConfirmed(report:AutotuneReport): boolean {
    if(!this.controllerFresh||this.restorationMalformed||report.status.active||report.status.ownsSettings||this.recovery?.phase!=='complete'||this.recovery.requested||!report.backup.restored||report.status.telemetry.miningActive!==true||report.status.telemetry.fault!==false)return false;
    return this.restoration?.phase==='complete'||this.backupMatches(report)&&this.telemetryFresh&&report.status.telemetry.fanOk===true;
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
    if (this.optionsLocked && !this.busy) return this.report?.status.state==='restoring' ? 'The miner is restoring the original backup. Wait for restoration to finish before changing options.' : 'The miner is applying a clock or voltage change. Wait for its confirmation before changing options.';
    if (!this.optionsUsable) return 'Tuning options or hardware limits are unavailable. Restore the backup or refresh after the miner has recovered.';
    if (value && this.wholeMHz && (!Number.isInteger(value.minFrequencyMHz) || !Number.isInteger(value.maxFrequencyMHz))) return 'Use whole MHz for this miner’s clock bounds.';
    if (value && (!Number.isInteger(value.minVoltageMv) || !Number.isInteger(value.maxVoltageMv))) return 'Use whole millivolts for core-voltage bounds.';
    if (!value || !limits || !this.validConfig(value)) return 'Enter valid positive limits and choose a tuning priority.';
    if (value.advancedClockOverride===true && (!this.advancedClockAvailable || !this.advancedClockWarningAccepted)) return 'Read and explicitly accept the advanced clock warning before saving or starting with the override.';
    if (value.minFrequencyMHz < limits.minFrequencyMHz) return 'The minimum clock is below the reported lower limit.';
    if (value.maxFrequencyMHz > this.maximumClockMHz) return value.advancedClockOverride===true
      ? 'The requested clock ceiling exceeds the experimental project maximum shown below.'
      : this.report?.config.advancedClockOverride===true
        ? 'To disable the override, lower the maximum to the standard profile limit. An active point above that limit must first be lowered or restored.'
        : this.advancedClockAvailable ? 'A ceiling above the standard profile limit requires the explicit advanced clock warning acknowledgement.'
          : 'Clock limits must stay inside the reported limits. Advanced override requires a matching updated application and web interface.';
    if (value.minVoltageMv < limits.minVoltageMv || value.maxVoltageMv > limits.maxVoltageMv) return 'Voltage limits must stay inside the hardware limits shown below.';
    const current=this.report?.status.current;
    if(this.isEngaged && current && this.pointValid(current) && (value.minFrequencyMHz>current.frequencyMHz || value.maxFrequencyMHz<current.frequencyMHz || value.minVoltageMv>current.coreVoltageMv || value.maxVoltageMv<current.coreVoltageMv)) return `While this tune owns the settings, clock and voltage bounds must include the current reported point: ${this.pointText(current)}.`;
    if (value.maxAsicTempC > limits.maxAsicTempC || value.maxVrmTempC > limits.maxVrmTempC) return 'Temperature ceilings must stay below the protected hardware limits shown below.';
    if (value.maxPowerW > limits.maxPowerW) return 'The power ceiling exceeds this hardware limit.';
    return '';
  }
  get startPointError(): string {
    if (!this.optionsUsable) return '';
    const report=this.report!, current=report.status.current, config=report.config;
    if (!current || !this.pointValid(current)) return 'The current clock and voltage must be available before Start.';
    if (config.advancedClockOverride!==true && current.frequencyMHz>report.limits.maxFrequencyMHz) return this.advancedClockAvailable
      ? 'The current clock is above the standard profile limit. Explicitly enable the advanced override and include this point in your bounds, or lower it through manual controls before Start.'
      : 'The current clock is above the reported profile limit. A matching updated application and web interface are required for advanced override; otherwise lower the current point before Start.';
    return current.frequencyMHz<config.minFrequencyMHz || current.frequencyMHz>config.maxFrequencyMHz
      || current.coreVoltageMv<config.minVoltageMv || current.coreVoltageMv>config.maxVoltageMv
      ? `Saved clock and voltage bounds must include the current point before Start: ${this.pointText(current)}.` : '';
  }
  onOptionsEdit(): void { this.message=''; this.actionError=''; this.waiverAccepted=false; }
  get clockSaveProblem(): string {
    if (!this.edit || !this.report || !this.finite(this.edit.maxFrequencyMHz)) return '';
    if (this.edit.maxFrequencyMHz>this.report.limits.maxFrequencyMHz && (!this.edit.advancedClockOverride || !this.advancedClockWarningAccepted))
      return `${this.edit.maxFrequencyMHz} MHz is not saved. Accept the advanced clock warning directly above, then select Save options. The run waiver is only for Start.`;
    return '';
  }
  get isEngaged(): boolean {return this.report?.status.active===true || this.report?.status.ownsSettings===true;}
  get optionsLocked(): boolean {return this.busy || this.isEngaged && ['applying','restoring'].includes(this.report?.status.state||'');}
  get saveActionLabel(): string {return this.busy?'Working…':this.isEngaged?'Save changes to this tune':'Save options';}
  get editExplanation(): string {
    if (this.busy) return 'Waiting for the miner to confirm this request. No further change is sent until readback finishes.';
    if (this.optionsLocked) return this.report?.status.state==='restoring'?'Original settings are being restored. Options unlock when restoration finishes.':'The miner is confirming an operating-point change. Options unlock when that transition finishes.';
    if (this.isEngaged) return 'You are editing this existing tune. No new waiver or Start is needed. Saving a changed goal or bounds makes the controller reassess; temperature and power reductions can trigger safety backoff. Continuous On resumes monitoring after a held round.';
    return 'Save your choices, then accept the waiver and Start when ready. Saving options alone does not start tuning.';
  }
  get startExplanation(): string {
    if(this.loading)return 'Refreshing the miner status. Wait for the current readiness checks.';
    if(this.optionsLocked)return this.editExplanation;
    if(this.isEngaged)return 'This tune already owns the settings. Change its options and save; there is no second Start or waiver to accept. Disable and restore the backup when you want to end it.';
    if(this.restorationMalformed)return 'The restoration report is not recognised. Refresh status before starting.';
    if(this.isFresh&&!this.controllerFresh)return this.controllerStatusText;
    if(!this.isFresh)return 'A fresh miner report is required before starting. Refresh status and check the connection.';
    if(['applying','restoring'].includes(this.report!.status.state))return 'Wait for the current operating-point transition or restoration to finish.';
    if(this.report!.status.state==='faulted'||this.report!.status.telemetry.fault)return 'The miner reports a fault. Check its cause; the waiver cannot override protection.';
    if(!this.optionsUsable)return 'Valid options and hardware limits are required before starting.';
    if(this.hasUnsavedChanges)return 'Save these options before starting a new tune.';
    if(this.validationError)return this.validationError;
    if(this.startPointError)return this.startPointError;
    if(!this.telemetryFresh||!this.report!.status.telemetry.fanOk||!this.report!.status.telemetry.miningActive||this.hardwareReadiness.some(row=>!row.ready))return this.blockedStartChecks.find(row=>row.key!=='waiver')?.detail||'Check the readiness requirements below before starting.';
    return this.waiverAccepted?'Ready to start a new tune.':'Accept the disclaimer below to enable Start for this new run.';
  }
  showView(view:'live'|'options'): void {
    this.viewMode=view;this.viewInitialized=true;
    requestAnimationFrame(()=>document.getElementById(view==='live'?'tuning-live-dashboard':'tuning-options')?.scrollIntoView({block:'start',behavior:'smooth'}));
  }
  private get hardwareReadiness(): AutotuneReadiness[] {
    const rows = this.report?.status.readiness;
    if (!Array.isArray(rows) || rows.length > 32 || !rows.every(row => !!row
      && typeof row.key === 'string' && row.key.length <= 64
      && typeof row.label === 'string' && row.label.length <= 100
      && typeof row.detail === 'string' && row.detail.length <= 400 && typeof row.ready === 'boolean')) return [];
    return rows.map(row => this.isFresh ? row : {...row,ready:false,detail:'No current reading. Refresh status to check this requirement.'});
  }
  get startChecks(): AutotuneReadiness[] {
    const report=this.report, telemetry=report?.status.telemetry;
    const row=(key:string,label:string,ready:boolean,detail:string):AutotuneReadiness=>({key,label,ready,detail});
    const checks=[row('report','Connection and live status',this.isFresh,this.isFresh?'The miner is responding.':'Refresh status and check the connection.')];
    if(this.activityService) checks.push(row('controller-heartbeat','Current controller status',this.controllerFresh,this.controllerFresh?'The controller report is current.':this.controllerStatusText));
    if (!report || !telemetry) return checks;
    checks.push(row('request','No request in progress',!this.busy && !this.loading,this.busy?'Wait for the miner to confirm the current request.':this.loading?'Refreshing the miner status.':'No request is pending.'),row('options','Saved tuning options',this.optionsUsable && !this.hasUnsavedChanges && !this.validationError,
      !this.optionsUsable?'Tuning options or hardware limits are unavailable.':this.hasUnsavedChanges?'Save the changed options before starting.':this.validationError||'The selected options are saved and valid.'),
      row('current-point','Starting clock and voltage',this.optionsUsable && !this.startPointError,!this.optionsUsable?'The current clock and voltage bounds cannot be verified.':this.startPointError||'The current point is within the saved search bounds.'),
      row('control','Controller available',!this.isEngaged && !this.optionsLocked && !['faulted','restoring','applying'].includes(report.status.state),
        this.isEngaged?'A tune already owns the settings. Edit that tune or disable and restore it first.':['restoring','applying'].includes(report.status.state)?'Wait for the current change or restoration to finish.':report.status.state==='faulted'?'The miner reports a fault. Check its cause before starting.':'No other tuning session owns the settings.'));
    const detailed=this.hardwareReadiness;
    checks.push(...detailed);
    // Keep the authoritative aggregate gates even with partial diagnostics.
    // Individual checks add detail and can conservatively block, never bypass.
    {
      checks.push(row('cooling','Cooling feedback',this.isFresh && telemetry.fanOk===true,telemetry.fanOk===true?'Fan feedback is available.':'Cooling feedback is missing or outside the permitted range.'),
        row('mining','Active mining',this.isFresh && telemetry.miningActive===true,telemetry.miningActive===true?'Mining is active.':'Mining must be active before starting. Check Pause, the pool connection and chip activity.'),
        row('protection','Hardware protection',this.isFresh && telemetry.fault===false,telemetry.fault===false?'No hardware fault is reported.':'The miner reports a fault. The waiver cannot override protection.'));
    }
    const age=telemetry.sampleAgeSeconds;
    const detail=!this.isFresh?'Refresh status and check the connection.':!this.validTelemetry(telemetry)
      ? (detailed.find(item=>!item.ready)?.detail||'A sensor or ASIC counter is invalid. This firmware does not identify which one. Copy diagnostics for investigation.')
      : !this.telemetryFresh?`The readings are too old to start${this.finite(age)?' (reported age '+age.toFixed(1)+' seconds)':''}. Wait for fresh readings or refresh status.`
      :'Sensor readings and ASIC counters are current.';
    checks.push(row('fresh-readings','Fresh sensor and ASIC readings',this.telemetryFresh,detail),
      row('waiver','Run acknowledgement',this.waiverAccepted,this.waiverAccepted?'The tuning disclaimer is accepted for this run.':'Accept the tuning disclaimer below when you are ready.'));
    return checks;
  }
  get blockedStartChecks(): AutotuneReadiness[] { return this.startChecks.filter(row=>!row.ready); }
  get visibleStartChecks(): AutotuneReadiness[] { return this.showAllReadiness?this.startChecks:this.blockedStartChecks; }
  async copyDiagnostics(): Promise<void> {
    this.diagnosticsText=JSON.stringify({capturedAt:new Date().toISOString(),statusFresh:this.isFresh,controllerStatus:{fresh:this.controllerFresh,legacy:this.heartbeatLegacy,lastReport:this.heartbeat,requestAgeSeconds:this.finite(this.heartbeatAt)?(performance.now()-this.heartbeatAt)/1000:null},
      startChecks:this.startChecks,report:this.report},null,2);
    try {
      if (!navigator.clipboard?.writeText) throw new Error('Clipboard unavailable');
      await navigator.clipboard.writeText(this.diagnosticsText);
      this.diagnosticsMessage='Tuning diagnostics copied. Include the installed firmware version when reporting a problem.';
    } catch {
      this.diagnosticsMessage='Select and copy the diagnostics below. Include the installed firmware version when reporting a problem.';
    }
  }
  get canSave(): boolean { return !this.busy && !this.loading && this.isFresh && this.optionsUsable && this.hasUnsavedChanges && !this.validationError; }
  get canStart(): boolean {
    const report = this.report;
    return !!report && !this.busy && !this.loading && this.isFresh && this.controllerFresh && !this.restorationMalformed && this.optionsUsable && !report.status.active
      && !['faulted','restoring','applying'].includes(report.status.state) && !report.status.ownsSettings && !this.hasUnsavedChanges
      && !this.validationError && !this.startPointError && this.telemetryFresh && !this.hardwareReadiness.some(row=>!row.ready) && report.status.telemetry.fanOk === true
      && report.status.telemetry.miningActive === true && report.status.telemetry.fault === false && this.waiverAccepted;
  }
  get canRestore(): boolean { return !this.busy && !this.loading && (this.report?.backup?.available === true || this.report?.status.ownsSettings === true || this.startUnconfirmed); }
  get recovery(): AutotuneRecovery | null {
    const value=this.report?.status.recovery;
    if(!value || typeof value!=='object' || Array.isArray(value) || typeof value.requested!=='boolean'
      || !['idle','validating-off','restoring','resuming','verifying','complete','blocked'].includes(value.phase)
      || typeof value.reason!=='string' || value.reason.length>256) return null;
    const pending=['validating-off','restoring','resuming','verifying'].includes(value.phase);
    return value.requested===pending ? value : null;
  }
  get canRestoreAndResume(): boolean {
    const canUpgradeStop=this.busy && this.pendingAction==='stop' && this.actionAcknowledged;
    return (!this.busy || canUpgradeStop) && !this.loading && this.isFresh && !!this.recovery && !this.recovery.requested
      && !this.recoveryUnconfirmed && this.report!.backup.available && this.report!.status.telemetry.miningActive===false;
  }
  get recoveryText(): string {
    if(!this.isFresh)return 'Refresh the miner status before requesting recovery.';
    const value=this.recovery;
    if(!value)return 'Restore-and-resume requires a matching updated application and web interface. Ordinary backup restore remains available.';
    if(this.recoveryUnconfirmed)return 'The previous request is not confirmed. Refresh status before making another request.';
    const reason=value.reason.replace(/[-_]/g,' ');
    if(value.phase==='blocked')return `Recovery blocked: ${reason || 'check the miner and its protection state'}. No automatic retry is sent; a new request needs your explicit confirmation.`;
    if(value.requested)return ({'validating-off':'Confirming that mining is paused and the ASIC is off before restoring.',restoring:'Restoring the original saved settings while mining stays off.',resuming:'Original settings are restored; the miner is checking whether normal mining can safely resume.',verifying:'Verifying original settings and fresh mining readings after resuming.'} as Record<string,string>)[value.phase]+(reason ? ` Miner reason: ${reason}.` : '');
    if(value.phase==='complete')return 'The miner reports recovery complete. Auto-tuning stays off; current readings show whether normal mining is still running.';
    if(!this.report!.backup.available)return 'No original tuning backup is available.';
    if(this.report!.status.telemetry.miningActive!==false)return 'This action requires confirmed paused mining. Use Disable and restore backup for a running tune; it does not resume mining.';
    return 'Restore the retained original settings, then request normal mining with auto-tuning off. The miner checks its actual power state, cooling and faults before resuming.';
  }
  get limitReached(): boolean {return this.report?.status.reason==='cannot-reduce-load-within-limits';}
  get limitStopText(): string {
    const minimum=this.optionsUsable ? `${this.report!.config.minFrequencyMHz} MHz` : 'the saved minimum clock';
    return `The miner could not reduce load further within the limits active at the stop. The current saved minimum is ${minimum}. This is a limit-constrained rejection, not a diagnosis of defective hardware. Review cooling and allow a lower minimum within the board limits, or explicitly restore the original settings and resume mining when safe.`;
  }
  get stateLabel(): string {
    if (!this.isFresh || !this.controllerFresh) return this.report ? 'Status unavailable' : 'Connecting';
    if(this.restorationBlocked)return this.restorationTimedOut?'Restoration blocked · time limit reached':'Restoration blocked';
    if(this.restoration?.phase==='complete')return this.report!.backup.restored?'Original settings restored':'Settings saved · mining paused';
    if (this.limitReached && !this.isEngaged) return 'Stopped · limit reached';
    if (!this.isEngaged && this.report?.status.telemetry.fault !== true) return this.report?.status.state === 'disabled' ? 'Disabled' : 'Inactive';
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
    if (this.limitReached && !this.isEngaged) return 'Stopped by limits · control released';
    if (this.isEngaged && this.report!.status.state === 'restoring') return 'Restoration in progress';
    if (this.report!.status.active) return 'Run in progress';
    if (this.report!.status.ownsSettings) return 'Holding result · Disable and restore to release control';
    return 'Explicit Start required';
  }
  get activityBanner(): {label: string; detail: string; kind: string} {
    if (!this.isFresh || !this.controllerFresh) return {label:this.report ? 'TUNING STATUS UNAVAILABLE' : 'CHECKING TUNING STATUS',detail:'Waiting for a fresh miner report. Do not infer the current state from older readings.',kind:'unknown'};
    const report=this.report!, status=report.status;
    if(this.restorationMalformed)return {label:'RESTORATION STATUS UNCONFIRMED',detail:'The restoration report is not recognised. Refresh status; no completion is assumed.',kind:'unknown'};
    if(this.restorationBlocked)return {label:this.restorationTimedOut?'RESTORATION BLOCKED · TIME LIMIT REACHED':'RESTORATION BLOCKED',detail:this.restorationReason,kind:'fault'};
    if(this.restoration?.phase==='complete')return {label:this.report!.backup.restored?'ORIGINAL SETTINGS RESTORED':'SETTINGS SAVED · MINING PAUSED',detail:this.restoration.qualityWarning||this.restoration.reason,kind:this.restoration.qualityWarning?'suspended':'off'};
    if (status.telemetry.fault === true) return {label:'TUNING FAULT',detail:'Miner protection is authoritative. Check the reported reason before taking action.',kind:'fault'};
    if (this.recovery?.requested) return {label:'RECOVERING MINING · AUTO-TUNING OFF',detail:this.recoveryText,kind:'restoring'};
    if (this.limitReached && !this.isEngaged) return {label:'STOPPED — LIMIT REACHED',detail:this.limitStopText,kind:'suspended'};
    if (!this.isEngaged) return {label:'AUTO-TUNING OFF',detail:report.backup.restored ? 'The miner reports its original pre-tuning settings restored.' : 'The miner reports no active tuning ownership. Any previous phase is historical; check the reported reason before a new Start.',kind:'off'};
    if (status.state === 'restoring') return {label:'RESTORING ORIGINAL SETTINGS',detail:'Restoration is pending until the miner confirms the backup is restored and control is released.',kind:'restoring'};
    if (!report.supported) return {label:'TUNING UNAVAILABLE',detail:'This hardware or operating state does not currently support automatic tuning.',kind:'unknown'};
    if (status.state === 'suspended') return {label:'TUNING SUSPENDED',detail:'The controller is waiting. Check the reported reason and fresh sensors before interpreting the current request.',kind:'suspended'};
    if (!this.telemetryFresh && (status.active || status.ownsSettings)) return {label:status.active ? 'LAST REPORTED ACTIVE · READINGS UNAVAILABLE' : 'LAST REPORTED HELD · READINGS UNAVAILABLE',detail:'The report still shows controller ownership, but fresh sensor and counter readings are not confirmed.',kind:'unknown'};
    if (status.active && status.state === 'observing') return {label:'CONTINUOUS MONITORING ACTIVE',detail:'The controller still owns the settings and is watching for a reason to reassess.',kind:'observing'};
    if (status.active) return {label:'AUTO-TUNING ACTIVE',detail:`Current phase: ${this.stateLabel}. The miner reports its controller active.`,kind:'active'};
    if (status.ownsSettings) return {label:'RESULT HELD · TUNER STILL OWNS SETTINGS',detail:'The bounded round has stopped searching. Disable and restore the backup to release control.',kind:'holding'};
    return {label:'AUTO-TUNING OFF',detail:report.backup.restored ? 'The miner reports its original pre-tuning settings restored.' : 'No active tuning run. Start requires your explicit waiver and action.',kind:'off'};
  }
  get ownedSettingsText(): string { return this.isFresh ? this.report!.status.ownsSettings ? 'Controller owns settings' : 'Controller has released settings' : 'Ownership unavailable'; }
  get activityText(): string {
    if (!this.isFresh || !this.controllerFresh) return 'Waiting for a fresh report. Earlier observations are kept below; they do not show the miner’s current condition.';
    const report = this.report!;
    if(this.restorationVisible)return this.restorationHeading+': '+this.restorationReason;
    if (report.status.telemetry.fault === true) return 'The miner reports an active protection fault. Tuning cannot clear it or force settings through it; check the miner’s reason below.';
    if (this.limitReached) return this.limitStopText;
    if (!report.supported) return 'Automatic tuning is unavailable for this hardware or operating state. Restore remains available when the miner has a backup or owns settings.';
    const errorDecision = this.errorDecisionText(report.status.reason);
    if (errorDecision) return errorDecision;
    const reasons: Record<string,string> = {
      'temperature-limit-backoff':'Backing off because a saved temperature ceiling is close or exceeded. Safety can require a larger step than a normal experiment.',
      'power-limit-backoff':'Backing off because the reported power ceiling is close or exceeded. This is the miner’s power reading, not wall power.',
      'new-user-limits':'Adjusting to newly saved search limits. The controller must stay inside those bounds.',
      'error-recovery-voltage-trial':'Trying a small voltage increase to reduce ASIC timing errors, after checking temperature and power headroom. It must still settle and pass measurement checks.',
      'error-recovery-clock-backoff':'Reducing clock to recover from excessive ASIC errors after limited voltage retries, or because voltage or cooling headroom prevents a raise.',
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
    if (!this.isEngaged && report.status.state !== 'disabled') return 'The miner reports that tuning is inactive and no longer owns the settings. The previous phase is historical; a new run still requires explicit Start and all readiness checks.';
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
  temperatureWarnings(config:AutotuneConfig|null|undefined): {sensor:string;value:number;recommended:number;severity:string}[] {
    if(!config)return [];
    const rows=[{sensor:'ASIC',value:config.maxAsicTempC,recommended:60,severity:config.maxAsicTempC>65?'danger':'warning'},
      {sensor:'VRM',value:config.maxVrmTempC,recommended:70,severity:config.maxVrmTempC>=80?'danger':'warning'}];
    return rows.filter(row=>this.finite(row.value)&&row.value>row.recommended);
  }
  get savedTemperatureWarnings(): {sensor:string;value:number;recommended:number;severity:string}[] {return this.temperatureWarnings(this.report?.config);}
  get editedTemperatureWarnings(): {sensor:string;value:number;recommended:number;severity:string}[] {return this.temperatureWarnings(this.edit);}
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
    if (last && (this.historyGap || at-last.at > 12000)) this.history.push({at:at-1,ths:null,joules:null,asicTempC:null,vrmTempC:null,powerW:null,boardErrors:null,worstErrors:null});
    const t = this.report!.status.telemetry;
    const valid = this.telemetryFresh;
    this.history.push({at,ths:valid ? t.hashRateGHs!/1000 : null,joules:valid ? this.energyValue(t.hashRateGHs,t.powerW) : null,
      asicTempC:valid ? t.asicTempC : null,vrmTempC:valid ? t.vrmTempC : null,powerW:valid ? t.powerW : null,
      boardErrors:valid ? t.errorPercentage : null,worstErrors:valid ? t.worstErrorPercentage : null});
    this.history = this.history.filter(point => at-point.at <= 600000).slice(-this.historyLimit);
    this.historyGap = !valid;
  }
  get trends(): {name: string; unit: string; path: string; points: {x:number;y:number}[]; lines: ObservedLine[]; minimum:string; maximum:string}[] {
    const charts: {name:string;unit:string;digits:number;lines:{name:string;key:ObservedMetric}[]}[] = [
      {name:'Useful hashrate',unit:'TH/s',digits:2,lines:[{name:'Useful hashrate',key:'ths'}]},
      {name:'ASIC temperature',unit:'°C',digits:1,lines:[{name:'ASIC temperature',key:'asicTempC'}]},
      {name:'VRM temperature',unit:'°C',digits:1,lines:[{name:'VRM temperature',key:'vrmTempC'}]},
      {name:'Reported power',unit:'W',digits:1,lines:[{name:'Reported power',key:'powerW'}]},
      {name:'ASIC errors',unit:'%',digits:2,lines:[{name:'Board',key:'boardErrors'},{name:'Worst ASIC',key:'worstErrors'}]},
      {name:'Energy per useful hash',unit:'J/TH',digits:2,lines:[{name:'Energy per useful hash',key:'joules'}]},
    ];
    return charts.map(chart => {
      const values = chart.lines.flatMap(line => this.history.map(point => point[line.key])).filter((n): n is number => n !== null && Number.isFinite(n));
      const min = values.length ? Math.min(...values) : 0, max = values.length ? Math.max(...values) : 0;
      const padding = Math.max((max-min)*.15,max*.01,.01), low=Math.max(0,min-padding), high=Number.isFinite(max+padding) ? max+padding : max;
      const first = this.history[0]?.at || 0, last = this.history[this.history.length-1]?.at || first;
      const lines = chart.lines.map((series,index) => {
        let connected=false; const points: {x:number;y:number}[]=[];
        const path=this.history.map(point => {
          const value=point[series.key];
          if (value === null || !Number.isFinite(value)) {connected=false;return '';}
          const x=8+584*(point.at-first)/Math.max(last-first,1000),y=88-72*((value-low)/(high-low));
          const command=connected ? 'L' : 'M';connected=true;points.push({x,y});return `${command}${x.toFixed(2)},${y.toFixed(2)}`;
        }).join(' ');
        return {name:series.name,secondary:index>0,path,points};
      });
      return {name:chart.name,unit:chart.unit,path:lines[0].path,points:lines[0].points,lines,
        minimum:values.length ? low.toFixed(chart.digits) : '—',maximum:values.length ? high.toFixed(chart.digits) : '—'};
    });
  }
  private cleanConditions(value: AutotuneConditions | null | undefined): AutotuneConditions | undefined {
    if(!value || typeof value!=='object' || Array.isArray(value))return undefined;
    const keys=['asicTempC','vrmTempC','powerW','hashRateGHs','errorPercentage','worstErrorPercentage'] as const;
    if(!keys.every(key=>this.finite(value[key])&&value[key]>=0)||value.errorPercentage>100||value.worstErrorPercentage>100)return undefined;
    return {asicTempC:value.asicTempC,vrmTempC:value.vrmTempC,powerW:value.powerW,hashRateGHs:value.hashRateGHs,errorPercentage:value.errorPercentage,worstErrorPercentage:value.worstErrorPercentage};
  }
  private eventValid(event: AutotuneEvent): boolean {
    const text=(value:unknown,max:number) => typeof value === 'string' && value.length>0 && value.length<=max;
    const measurement=event?.measurement;
    return !!event && typeof event === 'object' && !Array.isArray(event)
      && Number.isInteger(event.id) && event.id>0 && event.id<=0xffffffff
      && this.finite(event.atSeconds) && event.atSeconds>=0 && event.atSeconds<=Number.MAX_SAFE_INTEGER
      && text(event.kind,64) && text(event.outcome,64) && text(event.reason,256)
      && (event.from===null || !!event.from && this.pointValid(event.from)) && !!event.point && this.pointValid(event.point)
      && (measurement===null || !!measurement && typeof measurement==='object' && !Array.isArray(measurement)
        && [measurement.hashRateGHs,measurement.powerW,measurement.errorPercentage,measurement.worstErrorPercentage].every(value=>this.finite(value)&&value>=0)
        && measurement.powerW>0 && measurement.errorPercentage<=100 && measurement.worstErrorPercentage<=100)
      && (event.score===null || this.finite(event.score)) && (event.requiredScore===null || this.finite(event.requiredScore));
  }
  private observeEvents(): void {
    const status=this.report!.status;
    const incoming=status.events,capacity=status.eventCapacity;
    this.eventFeedAvailable=Number.isInteger(capacity) && capacity!>0 && capacity!<=64 && Array.isArray(incoming)
      && incoming.length<=capacity! && incoming.every((event,index)=>this.eventValid(event) && (index===0 || event.id>incoming[index-1].id));
    const oldById=new Map(this.events.map(event=>[event.id,event]));
    const sequenceReset=this.eventFeedAvailable && incoming!.length>0 && this.events.length>0
      && incoming![incoming!.length-1].id<this.events[this.events.length-1].id
      && Math.max(...incoming!.map(event=>event.atSeconds))<Math.min(...this.events.map(event=>event.atSeconds));
    const clockReset=this.eventFeedAvailable && (sequenceReset || incoming!.some(event=>oldById.has(event.id) && event.atSeconds<oldById.get(event.id)!.atSeconds));
    if (this.eventRun!==status.runId || clockReset) {
      this.events=[];this.eventGaps=[];this.visibleEventCount=12;this.eventRun=status.runId;
      if (clockReset) {this.history=[];this.historyRun=null;this.historyGap=false;}
    }
    if (!this.eventFeedAvailable) return;
    const last=this.events[this.events.length-1]?.id || 0;
    const newer=incoming!.filter(event=>event.id>last);
    if (newer.length && newer[0].id>last+1) this.eventGaps.push({beforeId:newer[0].id,missing:newer[0].id-last-1});
    const byId=new Map(this.events.map(event=>[event.id,event]));
    for(const event of incoming!) {
      // Keep only bounded presentation fields; unknown payloads never enter the tab cache.
      const point=(value:AutotunePoint):AutotunePoint => ({frequencyMHz:value.frequencyMHz,coreVoltageMv:value.coreVoltageMv});
      const measurement=event.measurement;
      byId.set(event.id,{id:event.id,atSeconds:event.atSeconds,kind:event.kind,reason:event.reason,outcome:event.outcome,
        from:event.from ? point(event.from) : null,point:point(event.point),
        measurement:measurement ? {hashRateGHs:measurement.hashRateGHs,powerW:measurement.powerW,errorPercentage:measurement.errorPercentage,worstErrorPercentage:measurement.worstErrorPercentage} : null,
        score:event.score,requiredScore:event.requiredScore,conditions:this.cleanConditions(event.conditions)});
    }
    this.events=Array.from(byId.values()).sort((a,b)=>a.id-b.id).slice(-this.eventHistoryLimit);
    const earliest=this.events[0]?.id || 0;this.eventGaps=this.eventGaps.filter(gap=>gap.beforeId>=earliest).slice(-this.eventHistoryLimit);
  }
  get visibleEvents(): AutotuneEvent[] { return this.events.slice(-this.visibleEventCount).reverse(); }
  showEarlierEvents(): void { this.visibleEventCount=this.eventHistoryLimit; }
  eventGapBefore(event: AutotuneEvent): number { return this.eventGaps.find(gap=>gap.beforeId===event.id)?.missing || 0; }
  get eventFeedNote(): string {
    if (!this.isFresh) return 'Current decisions are unavailable. Any entries below are earlier observations from this tab.';
    if (!this.eventFeedAvailable) return 'This report has no usable recent-event feed. Any entries below are earlier observations; Restore remains available.';
    return this.events.length ? 'Newest first. Requests are updated in place when the miner reports applied or failed; a request alone is not confirmation.' : 'No controller decisions have been reported for this run yet.';
  }
  eventTitle(event: AutotuneEvent): string {
    if (['early-error-limit','early-severe-error-limit'].includes(event.reason)) return 'Point rejected early';
    if (event.reason==='error-point-exclusion-full') return 'Error-rejection history full';
    if (event.reason==='error-window-limit') return 'Point rejected by measured errors';
    if (event.reason==='held-point-became-unstable') return 'Held point rejected by errors';
    if (event.reason==='error-recovery-exhausted') return 'Error correction exhausted';
    if(event.kind==='rejected' && event.outcome==='failed')return 'Point rejected by limits';
    return ({started:'Tuning started',baseline:'Round reference measured',accepted:'Measured point accepted',rejected:'Comparison rejected',retained:'Best point rechecked',errors:'ASIC error window',adjustment:'Operating-point adjustment'} as Record<string,string>)[event.kind] || 'Controller event';
  }
  eventOutcome(event: AutotuneEvent): string {
    return ({requested:'Requested · awaiting confirmation',applied:'Applied · miner confirmed',failed:'Failed · check the reason',measured:'Measured'} as Record<string,string>)[event.outcome] || `Reported outcome: ${event.outcome}`;
  }
  eventReason(event: AutotuneEvent): string {
    const errorDecision = this.errorDecisionText(event.reason);
    if (errorDecision) return errorDecision;
    if(event.kind==='rejected' && event.outcome==='failed')return `The controller could not reduce load further within its then-active limits (${event.reason.replace(/[-_]/g,' ')}). This point was rejected under those constraints, not marked as permanently defective.`;
    return ({'measuring-starting-point':'The run started at this operating point.',
      'round-reference-measured':'A valid measurement window established this round’s reference.',
      'score-beat-required-margin':'The measured goal score cleared the controller’s required comparison margin.',
      'improvement-within-noise-margin':'The trial did not clear the required goal and noise margin.',
      'best-point-remeasured':'The best point was measured again; no new trial was accepted.',
      'error-window-limit':'Measured ASIC errors exceeded the controller’s error limit.',
      'evaluating-nearby-operating-point':'Testing a nearby point within the saved search bounds.',
      'temperature-limit-backoff':'Reducing load because a saved temperature ceiling is close or exceeded.',
      'power-limit-backoff':'Reducing load because the reported power ceiling is close or exceeded.',
      'error-recovery-voltage-trial':'Trying higher voltage for timing errors after checking temperature and power headroom.',
      'error-recovery-clock-backoff':'Reducing clock to recover from excessive ASIC errors.',
      'restoring-better-measured-point':'Returning to the previously measured better point, within current safety headroom.',
      'rollback-lacks-headroom':'Backing off because returning to the measured best would lack headroom.',
      'new-user-limits':'Adjusting to newly saved user limits.',
      'held-point-became-unstable':'The held point no longer meets the error or stability checks.'} as Record<string,string>)[event.reason] || `Miner reason: ${event.reason.replace(/[-_]/g,' ')}`;
  }
  private errorDecisionText(reason: string): string | null {
    return ({
      'early-error-limit':'Rejected this operating point early after three consecutive fresh counter intervals spanning at least 10 seconds exceeded the ASIC error limits. Its full comparison window was not completed; no performance gain was accepted.',
      'early-severe-error-limit':'Rejected this operating point early because a fresh counter interval from the current evaluation reported severe ASIC errors. Its full comparison window was not completed; no performance gain was accepted.',
      'error-window-limit':'The completed measurement window exceeded the ASIC error limits. This point was rejected; the controller uses a measured stable point or bounded correction within its current headroom.',
      'held-point-became-unstable':'A fresh error reading shows that the held or monitored point is unstable. The controller immediately reduces clock; it does not wait for three error intervals or another full measurement window.',
      'restoring-last-stable-point':'Returning to a measured stable point within the current bounds and temperature and power headroom. This is separate from restoring the original pre-tuning backup.',
      'error-point-exclusion-full':'The current run has filled its error-rejection history. No further experiments can be proposed; bounded correction or guarded original-settings recovery must complete before another run.',
      'error-recovery-exhausted':'Bounded error correction has been exhausted. The miner can make one guarded original-settings recovery attempt with tuning off. Faults, unsafe conditions or newer manual control can block it; check the recovery status.'
    } as Record<string,string>)[reason] || null;
  }
  eventHasChange(event: AutotuneEvent): boolean { return !!event.from && (Math.abs(event.from.frequencyMHz-event.point.frequencyMHz)>.001 || Math.abs(event.from.coreVoltageMv-event.point.coreVoltageMv)>.001); }
  eventChange(event: AutotuneEvent): string {
    if (!event.from || !this.eventHasChange(event)) return '';
    const delta=(value:number,unit:string) => `${value>0?'+':''}${value.toFixed(2).replace(/\.?0+$/,'')} ${unit}`;
    const changes=[];
    if (Math.abs(event.point.frequencyMHz-event.from.frequencyMHz)>.001) changes.push(`clock ${delta(event.point.frequencyMHz-event.from.frequencyMHz,'MHz')}`);
    if (Math.abs(event.point.coreVoltageMv-event.from.coreVoltageMv)>.001) changes.push(`voltage ${delta(event.point.coreVoltageMv-event.from.coreVoltageMv,'mV')}`);
    return 'Request difference: '+changes.join(' · ');
  }
  eventTime(event: AutotuneEvent): string {
    const seconds=Math.floor(event.atSeconds),hours=Math.floor(seconds/3600),minutes=Math.floor(seconds%3600/60);
    return `${hours ? hours+' h ' : ''}${minutes} min ${seconds%60} s after miner boot`;
  }
  eventScore(value: number | null): string { return this.finite(value) ? value.toFixed(6) : '—'; }
  eventMeasurement(value: number | null | undefined,digits=2): string { return this.finite(value) ? value.toFixed(digits) : '—'; }
  get historySpan(): string {
    if (this.history.length < 2) return 'Waiting for more tab observations';
    const clock = (at: number) => new Date(performance.timeOrigin+at).toLocaleTimeString([], {hour:'2-digit',minute:'2-digit',second:'2-digit'});
    return `${clock(this.history[0].at)} – ${clock(this.history[this.history.length-1].at)} · run ${this.historyRun}`;
  }
  save(): void {
    if (!this.canSave) return;
    const payload = this.config(this.edit!);
    const body: Record<string,unknown>={action:'configure',config:payload};
    if (payload.advancedClockOverride===true) {body['advancedClockWarningAccepted']=true;body['advancedClockWarningVersion']=1;}
    this.execute(body, report => JSON.stringify(this.config(report.config)) === JSON.stringify(payload),
      this.isEngaged?`Changes saved for this tune: maximum clock ${payload.maxFrequencyMHz} MHz. The controller will apply its safety checks.`:`Tuning options saved: maximum clock ${payload.maxFrequencyMHz} MHz. Tuning has not started.`);
  }
  start(): void {
    if (!this.canStart) return;
    const previousRun = this.report!.status.runId;
    const body: Record<string,unknown>={action:'start',waiverAccepted:true,waiverVersion:1};
    const advanced=this.report!.config.advancedClockOverride===true;
    if (advanced) {body['advancedClockAccepted']=true;this.advancedClockWarningAccepted=false;}
    this.waiverAccepted = false;
    this.execute(body, report => report.status.active && report.status.runId > previousRun
      && (!advanced || this.advancedClockAvailable && report.config.advancedClockOverride===true),
      'Tuning started on the miner. You can close this page; the miner runs the controller.');
  }
  restore(): void {
    if (!this.canRestore) return;
    this.waiverAccepted = false;
    this.execute({action:'stop'}, report => this.stopConfirmed(report),
      'Auto-tuning disabled and the pre-tuning backup restored.');
  }
  restoreAndResume(): void {
    if(!this.canRestoreAndResume)return;
    const original=this.pointText({frequencyMHz:this.report!.backup.frequencyMHz!,coreVoltageMv:this.report!.backup.coreVoltageMv!});
    if(!window.confirm(`Restore original settings (${original}) and resume normal mining with auto-tuning off? The miner must be paused and will check cooling and faults before restarting mining. This does not reboot the miner or start another tuning run.`))return;
    // Only cancel the acknowledged Stop's GET wait; never cancel or repeat its POST.
    if(this.busy)this.cancelActionReadback.next();
    this.waiverAccepted=false;
    this.execute({action:'restore-and-resume'}, report=>this.resumeConfirmed(report),
      'Original settings restored and normal mining verified. Auto-tuning remains off.');
  }
  private execute(body: Record<string,unknown>, confirms: (report: AutotuneReport) => boolean, success: string): void {
    this.busy = true;
    this.pendingAction=String(body['action']);this.actionAcknowledged=false;
    this.readEpoch++;
    this.actionError = this.message = '';
    this.service.authorize().pipe(take(1),switchMap(({totp}) => {
      if (body['action'] === 'start') this.startUnconfirmed = true;
      if (body['action'] === 'restore-and-resume') {this.recoveryUnconfirmed=true;this.recoverySubmittedAt=performance.now();}
      return this.service.command(body,totp);
    }),switchMap(ack => {
      if (ack?.ok !== true || ack.restartRequired !== false) return throwError(() => new Error('unconfirmed'));
      this.actionAcknowledged=true;
      // Retry reads only while a bounded asynchronous transition completes.
      // The state-changing request is never repeated.
      return timer(0,1000).pipe(exhaustMap(() => {
        const requestedAt = performance.now();
        return this.service.get().pipe(map(report => ({report,requestedAt})));
      }),tap(({report,requestedAt}) => {
        if (!this.accept(report,requestedAt)) throw new Error('unconfirmed');
        if (['stop','restore-and-resume'].includes(String(body['action'])) && this.restorationBlocked)throw new Error('recovery-blocked');
        if (body['action']==='restore-and-resume') {if(this.recovery?.phase==='blocked')throw new Error('recovery-blocked');}
        else if (report.status.state === 'faulted') throw new Error('transition-fault');
      }),filter(({report}) => confirms(report)),take(1),timeout({first:['stop','restore-and-resume'].includes(String(body['action']))?195000:30000}));
    }),takeUntil(this.destroyed),takeUntil(this.cancelActionReadback),finalize(() => {this.busy=false;this.pendingAction='';this.actionAcknowledged=false;})).subscribe({
      next: ({report}) => {
        if(body['action']==='stop'&&this.restoration?.phase==='complete')success=this.report!.backup.restored ? 'Original settings restored. '+(this.restoration.qualityWarning||'Auto-tuning is off.') : 'Original settings saved; mining remains paused. '+(this.restoration.qualityWarning||'');
        if(body['action']==='restore-and-resume'&&this.restoration?.qualityWarning)success='Original settings restored; mining resumed with a quality warning. '+this.restoration.qualityWarning;
        this.startUnconfirmed = false;
        if (body['action'] === 'start') {this.viewMode='live';document.querySelector('.run-banner')?.scrollIntoView({block:'start'});}
        if (body['action'] === 'configure') { this.edit = this.config(report.config); this.baseline = JSON.stringify(this.edit); }
        this.message = success;
      },
      error: error => {
        this.advancedClockWarningAccepted=false;
        const explanation=({'advanced-clock-warning-required':'The miner requires the explicit advanced clock warning before saving this override.',
          'advanced-clock-acceptance-required':'The miner requires the advanced clock acknowledgement for this Start.',
          'advanced-clock-override-required':'The current clock is above the standard profile limit. Explicitly enable the advanced override or lower or restore the current point before tuning.'} as Record<string,string>)[error?.error?.error];
        this.actionError = error?.message==='recovery-blocked' ? this.restorationReason||this.recoveryText : explanation
          ? `${explanation} Refresh status before choosing another action. No repeat request was sent.`
          : 'The request could not be confirmed. Refresh status and check the miner before trying again. No repeat request was sent.';
      },
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
