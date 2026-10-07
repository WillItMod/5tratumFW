import { FormBuilder } from '@angular/forms';
import { of } from 'rxjs';
import { EditComponent } from './edit.component';
import { ISettingsV2 } from '../../models/ISettingsV2';
import { eASICModel } from '../../models/enum/eASICModel';

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
  beforeEach(() => {
    source = settingsFixture();
    update = jasmine.createSpy('updateSettingsV2').and.returnValue(of({}));
    saveProfile = jasmine.createSpy('saveProfile').and.returnValue(of({ok:true}));
    applyProfile = jasmine.createSpy('applyProfile').and.returnValue(of({ok:true}));
    saveSchedule = jasmine.createSpy('savePoolSchedule').and.returnValue(of({ok:true}));
    savePower = jasmine.createSpy('savePowerSchedule').and.returnValue(of({ok:true}));
    controlMining = jasmine.createSpy('miningControl').and.returnValue(of({ok:true}));
    auth = jasmine.createSpy('ensureOtp$').and.returnValue(of({ totp: '123456' }));
    component = new EditComponent(new FormBuilder(), {
      getSettingsV2: () => of(source), updateSettingsV2: update,
      getProfiles: () => of({schemaVersion:1,tuning:Array.from({length:10},(_,slot)=>({slot,configured:false,name:null})),pools:Array.from({length:10},(_,slot)=>({slot,configured:false,name:null})),limits:{frequency:{min:50,max:800,step:1},coreVoltage:{min:1005,max:1400,step:1}},current:{frequencyMHz:source.frequency,coreVoltageMv:source.coreVoltage}}),
      getPoolSchedule: () => of({schemaVersion:1,enabled:false,utcOffsetMinutes:0,events:[],clockValid:false}),
      getPowerSchedule: () => of({supported:true,schedule:{enabled:false,timezone:'UTC',windows:[]},status:{clockValid:false,localTime:null,scheduledPause:false,manualOverride:'none',requestedPaused:false,appliedPaused:false,transitionPending:false,error:null},limits:{maxWindows:8,timezones:['UTC','Europe/London']}}),
      savePowerSchedule:savePower, miningControl:controlMining,
      saveProfile, applyProfile, savePoolSchedule:saveSchedule,
    } as any, { success: () => {}, danger: () => {} } as any,
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

  it('saves the unchanged nested v2 payload without masked credentials or browser preferences', () => {
    component.updateSystem('123456').subscribe();
    const [uri, payload, totp] = update.calls.mostRecent().args;
    expect(uri).toBe('');
    expect(totp).toBe('123456');
    expect(payload).toEqual({
      hostname: source.hostname, ssid: source.ssid, frequency: 577, coreVoltage: 1137, vrFrequency: 25011,
      jobInterval: 333, stratumDifficulty: 16384, poolMode: 1, poolBalance: 37, stratumKeep: 1,
      pools: source.pools.map(pool => ({ ...pool, tls: !!pool.tls, enonceSubscribe: !!pool.enonceSubscribe })),
      fans: source.fans.map(fan => ({ mode: fan.mode, manualSpeed: fan.manualSpeed, overheatTemp: fan.overheatTemp, pid: { ...fan.pid } })),
      invertFanPolarity: false, pidUseMax: true, mempoolCustom: false, mempoolUrl: '',
      flipScreen: false, invertScreen: true, autoScreenOff: true, canMaster: false,
    });
    expect(payload.wifiPass).toBeUndefined();
    expect(payload.pools.every((pool: any) => !('password' in pool))).toBeTrue();
    expect(payload.timeFormat).toBeUndefined();
    expect(payload.fans[1].pid).toEqual(source.fans[1].pid); // linked/disabled fields are retained
  });

  it('sends an intentionally changed password, including an empty Wi-Fi password', () => {
    component.form.patchValue({ fallbackStratumPassword: 'fixture-replacement', wifiPass: '' });
    component.updateSystem().subscribe();
    const payload = update.calls.mostRecent().args[1];
    expect(payload.pools[0].password).toBeUndefined();
    expect(payload.pools[1].password).toBe('fixture-replacement');
    expect(payload.wifiPass).toBe('');
    expect(component.requiresReboot).toBeTrue();
  });

  it('runs OTP authorization before saving from the redesigned Save action', () => {
    component.confirmSave({} as any);
    expect(auth).toHaveBeenCalledTimes(1);
    expect(update.calls.mostRecent().args[2]).toBe('123456');
  });

  it('swaps complete pool configurations including SV2 and verification settings', () => {
    component.swapPools();
    component.updateSystem().subscribe();
    const payload = update.calls.mostRecent().args[1];
    expect(payload.pools[0]).toEqual({ ...source.pools[1], tls: true, enonceSubscribe: false });
    expect(payload.pools[1]).toEqual({ ...source.pools[0], tls: false, enonceSubscribe: true });
    expect(payload.poolBalance).toBe(37);
  });

  it('retains verification choice when toggled off and on', () => {
    component.toggleCoinbaseVerify(false, 'fallbackCoinbaseVerifyMode', 'lastFallbackCoinbaseVerifyMode');
    expect(component.form.get('fallbackCoinbaseVerifyMode')?.value).toBe(0);
    component.toggleCoinbaseVerify(true, 'fallbackCoinbaseVerifyMode', 'lastFallbackCoinbaseVerifyMode');
    expect(component.form.get('fallbackCoinbaseVerifyMode')?.value).toBe(2);
  });

  it('opens manual mode with saved QAxe values and sends no writes',()=>{
    source.frequency=500;source.coreVoltage=1130;component.ngOnInit();component.setManualTuning(true);
    expect(component.form.get('frequency')?.value).toBe(500);expect(component.form.get('coreVoltage')?.value).toBe(1130);
    expect(update).not.toHaveBeenCalled();expect(saveProfile).not.toHaveBeenCalled();expect(applyProfile).not.toHaveBeenCalled();
  });
  it('stores a named tuning slot without applying it',()=>{
    component.tuningSlot=9;component.tuningName='Custom 577';component.saveTuningProfile();
    expect(auth).toHaveBeenCalledTimes(1);expect(saveProfile.calls.mostRecent().args).toEqual(['',{type:'tuning',slot:9,name:'Custom 577',frequencyMHz:577,coreVoltageMv:1137},'123456']);expect(applyProfile).not.toHaveBeenCalled();expect(update).not.toHaveBeenCalled();
  });
  it('applies manual settings atomically without unrelated fields',()=>{
    component.applyTuning();expect(applyProfile.calls.mostRecent().args).toEqual(['',{type:'tuning',frequencyMHz:577,coreVoltageMv:1137},'123456']);expect(update).not.toHaveBeenCalled();expect(component.form.get('fan1ManualSpeed')?.value).toBe(71);
  });
  it('captures miner credentials and applies only the chosen pool target',()=>{
    component.poolSlot=8;component.poolName='DGB';component.poolTarget='fallback';component.savePoolProfile();
    expect(saveProfile.calls.mostRecent().args).toEqual(['',{type:'pool',slot:8,name:'DGB',captureCurrent:'fallback'},'123456']);
    component.applySavedProfile('pool');expect(applyProfile.calls.mostRecent().args).toEqual(['',{type:'pool',slot:8,poolTarget:'fallback'},'123456']);expect(component.form.get('stratumURL')?.value).toBe('mux.test');expect(component.form.get('poolBalance')?.value).toBe(37);
  });
  it('stores weekly timepoints and explicit offset without readonly metadata',()=>{
    component.poolSchedule={schemaVersion:1,enabled:true,utcOffsetMinutes:60,events:[{enabled:true,dayMask:62,timeMinutes:720,slot:2}],clockValid:true,selectedSlot:2};component.saveSchedule();
    expect(saveSchedule.calls.mostRecent().args).toEqual(['',{schemaVersion:1,enabled:true,utcOffsetMinutes:60,events:[{enabled:true,dayMask:62,timeMinutes:720,slot:2}]},'123456']);
  });

  it('uses the real fan count and preserves the single-channel payload', () => {
    source.fans = source.fans.slice(0, 1);
    component.ngOnInit();
    component.updateSystem().subscribe();
    expect(component.fanCount).toBe(1);
    expect(update.calls.mostRecent().args[1].fans.length).toBe(1);
  });
});
