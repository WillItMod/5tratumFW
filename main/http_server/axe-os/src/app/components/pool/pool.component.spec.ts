import { CommonModule } from '@angular/common';
import { Component, Input } from '@angular/core';
import { ComponentFixture, TestBed, fakeAsync, tick } from '@angular/core/testing';
import { FormsModule, ReactiveFormsModule } from '@angular/forms';
import { ToastrService } from 'ngx-toastr';
import { EMPTY, ReplaySubject, Subject, of } from 'rxjs';
import { SystemInfo } from 'src/app/generated/models';
import { LiveDataService } from 'src/app/services/live-data.service';
import { LoadingService } from 'src/app/services/loading.service';
import { SystemApiService } from 'src/app/services/system.service';
import { PoolComponent } from './pool.component';
import { OperatingProfilesComponent } from '../operating-profiles/operating-profiles.component';
import { OperatingProfilesService, Profiles } from 'src/app/services/operating-profiles.service';

@Component({ selector: 'tooltip-text-icon', template: '{{text}}' })
class TooltipStub { @Input() text = ''; @Input() tooltip = ''; }

const storedPools: Profiles = {schemaVersion: 1, tuning: Array.from({length:10},(_,slot)=>({slot,configured:false,name:null})),
  pools: Array.from({length:10},(_,slot)=>({slot,configured:slot===2,name:slot===2?'DGB':null,host:'saved.pool',port:3333})),
  current:{frequencyMHz:500,coreVoltageMv:1130},limits:{frequency:{min:400,max:625,step:.25,quantization:'nearest-pll'},coreVoltage:{min:1000,max:1250,step:1}}};
const reading = {
  ASICModel: 'BM1370', uptimeSeconds: 600, sharesAccepted: 100, sharesRejected: 1,
  stratumURL: 'direct.pool.local', stratumPort: 7331, stratumUser: 'wallet.worker',
  stratumProtocol: 'SV2', stratumV2AuthorityPubkey: '', stratumV2ChannelType: 'extended',
  stratumExtranonceSubscribe: false, stratumSuggestedDifficulty: 1024, stratumTLS: 2,
  stratumCert: '-----BEGIN CERTIFICATE-----\nexample\n-----END CERTIFICATE-----', stratumDecodeCoinbase: true,
  fallbackStratumURL: 'fallback.pool.local', fallbackStratumPort: 3333,
  fallbackStratumUser: 'fallback.wallet.worker', fallbackStratumProtocol: 'SV1',
  fallbackStratumExtranonceSubscribe: false, fallbackStratumSuggestedDifficulty: 0,
  fallbackStratumTLS: 0, fallbackStratumCert: '', fallbackStratumDecodeCoinbase: false,
  fallbackStratumV2AuthorityPubkey: '', fallbackStratumV2ChannelType: 'standard',
  isUsingFallbackStratum: 0, miningPaused: false, frequency: 541, coreVoltage: 1177,
} as unknown as SystemInfo;

describe('Unified Pool routing editor', () => {
  let component: PoolComponent;
  let fixture: ComponentFixture<PoolComponent>;
  let api: jasmine.SpyObj<SystemApiService>;
  let readings: ReplaySubject<SystemInfo>;
  let live: { info$: ReplaySubject<SystemInfo>; lastUpdateAt: number };
  let profiles: jasmine.SpyObj<OperatingProfilesService>;

  beforeEach(() => {
    readings = new ReplaySubject<SystemInfo>(1);
    live = { info$: readings, lastUpdateAt: Date.now() };
    readings.next(reading);
    api = jasmine.createSpyObj<SystemApiService>('SystemApiService', ['updateSystem', 'restart', 'getInfo']);
    api.updateSystem.and.returnValue(of(undefined));
    api.restart.and.returnValue(of({ message: 'Restart requested' }));
    api.getInfo.and.returnValue(EMPTY);
    profiles=jasmine.createSpyObj<OperatingProfilesService>('profiles',['get','save','apply']);
    profiles.get.and.returnValue(of(structuredClone(storedPools)));
    profiles.save.and.returnValue(of({ok:true,restartRequired:false}));
    profiles.apply.and.returnValue(of({ok:true,restartRequired:false}));
    TestBed.configureTestingModule({ declarations: [PoolComponent, TooltipStub, OperatingProfilesComponent], imports: [CommonModule, FormsModule, ReactiveFormsModule], providers: [
      { provide: SystemApiService, useValue: api }, { provide: LiveDataService, useValue: live },
      {provide: OperatingProfilesService,useValue:profiles},
      { provide: ToastrService, useValue: jasmine.createSpyObj('ToastrService', ['success', 'warning', 'error', 'info']) }, LoadingService,
    ] });
    fixture = TestBed.createComponent(PoolComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  function muxDraft() {
    component.selectConnectionMode('mux');
    component.form.patchValue({ stratumURL: '198.51.100.20', stratumPort: 7331, stratumUser: 'gamma-601-worker' });
    component.form.get('stratumURL')!.markAsDirty();
  }

  it('does not infer MUX from port 7331 and mode changes neither write nor alter the shared draft', () => {
    expect(component.connectionMode).toBe('direct');
    component.form.get('stratumUser')!.setValue('unsaved.worker');
    component.form.get('stratumUser')!.markAsDirty();
    component.form.get('fallbackStratumURL')!.setValue('unsaved.fallback');
    component.form.get('fallbackStratumURL')!.markAsDirty();
    const draft = component.form.getRawValue();
    component.selectConnectionMode('mux');
    component.selectConnectionMode('direct');
    expect(component.form.getRawValue()).toEqual(draft);
    expect(component.form.get('stratumUser')!.dirty).toBeTrue();
    expect(component.form.get('fallbackStratumURL')!.dirty).toBeTrue();
    expect(component.savedChanges).toBeFalse();
    expect(api.updateSystem).not.toHaveBeenCalled();
    expect(api.restart).not.toHaveBeenCalled();
  });

  it('renders one form with the same primary fields and hides direct advanced controls in MUX mode', () => {
    component.selectConnectionMode('mux');
    fixture.detectChanges();
    expect(fixture.nativeElement.querySelectorAll('form').length).toBe(1);
    expect(fixture.nativeElement.querySelectorAll('#stratumURL').length).toBe(1);
    expect(fixture.nativeElement.querySelector('app-mux')).toBeNull();
    expect(fixture.nativeElement.querySelector('.routing-advanced')).toBeNull();
    expect(fixture.nativeElement.querySelector('label[for="stratumURL"]').textContent).toBe('MUX host');
  });

  it('requires review and sends only the exact primary MUX preset through the supplied device URI', () => {
    component.uri = 'http://remote-miner.local';
    muxDraft();
    component.updateSystem();
    expect(api.updateSystem).not.toHaveBeenCalled();
    component.reviewMuxConnection();
    component.updateSystem();
    expect(api.updateSystem).toHaveBeenCalledOnceWith(component.uri, {
      stratumURL: '198.51.100.20', stratumPort: 7331, stratumUser: 'gamma-601-worker',
      stratumProtocol: 'SV1', stratumExtranonceSubscribe: true, stratumSuggestedDifficulty: 0,
      stratumTLS: 0, stratumDecodeCoinbase: false, useFallbackStratum: 0,
    });
    expect(api.restart).not.toHaveBeenCalled();
    expect(component.form.get('frequency')).toBeNull();
  });

  it('keeps invalid fallback drafts out of a MUX save and preserves their dirtiness after acknowledgement', () => {
    component.form.get('fallbackStratumPort')!.setValue(-1);
    component.form.get('fallbackStratumPort')!.markAsDirty();
    component.form.get('fallbackStratumPassword')!.setValue('unsaved-fallback-secret');
    component.form.get('fallbackStratumPassword')!.markAsDirty();
    muxDraft();
    expect(component.form.invalid).toBeTrue();
    component.reviewMuxConnection();
    component.updateSystem();
    const payload = api.updateSystem.calls.mostRecent().args[1];
    expect(Object.keys(payload).some(key => key.startsWith('fallback'))).toBeFalse();
    expect(payload.frequency).toBeUndefined();
    expect(payload.coreVoltage).toBeUndefined();
    expect(payload.wifiPass).toBeUndefined();
    expect(component.form.get('fallbackStratumPort')!.value).toBe(-1);
    expect(component.form.get('fallbackStratumPassword')!.value).toBe('unsaved-fallback-secret');
    expect(component.fallbackDraftDirty).toBeTrue();
    expect(component.form.dirty).toBeTrue();
    expect(component.restartPending).toBeTrue();
  });

  it('marks saved state and masks a replacement password only after the device acknowledges', () => {
    const confirmation = new Subject<void>();
    api.updateSystem.and.returnValue(confirmation);
    muxDraft();
    component.form.get('stratumPassword')!.setValue('new-secret');
    component.form.get('stratumPassword')!.markAsDirty();
    component.reviewMuxConnection();
    component.updateSystem();
    expect(component.savedChanges).toBeFalse();
    expect(component.restartPending).toBeFalse();
    expect(component.form.dirty).toBeTrue();
    expect(component.form.get('stratumPassword')!.value).toBe('new-secret');
    confirmation.next(undefined);
    confirmation.complete();
    expect(component.savedChanges).toBeTrue();
    expect(component.restartPending).toBeTrue();
    expect(component.form.get('stratumPassword')!.value).toBe('*****');
    expect(component.form.get('stratumProtocol')!.value).toBe('SV1');
    expect(api.restart).not.toHaveBeenCalled();
  });

  it('retains a failed save draft without claiming it was saved or restarting', () => {
    const failure = new Subject<void>();
    api.updateSystem.and.returnValue(failure);
    muxDraft();
    component.reviewMuxConnection();
    component.updateSystem();
    failure.error(new Error('Unavailable'));
    expect(component.savedChanges).toBeFalse();
    expect(component.restartPending).toBeFalse();
    expect(component.form.dirty).toBeTrue();
    expect(component.error).toContain('did not confirm saving');
    expect(api.restart).not.toHaveBeenCalled();
  });

  it('invalidates a review after an endpoint edit and does not save a stale reviewed target', () => {
    muxDraft();
    component.reviewMuxConnection();
    component.form.get('stratumURL')!.setValue('different-mux.local');
    component.updateSystem();
    expect(component.reviewedPatch).toBeUndefined();
    expect(api.updateSystem).not.toHaveBeenCalled();
  });

  it('retains inactive custom CA and SV2 key drafts without blocking Direct mode after forced MUX defaults', () => {
    component.form.patchValue({ stratumProtocol: 'SV1', stratumTLS: 2, stratumCert: '' });
    expect(component.form.get('stratumCert')!.hasError('required')).toBeTrue();
    component.form.patchValue({ stratumProtocol: 'SV2', stratumV2AuthorityPubkey: 'invalid-0-key' });
    expect(component.form.get('stratumV2AuthorityPubkey')!.invalid).toBeTrue();
    muxDraft();
    component.reviewMuxConnection();
    component.updateSystem();
    component.selectConnectionMode('direct');
    expect(component.form.get('stratumCert')!.value).toBe('');
    expect(component.form.get('stratumV2AuthorityPubkey')!.value).toBe('invalid-0-key');
    expect(component.form.get('stratumCert')!.valid).toBeTrue();
    expect(component.form.get('stratumV2AuthorityPubkey')!.valid).toBeTrue();
    component.form.get('stratumProtocol')!.setValue('SV2');
    expect(component.form.get('stratumV2AuthorityPubkey')!.invalid).toBeTrue();
  });

  it('shows the specific MUX handoff mismatch rather than hiding the cause behind a generic state', () => {
    muxDraft();
    live.lastUpdateAt = Date.now();
    readings.next({ ...reading, stratumURL: '198.51.100.20', stratumUser: 'gamma-601-worker', stratumProtocol: 'SV1', stratumTLS: false, stratumSuggestedDifficulty: 0 });
    fixture.detectChanges();
    expect(fixture.nativeElement.querySelector('.connection-observation small').textContent).toContain('Extranonce subscription: disabled');
    expect(fixture.nativeElement.querySelector('.connection-observation small').textContent).toContain('Coinbase decoding: enabled');
  });

  it('omits both masked passwords in Direct mode and keeps explicit replacement passwords only in their own route', () => {
    component.form.get('stratumURL')!.setValue('new-direct.pool');
    component.form.get('stratumURL')!.markAsDirty();
    component.updateSystem();
    const first = api.updateSystem.calls.mostRecent().args[1];
    expect(first.stratumPassword).toBeUndefined();
    expect(first.fallbackStratumPassword).toBeUndefined();
    component.form.get('fallbackStratumPassword')!.setValue('fallback-secret');
    component.form.get('fallbackStratumPassword')!.markAsDirty();
    component.selectedPool='fallbackStratum';
    component.updateSystem();
    expect(api.updateSystem.calls.mostRecent().args[1].fallbackStratumPassword).toBe('fallback-secret');
    expect(component.form.get('fallbackStratumPassword')!.value).toBe('*****');
    expect(api.restart).not.toHaveBeenCalled();
  });

  it('saves only the selected Direct route and retains an invalid password draft on the other route', () => {
    component.form.get('stratumUser')!.setValue('new.primary.worker');component.form.get('stratumUser')!.markAsDirty();
    component.form.get('fallbackStratumPort')!.setValue(-1);component.form.get('fallbackStratumPort')!.markAsDirty();
    component.form.get('fallbackStratumPassword')!.setValue('unsaved.secondary.secret');component.form.get('fallbackStratumPassword')!.markAsDirty();
    component.updateSystem();const body=api.updateSystem.calls.mostRecent().args[1];
    expect(body.stratumUser).toBe('new.primary.worker');expect(Object.keys(body).some(k=>k.startsWith('fallback'))).toBeFalse();
    expect(component.form.get('fallbackStratumPort')!.value).toBe(-1);expect(component.form.get('fallbackStratumPassword')!.value).toBe('unsaved.secondary.secret');
    expect(component.form.get('fallbackStratumPassword')!.dirty).toBeTrue();expect(api.restart).not.toHaveBeenCalled();
  });

  it('does not overwrite the shared draft when fresh device readings arrive', () => {
    component.form.get('stratumUser')!.setValue('my.unsaved.worker');
    component.form.get('stratumUser')!.markAsDirty();
    live.lastUpdateAt = Date.now();
    readings.next({ ...reading, stratumUser: 'external.saved.worker', frequency: 550 });
    expect(component.info?.stratumUser).toBe('external.saved.worker');
    expect(component.form.get('stratumUser')!.value).toBe('my.unsaved.worker');
    expect(component.form.get('stratumUser')!.dirty).toBeTrue();
  });

  it('separates a restart request from a fresh observed new boot', fakeAsync(() => {
    muxDraft();
    component.reviewMuxConnection();
    component.updateSystem();
    component.form.get('fallbackStratumURL')!.setValue('still.unsaved.fallback');
    component.form.get('fallbackStratumURL')!.markAsDirty();
    component.restart();
    expect(api.restart).toHaveBeenCalledOnceWith('');
    expect(component.restartRequested).toBeTrue();
    expect(component.restartObserved).toBeFalse();
    tick(1000);
    component.receiveInfo({ ...reading, uptimeSeconds: 601 }, Date.now());
    expect(component.restartPending).toBeTrue();
    tick(1000);
    component.receiveInfo({ ...reading, uptimeSeconds: 2, sharesAccepted: 0 }, Date.now());
    expect(component.restartPending).toBeFalse();
    expect(component.restartObserved).toBeTrue();
    expect(component.form.get('fallbackStratumURL')!.value).toBe('still.unsaved.fallback');
    component.receiveInfo({ ...reading, uptimeSeconds: 0 }, Date.now() - 1000);
    expect(component.info?.uptimeSeconds).toBe(2);
  }));
  it('preserves custom CA security on a bare-host edit and changes TLS only for an explicit scheme', () => {
    component.form.get('stratumProtocol')!.setValue('SV1');
    const certificate = component.form.get('stratumCert')!.value;
    component.form.get('stratumURL')!.setValue('renamed.secure.pool');
    component.onUrlChange('stratum');
    expect(component.form.get('stratumTLS')!.value).toBe(2);
    expect(component.form.get('stratumCert')!.value).toBe(certificate);
    expect(component.form.get('stratumCert')!.valid).toBeTrue();
    component.form.get('stratumURL')!.setValue('stratum+tls://tls.pool:4444');
    component.onUrlChange('stratum');
    expect(component.form.get('stratumTLS')!.value).toBe(1);
    expect(component.form.get('stratumPort')!.value).toBe(4444);
    component.form.get('stratumURL')!.setValue('stratum+tcp://tcp.pool:3333');
    component.onUrlChange('stratum');
    expect(component.form.get('stratumTLS')!.value).toBe(0);
    expect(api.updateSystem).not.toHaveBeenCalled();
  });

  it('places both route editors before the ten saved slots, with slot saving beside connection saving', () => {
    const root: HTMLElement=fixture.nativeElement;
    const form=root.querySelector('form')!, slots=root.querySelector('app-operating-profiles')!;
    expect(form.compareDocumentPosition(slots) & Node.DOCUMENT_POSITION_FOLLOWING).toBeTruthy();
    expect(root.querySelectorAll('.profiles-slot').length).toBe(10);
    const bar=root.querySelector('.routing-save-actions')!;
    expect(bar.textContent).toContain('Save pool settings');expect(bar.textContent).toContain('Save to slot');
    expect(bar.querySelector('select')?.querySelectorAll('option').length).toBe(10);
    expect(root.querySelectorAll('.routing-path button')[1].textContent).toContain('Secondary route');
    expect(profiles.save).not.toHaveBeenCalled();expect(profiles.apply).not.toHaveBeenCalled();
  });

  it('refuses saving unsaved route or password edits to a slot without silently saving the connection', () => {
    component.poolSlotName='BTC';component.form.get('stratumPassword')!.setValue('draft-secret');
    component.saveConnectionToSlot();
    expect(component.poolSlotError).toContain('Save Primary pool settings first');
    expect(profiles.save).not.toHaveBeenCalled();expect(api.updateSystem).not.toHaveBeenCalled();
    expect(component.form.get('stratumPassword')!.value).toBe('draft-secret');
    component.form.get('stratumPassword')!.setValue('*****');
    component.selectedPool='fallbackStratum';component.form.get('fallbackStratumURL')!.markAsDirty();
    component.saveConnectionToSlot();expect(component.poolSlotError).toContain('Save Secondary pool settings first');
    expect(profiles.save).not.toHaveBeenCalled();expect(api.restart).not.toHaveBeenCalled();
  });

  it('captures only the saved chosen route and name; no password placeholder or current fields reach the slot API', () => {
    component.selectedPool='fallbackStratum';component.selectPoolSlot(7);component.poolSlotName=' Evening ';
    component.saveConnectionToSlot();
    expect(profiles.save).toHaveBeenCalledOnceWith({type:'pool',slot:7,name:'Evening',captureCurrent:'fallback'},'');
    expect(api.updateSystem).not.toHaveBeenCalled();expect(api.restart).not.toHaveBeenCalled();
    expect(profiles.apply).not.toHaveBeenCalled();expect(component.poolSlotMessage).toContain('Secondary saved to slot 8');
  });

  it('requires saving the MUX preset before storing it, and waits for the connection acknowledgement', () => {
    const ack=new Subject<void>();api.updateSystem.and.returnValue(ack);
    muxDraft();component.poolSlotName='MUX';component.saveConnectionToSlot();
    expect(profiles.save).not.toHaveBeenCalled();component.reviewMuxConnection();component.updateSystem();
    component.saveConnectionToSlot();expect(profiles.save).not.toHaveBeenCalled();
    ack.next();ack.complete();component.saveConnectionToSlot();
    expect(profiles.save).toHaveBeenCalledOnceWith({type:'pool',slot:0,name:'MUX',captureCurrent:'primary'},'');
    expect(api.restart).not.toHaveBeenCalled();
  });

  it('applies a stored slot explicitly to Secondary, refreshes that editor, and preserves a separate Primary draft', () => {
    component.form.get('stratumUser')!.setValue('my.unsaved.primary');component.form.get('stratumUser')!.markAsDirty();
    api.getInfo.and.returnValue(of({...reading,fallbackStratumURL:'saved.secondary',fallbackStratumUser:'saved.worker'}));
    profiles.apply.and.returnValue(of({ok:true,restartRequired:true}));
    const row:HTMLElement=fixture.nativeElement.querySelectorAll('.profiles-slot')[2];
    const buttons=Array.from(row.querySelectorAll('button'));
    expect(buttons.map(b=>b.textContent?.trim())).toEqual(['Apply to Primary','Apply to Secondary','Rename','Clear']);
    buttons[1].click();fixture.detectChanges();
    expect(profiles.apply).toHaveBeenCalledOnceWith({type:'pool',slot:2,poolTarget:'fallback'},'');
    expect(component.form.get('fallbackStratumURL')!.value).toBe('saved.secondary');
    expect(component.form.get('stratumUser')!.value).toBe('my.unsaved.primary');
    expect(component.form.get('stratumUser')!.dirty).toBeTrue();expect(component.selectedPool).toBe('fallbackStratum');
    expect(component.restartPending).toBeTrue();expect(api.restart).not.toHaveBeenCalled();
  });

  it('does not apply a slot over destination edits and does not restart on failed application', () => {
    component.form.get('fallbackStratumURL')!.setValue('draft.secondary');component.form.get('fallbackStratumURL')!.markAsDirty();
    fixture.detectChanges();component.poolProfiles!.applyPoolSlot(component.poolProfiles!.slots[2],'fallback');
    expect(profiles.apply).not.toHaveBeenCalled();expect(component.poolProfiles!.error).toContain('unsaved Secondary');
    component.form.get('fallbackStratumURL')!.setValue(reading.fallbackStratumURL);component.form.get('fallbackStratumURL')!.markAsPristine();
    fixture.detectChanges();const failure=new Subject<{ok:boolean;restartRequired:boolean}>();profiles.apply.and.returnValue(failure);
    component.poolProfiles!.applyPoolSlot(component.poolProfiles!.slots[2],'fallback');failure.error(new Error('lost acknowledgement'));
    expect(component.form.get('fallbackStratumURL')!.value).toBe(reading.fallbackStratumURL);
    expect(api.restart).not.toHaveBeenCalled();
  });

  it('refreshes renamed slots in the selector without replacing an unsaved slot-name draft', () => {
    component.selectPoolSlot(2);const renamed=structuredClone(storedPools.pools);renamed[2].name='New name';
    component.poolSlotsUpdated(renamed);expect(component.poolSlotName).toBe('New name');
    component.editPoolSlotName('My unsaved name');renamed[2].name='Another saved name';component.poolSlotsUpdated(renamed);
    expect(component.poolSlotName).toBe('My unsaved name');expect(component.poolSlots[2].name).toBe('Another saved name');
    expect(profiles.save).not.toHaveBeenCalled();expect(profiles.apply).not.toHaveBeenCalled();
  });

  it('does not claim a slot was saved for an invalid acknowledgement or replay an ambiguous write', () => {
    component.poolSlotName='BTC';profiles.save.and.returnValue(of({ok:false,restartRequired:false}));
    const reads=profiles.get.calls.count();component.saveConnectionToSlot();
    expect(component.poolSlotMessage).toBe('');expect(component.poolSlotError).toContain('did not confirm');
    expect(profiles.save).toHaveBeenCalledTimes(1);expect(profiles.get.calls.count()).toBe(reads);
    expect(api.updateSystem).not.toHaveBeenCalled();expect(api.restart).not.toHaveBeenCalled();
  });

  it('holds Apply busy through delayed route readback and preserves only newer edited fields', () => {
    const next=new Subject<SystemInfo>();api.getInfo.and.returnValue(next);
    component.poolSlotApplied({poolTarget:'fallback',restartRequired:false});
    expect(component.busy).toBeTrue();component.form.get('fallbackStratumUser')!.setValue('newer.draft');
    component.form.get('fallbackStratumUser')!.markAsDirty();
    next.next({...reading,fallbackStratumURL:'stored.new.host',fallbackStratumUser:'stored.new.worker'});next.complete();
    expect(component.busy).toBeFalse();expect(component.form.get('fallbackStratumURL')!.value).toBe('stored.new.host');
    expect(component.form.get('fallbackStratumUser')!.value).toBe('newer.draft');
    expect(component.connectionDirty.fallback).toBeTrue();expect(component.error).toContain('New connection edits were retained');
    expect(api.updateSystem).not.toHaveBeenCalled();expect(api.restart).not.toHaveBeenCalled();
  });

});
