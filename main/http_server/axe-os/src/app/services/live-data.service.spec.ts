import { discardPeriodicTasks, fakeAsync, tick } from '@angular/core/testing';
import { EMPTY, of, Subject, throwError } from 'rxjs';
import { LiveDataService } from './live-data.service';
import { SystemApiService } from './system.service';
import { SystemInfo } from '../generated/models';

describe('LiveDataService telemetry freshness', () => {
  let api: jasmine.SpyObj<SystemApiService>;
  const reading = {
    version: '5tratumFW-test', uptimeSeconds: 10, hashRate: 1200,
    power: 18.4, temp: 55,
  } as SystemInfo;

  beforeEach(() => {
    api = jasmine.createSpyObj<SystemApiService>('SystemApiService', ['getInfo']);
    // Test local data handling without opening a WebSocket or making HTTP requests.
    spyOn(LiveDataService.prototype as any, 'connect').and.returnValue(EMPTY);
    spyOnProperty(document, 'visibilityState', 'get').and.returnValue('visible');
  });

  it('does not refresh cached telemetry after an empty or malformed response', fakeAsync(() => {
    const responses = new Subject<SystemInfo>();
    api.getInfo.and.returnValue(responses);
    const service = new LiveDataService(api);
    const received: SystemInfo[] = [];
    const subscription = service.info$.subscribe(info => received.push(info));
    responses.next(reading);
    const confirmedAt = service.lastUpdateAt;

    tick(1000);
    responses.next({} as SystemInfo);
    responses.next({ hashRate: 'invalid' } as unknown as SystemInfo);
    responses.next({ power: Number.NaN } as SystemInfo);

    expect(received.length).toBe(1);
    expect(service.lastUpdateAt).toBe(confirmedAt);
    expect(received[0].hashRate).toBe(1200);

    responses.next({ hashRate: 1250 } as SystemInfo);
    expect(received.length).toBe(2);
    expect(service.lastUpdateAt).toBeGreaterThan(confirmedAt);
    subscription.unsubscribe();
    discardPeriodicTasks();
  }));

  it('does not turn a cached replay into a new transport timestamp', fakeAsync(() => {
    api.getInfo.and.returnValue(of(reading));
    const service = new LiveDataService(api);
    const first = service.info$.subscribe();
    const confirmedAt = service.lastUpdateAt;
    tick(2000);
    const second = service.info$.subscribe();

    expect(service.lastUpdateAt).toBe(confirmedAt);
    expect(api.getInfo).toHaveBeenCalledTimes(1);
    first.unsubscribe();
    second.unsubscribe();
    discardPeriodicTasks();
  }));

  it('resumes fallback polling after a temporary read failure', fakeAsync(() => {
    api.getInfo.and.returnValues(
      of(reading),
      throwError(() => new Error('Temporary outage')),
      of({ ...reading, uptimeSeconds: 20, hashRate: 1250 }),
    );
    const service = new LiveDataService(api);
    const received: SystemInfo[] = [];
    const subscription = service.info$.subscribe(info => received.push(info));
    const confirmedAt = service.lastUpdateAt;

    tick(5000);
    expect(api.getInfo).toHaveBeenCalledTimes(2);
    expect(received.length).toBe(1);
    expect(service.lastUpdateAt).toBe(confirmedAt);

    tick(5000);
    expect(api.getInfo).toHaveBeenCalledTimes(3);
    expect(received.length).toBe(2);
    expect(received[1].hashRate).toBe(1250);
    expect(service.lastUpdateAt).toBeGreaterThan(confirmedAt);
    subscription.unsubscribe();
    discardPeriodicTasks();
  }));

  it('recovers after a failed initial fetch without inventing a first reading', fakeAsync(() => {
    api.getInfo.and.returnValues(throwError(() => new Error('Initially offline')), of(reading));
    const service = new LiveDataService(api);
    const received: SystemInfo[] = [];
    const subscription = service.info$.subscribe(info => received.push(info));
    expect(received).toEqual([]);
    expect(service.lastUpdateAt).toBe(0);
    tick(5000);
    expect(received).toEqual([reading]);
    expect(service.lastUpdateAt).toBeGreaterThan(0);
    subscription.unsubscribe();
    discardPeriodicTasks();
  }));

  it('lets a slow fallback fetch finish instead of canceling it at every poll', fakeAsync(() => {
    const slow = new Subject<SystemInfo>();
    api.getInfo.and.returnValues(throwError(() => new Error('Initially offline')), slow);
    const service = new LiveDataService(api);
    const received: SystemInfo[] = [];
    const subscription = service.info$.subscribe(info => received.push(info));
    tick(10000);
    expect(api.getInfo).toHaveBeenCalledTimes(2);
    slow.next(reading);
    slow.complete();
    expect(received).toEqual([reading]);
    subscription.unsubscribe();
    discardPeriodicTasks();
  }));
});
