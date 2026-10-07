import { HttpEventType } from '@angular/common/http';
import { convertToParamMap } from '@angular/router';
import { BehaviorSubject, of } from 'rxjs';
import { SettingsComponent } from './settings.component';

describe('5tratumFW settings navigation and OTA', () => {
  let component: SettingsComponent;
  let params: BehaviorSubject<any>;
  let routeData: BehaviorSubject<any>;
  let navigate: jasmine.Spy;
  let appUpload: jasmine.Spy;
  let webUpload: jasmine.Spy;
  let auth: jasmine.Spy;

  beforeEach(() => {
    params = new BehaviorSubject(convertToParamMap({}));
    routeData = new BehaviorSubject({ section: 'controls' });
    navigate = jasmine.createSpy('navigate');
    appUpload = jasmine.createSpy('performOTAUpdate').and.returnValue(of({ type: HttpEventType.Response }));
    webUpload = jasmine.createSpy('performWWWOTAUpdate').and.returnValue(of({ type: HttpEventType.UploadProgress, loaded: 1, total: 2 }));
    auth = jasmine.createSpy('ensureOtp$').and.returnValue(of({ totp: '123456' }));
    component = new SettingsComponent({
      getSettingsV2: () => of({ deviceModel: 'NerdQAxe++', version: '5tratumFW-test' }),
      performOTAUpdate: appUpload, performWWWOTAUpdate: webUpload,
    } as any, { success: () => {}, danger: () => {} } as any,
    { lockUIUntilComplete: () => (stream: any) => stream } as any,
    { ensureOtp$: auth } as any, { data: routeData, queryParamMap: params } as any,
    { navigate } as any);
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
