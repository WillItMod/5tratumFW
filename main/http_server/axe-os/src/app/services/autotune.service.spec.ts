import { TestBed } from '@angular/core/testing';
import { provideHttpClient } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { of } from 'rxjs';
import { AutotuneService } from './autotune.service';


describe('Auto-tuning transport', () => {
  let service: AutotuneService;
  let http: HttpTestingController;

  beforeEach(() => {

    TestBed.configureTestingModule({providers:[provideHttpClient(),provideHttpClientTesting(),AutotuneService ]});
    service=TestBed.inject(AutotuneService);http=TestBed.inject(HttpTestingController);
  });
  afterEach(() => http.verify());
  it('uses only GET when reading a tuning report', () => {
    service.get().subscribe();const request=http.expectOne('/api/5tratum/autotune');
    expect(request.request.method).toBe('GET');request.flush({schemaVersion:1});
    http.expectNone(req=>req.method==='POST');
  });
  it('sends one explicit command with its waiver and optional one-shot OTP', () => {
    const body={action:'start',waiverAccepted:true,waiverVersion:1};service.command(body,'123456').subscribe();
    const request=http.expectOne('/api/5tratum/autotune');expect(request.request.method).toBe('POST');
    expect(request.request.body).toEqual(body);expect(request.request.headers.get('X-TOTP')).toBe('123456');
    request.flush({ok:true,restartRequired:false});http.expectNone('/api/5tratum/autotune');
  });
  it('keeps restore requests separate from configuration and does not attach an empty OTP', () => {
    service.command({action:'stop'}).subscribe();const request=http.expectOne('/api/5tratum/autotune');
    expect(request.request.body).toEqual({action:'stop'});expect(request.request.headers.has('X-TOTP')).toBeFalse();request.flush({ok:true,restartRequired:false});
  });
  it('preserves Gamma local-network authorization without inventing an OTP request', () => { service.authorize().subscribe(result=>expect(result).toEqual({}));http.expectNone('/api/5tratum/autotune'); });
});
