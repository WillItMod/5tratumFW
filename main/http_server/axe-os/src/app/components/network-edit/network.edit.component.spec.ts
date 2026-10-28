import { ComponentFixture, TestBed } from '@angular/core/testing';

import { NetworkEditComponent } from './network.edit.component';
import { provideHttpClient } from '@angular/common/http';
import { provideToastr } from 'ngx-toastr';
import { DialogService } from 'src/app/services/dialog.service';
import { DialogService as PrimeDialogService } from 'primeng/dynamicdialog';
import { FormBuilder } from '@angular/forms';
import { of, Subject, throwError } from 'rxjs';
import { fakeAsync, tick } from '@angular/core/testing';
import { ReactiveFormsModule } from '@angular/forms';
import { CommonModule } from '@angular/common';
import { SystemApiService } from 'src/app/services/system.service';
import { LiveDataService } from 'src/app/services/live-data.service';
import { LoadingService } from 'src/app/services/loading.service';
import { ToastrService } from 'ngx-toastr';

describe('NetworkEditComponent', () => {
  let component: NetworkEditComponent;
  let fixture: ComponentFixture<NetworkEditComponent>;

  beforeEach(() => {
    TestBed.configureTestingModule({
      declarations: [NetworkEditComponent],
      providers: [provideHttpClient(), provideToastr(), DialogService, PrimeDialogService]
    });
    fixture = TestBed.createComponent(NetworkEditComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});

describe('Native hostname update response', () => {
  function editor(url:string) {
    const api={getInfo:jasmine.createSpy('getInfo').and.returnValue(of({hostname:'miner-new',ssid:'existing-wifi'})),
      updateSystem:jasmine.createSpy('updateSystem').and.returnValue(of({status:'success',redirect:{url,delay:2000}}))};
    const component=new NetworkEditComponent(new FormBuilder(),api as any,{info$:of({hostname:'miner-new',ssid:'existing-wifi'})} as any,
      {success:()=>{},warning:()=>{},error:()=>{}} as any,{lockUIUntilComplete:()=>((stream:any)=>stream)} as any,{} as any,{} as any);
    component.uri='http://selected-miner.local';component.ngOnInit();return {component,api};
  }
  it('reads the explicitly selected miner and offers its confirmed hostname without following the redirect automatically', () => {
    const {component,api}=editor('http://miner-new.local');component.updateSystem();
    expect(api.getInfo).toHaveBeenCalledWith('http://selected-miner.local');expect(component.savedHostnameUrl).toBe('http://miner-new.local/');
    expect(api.updateSystem.calls.mostRecent().args[0]).toBe('http://selected-miner.local');expect(api.updateSystem.calls.mostRecent().args[1].wifiPass).toBeUndefined();
  });
  it('does not offer an unrelated or credential-bearing redirect received with a settings acknowledgment', () => {
    for(const url of ['https://example.com','http://user:secret@miner-new.local','javascript:alert(1)']) {
      const {component}=editor(url);component.updateSystem();expect(component.savedHostnameUrl).toBeNull();
    }
  });
});


describe('Gamma wired Ethernet network settings', () => {
  const info = (changes: any = {}) => ({ hostname: 'gamma', ssid: 'existing-wifi', boardVersion: '601', ASICModel: 'BM1370',
    uptimeSeconds: 600, ethernetEnabled: false, ethernetDriverEnabled: false, ethernetStatus: 'disabled', ethernetIP: '', activeNetwork: 'wifi', ...changes });
  const created: NetworkEditComponent[] = [];
  function editor(initial: any = info()) {
    const api = { getInfo: jasmine.createSpy('getInfo').and.returnValue(of(initial)),
      updateSystem: jasmine.createSpy('updateSystem').and.returnValue(of({ status: 'success' })),
      restart: jasmine.createSpy('restart').and.returnValue(of({ status: 'success' })) };
    const toast = { success: jasmine.createSpy('success'), warning: jasmine.createSpy('warning'), error: jasmine.createSpy('error') };
    const component = new NetworkEditComponent(new FormBuilder(), api as any, { info$: of(initial) } as any,
      toast as any, { lockUIUntilComplete: () => ((stream: any) => stream) } as any, {} as any, {} as any);
    component.ngOnInit(); created.push(component); return { component, api, toast };
  }
  function select(component: NetworkEditComponent, enabled: any): void {
    component.form.get('ethernetEnabled')!.setValue(enabled); component.form.get('ethernetEnabled')!.markAsDirty();
  }
  afterEach(() => created.splice(0).forEach(component => component.ngOnDestroy()));

  it('starts disabled, preserves Wi-Fi, and never writes merely by changing its checkbox', () => {
    const { component, api } = editor();
    expect(component.form.get('ethernetEnabled')!.value).toBeFalse(); expect(component.ethernetStatusLabel).toBe('Disabled');
    expect(component.activeNetworkLabel).toBe('Wi-Fi'); expect(component.ethernetRestartPending).toBeFalse();
    select(component, true); expect(api.updateSystem).not.toHaveBeenCalled(); expect(api.restart).not.toHaveBeenCalled();
    expect(component.form.get('ssid')!.value).toBe('existing-wifi'); expect(component.form.get('wifiPass')!.value).toBe('*****');
  });
  it('saves only the explicit Ethernet boolean and leaves the running network alone until restart', () => {
    const { component, api } = editor(info({ ssid: ' retained Wi-Fi ' })); select(component, true); component.updateSystem();
    expect(api.updateSystem).toHaveBeenCalledOnceWith('', { ethernetEnabled: true }); expect(api.restart).not.toHaveBeenCalled();
    expect(component.ethernet?.enabled).toBeTrue(); expect(component.ethernet?.driverEnabled).toBeFalse();
    expect(component.ethernetRestartPending).toBeTrue(); expect(component.savedChanges).toBeTrue();
    expect(component.form.pristine).toBeTrue(); expect(component.form.get('ssid')!.value).toBe(' retained Wi-Fi ');
  });
  it('allows an Ethernet-only opt-in with empty saved Wi-Fi and hostname fields without sending either field', () => {
    const { component, api } = editor(info({ ssid: '', hostname: '' }));
    expect(component.form.invalid).toBeTrue(); select(component, true); expect(component.networkSettingsValid).toBeTrue();
    component.updateSystem(); expect(api.updateSystem).toHaveBeenCalledOnceWith('', { ethernetEnabled: true });
    expect(component.form.get('ssid')!.value).toBe(''); expect(component.form.get('hostname')!.value).toBe(''); expect(api.restart).not.toHaveBeenCalled();
  });
  it('retains full network validation when Wi-Fi or hostname edits accompany the Ethernet choice', () => {
    for (const key of ['ssid', 'hostname']) {
      const { component, api } = editor(); select(component, true);
      component.form.get(key)!.setValue(''); component.form.get(key)!.markAsDirty();
      expect(component.networkSettingsValid).toBeFalse(); component.updateSystem(); expect(api.updateSystem).not.toHaveBeenCalled();
    }
  });
  it('round-trips an explicit false rather than converting it to a missing or true value', () => {
    const { component, api } = editor(info({ ethernetEnabled: true, ethernetDriverEnabled: true, ethernetStatus: 'connected', ethernetIP: '192.0.2.44', activeNetwork: 'ethernet' }));
    select(component, false); component.updateSystem(); expect(api.updateSystem).toHaveBeenCalledOnceWith('', { ethernetEnabled: false });
    expect(component.ethernet?.enabled).toBeFalse(); expect(component.ethernet?.driverEnabled).toBeTrue(); expect(component.savedChanges).toBeTrue();
  });
  it('keeps unsaved state and the prior saved/running state after an HTTP failure or malformed acknowledgment', () => {
    for (const response of [throwError(() => ({ message: 'synthetic rejection' })), of({ status: 'error' }), of(undefined)]) {
      const { component, api, toast } = editor(); api.updateSystem.and.returnValue(response as any); select(component, true); component.updateSystem();
      expect(component.form.dirty).toBeTrue(); expect(component.ethernet?.enabled).toBeFalse(); expect(component.ethernet?.driverEnabled).toBeFalse();
      expect(component.savedChanges).toBeFalse(); expect(component.busy).toBeFalse(); expect(api.restart).not.toHaveBeenCalled(); expect(toast.success).not.toHaveBeenCalled();
    }
  });
  it('recovers a saved pending enable or disable after reloading the Network page', () => {
    for (const [saved, running] of [[true, false], [false, true]]) {
      const { component, api } = editor(info({ ethernetEnabled: saved, ethernetDriverEnabled: running, ethernetStatus: running ? 'not-detected' : 'disabled' }));
      expect(component.form.get('ethernetEnabled')!.value).toBe(saved); expect(component.ethernetRestartPending).toBeTrue(); expect(component.savedChanges).toBeTrue();
      expect(component.form.pristine).toBeTrue(); expect(api.updateSystem).not.toHaveBeenCalled();
    }
  });
  it('can cancel an unbooted enable without claiming a restart is still required', () => {
    const { component, api, toast } = editor(info({ ethernetEnabled: true, ethernetDriverEnabled: false }));
    select(component, false); component.updateSystem(); expect(api.updateSystem).toHaveBeenCalledOnceWith('', { ethernetEnabled: false });
    expect(component.savedChanges).toBeFalse(); expect(component.ethernetRestartPending).toBeFalse(); expect(toast.warning).not.toHaveBeenCalled();
    expect(api.restart).not.toHaveBeenCalled();
  });
  it('updates live Ethernet status without replacing an unsaved checkbox or Wi-Fi draft', fakeAsync(() => {
    const { component, api } = editor(); select(component, true); component.form.patchValue({ ssid: 'unsaved-wifi' }); component.form.get('ssid')!.markAsDirty();
    api.getInfo.and.returnValue(of(info({ ethernetStatus: 'link-down' }))); tick(5000);
    expect(api.getInfo).toHaveBeenCalledOnceWith(''); expect(component.ethernetStatusLabel).toBe('Cable disconnected');
    expect(component.form.get('ethernetEnabled')!.value).toBeTrue(); expect(component.form.get('ssid')!.value).toBe('unsaved-wifi'); expect(component.form.dirty).toBeTrue();
    component.ngOnDestroy(); tick(10000); expect(api.getInfo).toHaveBeenCalledTimes(1);
  }));
  it('shows a missing adapter as a boot-enabled failure, not an unsaved configuration change', () => {
    const { component } = editor(info({ ethernetEnabled: true, ethernetDriverEnabled: true, ethernetStatus: 'not-detected' }));
    expect(component.ethernetStatusLabel).toBe('Adapter not detected'); expect(component.ethernetRestartPending).toBeFalse(); expect(component.savedChanges).toBeFalse();
  });
  it('shows the address and actual active network only from the report', () => {
    const { component, api } = editor(info({ ethernetEnabled: true, ethernetDriverEnabled: true, ethernetStatus: 'connected', ethernetIP: '192.0.2.44', activeNetwork: 'ethernet' }));
    expect(component.ethernetStatusLabel).toBe('Connected'); expect(component.ethernet?.ip).toBe('192.0.2.44'); expect(component.activeNetworkLabel).toBe('Ethernet');
    api.getInfo.and.returnValue(of(info({ ethernetEnabled: true, ethernetDriverEnabled: true, ethernetStatus: 'link-down', activeNetwork: 'wifi' }))); component.refreshEthernet();
    expect(component.activeNetworkLabel).toBe('Wi-Fi'); expect(component.ethernet?.ip).toBe('');
  });
  it('keeps the feature absent for older, incomplete or non-Gamma firmware reports', () => {
    const old: any = info(); delete old.ethernetEnabled;
    for (const report of [old, info({ ethernetEnabled: 0 }), info({ ethernetDriverEnabled: 0 }), info({ ethernetStatus: 'invented' }),
      info({ activeNetwork: 'both' }), info({ ethernetIP: 'javascript:alert(1)' }), info({ ethernetIP: '192.0.2.999' }),
      info({ boardVersion: '603' }), info({ boardVersion: '601', ASICModel: 'BM1368' }), info({ deviceModel: 'NerdQAxe++' }), info({ asicCount: 4 })]) {
      const { component, api } = editor(report); expect(component.ethernet).toBeNull(); expect(component.form.contains('ethernetEnabled')).toBeFalse();
      component.form.get('hostname')!.setValue('new-hostname'); component.form.get('hostname')!.markAsDirty(); component.updateSystem();
      expect(api.updateSystem.calls.mostRecent().args[1].ethernetEnabled).toBeUndefined();
    }
  });
  it('refuses forged non-boolean checkbox values and an Ethernet save while status is unavailable', () => {
    for (const value of [0, 1, 'false', null]) {
      const { component, api } = editor(); select(component, value); component.updateSystem(); expect(api.updateSystem).not.toHaveBeenCalled();
    }
    const { component, api } = editor(); api.getInfo.and.returnValue(throwError(() => Error('offline'))); component.refreshEthernet();
    select(component, true); component.updateSystem(); expect(component.ethernetStatusLabel).toBe('Ethernet status unavailable'); expect(api.updateSystem).not.toHaveBeenCalled();
    api.getInfo.and.returnValue(of(info())); component.refreshEthernet(); component.updateSystem(); expect(api.updateSystem).toHaveBeenCalledOnceWith('', { ethernetEnabled: true });
  });
  it('requires explicit restart and waits for observed boot state after its acknowledgment', () => {
    const { component, api } = editor(); select(component, true); component.updateSystem(); component.restart();
    expect(api.restart).toHaveBeenCalledOnceWith(''); expect(component.restartRequested).toBeTrue(); expect(component.savedChanges).toBeTrue();
    component.restart(); expect(api.restart).toHaveBeenCalledTimes(1);
    api.getInfo.and.returnValue(of(info({ ethernetEnabled: true, ethernetDriverEnabled: false }))); component.refreshEthernet(); expect(component.restartRequested).toBeTrue();
    api.getInfo.and.returnValue(of(info({ ethernetEnabled: true, ethernetDriverEnabled: true, ethernetStatus: 'not-detected', uptimeSeconds: 5 })));
    component.refreshEthernet(); expect(component.restartRequested).toBeFalse(); expect(component.savedChanges).toBeFalse(); expect(component.ethernetStatusLabel).toBe('Adapter not detected');
  });
  it('ignores a status read started before saving and preserves edits made while its acknowledgment is pending', () => {
    const { component, api } = editor(); const oldRead = new Subject<any>(), pendingSave = new Subject<any>();
    api.getInfo.and.returnValue(oldRead); component.refreshEthernet(); select(component, true); api.updateSystem.and.returnValue(pendingSave); component.updateSystem();
    component.form.get('ssid')!.setValue('typed-while-saving'); component.form.get('ssid')!.markAsDirty();
    pendingSave.next({ status: 'success' }); pendingSave.complete(); oldRead.next(info()); oldRead.complete();
    expect(component.ethernet?.enabled).toBeTrue(); expect(component.savedChanges).toBeTrue();
    expect(component.form.get('ssid')!.value).toBe('typed-while-saving'); expect(component.form.get('ssid')!.dirty).toBeTrue();
  });
  it('retains the unsaved checkbox and blocks its save when a later optional report is malformed', () => {
    const { component, api } = editor(); select(component, true); api.getInfo.and.returnValue(of(info({ ethernetEnabled: 1 })));
    component.refreshEthernet(); expect(component.form.get('ethernetEnabled')!.value).toBeTrue(); expect(component.ethernetReadError).toBeTrue();
    component.updateSystem(); expect(api.updateSystem).not.toHaveBeenCalled();
  });
  it('retains pending changes when restart fails and prevents overlapping saves or status polls', () => {
    const { component, api } = editor(); const pending = new Subject<any>(); api.updateSystem.and.returnValue(pending); select(component, true);
    component.updateSystem(); component.updateSystem(); component.refreshEthernet(); expect(api.updateSystem).toHaveBeenCalledTimes(1); expect(api.getInfo).not.toHaveBeenCalled();
    pending.next({ status: 'success' }); pending.complete(); api.restart.and.returnValue(throwError(() => ({ message: 'synthetic rejection' })));
    component.restart(); expect(component.savedChanges).toBeTrue(); expect(component.restartRequested).toBeFalse(); expect(component.busy).toBeFalse();
  });
});

describe('Wired Ethernet Network template', () => {
  function render(extra: any = {}) {
    const info = { hostname: 'gamma', ssid: 'saved-wifi', boardVersion: '602', ASICModel: 'BM1370', uptimeSeconds: 900,
      ethernetEnabled: false, ethernetDriverEnabled: false, ethernetStatus: 'disabled', ethernetIP: '', activeNetwork: 'wifi', ...extra };
    const api = { getInfo: jasmine.createSpy('getInfo').and.returnValue(of(info)), updateSystem: jasmine.createSpy('updateSystem').and.returnValue(of({ status: 'success' })), restart: jasmine.createSpy('restart').and.returnValue(of({ status: 'success' })) };
    TestBed.configureTestingModule({ declarations: [NetworkEditComponent], imports: [CommonModule, ReactiveFormsModule],
      providers: [provideHttpClient(), { provide: SystemApiService, useValue: api }, { provide: LiveDataService, useValue: { info$: of(info) } },
        { provide: LoadingService, useValue: { lockUIUntilComplete: () => ((stream: any) => stream) } },
        { provide: ToastrService, useValue: { success: () => {}, warning: () => {}, error: () => {} } }, { provide: DialogService, useValue: {} }] });
    const fixture = TestBed.createComponent(NetworkEditComponent); fixture.detectChanges(); return { fixture, api };
  }
  it('renders an off-by-default labelled checkbox, preserves Wi-Fi and shows saved-versus-running after Save', () => {
    const { fixture, api } = render(); const element: HTMLElement = fixture.nativeElement;
    const checkbox = element.querySelector<HTMLInputElement>('#ethernetEnabled')!;
    expect(checkbox.checked).toBeFalse(); expect(element.querySelector('label[for="ethernetEnabled"]')!.textContent).toContain('Enable wired Ethernet (W5500)');
    expect(element.querySelector('#networkEthernetHeading')!.textContent).toContain('Experimental');
    expect(element.querySelector('#ethernetHint')!.textContent).toContain('Testing with a physical adapter is still pending.');
    expect(element.textContent).toContain('Wi-Fi remains available as fallback.'); expect(element.querySelector<HTMLInputElement>('#ssid')!.value).toBe('saved-wifi');
    checkbox.checked = true; checkbox.dispatchEvent(new Event('change')); fixture.detectChanges(); expect(api.updateSystem).not.toHaveBeenCalled();
    const save = Array.from(element.querySelectorAll('button')).find(button => button.textContent?.trim() === 'Save network settings')!; save.click(); fixture.detectChanges();
    expect(api.updateSystem).toHaveBeenCalledOnceWith('', { ethernetEnabled: true }); expect(element.textContent).toContain('Saved: On · At startup: Off'); expect(element.textContent).toContain('Restart required'); expect(api.restart).not.toHaveBeenCalled();
  });
  it('does not display an Ethernet enable control when an older API lacks its optional contract', () => {
    const { fixture } = render({ ethernetEnabled: undefined }); const element: HTMLElement = fixture.nativeElement;
    expect(element.querySelector('#ethernetEnabled')).toBeNull(); expect(element.querySelector('#ssid')).not.toBeNull();
  });
  it('enables Save for a wired-only choice with an empty saved SSID and keeps mixed invalid Wi-Fi edits blocked', () => {
    const { fixture, api } = render({ ssid: '' }); const element: HTMLElement = fixture.nativeElement;
    const save = Array.from(element.querySelectorAll('button')).find(button => button.textContent?.trim() === 'Save network settings')!;
    const checkbox = element.querySelector<HTMLInputElement>('#ethernetEnabled')!;
    expect(save.disabled).toBeTrue(); checkbox.checked = true; checkbox.dispatchEvent(new Event('change')); fixture.detectChanges();
    expect(save.disabled).toBeFalse();
    const ssid = element.querySelector<HTMLInputElement>('#ssid')!; ssid.dispatchEvent(new Event('input')); fixture.detectChanges();
    expect(save.disabled).toBeTrue(); save.click(); expect(api.updateSystem).not.toHaveBeenCalled();
    fixture.componentInstance.form.get('ssid')!.markAsPristine(); fixture.detectChanges(); expect(save.disabled).toBeFalse();
    save.click(); fixture.detectChanges(); expect(api.updateSystem).toHaveBeenCalledOnceWith('', { ethernetEnabled: true });
  });
});
