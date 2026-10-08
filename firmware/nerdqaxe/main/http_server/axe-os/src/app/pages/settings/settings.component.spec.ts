import { HttpEventType } from '@angular/common/http';
import { convertToParamMap } from '@angular/router';
import { BehaviorSubject, of, Subject, throwError } from 'rxjs';
import { SettingsComponent } from './settings.component';

describe('5tratumFW settings navigation and OTA', () => {
  let component: SettingsComponent;
  let params: BehaviorSubject<any>;
  let routeData: BehaviorSubject<any>;
  let navigate: jasmine.Spy;
  let appUpload: jasmine.Spy;
  let webUpload: jasmine.Spy;
  let auth: jasmine.Spy;
  let identity: jasmine.Spy;
  let checkRelease: jasmine.Spy;

  beforeEach(() => {
    params = new BehaviorSubject(convertToParamMap({}));
    routeData = new BehaviorSubject({ section: 'controls' });
    navigate = jasmine.createSpy('navigate');
    appUpload = jasmine.createSpy('performOTAUpdate').and.returnValue(of({ type: HttpEventType.Response }));
    webUpload = jasmine.createSpy('performWWWOTAUpdate').and.returnValue(of({ type: HttpEventType.UploadProgress, loaded: 1, total: 2 }));
    auth = jasmine.createSpy('ensureOtp$').and.returnValue(of({ totp: '123456' }));
    identity = jasmine.createSpy('getUpdateInfo').and.returnValue(of({ deviceModel: 'NerdQAxe++', version: '5tratumFW-test' }));
    checkRelease = jasmine.createSpy('check').and.returnValue(of({ status: 'no-release', release: null }));
    component = new SettingsComponent({
      getUpdateInfo: identity,
      performOTAUpdate: appUpload, performWWWOTAUpdate: webUpload,
    } as any, { success: () => {}, danger: () => {} } as any,
    { lockUIUntilComplete: () => (stream: any) => stream } as any,
    { ensureOtp$: auth } as any, { data: routeData, queryParamMap: params } as any,
    { navigate } as any, { check: checkRelease } as any);
    component.ngOnInit();
  });

  afterEach(() => component.ngOnDestroy());

  it('opens primary pages from route data and control subsections from query parameters', () => {
    expect(component.page).toBe('controls');
    expect(component.section).toBe('performance');
    params.next(convertToParamMap({ section: 'cooling' }));
    expect(component.section).toBe('cooling');
    params.next(convertToParamMap({ section: 'unknown' }));
    expect(component.section).toBe('performance');
    params.next(convertToParamMap({}));
    for (const page of ['pool', 'scheduler', 'network', 'update']) {
      routeData.next({ section: page });
      expect(component.page).toBe(page);
      expect(component.section).toBe(page);
    }
    expect(navigate).not.toHaveBeenCalled();
  });

  it('redirects legacy pool, scheduler, network and update links to their primary pages', () => {
    for (const page of ['pool', 'scheduler', 'network', 'update']) {
      params.next(convertToParamMap({ section: page }));
      expect(navigate.calls.mostRecent().args).toEqual([['/pages', page], { replaceUrl: true }]);
    }
    expect(component.page).toBe('controls');
  });

  it('selects sections using query parameters instead of a duplicate MUX page', () => {
    component.selectSection('cooling');
    expect(navigate).toHaveBeenCalledWith([], jasmine.objectContaining({ queryParams: { section: 'cooling' }, queryParamsHandling: 'merge' }));
    expect(component.sections.some(section => section.id === 'mux')).toBeFalse();
  });

  it('rejects firmware filenames for another model before authorization or upload', () => {
    component.selectedFirmwareFile = new File(['fixture'], 'esp-miner-Gamma.bin');
    component.uploadFirmwareFile();
    expect(component.firmwareFileValid).toBeFalse();
    expect(auth).not.toHaveBeenCalled();
    expect(appUpload).not.toHaveBeenCalled();
  });

  it('holds a selected image until the device identity has arrived', () => {
    const delayed = new Subject<any>();
    component.info$ = delayed;
    component.loadDeviceIdentity();
    component.selectedFirmwareFile = new File(['fixture'], 'esp-miner-NerdQAxe++.bin');
    expect(component.identityLoading).toBeTrue();
    expect(component.expectedFileName).toBe('');
    expect(component.identityError).toBe('');
    component.uploadFirmwareFile();
    expect(auth).not.toHaveBeenCalled();
    expect(appUpload).not.toHaveBeenCalled();
    delayed.next({ deviceModel: 'NerdQAxe++', version: 'v1.0.37.3-LTS' });
    delayed.complete();
    expect(component.identityLoading).toBeFalse();
    expect(component.expectedFileName).toBe('esp-miner-NerdQAxe++.bin');
    expect(component.firmwareFileValid).toBeTrue();
  });

  it('reports an identity failure separately and accepts a successful retry', () => {
    component.info$ = throwError(() => new Error('HTTP request failed'));
    component.loadDeviceIdentity();
    component.selectedFirmwareFile = new File(['fixture'], 'esp-miner-NerdQAxe++.bin');
    expect(component.identityLoading).toBeFalse();
    expect(component.identityError).toContain('Could not read the device model');
    expect(component.expectedFileName).toBe('');
    component.uploadFirmwareFile();
    expect(auth).not.toHaveBeenCalled();
    expect(appUpload).not.toHaveBeenCalled();
    component.info$ = of({ deviceModel: 'NerdQAxe++', version: 'v1.0.37.3-LTS' });
    component.loadDeviceIdentity();
    expect(component.identityError).toBe('');
    expect(component.firmwareFileValid).toBeTrue();
  });

  it('uses the existing app OTA handler and OTP for a matching Nerd model filename', () => {
    const file = new File(['fixture'], 'esp-miner-NerdQAxe++.bin');
    component.selectedFirmwareFile = file;
    component.uploadFirmwareFile();
    expect(auth).toHaveBeenCalledTimes(1);
    expect(appUpload).toHaveBeenCalledWith(file, '123456');
    expect(component.firmwareRestartPending).toBeTrue();
    expect(component.firmwareUpdateProgress).toBe(100);
    expect(webUpload).not.toHaveBeenCalled();
  });

  it('checks public releases only on request and never authorizes or uploads an image', () => {
    expect(checkRelease).not.toHaveBeenCalled();
    component.checkUpdates();
    expect(checkRelease).toHaveBeenCalledOnceWith('NerdQAxe++', '5tratumFW-test');
    expect(component.releaseCheckState).toBe('no-release');
    expect(component.releaseCheckMessage).toContain('No complete QAxe');
    expect(auth).not.toHaveBeenCalled();
    expect(appUpload).not.toHaveBeenCalled();
    expect(webUpload).not.toHaveBeenCalled();
  });

  it('shows checking, error and retry states while clearing outdated download links', () => {
    const delayed = new Subject<any>();
    checkRelease.and.returnValue(delayed);
    component.checkUpdates();
    expect(component.releaseCheckState).toBe('checking');
    component.checkUpdates();
    expect(checkRelease).toHaveBeenCalledTimes(1);
    delayed.error(new Error('unavailable'));
    expect(component.releaseCheckState).toBe('error');
    expect(component.availableRelease).toBeNull();
    checkRelease.and.returnValue(of({ status: 'up-to-date', release: null }));
    component.checkUpdates();
    expect(component.releaseCheckState).toBe('up-to-date');
    expect(component.releaseCheckMessage).toContain('up to date');
  });

  it('holds release checking until identity arrives and cancels it when identity is reloaded', () => {
    component.identityLoading = true;
    component.checkUpdates();
    expect(checkRelease).not.toHaveBeenCalled();
    component.identityLoading = false;
    const delayed = new Subject<any>();
    checkRelease.and.returnValue(delayed);
    component.checkUpdates();
    component.loadDeviceIdentity();
    delayed.next({ status: 'available', release: { version: 'stale-version' } });
    expect(component.releaseCheckState).toBe('idle');
    expect(component.availableRelease).toBeNull();
  });

  it('uses the exact Oct reported model and prevents a QAxe application from passing its file guard', () => {
    component.info$ = of({ deviceModel: 'NerdOCTAXE-γ', version: '5tratumFW-oct-0.1.0-beta.1' });
    component.loadDeviceIdentity();
    expect(component.expectedFileName).toBe('esp-miner-NerdOCTAXE-Gamma.bin');
    expect(component.releaseModelLabel).toBe('OctAxe Gamma');
    component.selectedFirmwareFile = new File(['fixture'], 'esp-miner-NerdQAxe++.bin');
    component.uploadFirmwareFile();
    expect(auth).not.toHaveBeenCalled();
    expect(appUpload).not.toHaveBeenCalled();
    component.selectedFirmwareFile = new File(['fixture'], 'esp-miner-NerdOCTAXE-Gamma.bin');
    expect(component.firmwareFileValid).toBeTrue();
    component.checkUpdates();
    expect(checkRelease).toHaveBeenCalledOnceWith('NerdOCTAXE-γ', '5tratumFW-oct-0.1.0-beta.1');
  });

  it('does not derive an upload filename for an unsupported model', () => {
    component.info$ = of({ deviceModel: 'NerdOCTAXE+', version: 'stock' });
    component.loadDeviceIdentity();
    expect(component.expectedFileName).toBe('');
    component.selectedFirmwareFile = new File(['fixture'], 'esp-miner-NerdOCTAXE+.bin');
    component.uploadFirmwareFile();
    expect(auth).not.toHaveBeenCalled();
    expect(appUpload).not.toHaveBeenCalled();
  });

  it('uses the separate WWW OTA handler and rejects non-www filenames', () => {
    component.selectedWebsiteFile = new File(['fixture'], 'esp-miner-NerdQAxe++.bin');
    component.uploadWebsiteFile();
    expect(webUpload).not.toHaveBeenCalled();
    const file = new File(['fixture'], 'www.bin');
    component.selectedWebsiteFile = file;
    component.uploadWebsiteFile();
    expect(webUpload).toHaveBeenCalledWith(file, '123456');
    expect(component.websiteUpdateProgress).toBe(50);
    expect(appUpload).not.toHaveBeenCalled();
  });
});
