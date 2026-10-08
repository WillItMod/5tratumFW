import { fakeAsync, TestBed, tick } from '@angular/core/testing';

import { SystemApiService } from './system.service';
import { HttpClient, provideHttpClient } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { environment } from '../../environments/environment';
import { Api } from '../generated/api';
import { FiveTratumStatusSnapshot, SystemInfo } from '../generated/models';
import * as functions from '../generated/functions';

function statusFixture(): FiveTratumStatusSnapshot {
  return {
    schemaVersion: 1,
    identity: { deviceId: '5tfw:0102030405060708090a0b0c0d0e0f10' },
    hardware: { boardModel: 'Gamma 601', asicModel: 'BM1370', asicCount: 1 },
    firmware: { product: '5tratumFW', version: '5tratumFW-test' },
    observedUptimeSeconds: 90,
    work: { scope: 'chain-broadcast', independentAssignment: false, poolMode: 'failover', activePool: 1 },
    asics: [], coin: null, workContext: null,
    mux: {
      independentWorkAssignment: false,
      pools: [
        { index: 0, connected: false, transportConnected: false, expired: false, serverId: null,
          ttlSeconds: 90, statusAgeSeconds: null, coin: null, workContext: null },
        { index: 1, connected: true, transportConnected: true, expired: false, serverId: null,
          ttlSeconds: 90, statusAgeSeconds: 17, coin: null, workContext: null },
      ],
    },
  };
}

describe('SystemApiService', () => {
  let service: SystemApiService;
  let http: HttpTestingController;
  let previousProduction: boolean;

  beforeEach(() => {
    previousProduction = environment.production;
    environment.production = false;
    TestBed.configureTestingModule({
      providers: [provideHttpClient(), provideHttpClientTesting()]
    });
    service = TestBed.inject(SystemApiService);
    http = TestBed.inject(HttpTestingController);
  });

  afterEach(() => {
    environment.production = previousProduction;
    http.verify();
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });

  it('keeps development status unknown, detached and consistent with info', fakeAsync(() => {
    let first!: FiveTratumStatusSnapshot;
    let second!: FiveTratumStatusSnapshot;
    let info!: SystemInfo;
    service.getFiveTratumStatus().subscribe(value => first = value);
    service.getInfo().subscribe(value => info = value);
    tick(1000);
    expect(first.firmware.version).toBe(info.version);
    expect(first.hardware.boardModel).toBe(`Gamma ${info.boardVersion}`);
    expect(first.hardware.asicModel).toBe(info.ASICModel);
    expect(first.asics).toEqual([]);
    expect(first.coin).toBeNull();
    expect(first.workContext).toBeNull();
    expect(first.work.independentAssignment).toBeFalse();
    expect(first.mux.independentWorkAssignment).toBeFalse();
    expect(first.mux.pools.every(pool => !pool.connected && !pool.transportConnected && pool.statusAgeSeconds === null)).toBeTrue();
    first.mux.pools[0].connected = true;
    service.getFiveTratumStatus().subscribe(value => second = value);
    expect(second.mux.pools[0].connected).toBeFalse();
    http.expectNone('/api/5tratum/status');
  }));

  it('reads an explicit miner URI without writing or inferring coin data', () => {
    const fixture = statusFixture();
    let value!: FiveTratumStatusSnapshot;
    service.getFiveTratumStatus('http://miner.example.test').subscribe(result => value = result);
    const request = http.expectOne('http://miner.example.test/api/5tratum/status');
    expect(request.request.method).toBe('GET');
    expect(request.request.body).toBeNull();
    request.flush(fixture);
    expect(value).toEqual(fixture);
    expect(value.work.activePool).toBe(1);
    expect(value.coin).toBeNull();
  });

  it('uses the regenerated operation for a same-origin production read', fakeAsync(() => {
    environment.production = true;
    const fixture = statusFixture();
    const invoke = jasmine.createSpy('invoke').and.returnValue(Promise.resolve(fixture));
    const generatedService = new SystemApiService(TestBed.inject(HttpClient), { invoke } as unknown as Api);
    let value!: FiveTratumStatusSnapshot;
    generatedService.getFiveTratumStatus().subscribe(result => value = result);
    tick();
    expect(invoke).toHaveBeenCalledOnceWith(functions.getFiveTratumStatus, {});
    expect(value).toEqual(fixture);
    http.expectNone('/api/5tratum/status');
  }));

  it('uses a real same-origin GET when the generated API is not injected', () => {
    environment.production = true;
    const fixture = statusFixture();
    const directService = new SystemApiService(TestBed.inject(HttpClient), null as unknown as Api);
    directService.getFiveTratumStatus().subscribe(value => expect(value).toEqual(fixture));
    const request = http.expectOne('/api/5tratum/status');
    expect(request.request.method).toBe('GET');
    request.flush(fixture);
  });

  it('regenerates the actual GET operation with the exact status route and response', () => {
    const fixture = statusFixture();
    let value!: FiveTratumStatusSnapshot;
    functions.getFiveTratumStatus(TestBed.inject(HttpClient), '', {}).subscribe(response => value = response.body);
    const request = http.expectOne('/api/5tratum/status');
    expect(request.request.method).toBe('GET');
    expect(request.request.body).toBeNull();
    expect(request.request.headers.get('Accept')).toBe('application/json');
    request.flush(fixture);
    expect(value).toEqual(fixture);
  });

  it('propagates unavailable status without manufacturing a connection or retry', () => {
    let received = false;
    let status = 0;
    service.getFiveTratumStatus('http://miner.example.test').subscribe({
      next: () => received = true,
      error: error => status = error.status,
    });
    http.expectOne('http://miner.example.test/api/5tratum/status').flush(
      { ok: false, error: 'status-unavailable' }, { status: 503, statusText: 'Service Unavailable' });
    expect(received).toBeFalse();
    expect(status).toBe(503);
    http.expectNone('http://miner.example.test/api/5tratum/status');
  });
});
