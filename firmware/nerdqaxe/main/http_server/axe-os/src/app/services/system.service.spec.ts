import { TestBed } from '@angular/core/testing';
import { provideHttpClient } from '@angular/common/http';
import { provideHttpClientTesting, HttpTestingController } from '@angular/common/http/testing';

import { SystemService } from './system.service';

describe('SystemService', () => {
  let service: SystemService;
  let httpMock: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [
        provideHttpClient(),
        provideHttpClientTesting(),
      ],
    });
    service = TestBed.inject(SystemService);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => {
    httpMock.verify();
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });

  it('reads and saves tuning schedules for the selected miner with its authentication header', () => {
    const schedule = {schemaVersion:1 as const,enabled:true,utcOffsetMinutes:0,events:[{enabled:true,dayMask:127,timeMinutes:540,slot:0}]};
    service.getTuningSchedule('/selected-miner').subscribe();
    const read = httpMock.expectOne('/selected-miner/api/5tratum/tuning-schedule');
    expect(read.request.method).toBe('GET');read.flush(schedule);
    service.saveTuningSchedule('/selected-miner',schedule,'123456').subscribe();
    const write = httpMock.expectOne('/selected-miner/api/5tratum/tuning-schedule');
    expect(write.request.method).toBe('POST');expect(write.request.headers.get('X-TOTP')).toBe('123456');expect(write.request.body).toEqual(schedule);
    write.flush({ok:true});httpMock.expectNone('/api/v2/settings');httpMock.expectNone('/api/5tratum/profiles/apply');
  });

  it('reads display runtime metadata from the same selected miner without a settings write', () => {
    let result: any;
    service.getSystemV2('/selected-miner').subscribe(info => result = info);
    const req = httpMock.expectOne('/selected-miner/api/v2/system');
    expect(req.request.method).toBe('GET');
    req.flush({deviceModel:'NerdQAxe++',version:'5tratumFW-test',displayActiveProfile:'320x170',
      displayProfileOptions:['320x170','480x320-rgb','480x320-bgr'],displayRestartRequired:false});
    expect(result.displayActiveProfile).toBe('320x170');
    httpMock.expectNone('/api/v2/system');
    httpMock.expectNone('/selected-miner/api/v2/settings');
  });

  it('reads the update identity through the v1 endpoint shared by old and new applications', () => {
    let result: any;
    service.getUpdateInfo().subscribe(info => result = info);
    const req = httpMock.expectOne('/api/system/info');
    expect(req.request.method).toBe('GET');
    req.flush({ deviceModel: 'NerdQAxe++', version: 'v1.0.37.3-LTS', ASICModel: 'BM1370',
      asicCount: 4, otp: true, stratumUser: 'private-fixture' });
    expect(result).toEqual({ deviceModel: 'NerdQAxe++', version: 'v1.0.37.3-LTS' });
    httpMock.expectNone('/api/v2/settings');
  });

  it('rejects missing update identity instead of inferring a model from the selected image', () => {
    let failure: any;
    service.getUpdateInfo().subscribe({ error: error => failure = error });
    httpMock.expectOne('/api/system/info').flush({ deviceModel: '', version: 'v1.0.37.3-LTS' });
    expect(failure.message).toContain('identity is unavailable');
  });

  for (const otp of [true, false]) {
    it(`preserves legacy OTP=${otp} when the v2 identify endpoint is absent`, () => {
      let result: any;
      service.getIdentifyV2().subscribe(info => result = info);
      httpMock.expectOne('/api/v2/identify').flush('Not found', { status: 404, statusText: 'Not Found' });
      httpMock.expectOne('/api/system/info').flush({ deviceModel: 'NerdQAxe++', otp, defaultTheme: 'NerdQAxe++' });
      expect(result.deviceModel).toBe('NerdQAxe++');
      expect(result.otp).toBe(otp);
    });
  }

  it('blocks legacy updates when the authentication flag cannot be determined', () => {
    let failure: any;
    service.getIdentifyV2().subscribe({ error: error => failure = error });
    httpMock.expectOne('/api/v2/identify').flush('Not found', { status: 404, statusText: 'Not Found' });
    httpMock.expectOne('/api/system/info').flush({ deviceModel: 'NerdQAxe++' });
    expect(failure.message).toContain('authentication status is unavailable');
  });

  it('does not bypass an authentication error with a legacy request', () => {
    let failure: any;
    service.getIdentifyV2().subscribe({ error: error => failure = error });
    httpMock.expectOne('/api/v2/identify').flush('Unauthorized', { status: 401, statusText: 'Unauthorized' });
    expect(failure.status).toBe(401);
    httpMock.expectNone('/api/system/info');
  });

  it('sendAlertTest sends one-shot OTP using the backend accepted X-TOTP header', () => {
    service.sendAlertTest('', '123456').subscribe();

    const req = httpMock.expectOne('/api/v2/alert/test');
    expect(req.request.method).toBe('POST');
    expect(req.request.headers.get('X-TOTP')).toBe('123456');
    expect(req.request.headers.has('X-OTP-Code')).toBeFalse();
    req.flush('ok');
  });

  it('downloads a core dump as a blob with a one-shot OTP', () => {
    let elfSha256 = '';
    service.downloadCoreDump('123456').subscribe((response) => {
      elfSha256 = response.headers.get('X-ESP-App-ELF-SHA256') ?? '';
    });

    const req = httpMock.expectOne('/api/system/coredump');
    expect(req.request.method).toBe('GET');
    expect(req.request.responseType).toBe('blob');
    expect(req.request.headers.get('X-TOTP')).toBe('123456');
    req.flush(new Blob(), { headers: { 'X-ESP-App-ELF-SHA256': '0123456789abcdef' } });
    expect(elfSha256).toBe('0123456789abcdef');
  });
  it('keeps ordinary session resets separate from authenticated persisted-stat resets', () => {
    service.resetStats().subscribe();const session=httpMock.expectOne('/api/system/reset-stats');
    expect(session.request.method).toBe('POST');expect(session.request.headers.has('X-TOTP')).toBeFalse();session.flush('ok');
    service.resetStats('/selected-miner',true,'123456').subscribe();
    const allTime=httpMock.expectOne('/selected-miner/api/system/reset-stats?allTime=1');
    expect(allTime.request.headers.get('X-TOTP')).toBe('123456');allTime.flush('ok');
  });
});
