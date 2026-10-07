import { HttpErrorResponse } from '@angular/common/http';
import { Component, Input, OnInit, OnDestroy, TemplateRef } from '@angular/core';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
import { switchMap, startWith, tap, catchError, of, Observable, timer, Subscription } from 'rxjs';
import { FiveTratumProfiles, ProfileSlot, PoolSchedule, PowerReport, PowerSchedule, PowerWindow } from '../../models/IFiveTratumProfiles';
import { LoadingService } from '../../services/loading.service';
import { SystemService } from '../../services/system.service';
import { eASICModel } from '../../models/enum/eASICModel';
import { NbToastrService, NbDialogService, NbDialogRef } from '@nebular/theme';
import { LocalStorageService } from 'src/app/services/local-storage.service';
import { OtpAuthService, EnsureOtpResult, EnsureOtpOptions } from '../../services/otp-auth.service';
import { TranslateService } from '@ngx-translate/core';
import { ISettingsV2, ISettingsV2Fan } from '../../models/ISettingsV2';
import { sectionFields, changedFields, settingsPatch } from './settings-scope';

enum SupportLevel { Safe = 0, Advanced = 1, Pro = 2 }

@Component({
  selector: 'app-edit',
  templateUrl: './edit.component.html',
  styleUrls: ['./edit.component.scss']
})
export class EditComponent implements OnInit, OnDestroy {
  public manualTuning = false;
  public saving = false;
  public restartPending = false;
  public tuningReview: {frequencyMHz:number;coreVoltageMv:number;name:string;source:'manual'|'slot'} | null = null;
  private baseline: Record<string,any> = {};
  private scheduleBaseline = '';
  private powerBaseline = '';
  @Input() showSectionPicker = false;
  public readonly editorSections = [{id:'performance',label:'Performance'},{id:'cooling',label:'Cooling'},{id:'display',label:'Display'},{id:'pool',label:'Pool routing'},{id:'scheduler',label:'Scheduler'},{id:'network',label:'Network'},{id:'advanced',label:'Integrations'}];
  public profiles: FiveTratumProfiles | null = null;
  public profilesError = '';
  public profileBusy = false;
  public tuningSlot = 0;
  public tuningName = '';
  public poolSlot = 0;
  public poolName = '';
  public poolTarget: 'primary' | 'fallback' = 'primary';
  public poolSchedule: PoolSchedule | null = null;
  public scheduleError = '';
  public powerReport: PowerReport | null = null;
  public powerSchedule: PowerSchedule | null = null;
  public powerError = '';
  public powerBusy = false;
  private powerWatch?: Subscription;
  public readonly weekDays = [{label:'Sun',bit:1},{label:'Mon',bit:2},{label:'Tue',bit:4},{label:'Wed',bit:8},{label:'Thu',bit:16},{label:'Fri',bit:32},{label:'Sat',bit:64}];
  public readonly utcOffsets = Array.from({length: 105}, (_,i) => (i-48)*15);

  public supportLevel: SupportLevel = SupportLevel.Safe;

  public form!: FormGroup;

  public dialogRef!: NbDialogRef<any>; // Store reference

  public frequencyOptions: { name: string; value: number }[] = [];
  public voltageOptions: { name: string; value: number }[] = [];

  public firmwareUpdateProgress: number | null = null;
  public websiteUpdateProgress: number | null = null;

  public dontShowWarning: boolean = false;

  public eASICModel = eASICModel;
  public asicModel!: eASICModel;

  public defaultFrequency: number = 0;
  public defaultCoreVoltage: number = 0;
  public defaultVrFrequency: number = 0;
  public fanCount: number = 1;

  public lastCoinbaseVerifyMode: number = 1;
  public lastFallbackCoinbaseVerifyMode: number = 1;

  // WiFi scan
  public apActive = false;
  public wifiScanning = false;
  public wifiScanResults: { ssid: string; rssi: number; authmode: number }[] = [];

  toggleCoinbaseVerify(enabled: boolean, controlName: string, lastRef: 'lastCoinbaseVerifyMode' | 'lastFallbackCoinbaseVerifyMode') {
    const ctrl = this.form.controls[controlName];
    if (enabled) {
      ctrl.setValue(this[lastRef]);
    } else {
      this[lastRef] = ctrl.value;
      ctrl.setValue(0);
    }
  }
  public fanLabels: string[] = ['Fan 1', 'Fan 2'];

  public ecoFrequency: number = 0;
  public ecoCoreVoltage: number = 0;

  private originalSettings!: any;

  public otpEnabled = false;
  public hasCanExtension = false;
  private pendingTotp: string | undefined;

  private asicFrequencyValues: number[] = [];
  private asicVoltageValues: number[] = [];

  private rebootRequiredFields = new Set<string>([
    'flipScreen',
    'invertScreen',
    'hostname',
    'ssid',
    'wifiPass',
    'invertFanPolarity',
    'stratumDifficulty',
    'stratumKeep',
    'canMaster',
    'poolMode',
    'stratumProtocol',
    'fallbackStratumProtocol',
    'sv2AuthorityPubkey','sv2ChannelType','fallbackSv2AuthorityPubkey','fallbackSv2ChannelType',
  ]);

  @Input() uri = '';
  @Input() section = 'performance';

  public readonly poolPanels = [
    { index: 0 as const, url: 'stratumURL', port: 'stratumPort', user: 'stratumUser', password: 'stratumPassword', protocol: 'stratumProtocol', tls: 'stratumTLS', enonce: 'stratumEnonceSubscribe', authority: 'sv2AuthorityPubkey', channel: 'sv2ChannelType', verify: 'coinbaseVerifyMode', maxFee: 'coinbaseMaxFee', force: 'coinbaseVerifyForce', lastVerify: 'lastCoinbaseVerifyMode' as const },
    { index: 1 as const, url: 'fallbackStratumURL', port: 'fallbackStratumPort', user: 'fallbackStratumUser', password: 'fallbackStratumPassword', protocol: 'fallbackStratumProtocol', tls: 'fallbackStratumTLS', enonce: 'fallbackStratumEnonceSubscribe', authority: 'fallbackSv2AuthorityPubkey', channel: 'fallbackSv2ChannelType', verify: 'fallbackCoinbaseVerifyMode', maxFee: 'fallbackCoinbaseMaxFee', force: 'fallbackCoinbaseVerifyForce', lastVerify: 'lastFallbackCoinbaseVerifyMode' as const },
  ];
  public readonly fanPanels = [
    { index: 0, mode: 'autofanspeed', manual: 'manualFanSpeed', target: 'pidTargetTemp', overheat: 'overheat_temp', p: 'pidP', i: 'pidI', d: 'pidD' },
    { index: 1, mode: 'fan1Mode', manual: 'fan1ManualSpeed', target: 'fan1PidTargetTemp', overheat: 'fan1OverheatTemp', p: 'fan1PidP', i: 'fan1PidI', d: 'fan1PidD' },
  ];

  public poolPasswordVisible(index: number): boolean {
    return index === 0 ? this.showStratumPassword : this.showFallbackStratumPassword;
  }

  public togglePoolPassword(index: number): void {
    if (index === 0) this.toggleStratumPasswordVisibility();
    else this.toggleFallbackStratumPasswordVisibility();
  }


  constructor(
    private fb: FormBuilder,
    private systemService: SystemService,
    private toastrService: NbToastrService,
    private loadingService: LoadingService,
    private localStorageService: LocalStorageService,
    private dialogService: NbDialogService,
    private otpAuth: OtpAuthService,
    private translate: TranslateService,
  ) { }

  refreshProfiles(): void {
    this.systemService.getProfiles(this.uri).subscribe({next: data => { this.profiles = data; this.profilesError = ''; this.selectTuningSlot(this.tuningSlot); this.selectPoolSlot(this.poolSlot); }, error: () => { this.profiles = null; this.profilesError = 'Profiles require a matching 5tratumFW application.'; }});
  }
  get sectionDirty(): boolean {return !!this.form && changedFields(this.form.getRawValue(),this.baseline,this.section,this.fanCount).length>0;}
  get sectionInvalid(): boolean {return !!this.form && sectionFields(this.section,this.fanCount).some(k=>this.form.get(k)?.enabled && this.form.get(k)?.invalid);}
  get hasUnsavedChanges(): boolean {return !!this.form && Object.keys(this.baseline).some(k=>this.form.getRawValue()[k] !== this.baseline[k]) || !!this.poolSchedule && this.scheduleSnapshot() !== this.scheduleBaseline || !!this.powerSchedule && JSON.stringify(this.powerSchedule) !== this.powerBaseline;}
  private scheduleSnapshot(): string {const s=this.poolSchedule;return s ? JSON.stringify({enabled:s.enabled,utcOffsetMinutes:s.utcOffsetMinutes,events:s.events}) : '';}
  refreshPoolSchedule(): void {
    this.systemService.getPoolSchedule(this.uri).subscribe({next: data => {this.poolSchedule=data;this.scheduleBaseline=this.scheduleSnapshot();this.scheduleError='';},error:()=>{this.poolSchedule=null;this.scheduleError='Pool scheduler is unavailable.';}});
  }
  refreshPower(reloadSchedule = false): void {
    this.systemService.getPowerSchedule(this.uri).subscribe({next: data => {
      this.powerReport = data; this.powerError = '';
      if (!this.powerSchedule || reloadSchedule) {this.powerSchedule = structuredClone(data.schedule);this.powerBaseline=JSON.stringify(this.powerSchedule);}
    }, error: () => {this.powerReport = null; this.powerError = 'Power controls require a matching 5tratumFW application.';}});
  }
  addPowerWindow(): void { if (this.powerSchedule && this.powerSchedule.windows.length < 8) this.powerSchedule.windows.push({days:127,start:'22:00',end:'06:00'}); }
  togglePowerDay(window: PowerWindow, bit: number): void { window.days ^= bit; }
  get validPowerSchedule(): boolean {
    const s = this.powerSchedule;
    const hhmm = /^(?:[01]\d|2[0-3]):[0-5]\d$/;
    return !!s && !!this.powerReport?.supported && this.powerReport.limits.timezones.includes(s.timezone) && s.windows.length <= 8 && (!s.enabled || !!s.windows.length) && s.windows.every(w => w.days >= 1 && w.days <= 127 && hhmm.test(w.start) && hhmm.test(w.end) && w.start !== w.end);
  }
  savePower(): void {
    if (!this.validPowerSchedule || !this.powerSchedule) return;
    const payload = structuredClone(this.powerSchedule);
    this.powerAction(totp => this.systemService.savePowerSchedule(this.uri,payload,totp),'Power schedule saved on miner.',true);
  }
  controlMining(action: 'pause' | 'resume' | 'schedule'): void {
    if (!this.powerReport?.supported) return;
    this.powerAction(totp => this.systemService.miningControl(this.uri,action,totp),'Mining request sent. Applied state is shown below.');
  }
  private powerAction(action: (totp?: string) => Observable<any>, message: string, reloadSchedule = false): void {
    if (this.powerBusy) return;
    this.powerBusy = true;
    this.otpAuth.ensureOtp$(this.uri,this.translate.instant('SECURITY.OTP_TITLE'),this.translate.instant('SECURITY.OTP_HINT')).pipe(switchMap(({totp}: EnsureOtpResult)=>action(totp))).subscribe({
      next:()=>{this.powerBusy=false;this.toastrService.success(message,'Power');this.refreshPower(reloadSchedule);},
      error:()=>{this.powerBusy=false;this.powerError='Power request could not be confirmed. Check the miner status before trying again.';}
    });
  }
  ngOnDestroy(): void { this.powerWatch?.unsubscribe(); }
  selectTuningSlot(slot: number): void { this.tuningSlot = Number(slot); this.tuningName = this.profiles?.tuning[this.tuningSlot]?.name || ''; }
  selectPoolSlot(slot: number): void { this.poolSlot = Number(slot); this.poolName = this.profiles?.pools[this.poolSlot]?.name || ''; }
  selectedProfile(kind: 'tuning' | 'pool'): ProfileSlot | undefined { return kind === 'tuning' ? this.profiles?.tuning[this.tuningSlot] : this.profiles?.pools[this.poolSlot]; }
  setManualTuning(enabled: boolean): void { this.manualTuning = enabled; } // UI only: current values are never replaced.
  saveTuningProfile(): void { this.profileAction((totp) => this.systemService.saveProfile(this.uri, {type:'tuning',slot:this.tuningSlot,name:this.tuningName.trim(),frequencyMHz:this.form.get('frequency')?.value,coreVoltageMv:this.form.get('coreVoltage')?.value},totp), 'Profile saved on miner.'); }
  savePoolProfile(): void { this.profileAction((totp) => this.systemService.saveProfile(this.uri,{type:'pool',slot:this.poolSlot,name:this.poolName.trim(),captureCurrent:this.poolTarget},totp),'Saved pool connection captured on miner.'); }
  clearProfile(kind: 'tuning' | 'pool'): void { this.profileAction(totp => this.systemService.saveProfile(this.uri,{type:kind,slot:kind==='tuning'?this.tuningSlot:this.poolSlot,clear:true},totp),'Profile cleared.'); }
  reviewTuning(saved=false):void {
    const p=saved ? this.selectedProfile('tuning') : null;
    const frequencyMHz=Number(saved ? p?.frequencyMHz : this.form.get('frequency')?.value);
    const coreVoltageMv=Number(saved ? p?.coreVoltageMv : this.form.get('coreVoltage')?.value);
    if(!Number.isInteger(frequencyMHz)||!Number.isInteger(coreVoltageMv))return;
    this.tuningReview={frequencyMHz,coreVoltageMv,name:saved ? p?.name || 'Saved slot' : 'Manual operating point',source:saved?'slot':'manual'};
  }
  applyReviewedTuning():void {const point=this.tuningReview;if(!point)return;this.profileAction(totp=>this.systemService.applyProfile(this.uri,{type:'tuning',frequencyMHz:point.frequencyMHz,coreVoltageMv:point.coreVoltageMv},totp),'Clock and voltage applied.','tuning');this.tuningReview=null;}
  get poolTargetLabel():string {return this.form?.get('poolMode')?.value===1 ? (this.poolTarget==='primary'?'connection A':'connection B') : this.poolTarget;}
  applyTuning(): void { this.profileAction(totp => this.systemService.applyProfile(this.uri,{type:'tuning',frequencyMHz:this.form.get('frequency')?.value,coreVoltageMv:this.form.get('coreVoltage')?.value},totp),'Clock and voltage applied.', 'tuning'); }
  applySavedProfile(kind: 'tuning' | 'pool'): void { this.profileAction(totp => this.systemService.applyProfile(this.uri,kind==='tuning'?{type:kind,slot:this.tuningSlot}:{type:kind,slot:this.poolSlot,poolTarget:this.poolTarget},totp),'Profile applied.',kind); }
  private profileAction(action: (totp?: string) => Observable<any>, message: string, applied?: 'tuning' | 'pool'): void {
    if (this.profileBusy) return;
    this.profileBusy=true;
    this.otpAuth.ensureOtp$(this.uri,this.translate.instant('SECURITY.OTP_TITLE'),this.translate.instant('SECURITY.OTP_HINT')).pipe(switchMap(({totp}: EnsureOtpResult)=>action(totp))).subscribe({
      next:()=>{this.profileBusy=false;this.toastrService.success(message,'Saved');this.refreshProfiles();if(applied==='pool')this.refreshPoolSchedule();if(applied)this.reloadAppliedSettings(applied);},
      error:(err)=>{this.profileBusy=false;const reason=err?.error?.error || 'request-failed';this.toastrService.danger(reason==='profile-in-use'?'Remove this profile from the pool schedule before clearing it.':`Could not save: ${reason}.`,'Device settings');}
    });
  }
  private reloadAppliedSettings(kind:'tuning'|'pool'):void {
    this.systemService.getSettingsV2(this.uri).subscribe(info=>{
      if(kind==='tuning'){this.form.patchValue({frequency:info.frequency,coreVoltage:info.coreVoltage});this.originalSettings.frequency=info.frequency;this.originalSettings.coreVoltage=info.coreVoltage;for(const k of ['frequency','coreVoltage']){this.baseline[k]=this.form.get(k)?.value;this.form.get(k)?.markAsPristine();}return;}
      const i=this.poolTarget==='primary'?0:1;const pool=info.pools[i],panel=this.poolPanels[i];
      const values:any={}; values[panel.url]=pool.url;values[panel.port]=pool.port;values[panel.user]=pool.user;values[panel.password]='*****';values[panel.protocol]=pool.protocol;values[panel.tls]=!!pool.tls;values[panel.enonce]=!!pool.enonceSubscribe;values[panel.authority]=pool.sv2AuthorityPubkey;values[panel.channel]=pool.sv2ChannelType;values[panel.verify]=pool.coinbaseVerifyMode;values[panel.maxFee]=pool.coinbaseMaxFee;values[panel.force]=pool.coinbaseVerifyForce;
      this.form.patchValue(values);this.originalSettings.pools[i]=structuredClone(pool);this.originalSettings[panel.protocol]=pool.protocol;for(const k of Object.keys(values)){this.baseline[k]=values[k];this.form.get(k)?.markAsPristine();}
    });
  }
  addScheduleEvent():void { if(this.poolSchedule && this.poolSchedule.events.length<16)this.poolSchedule.events.push({enabled:true,dayMask:127,timeMinutes:720,slot:this.poolSlot}); }
  hasScheduleDay(mask:number,bit:number):boolean {return (mask & bit)!==0;}
  toggleScheduleDay(event:any,bit:number):void {event.dayMask ^= bit;}
  timeText(minutes:number):string {return `${String(Math.floor(minutes/60)).padStart(2,'0')}:${String(minutes%60).padStart(2,'0')}`;}
  changeEventTime(event:any,value:string):void {const m=/^(\d{2}):(\d{2})$/.exec(value);if(m)event.timeMinutes=Number(m[1])*60+Number(m[2]);}
  offsetText(minutes:number):string {return minutes===0?'UTC':`UTC${minutes<0?'-':'+'}${this.timeText(Math.abs(minutes))}`;}
  get validPoolSchedule():boolean {const s=this.poolSchedule;return !!s && Number.isInteger(Number(s.utcOffsetMinutes)) && this.utcOffsets.includes(Number(s.utcOffsetMinutes)) && s.events.length<=16 && (!s.enabled || s.events.some(e=>e.enabled)) && s.events.every(e=>Number.isInteger(e.dayMask) && e.dayMask>=1 && e.dayMask<=127 && Number.isInteger(e.timeMinutes) && e.timeMinutes>=0 && e.timeMinutes<1440 && Number.isInteger(Number(e.slot)) && !!this.profiles?.pools[Number(e.slot)]?.configured);}
  saveSchedule():void {if(!this.poolSchedule || !this.validPoolSchedule)return;const s=this.poolSchedule;const payload:PoolSchedule={schemaVersion:1,enabled:s.enabled,utcOffsetMinutes:Number(s.utcOffsetMinutes),events:s.events.map(e=>({enabled:e.enabled,dayMask:e.dayMask,timeMinutes:e.timeMinutes,slot:Number(e.slot)}))};this.profileAction(totp=>this.systemService.savePoolSchedule(this.uri,payload,totp),'Pool schedule saved on miner.');}

  ngOnInit(): void {
    this.refreshProfiles();
    this.refreshPoolSchedule();
    this.refreshPower();
    this.powerWatch = timer(2000,2000).subscribe(()=>{if(this.section==='scheduler' && !this.powerBusy)this.refreshPower();});
    this.systemService.getSettingsV2(this.uri)
      .pipe(this.loadingService.lockUIUntilComplete())
      .subscribe((info: ISettingsV2) => {
        this.originalSettings = structuredClone(info);

        this.originalSettings["poolMode"] = info.poolMode ?? 0;
        this.originalSettings["stratumProtocol"] = info.pools?.[0]?.protocol ?? 0;
        this.originalSettings["fallbackStratumProtocol"] = info.pools?.[1]?.protocol ?? 0;
        this.originalSettings["canMaster"] = info.can?.enabled ? 1 : 0;

        this.otpEnabled = !!info.otp;
        this.apActive = !!info.apActive;
        this.hasCanExtension = !!info.can.hasExtension;

        this.asicModel = info.asicModel;

        this.defaultFrequency = info.defaultFrequency ?? 0;
        this.defaultCoreVoltage = info.defaultCoreVoltage ?? 0;

        this.ecoFrequency = info.ecoFrequency ?? undefined;
        this.ecoCoreVoltage = info.ecoCoreVoltage ?? undefined;

        this.asicFrequencyValues = info.frequencyOptions ?? [];
        this.asicVoltageValues = info.voltageOptions ?? [];

        this.defaultVrFrequency = info.defaultVrFrequency ?? undefined;

        this.fanCount = info.fans?.length ?? 1;
        this.fanLabels = info.fans?.map((f: ISettingsV2Fan, i: number) => f.label || `Fan ${i + 1}`) ?? ['Fan 1', 'Fan 2'];
        const fan1cfg = info.fans?.[1];

        const freqBase = this.asicFrequencyValues.map(v => {
          let suffix = '';
          if (v === this.defaultFrequency) suffix = ' (default)';
          if (this.ecoFrequency != null && v === this.ecoFrequency) suffix = ' (eco)';
          return { name: `${v}${suffix}`, value: v };
        });

        const voltBase = this.asicVoltageValues.map(v => {
          let suffix = '';
          if (v === this.defaultCoreVoltage) suffix = ' (default)';
          if (this.ecoCoreVoltage != null && v === this.ecoCoreVoltage) suffix = ' (eco)';
          return { name: `${v}${suffix}`, value: v };
        });

        // Build dropdowns and, if needed, append the current custom value
        this.frequencyOptions = this.assembleDropdownOptions(freqBase, info.frequency);
        this.voltageOptions = this.assembleDropdownOptions(voltBase, info.coreVoltage);

        // Build the form (Min/Max for volt/freq will be set dynamically right after)
        this.form = this.fb.group({
          stratumKeep: [info.stratumKeep == 1],
          canMaster: [info.can.enabled == true],
          flipScreen: [info.flipScreen == 1],
          invertScreen: [info.invertScreen == 1],
          autoScreenOff: [info.autoScreenOff == 1],
          customMempoolEnabled: [!!info.mempoolCustom],
          mempoolUrl: [info.mempoolUrl || 'https://mempool.space'],
          timeFormat: [this.localStorageService.getItem('timeFormat') || '24h'],
          stratumURL: [info.pools[0].url, [
            Validators.required,
            Validators.pattern(/^(?!.*stratum\+tcp:\/\/).*$/),
            Validators.pattern(/^[^:]*$/),
          ]],
          stratumPort: [info.pools[0].port, [
            Validators.required,
            Validators.pattern(/^[^:]*$/),
            Validators.min(0),
            Validators.max(65535)
          ]],
          stratumUser: [info.pools[0].user, [Validators.required]],
          stratumPassword: ['*****', [Validators.required]],
          stratumEnonceSubscribe: [info.pools[0].enonceSubscribe == 1],
          stratumTLS: [info.pools[0].tls == 1],
          coinbaseVerifyMode: [info.pools[0].coinbaseVerifyMode ?? 0],
          coinbaseMaxFee: [info.pools[0].coinbaseMaxFee ?? 3.0],
          coinbaseVerifyForce: [info.pools[0].coinbaseVerifyForce ?? false],

          fallbackStratumURL: [info.pools[1].url, [
            Validators.pattern(/^(?!.*stratum\+tcp:\/\/).*$/),
            Validators.pattern(/^[^:]*$/),
          ]],
          fallbackStratumPort: [info.pools[1].port, [
            Validators.pattern(/^[^:]*$/),
            Validators.min(0),
            Validators.max(65535)
          ]],
          fallbackStratumUser: [info.pools[1].user],
          fallbackStratumPassword: ['*****'],
          fallbackStratumEnonceSubscribe: [info.pools[1].enonceSubscribe == 1],
          fallbackStratumTLS: [info.pools[1].tls == 1],
          fallbackCoinbaseVerifyMode: [info.pools[1].coinbaseVerifyMode ?? 0],
          fallbackCoinbaseMaxFee: [info.pools[1].coinbaseMaxFee ?? 3],
          fallbackCoinbaseVerifyForce: [info.pools[1].coinbaseVerifyForce ?? false],

          hostname: [info.hostname, [Validators.required]],
          ssid: [info.ssid, [Validators.required]],
          wifiPass: ['*****'],

          coreVoltage: [info.coreVoltage, [Validators.min(info.absMinCoreVoltage || 1005), Validators.max(info.absMaxCoreVoltage || 1400), Validators.pattern(/^\d+$/), Validators.required]],
          frequency: [info.frequency, [Validators.required,Validators.min(50),Validators.max(800),Validators.pattern(/^\d+$/)]],
          jobInterval: [info.jobInterval, [Validators.required]],
          stratumDifficulty: [info.stratumDifficulty, [Validators.required, Validators.min(1)]],

          stratumProtocol: [info.pools[0].protocol ?? 0, [Validators.required]],   // 0 = V1, 1 = V2
          fallbackStratumProtocol: [info.pools[1].protocol ?? 0],
          sv2AuthorityPubkey: [info.pools[0].sv2AuthorityPubkey ?? ''],
          fallbackSv2AuthorityPubkey: [info.pools[1].sv2AuthorityPubkey ?? ''],
          sv2ChannelType: [info.pools[0].sv2ChannelType ?? 0],                      // 0 = Extended, 1 = Standard
          fallbackSv2ChannelType: [info.pools[1].sv2ChannelType ?? 0],

          poolMode: [info.poolMode ?? 0, [Validators.required]],                   // 0 = Failover, 1 = Dual
          poolBalance: [info.poolBalance ?? 50, [                                   // Anteil PRIMARY in %
            Validators.required,
            Validators.min(0),
            Validators.max(100),
          ]],

          autofanspeed: [info.fans[0]?.mode ?? 0, [Validators.required]],
          pidTargetTemp: [info.fans[0]?.pid?.targetTemp ?? 55, [
            Validators.min(30),
            Validators.max(80),
            Validators.required
          ]],
          pidP: [info.fans[0]?.pid?.p ?? 6, [
            Validators.min(0),
            Validators.max(100),
            Validators.required
          ]],
          pidI: [info.fans[0]?.pid?.i ?? 0.1, [
            Validators.min(0),
            Validators.max(10),
            Validators.required
          ]],
          pidD: [info.fans[0]?.pid?.d ?? 10, [
            Validators.min(0),
            Validators.max(100),
            Validators.required
          ]],
          invertFanPolarity: [info.invertFanPolarity == 1, [Validators.required]],
          pidUseMax: [info.pidUseMax ?? true],
          manualFanSpeed: [info.fans[0]?.manualSpeed ?? 100, [Validators.required]],
          overheat_temp: [info.fans[0]?.overheatTemp ?? 70, [
            Validators.min(40),
            Validators.max(90),
            Validators.required
          ]],
          vrFrequency: [info.vrFrequency, [
            Validators.min(1000),
            Validators.max(100000),
            Validators.pattern(/^\d+$/),   // only ints
            Validators.required,
          ]],
          otpEnabled: [info.otp],

          fan1Mode: [fan1cfg?.mode ?? 3, [Validators.required]],
          fan1ManualSpeed: [fan1cfg?.manualSpeed ?? 100, [Validators.min(0), Validators.max(100), Validators.required]],
          fan1OverheatTemp: [fan1cfg?.overheatTemp ?? 70, [Validators.min(40), Validators.max(90), Validators.required]],
          fan1PidTargetTemp: [fan1cfg?.pid?.targetTemp ?? 65, [Validators.min(30), Validators.max(80), Validators.required]],
          fan1PidP: [fan1cfg?.pid?.p ?? 6, [Validators.min(0), Validators.max(100), Validators.required]],
          fan1PidI: [fan1cfg?.pid?.i ?? 0.1, [Validators.min(0), Validators.max(10), Validators.required]],
          fan1PidD: [fan1cfg?.pid?.d ?? 10, [Validators.min(0), Validators.max(100), Validators.required]],
        });

        this.baseline=this.form.getRawValue();
        this.lastCoinbaseVerifyMode = info.pools[0].coinbaseVerifyMode || 1;
        this.lastFallbackCoinbaseVerifyMode = info.pools[1].coinbaseVerifyMode || 1;

        this.form.controls['autofanspeed'].valueChanges
          .pipe(startWith(this.form.controls['autofanspeed'].value))
          .subscribe(() => this.updatePIDFieldStates());

        this.form.controls['fan1Mode'].valueChanges
          .pipe(startWith(this.form.controls['fan1Mode'].value))
          .subscribe(() => this.updateFan1FieldStates());

        this.updatePIDFieldStates();
        this.updateFan1FieldStates();

      });
  }

  private updatePIDFieldStates(): void {
    const mode = this.form.controls['autofanspeed'].value;
    const enable = (ctrl: string) => this.form.controls[ctrl]?.enable({ emitEvent: false });
    const disable = (ctrl: string) => this.form.controls[ctrl]?.disable({ emitEvent: false });

    if (mode === 0) {
      enable('manualFanSpeed');
      disable('pidTargetTemp');
      disable('pidP');
      disable('pidI');
      disable('pidD');
    } else if (mode === 1) {
      disable('manualFanSpeed');
      disable('pidTargetTemp');
      disable('pidP');
      disable('pidI');
      disable('pidD');
    } else if (mode === 2) {
      disable('manualFanSpeed');
      enable('pidTargetTemp');
      if (this.supportLevel >= 1) {
        enable('pidP');
        enable('pidI');
        enable('pidD');
      } else {
        disable('pidP');
        disable('pidI');
        disable('pidD');
      }
    }
  }

  private updateFan1FieldStates(): void {
    const mode = this.form.controls['fan1Mode'].value;
    const enable = (ctrl: string) => this.form.controls[ctrl]?.enable({ emitEvent: false });
    const disable = (ctrl: string) => this.form.controls[ctrl]?.disable({ emitEvent: false });

    if (mode === 3) {
      // LINKED — disable fan1-controls; overheatTemp stays enabled (VReg shutdown threshold)
      enable('fan1OverheatTemp');
      disable('fan1ManualSpeed');
      disable('fan1PidTargetTemp');
      disable('fan1PidP');
      disable('fan1PidI');
      disable('fan1PidD');
    } else if (mode === 0) {
      // MANUAL
      enable('fan1ManualSpeed');
      enable('fan1OverheatTemp');
      disable('fan1PidTargetTemp');
      disable('fan1PidP');
      disable('fan1PidI');
      disable('fan1PidD');
    } else if (mode === 2) {
      // PID
      disable('fan1ManualSpeed');
      enable('fan1OverheatTemp');
      enable('fan1PidTargetTemp');
      if (this.supportLevel >= 1) {
        enable('fan1PidP');
        enable('fan1PidI');
        enable('fan1PidD');
      } else {
        disable('fan1PidP');
        disable('fan1PidI');
        disable('fan1PidD');
      }
    }
  }

  public updateSystem(totp?: string) {
    const snapshot=this.form.getRawValue();
    const section=this.section;
    const fields=changedFields(snapshot,this.baseline,section,this.fanCount);
    const needsRestart=this.requiresReboot;
    const payload=settingsPatch(snapshot,this.baseline,section,this.fanCount);
    const request=Object.keys(payload).length ? this.systemService.updateSettingsV2(this.uri,payload,totp) : of(null);
    return request.pipe(tap(()=>{
      if(section==='display' && fields.includes('timeFormat')) {
        this.localStorageService.setItem('timeFormat',snapshot.timeFormat);
        window.dispatchEvent(new CustomEvent('timeFormatChanged',{detail:snapshot.timeFormat}));
      }
      for(const field of fields){this.baseline[field]=snapshot[field];this.form.get(field)?.markAsPristine();}
      this.restartPending=this.restartPending || needsRestart;
    }));
  }

  get requiresReboot(): boolean {
    return !!this.form && changedFields(this.form.getRawValue(),this.baseline,this.section,this.fanCount).some(key=>this.rebootRequiredFields.has(key));
  }

  private normalizeValue(value: any): any {
    if (typeof value === 'boolean') {
      return value ? 1 : 0;
    }
    return value;
  }

  showStratumPassword: boolean = false;
  toggleStratumPasswordVisibility() {
    this.showStratumPassword = !this.showStratumPassword;
  }

  showFallbackStratumPassword: boolean = false;
  toggleFallbackStratumPasswordVisibility() {
    this.showFallbackStratumPassword = !this.showFallbackStratumPassword;
  }

  showWifiPassword: boolean = false;
  toggleWifiPasswordVisibility() {
    this.showWifiPassword = !this.showWifiPassword;
  }

  public setDevToolsOpen(supportLevel: number) {
    this.supportLevel = supportLevel;
    if(!this.form)return;

    const freqBase = this.asicFrequencyValues.map(v => {
      let suffix = '';
      if (v === this.defaultFrequency) suffix = ' (default)';
      if (this.ecoFrequency != null && v === this.ecoFrequency) suffix = ' (eco)';
      return { name: `${v}${suffix}`, value: v };
    });

    const voltBase = this.asicVoltageValues.map(v => {
      let suffix = '';
      if (v === this.defaultCoreVoltage) suffix = ' (default)';
      if (this.ecoCoreVoltage != null && v === this.ecoCoreVoltage) suffix = ' (eco)';
      return { name: `${v}${suffix}`, value: v };
    });

    this.frequencyOptions = this.assembleDropdownOptions(freqBase, this.form.controls['frequency'].value);
    this.voltageOptions = this.assembleDropdownOptions(voltBase, this.form.controls['coreVoltage'].value);

    this.updatePIDFieldStates();
    this.updateFan1FieldStates();
  }

  public isVoltageTooHigh(): boolean {
    if (!this.asicVoltageValues.length) return false;
    const maxVoltage = Math.max(...this.asicVoltageValues);
    return this.form?.controls['coreVoltage'].value > maxVoltage;
  }

  public isFrequencyTooHigh(): boolean {
    if (!this.asicFrequencyValues.length) return false;
    const maxFrequency = Math.max(...this.asicFrequencyValues);
    return this.form?.controls['frequency'].value > maxFrequency;
  }

  public checkVoltageLimit(): void {
    this.form.controls['coreVoltage'].updateValueAndValidity({ emitEvent: false });
  }

  public checkFrequencyLimit(): void {
    this.form.controls['frequency'].updateValueAndValidity({ emitEvent: false });
  }

  /**
   * Dynamically assemble dropdown options, including custom values.
   * @param predefined The predefined options.
   * @param currentValue The current value to include as a custom option if needed.
   */
  private assembleDropdownOptions(predefined: { name: string, value: number }[], currentValue: number): { name: string, value: number }[] {
    const options = [...predefined];
    if (!options.some(option => option.value === currentValue)) {
      options.push({
        name: `${currentValue} (custom)`,
        value: currentValue
      });
    }
    return options;
  }

  public restart() {
    this.otpAuth.ensureOtp$(
      this.uri,
      this.translate.instant('SECURITY.OTP_TITLE'),
      this.translate.instant('SECURITY.OTP_HINT'),
      { disableOtp: true },
    )
      .pipe(
        switchMap(({ totp }: EnsureOtpResult) =>
          this.systemService.restart(this.uri, totp).pipe(
            // drop session on reboot
            tap(() => this.otpAuth.clearSession()),
            this.loadingService.lockUIUntilComplete()
          )
        ),
        catchError((err: HttpErrorResponse) => {
          console.log(err);
          this.toastrService.danger(this.translate.instant('SYSTEM.RESTART_FAILED'), this.translate.instant('COMMON.ERROR'));
          return of(null);
        })
      )
      .subscribe(res => {
        if (res !== null) {
          this.toastrService.success(this.translate.instant('SYSTEM.RESTART_SUCCESS'), this.translate.instant('COMMON.SUCCESS'));
        }
      });
  }

  // Function to check if settings are unsafe
  public hasUnsafeSettings(): boolean {
    return this.section==='performance' && (this.isVoltageTooHigh() || this.isFrequencyTooHigh());
  }

  // Open warning modal unless user disabled it
  public confirmSave(dialog: TemplateRef<any>): void {
    if (!this.localStorageService.getBool('hideUnsafeSettingsWarning') && this.hasUnsafeSettings()) {
      this.dialogRef = this.dialogService.open(dialog, { closeOnBackdropClick: false });
    } else {
      this.runSaveWithOptionalOtp();
    }
  }

  // Save preference and close modal
  public saveAfterWarning(): void {
    if (this.dontShowWarning) {
      this.localStorageService.setBool('hideUnsafeSettingsWarning', true);
    }
    this.dialogRef.close();
    this.runSaveWithOptionalOtp();
  }

  get wrapAroundTime(): number {
    const freq = this.form.get('vrFrequency')?.value;
    if (!freq || freq <= 0) {
      return 0;
    }
    const wrap = 65536 / freq; // seconds
    return wrap;
  }

  private runSaveWithOptionalOtp(): void {
    if(this.saving || this.sectionInvalid || !this.sectionDirty)return;
    this.saving=true;
    this.otpAuth.ensureOtp$(
      this.uri,
      this.translate.instant('SECURITY.OTP_TITLE'),
      this.translate.instant('SECURITY.OTP_HINT')
    )
      .pipe(
        switchMap(({ totp }: EnsureOtpResult) =>
          this.updateSystem(totp).pipe(this.loadingService.lockUIUntilComplete())
        ),
      )
      .subscribe({
        next: () => {
          this.saving=false;this.toastrService.success('This section has been saved on the miner.','Saved');
        },
        error: (err: HttpErrorResponse) => {
          this.saving=false;this.toastrService.danger('Error.', `Could not save. ${err.message}`);
        }
      });
  }

  public poolTabHeader(i: 0 | 1) {
    const protoKey = i === 0 ? 'stratumProtocol' : 'fallbackStratumProtocol';
    const proto = this.form?.get(protoKey)?.value;
    const protoLabel = proto === 1 ? ' (SV2)' : ' (SV1)';

    if (this.form?.get("poolMode")?.value == 0) {
      if (i == 0) {
        return this.translate.instant('SETTINGS.PRIMARY_STRATUM_POOL') + protoLabel;
      }
      return this.translate.instant('SETTINGS.FALLBACK_STRATUM_POOL') + protoLabel;
    }
    return `Connection ${i === 0 ? 'A' : 'B'}` + protoLabel;
  }

  public swapPools(): void {
    if (!this.form) return;

    const pairs: [string, string][] = [
      ['stratumURL',              'fallbackStratumURL'],
      ['stratumPort',             'fallbackStratumPort'],
      ['stratumUser',             'fallbackStratumUser'],
      ['stratumPassword',         'fallbackStratumPassword'],
      ['stratumTLS',              'fallbackStratumTLS'],
      ['stratumEnonceSubscribe',  'fallbackStratumEnonceSubscribe'],
      ['stratumProtocol',         'fallbackStratumProtocol'],
      ['sv2AuthorityPubkey',      'fallbackSv2AuthorityPubkey'],
      ['sv2ChannelType',          'fallbackSv2ChannelType'],
      ['coinbaseVerifyMode',      'fallbackCoinbaseVerifyMode'],
      ['coinbaseMaxFee',          'fallbackCoinbaseMaxFee'],
      ['coinbaseVerifyForce',     'fallbackCoinbaseVerifyForce'],
    ];

    const get = (k: string) => this.form.get(k)?.value;
    const set = (k: string, v: any) => this.form.get(k)?.setValue(v, { emitEvent: false });

    for (const [ka, kb] of pairs) {
      const tmp = get(ka);
      set(ka, get(kb));
      set(kb, tmp);
    }
  }

  public scanWifi(dialog: TemplateRef<any>): void {
    if (this.wifiScanning) return;
    this.wifiScanning = true;
    this.systemService.scanWifi().subscribe({
      next: (response) => {
        const byStrength: { [ssid: string]: { ssid: string; rssi: number; authmode: number } } = {};
        for (const n of response.networks || []) {
          if (!n.ssid) continue;
          if (!byStrength[n.ssid] || n.rssi > byStrength[n.ssid].rssi) {
            byStrength[n.ssid] = n;
          }
        }
        this.wifiScanResults = Object.values(byStrength).sort((a, b) => b.rssi - a.rssi);
        this.wifiScanning = false;
        this.dialogRef = this.dialogService.open(dialog, { closeOnBackdropClick: true });
      },
      error: () => {
        this.wifiScanning = false;
        this.toastrService.danger(
          this.translate.instant('SETTINGS.WIFI_SCAN_FAILED'),
          this.translate.instant('COMMON.ERROR')
        );
      }
    });
  }

  public selectWifiNetwork(ssid: string): void {
    this.form.patchValue({ ssid });
    this.form.markAsDirty();
    if (this.dialogRef) {
      this.dialogRef.close();
    }
  }

  public wifiSignalStrength(rssi: number): string {
    if (rssi >= -50) return 'excellent';
    if (rssi >= -60) return 'good';
    if (rssi >= -70) return 'fair';
    return 'weak';
  }

}
