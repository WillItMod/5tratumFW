import { FormBuilder } from '@angular/forms';
import { ComponentFixture, TestBed, fakeAsync, flushMicrotasks, flush, discardPeriodicTasks } from '@angular/core/testing';
import { NoopAnimationsModule } from '@angular/platform-browser/animations';
import { NbDialogService, NbLayoutModule, NbThemeModule, NbToastrService } from '@nebular/theme';
import { NbEvaIconsModule } from '@nebular/eva-icons';
import { TranslateModule } from '@ngx-translate/core';
import { of, Subject, throwError } from 'rxjs';
import { EditComponent } from './edit.component';
import { EditModule } from './edit.module';
import { ISettingsV2 } from '../../models/ISettingsV2';
import { eASICModel } from '../../models/enum/eASICModel';
import { SystemService } from '../../services/system.service';
import { LoadingService } from '../../services/loading.service';
import { LocalStorageService } from '../../services/local-storage.service';
import { OtpAuthService } from '../../services/otp-auth.service';

function settingsFixture(): ISettingsV2 {
  return {
    asicModel: 'BM1370' as eASICModel, deviceModel: 'NerdQAxe++', version: '5tratumFW-test', otp: true, apActive: false,
    can: { hasExtension: true, enabled: false }, frequency: 577, coreVoltage: 1137, vrFrequency: 25011,
    defaultFrequency: 500, defaultCoreVoltage: 1130, defaultVrFrequency: 25011,
    frequencyOptions: [450, 500, 600], voltageOptions: [1100, 1130, 1150], absMinCoreVoltage: 1005, absMaxCoreVoltage: 1400,
    poolMode: 1, poolBalance: 37, stratumKeep: 1, jobInterval: 333, stratumDifficulty: 16384,
    pools: [
      { url: 'mux.test', port: 7331, user: 'fixture.worker1', enonceSubscribe: 1, tls: 0, protocol: 0,
        sv2AuthorityPubkey: '', sv2ChannelType: 0, coinbaseVerifyMode: 0, coinbaseMaxFee: 3.25, coinbaseVerifyForce: false },
      { url: 'pool.test', port: 4444, user: 'fixture.worker2', enonceSubscribe: 0, tls: 1, protocol: 1,
        sv2AuthorityPubkey: 'fixture-authority', sv2ChannelType: 1, coinbaseVerifyMode: 2, coinbaseMaxFee: 4.5, coinbaseVerifyForce: true },
    ],
    fans: [
      { label: 'ASIC fan', mode: 2, manualSpeed: 83, overheatTemp: 72, pid: { targetTemp: 53, p: 5.1, i: .15, d: 9.5 } },
      { label: 'Regulator fan', mode: 3, manualSpeed: 71, overheatTemp: 78, pid: { targetTemp: 65, p: 6.2, i: .21, d: 11.5 } },
    ],
    invertFanPolarity: 0, pidUseMax: true, hostname: 'fixture-miner', ssid: 'fixture-wifi',
    mempoolCustom: false, mempoolUrl: '', flipScreen: 0, invertScreen: 1, autoScreenOff: 1,
  };
}

describe('Nerd settings payload preservation', () => {
  let component: EditComponent;
  let source: ISettingsV2;
  let update: jasmine.Spy;
  let auth: jasmine.Spy;
  let saveProfile: jasmine.Spy;
  let applyProfile: jasmine.Spy;
  let saveSchedule: jasmine.Spy;
  let savePower: jasmine.Spy;
  let controlMining: jasmine.Spy;
  let getSettings: jasmine.Spy;
  let getProfiles: jasmine.Spy;
  let success: jasmine.Spy;
  let danger: jasmine.Spy;
  let profileData: any;
  beforeEach(() => {
    source = settingsFixture();
    update = jasmine.createSpy('updateSettingsV2').and.returnValue(of({}));
    saveProfile = jasmine.createSpy('saveProfile').and.returnValue(of({ok:true,restartRequired:false}));
    applyProfile = jasmine.createSpy('applyProfile').and.returnValue(of({ok:true,restartRequired:false}));
    saveSchedule = jasmine.createSpy('savePoolSchedule').and.returnValue(of({ok:true,restartRequired:false}));
    savePower = jasmine.createSpy('savePowerSchedule').and.returnValue(of({ok:true}));
    controlMining = jasmine.createSpy('miningControl').and.returnValue(of({ok:true}));
    auth = jasmine.createSpy('ensureOtp$').and.returnValue(of({ totp: '123456' }));
    getSettings = jasmine.createSpy('getSettingsV2').and.callFake(() => of(source));
    profileData = {schemaVersion:1,tuning:Array.from({length:10},(_,slot)=>({slot,configured:false,name:null})),pools:Array.from({length:10},(_,slot)=>({slot,configured:false,name:null})),limits:{frequency:{min:50,max:800,step:1},coreVoltage:{min:1005,max:1400,step:1}},current:{frequencyMHz:source.frequency,coreVoltageMv:source.coreVoltage}};
    getProfiles = jasmine.createSpy('getProfiles').and.callFake(() => of(profileData));
    success = jasmine.createSpy('success');danger = jasmine.createSpy('danger');
    component = new EditComponent(new FormBuilder(), {
      getSettingsV2: getSettings, updateSettingsV2: update,
      getProfiles,
      getPoolSchedule: () => of({schemaVersion:1,enabled:false,utcOffsetMinutes:0,events:[],clockValid:false}),
      getPowerSchedule: () => of({supported:true,schedule:{enabled:false,timezone:'UTC',windows:[]},status:{clockValid:false,localTime:null,scheduledPause:false,manualOverride:'none',requestedPaused:false,appliedPaused:false,transitionPending:false,error:null},limits:{maxWindows:8,timezones:['UTC','Europe/London']}}),
      savePowerSchedule:savePower, miningControl:controlMining,
      saveProfile, applyProfile, savePoolSchedule:saveSchedule,
    } as any, { success, danger } as any,
    { lockUIUntilComplete: () => (stream: any) => stream } as any,
    { getItem: () => '24h', setItem: () => {}, getBool: () => false } as any,
    {} as any, { ensureOtp$: auth } as any, { instant: (key: string) => key } as any);
    component.ngOnInit();
  });
  afterEach(()=>component.ngOnDestroy());

  it('saves only weekly power policy after OTP without modifying clock, voltage or pool settings',()=>{
    component.powerSchedule={enabled:true,timezone:'Europe/London',windows:[{days:62,start:'22:00',end:'06:00'}]};
    component.savePower();
    expect(savePower.calls.mostRecent().args).toEqual(['', {enabled:true,timezone:'Europe/London',windows:[{days:62,start:'22:00',end:'06:00'}]}, '123456']);
    expect(update).not.toHaveBeenCalled(); expect(applyProfile).not.toHaveBeenCalled();
    expect(component.form.get('frequency')?.value).toBe(577);
  });
  it('keeps unsaved power windows during status reads and sends manual requests through their own API',()=>{
    component.addPowerWindow();component.powerSchedule!.timezone='Europe/London';component.refreshPower();
    expect(component.powerSchedule!.windows.length).toBe(1);expect(component.powerSchedule!.timezone).toBe('Europe/London');
    component.controlMining('pause');expect(controlMining.calls.mostRecent().args).toEqual(['','pause','123456']);
    expect(update).not.toHaveBeenCalled();expect(savePower).not.toHaveBeenCalled();
  });
  it('blocks empty enabled power policies and unsupported boards',()=>{
    component.powerSchedule={enabled:true,timezone:'UTC',windows:[]};component.savePower();expect(savePower).not.toHaveBeenCalled();
    component.powerReport!.supported=false;component.controlMining('resume');expect(controlMining).not.toHaveBeenCalled();
  });

  it('preserves saved custom clock and voltage in the visible option lists', () => {
    expect(component.form.get('frequency')?.value).toBe(577);
    expect(component.form.get('coreVoltage')?.value).toBe(1137);
    expect(component.frequencyOptions.find(option => option.value === 577)?.name).toContain('custom');
    expect(component.voltageOptions.find(option => option.value === 1137)?.name).toContain('custom');
    component.setDevToolsOpen(2);
    expect(component.form.get('frequency')?.value).toBe(577);
    expect(component.form.get('vrFrequency')?.value).toBe(25011);
  });

  it('keeps unchanged settings and masked credentials without issuing a PATCH', () => {
    component.updateSystem('123456').subscribe();
    expect(update).not.toHaveBeenCalled();
    expect(auth).not.toHaveBeenCalled();
    expect(component.form.get('frequency')?.value).toBe(577);
    expect(component.form.get('coreVoltage')?.value).toBe(1137);
    expect(component.form.get('fan1PidI')?.value).toBe(source.fans[1].pid.i);
    expect(component.form.get('fallbackStratumPassword')?.value).toBe('*****');
    expect(component.sectionDirty).toBeFalse();
  });

  it('writes a changed performance field while retaining unrelated pool and cooling settings', () => {
    component.form.patchValue({ frequency: 578, fallbackStratumUser: 'unsaved-other-section' });
    component.updateSystem('123456').subscribe();
    expect(update.calls.mostRecent().args).toEqual(['', { frequency: 578 }, '123456']);
    expect(component.form.get('coreVoltage')?.value).toBe(1137);
    expect(component.form.get('fan1PidI')?.value).toBe(source.fans[1].pid.i);
    expect(component.sectionDirty).toBeFalse();
    expect(component.hasUnsavedChanges).toBeTrue();
    component.section = 'pool';
    expect(component.sectionDirty).toBeTrue();
  });

  it('writes changed passwords only from their owning section, including empty Wi-Fi passwords', () => {
    component.section = 'pool';
    component.form.patchValue({ fallbackStratumPassword: 'fixture-replacement', wifiPass: '' });
    component.updateSystem().subscribe();
    expect(update.calls.mostRecent().args).toEqual(['', { pools: [{}, { password: 'fixture-replacement' }] }, undefined]);
    expect(component.form.get('stratumPassword')?.value).toBe('*****');
    component.section = 'network';
    expect(component.requiresReboot).toBeTrue();
    component.updateSystem().subscribe();
    expect(update.calls.mostRecent().args).toEqual(['', { wifiPass: '' }, undefined]);
    expect(update).toHaveBeenCalledTimes(2);
    expect(component.restartPending).toBeTrue();
    expect(component.requiresReboot).toBeFalse();
  });

  it('runs OTP authorization before saving from the redesigned Save action', () => {
    component.form.patchValue({ frequency: 578 });
    expect(component.sectionDirty).toBeTrue();
    component.confirmSave({} as any);
    expect(auth).toHaveBeenCalledTimes(1);
    expect(update.calls.mostRecent().args[2]).toBe('123456');
  });

  it('swaps complete pool configurations including SV2 and verification settings', () => {
    component.section = 'pool';
    component.swapPools();
    component.updateSystem().subscribe();
    const payload = update.calls.mostRecent().args[1];
    expect(payload.pools[0]).toEqual({ ...source.pools[1], tls: true, enonceSubscribe: false });
    expect(payload.pools[1]).toEqual({ ...source.pools[0], tls: false, enonceSubscribe: true });
    expect(payload.poolBalance).toBeUndefined();
    expect(component.form.get('poolBalance')?.value).toBe(37);
  });

  it('retains verification choice when toggled off and on', () => {
    component.toggleCoinbaseVerify(false, 'fallbackCoinbaseVerifyMode', 'lastFallbackCoinbaseVerifyMode');
    expect(component.form.get('fallbackCoinbaseVerifyMode')?.value).toBe(0);
    component.toggleCoinbaseVerify(true, 'fallbackCoinbaseVerifyMode', 'lastFallbackCoinbaseVerifyMode');
    expect(component.form.get('fallbackCoinbaseVerifyMode')?.value).toBe(2);
  });

  it('opens manual mode with saved QAxe values and sends no writes',()=>{
    source.frequency=500;source.coreVoltage=1130;component.ngOnDestroy();component.ngOnInit();component.setManualTuning(true);
    expect(component.form.get('frequency')?.value).toBe(500);expect(component.form.get('coreVoltage')?.value).toBe(1130);
    expect(update).not.toHaveBeenCalled();expect(saveProfile).not.toHaveBeenCalled();expect(applyProfile).not.toHaveBeenCalled();
  });
  it('stores a named tuning slot without applying it',()=>{
    component.tuningSlot=9;component.tuningName='Custom 577';component.saveTuningProfile();
    expect(auth).toHaveBeenCalledTimes(1);expect(saveProfile.calls.mostRecent().args).toEqual(['',{type:'tuning',slot:9,name:'Custom 577',frequencyMHz:577,coreVoltageMv:1137},'123456']);expect(applyProfile).not.toHaveBeenCalled();expect(update).not.toHaveBeenCalled();
  });
  for (const [label, response] of [
    ['null',null],['missing body',undefined],['empty object',{}],['array',[]],
    ['refused',{ok:false,restartRequired:false}],['missing flag',{ok:true}],
    ['nonboolean flag',{ok:true,restartRequired:'false'}],['nonboolean ok',{ok:1,restartRequired:false}],
  ] as const) {
    it(`does not claim profile success or read back after a ${label} acknowledgement`,()=>{
      profileData.pools[2]={slot:2,configured:true,name:'Stored'};
      component.poolSaveNames[0]='Saved';component.poolRenameNames[2]='Renamed';
      saveProfile.and.returnValue(of(response));applyProfile.and.returnValue(of(response));saveSchedule.and.returnValue(of(response));
      getProfiles.calls.reset();getSettings.calls.reset();
      const actions=[()=>component.saveTuningProfile(),()=>component.applyTuning(),()=>component.saveConnectionToSlot(0),
        ()=>component.renamePoolSlot(2),()=>component.clearPoolSlot(2),()=>component.applyPoolSlot(2,1),
        ()=>component.clearProfile('tuning'),()=>component.saveSchedule()];
      for (const action of actions) {
        action();
        expect(component.profileBusy).toBeFalse();expect(component.restartPending).toBeFalse();
        expect(component.profilesError).toContain('did not confirm');
      }
      expect(saveProfile.calls.count()).toBe(5);expect(applyProfile.calls.count()).toBe(2);expect(saveSchedule.calls.count()).toBe(1);
      expect(success).not.toHaveBeenCalled();expect(danger.calls.count()).toBe(8);
      expect(getProfiles).not.toHaveBeenCalled();expect(getSettings).not.toHaveBeenCalled();expect(update).not.toHaveBeenCalled();
      expect(component.form.get('fallbackStratumUser')?.value).toBe(source.pools[1].user);
      expect(component.form.get('frequency')?.value).toBe(source.frequency);
    });
  }
  it('reports acknowledged Apply with failed settings readback without retry or success feedback',()=>{
    profileData.pools[2]={slot:2,configured:true,name:'Stored'};
    getSettings.and.returnValue(throwError(()=>new Error('readback unavailable')));
    component.applyPoolSlot(2,1);
    expect(applyProfile.calls.count()).toBe(1);expect(component.profileBusy).toBeFalse();
    expect(component.profilesError).toContain('acknowledged');expect(component.profilesError).toContain('could not be read back');
    expect(success).not.toHaveBeenCalled();expect(danger).toHaveBeenCalled();expect(update).not.toHaveBeenCalled();
    expect(component.form.get('fallbackStratumUser')?.value).toBe(source.pools[1].user);
  });
  it('keeps a stored request unconfirmed when profile readback fails after its valid ACK',()=>{
    component.poolSaveNames[0]='Saved';getProfiles.and.returnValue(throwError(()=>new Error('readback unavailable')));
    component.saveConnectionToSlot(0);
    expect(saveProfile.calls.count()).toBe(1);expect(component.profileBusy).toBeFalse();
    expect(component.profilesError).toContain('acknowledged');expect(success).not.toHaveBeenCalled();expect(danger).toHaveBeenCalled();
    expect(component.profiles).toBe(profileData);expect(applyProfile).not.toHaveBeenCalled();expect(update).not.toHaveBeenCalled();
  });
  it('reports success and the restart requirement only after a strict ACK and profile readback',()=>{
    component.poolSaveNames[0]='Saved';const readback=new Subject<any>();getProfiles.and.returnValue(readback);
    saveProfile.and.returnValue(of({ok:true,restartRequired:true}));component.saveConnectionToSlot(0);
    expect(component.profileBusy).toBeTrue();expect(component.restartPending).toBeTrue();expect(success).not.toHaveBeenCalled();
    readback.next(profileData);expect(component.profileBusy).toBeFalse();expect(success.calls.count()).toBe(1);
  });
  it('applies manual settings atomically without unrelated fields',()=>{
    component.applyTuning();expect(applyProfile.calls.mostRecent().args).toEqual(['',{type:'tuning',frequencyMHz:577,coreVoltageMv:1137},'123456']);expect(update).not.toHaveBeenCalled();expect(component.form.get('fan1ManualSpeed')?.value).toBe(71);
  });
  it('saves miner credentials to a named slot and applies only the chosen Secondary target',()=>{
    component.selectPoolSaveSlot(1,8);component.poolSaveNames[1]='DGB';component.saveConnectionToSlot(1);
    expect(saveProfile.calls.mostRecent().args).toEqual(['',{type:'pool',slot:8,name:'DGB',captureCurrent:'fallback'},'123456']);
    profileData.pools[8]={slot:8,configured:true,name:'DGB'};
    component.applyPoolSlot(8,1);expect(applyProfile.calls.mostRecent().args).toEqual(['',{type:'pool',slot:8,poolTarget:'fallback'},'123456']);expect(component.form.get('stratumURL')?.value).toBe('mux.test');expect(component.form.get('poolBalance')?.value).toBe(37);
  });
  it('refuses storing a dirty route including security options and keeps credentials miner-side',()=>{
    component.poolSaveNames[0]='Primary saved';
    component.form.patchValue({stratumTLS:true,stratumPassword:'private-draft'});
    component.saveConnectionToSlot(0);
    expect(component.profilesError).toBe('Save pool settings first.');
    expect(saveProfile).not.toHaveBeenCalled();expect(auth).not.toHaveBeenCalled();
    expect(component.form.get('stratumPassword')?.value).toBe('private-draft');
  });
  it('saves only Primary and shared routing options, leaving Secondary and tuning drafts intact',()=>{
    component.section='pool';
    component.form.patchValue({stratumUser:'new-primary',stratumTLS:true,poolBalance:42,fallbackStratumUser:'secondary-draft',frequency:578});
    component.savePoolSettings(0);
    expect(update.calls.mostRecent().args).toEqual(['',{poolBalance:42,pools:[{user:'new-primary',tls:true},{}]},'123456']);
    expect(component.poolRouteDirty(0)).toBeFalse();expect(component.poolRouteDirty(1)).toBeTrue();
    expect(component.form.get('fallbackStratumUser')?.value).toBe('secondary-draft');
    component.poolSaveNames[0]='Saved Primary';component.saveConnectionToSlot(0);
    expect(saveProfile.calls.mostRecent().args[1]).toEqual({type:'pool',slot:0,name:'Saved Primary',captureCurrent:'primary'});
    expect(component.form.get('frequency')?.value).toBe(578);
  });
  it('saves Secondary with positional placeholders and all changed verification fields only',()=>{
    component.section='pool';component.form.patchValue({fallbackSv2AuthorityPubkey:'new-authority',fallbackCoinbaseMaxFee:2.5,fallbackCoinbaseVerifyForce:false,stratumUser:'primary-draft'});
    component.savePoolSettings(1);
    expect(update.calls.mostRecent().args[1]).toEqual({pools:[{},{sv2AuthorityPubkey:'new-authority',coinbaseMaxFee:2.5,coinbaseVerifyForce:false}]});
    expect(component.poolRouteDirty(1)).toBeFalse();expect(component.poolRouteDirty(0)).toBeTrue();
  });
  it('renames a configured slot with metadata only while retaining route drafts',()=>{
    profileData.pools[4]={slot:4,configured:true,name:'Old',host:'saved.test',passwordConfigured:true};
    component.poolRenameNames[4]=' Renamed ';component.form.patchValue({stratumPassword:'primary-draft',fallbackStratumTLS:false});
    component.renamePoolSlot(4);
    expect(saveProfile.calls.mostRecent().args).toEqual(['',{type:'pool',slot:4,name:'Renamed'},'123456']);
    expect(applyProfile).not.toHaveBeenCalled();expect(update).not.toHaveBeenCalled();
    expect(component.form.get('stratumPassword')?.value).toBe('primary-draft');
    expect(component.poolRouteDirty(1)).toBeTrue();
  });
  it('blocks empty-slot actions and Apply to a dirty destination without asking for OTP',()=>{
    component.poolRenameNames[3]='Empty rename';component.renamePoolSlot(3);component.clearPoolSlot(3);component.applyPoolSlot(3,0);
    profileData.pools[4]={slot:4,configured:true,name:'Saved'};component.form.patchValue({fallbackCoinbaseVerifyMode:1});component.applyPoolSlot(4,1);
    expect(component.profilesError).toBe('Save pool settings first.');expect(auth).not.toHaveBeenCalled();expect(saveProfile).not.toHaveBeenCalled();expect(applyProfile).not.toHaveBeenCalled();
  });
  it('binds delayed Apply to its explicit destination and retains the other route draft on readback',()=>{
    profileData.pools[7]={slot:7,configured:true,name:'Saved'};
    const otp=new Subject<any>(), settings=new Subject<ISettingsV2>();
    auth.and.returnValue(otp);getSettings.and.returnValue(settings);
    component.form.patchValue({stratumUser:'keep-primary-draft'});
    component.applyPoolSlot(7,1);
    component.selectPoolSaveSlot(0,2);component.selectPoolSaveSlot(1,9);
    otp.next({totp:'654321'});
    expect(applyProfile.calls.mostRecent().args).toEqual(['',{type:'pool',slot:7,poolTarget:'fallback'},'654321']);
    const readback=structuredClone(source);readback.pools[1].user='saved-secondary';settings.next(readback);
    expect(component.form.get('fallbackStratumUser')?.value).toBe('saved-secondary');
    expect(component.form.get('stratumUser')?.value).toBe('keep-primary-draft');expect(component.poolRouteDirty(0)).toBeTrue();
    expect(component.form.get('poolBalance')?.value).toBe(37);
  });
  it('clears only the requested slot without changing the pool schedule',()=>{
    profileData.pools[6]={slot:6,configured:true,name:'Scheduled'};component.clearPoolSlot(6);
    expect(saveProfile.calls.mostRecent().args).toEqual(['',{type:'pool',slot:6,clear:true},'123456']);
    expect(saveSchedule).not.toHaveBeenCalled();expect(update).not.toHaveBeenCalled();
  });
  it('rechecks route drafts after delayed OTP before storing or applying a slot',()=>{
    profileData.pools[2]={slot:2,configured:true,name:'Saved'};
    const otp=new Subject<any>();auth.and.returnValue(otp);
    component.poolSaveNames[0]='Primary';component.saveConnectionToSlot(0);
    component.form.get('stratumPassword')!.setValue('late-draft');otp.next({totp:'123456'});
    expect(saveProfile).not.toHaveBeenCalled();expect(component.profilesError).toBe('Save pool settings first.');
    const secondOtp=new Subject<any>();auth.and.returnValue(secondOtp);
    component.applyPoolSlot(2,1);component.form.get('fallbackStratumUser')!.setValue('late-worker');secondOtp.next({totp:'123456'});
    expect(applyProfile).not.toHaveBeenCalled();
  });
  it('keeps a new destination draft made during Apply readback and blocks another action until readback',()=>{
    profileData.pools[2]={slot:2,configured:true,name:'Saved'};
    const settings=new Subject<ISettingsV2>();getSettings.and.returnValue(settings);
    component.applyPoolSlot(2,1);expect(component.profileBusy).toBeTrue();
    component.form.get('fallbackStratumUser')!.setValue('late-draft');component.applyPoolSlot(2,0);
    expect(applyProfile).toHaveBeenCalledTimes(1);
    const readback=structuredClone(source);readback.pools[1].user='saved-secondary';settings.next(readback);
    expect(component.profileBusy).toBeFalse();expect(component.form.get('fallbackStratumUser')?.value).toBe('late-draft');expect(component.poolRouteDirty(1)).toBeTrue();
  });
  it('stores weekly timepoints and explicit offset without readonly metadata',()=>{
    component.profiles!.pools[2] = {slot:2,configured:true,name:'Configured pool'};
    component.poolSchedule={schemaVersion:1,enabled:true,utcOffsetMinutes:60,events:[{enabled:true,dayMask:62,timeMinutes:720,slot:2}],clockValid:true,selectedSlot:2};
    expect(component.validPoolSchedule).toBeTrue();component.saveSchedule();
    expect(saveSchedule.calls.mostRecent().args).toEqual(['',{schemaVersion:1,enabled:true,utcOffsetMinutes:60,events:[{enabled:true,dayMask:62,timeMinutes:720,slot:2}]},'123456']);
  });

  it('rejects a scheduled slot that has not been configured on the miner', () => {
    component.poolSchedule={schemaVersion:1,enabled:true,utcOffsetMinutes:0,events:[{enabled:true,dayMask:127,timeMinutes:720,slot:2}],clockValid:true};
    expect(component.validPoolSchedule).toBeFalse();
    component.saveSchedule();
    expect(saveSchedule).not.toHaveBeenCalled();
    expect(auth).not.toHaveBeenCalled();
  });

  it('uses the real fan count and preserves the single-channel payload', () => {
    source.fans = source.fans.slice(0, 1);
    component.ngOnDestroy();
    component.ngOnInit();
    component.section = 'cooling';
    component.form.patchValue({ overheat_temp: 74 });
    component.updateSystem().subscribe();
    expect(component.fanCount).toBe(1);
    expect(update.calls.mostRecent().args).toEqual(['', { fans: [{ overheatTemp: 74 }] }, undefined]);
    expect(component.form.get('manualFanSpeed')?.value).toBe(83);
  });
});

describe('Pool connection options in the production template', () => {
  let fixture: ComponentFixture<EditComponent>;
  let source: ISettingsV2;
  let update: jasmine.Spy;
  let authorize: jasmine.Spy;
  let saveProfile: jasmine.Spy;
  let applyProfile: jasmine.Spy;

  beforeEach(async () => {
    source = settingsFixture();
    update = jasmine.createSpy('updateSettingsV2').and.returnValue(of({}));
    authorize = jasmine.createSpy('ensureOtp$').and.returnValue(of({ totp: '123456' }));
    saveProfile = jasmine.createSpy('saveProfile').and.returnValue(of({ok:true,restartRequired:false}));
    applyProfile = jasmine.createSpy('applyProfile').and.returnValue(of({ok:true,restartRequired:false}));
    await TestBed.configureTestingModule({
      imports: [EditModule, NbThemeModule.forRoot({ name: 'default' }), NbLayoutModule,
        NbEvaIconsModule, NoopAnimationsModule, TranslateModule.forRoot()],
      providers: [
        { provide: SystemService, useValue: {
          getSettingsV2: () => of(source), updateSettingsV2: update,
          getProfiles: () => of({ tuning: [], pools: Array.from({length:10},(_,slot)=>slot===2 ? {slot,configured:true,name:'Saved BTC',host:'saved.test',port:3333,user:'saved.worker',passwordConfigured:true} : {slot,configured:false,name:null}) }),
          saveProfile, applyProfile,
          getPoolSchedule: () => of({ schemaVersion: 1, enabled: false, utcOffsetMinutes: 0, events: [] }),
          getPowerSchedule: () => of({ supported: false, schedule: { enabled: false, timezone: 'UTC', windows: [] } }),
        } },
        { provide: NbToastrService, useValue: { success: () => {}, danger: () => {} } },
        { provide: LoadingService, useValue: { lockUIUntilComplete: () => (stream: any) => stream } },
        { provide: LocalStorageService, useValue: {
          getItem: () => '24h', getBool: () => false, getNumber: () => undefined,
          setItem: () => {}, setBool: () => {}, setNumber: () => {},
        } },
        { provide: NbDialogService, useValue: { open: jasmine.createSpy('open') } },
        { provide: OtpAuthService, useValue: { ensureOtp$: authorize } },
      ],
    }).compileComponents();
  });

  afterEach(() => fixture?.destroy());

  function render(model = 'NerdQAxe++'): EditComponent {
    source.deviceModel = model;
    fixture = TestBed.createComponent(EditComponent);
    fixture.componentInstance.section = 'pool';
    fixture.detectChanges();
    return fixture.componentInstance;
  }

  function connectionOptions(): HTMLDetailsElement {
    const details = Array.from(fixture.nativeElement.querySelectorAll('details')) as HTMLDetailsElement[];
    return details.find(item => item.querySelector('summary')?.textContent?.trim() === 'Connection options')!;
  }

  for (const model of ['NerdQAxe++', 'NerdOCTAXE-γ']) {
    it(`shows the saved difficulty when ${model} opens Connection options at the default level`, () => {
      const component = render(model);
      expect(component.supportLevel).toBe(0);
      const disclosure = connectionOptions();
      expect(disclosure).toBeTruthy();
      disclosure.querySelector('summary')!.click();
      fixture.detectChanges();
      expect(disclosure.open).toBeTrue();
      const input = disclosure.querySelector('input[formControlName="stratumDifficulty"]') as HTMLInputElement;
      expect(input).not.toBeNull();
      expect(input.value).toBe('16384');
      expect(input.getBoundingClientRect().height).toBeGreaterThan(0);
      expect(component.sectionDirty).toBeFalse();
      expect(update).not.toHaveBeenCalled();
      expect(authorize).not.toHaveBeenCalled();
    });
  }

  it('saves only an edited connection difficulty through the existing OTP action', () => {
    const component = render();
    const disclosure = connectionOptions();
    disclosure.open = true;
    const input = disclosure.querySelector('input[formControlName="stratumDifficulty"]') as HTMLInputElement;
    expect(input).not.toBeNull();
    input.value = '32768';
    input.dispatchEvent(new Event('input', { bubbles: true }));
    fixture.detectChanges();
    const save = fixture.nativeElement.querySelector('[data-save-pool-settings]') as HTMLButtonElement;
    expect(save.disabled).toBeFalse();
    save.click();
    fixture.detectChanges();
    expect(authorize).toHaveBeenCalledTimes(1);
    expect(update.calls.mostRecent().args).toEqual(['', { stratumDifficulty: 32768 }, '123456']);
    expect(component.form.get('frequency')?.value).toBe(source.frequency);
    expect(component.form.get('coreVoltage')?.value).toBe(source.coreVoltage);
    expect(component.form.get('stratumPassword')?.value).toBe('*****');
    expect(component.form.get('fallbackStratumProtocol')?.value).toBe(source.pools[1].protocol);
    expect(component.form.get('fallbackCoinbaseVerifyForce')?.value).toBeTrue();
    expect(component.restartPending).toBeTrue();
  });

  it('keeps unchanged and invalid difficulty from writing and leaves tuning gates intact', () => {
    const component = render();
    const save = fixture.nativeElement.querySelector('[data-save-pool-settings]') as HTMLButtonElement;
    expect(save.disabled).toBeTrue();
    expect(fixture.nativeElement.querySelector('input[formControlName="jobInterval"]')).toBeNull();
    const disclosure = connectionOptions();
    disclosure.open = true;
    const input = disclosure.querySelector('input[formControlName="stratumDifficulty"]') as HTMLInputElement;
    expect(input).not.toBeNull();
    input.value = '0';
    input.dispatchEvent(new Event('input', { bubbles: true }));
    fixture.detectChanges();
    expect(component.sectionInvalid).toBeTrue();
    expect(save.disabled).toBeTrue();
    save.click();
    expect(update).not.toHaveBeenCalled();
    expect(authorize).not.toHaveBeenCalled();
  });

  for (const model of ['NerdQAxe++','NerdOCTAXE-γ']) {
    it(`renders ${model} route editors before ten saved slots with explicit destination actions`,()=>{
      const component=render(model), root=fixture.nativeElement as HTMLElement;
      const editors=root.querySelectorAll('[data-pool-editor]'), saved=root.querySelector('[data-saved-pools]')!;
      expect(editors.length).toBe(2);expect(editors[1].compareDocumentPosition(saved)&Node.DOCUMENT_POSITION_FOLLOWING).toBeTruthy();
      expect(editors[0].querySelector('h3')?.textContent).toContain('Primary · A');expect(editors[1].querySelector('h3')?.textContent).toContain('Secondary · B');
      expect(root.querySelectorAll('[data-pool-slot-row]').length).toBe(10);
      const row=root.querySelector('[data-pool-slot-row="2"]')!;
      expect(Array.from(row.querySelectorAll('button')).map(b=>b.textContent?.trim())).toEqual(['Apply to Primary','Apply to Secondary','Rename','Clear slot']);
      expect(saved.textContent).not.toContain('Capture');expect(root.querySelector('[data-pool-editor="0"] select')?.querySelectorAll('option').length).toBe(10);
      component.form.get('poolMode')!.setValue(0);fixture.detectChanges();
      expect(editors[1].querySelector('h3')?.textContent).toContain('Secondary · standby');
      expect(update).not.toHaveBeenCalled();expect(saveProfile).not.toHaveBeenCalled();expect(applyProfile).not.toHaveBeenCalled();
    });
  }
  it('uses explicit rendered Secondary Apply and metadata-only Rename actions',fakeAsync(()=>{
    const component=render();
    flushMicrotasks();fixture.detectChanges();
    let row=fixture.nativeElement.querySelector('[data-pool-slot-row="2"]') as HTMLElement;
    row.querySelectorAll('button')[1].click();fixture.detectChanges();
    expect(applyProfile.calls.mostRecent().args[1]).toEqual({type:'pool',slot:2,poolTarget:'fallback'});
    flushMicrotasks();fixture.detectChanges();
    row=fixture.nativeElement.querySelector('[data-pool-slot-row="2"]') as HTMLElement;
    const name=row.querySelector('input') as HTMLInputElement;name.value='Bitcoin backup';name.dispatchEvent(new Event('input',{bubbles:true}));
    flushMicrotasks();fixture.detectChanges();row.querySelectorAll('button')[2].click();fixture.detectChanges();
    expect(saveProfile.calls.mostRecent().args[1]).toEqual({type:'pool',slot:2,name:'Bitcoin backup'});
    expect(update).not.toHaveBeenCalled();
    // Nebular schedules one-shot value/accessibility updates for each control.
    fixture.destroy();discardPeriodicTasks();flush(500);
  }));
  it('shows the save-first instruction and disables slot saving until the route is saved',()=>{
    const component=render();component.poolSaveNames[0]='New Primary';component.form.get('stratumUser')!.setValue('edited-worker');fixture.detectChanges();
    const editor=fixture.nativeElement.querySelector('[data-pool-editor="0"]') as HTMLElement;
    expect((editor.querySelector('[data-save-pool-slot]') as HTMLButtonElement).disabled).toBeTrue();expect(editor.textContent).toContain('Save pool settings first.');
    (editor.querySelector('[data-save-pool-settings]') as HTMLButtonElement).click();fixture.detectChanges();
    expect((editor.querySelector('[data-save-pool-slot]') as HTMLButtonElement).disabled).toBeFalse();
    (editor.querySelector('[data-save-pool-slot]') as HTMLButtonElement).click();fixture.detectChanges();
    expect(saveProfile.calls.mostRecent().args[1]).toEqual({type:'pool',slot:0,name:'New Primary',captureCurrent:'primary'});
  });
});
