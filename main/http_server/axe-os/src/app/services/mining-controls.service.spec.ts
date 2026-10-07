import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { TestBed, fakeAsync, tick } from '@angular/core/testing';
import { MiningControlsService, MiningSchedule, MiningScheduleResponse, MiningScheduleStatus, miningSchedulePayload, miningStateLabel, validateMiningSchedule } from './mining-controls.service';

const schedule: MiningSchedule = { enabled: false, timezone: 'UTC', windows: [] };
const status: MiningScheduleStatus = { clockValid: true, localTime: '2026-10-07 12:00:00', scheduledPause: false, manualOverride: 'none', requestedPaused: false, appliedPaused: false, transitionPending: false, error: null };
const response: MiningScheduleResponse = { supported: true, schedule, status };

describe('Mining schedule contract', () => {
  let service: MiningControlsService;
  let http: HttpTestingController;
  beforeEach(() => {
    TestBed.configureTestingModule({ providers: [provideHttpClient(), provideHttpClientTesting()] });
    service = TestBed.inject(MiningControlsService);
    http = TestBed.inject(HttpTestingController);
  });
  afterEach(() => http.verify());

  it('reads a disabled schedule without enabling it or writing settings', () => {
    let received: MiningScheduleResponse | undefined;
    service.getSchedule().subscribe(value => received = value);
    http.expectOne('/api/system/mining/schedule').flush(response);
    expect(received?.schedule?.enabled).toBeFalse();
    http.expectNone(request => request.method !== 'GET');
  });

  it('maps an upstream404 to unavailable while preserving actual read failures', () => {
    let supported: boolean | undefined;
    service.getSchedule().subscribe(value => supported = value.supported);
    http.expectOne('/api/system/mining/schedule').flush({}, { status: 404, statusText: 'Not Found' });
    expect(supported).toBeFalse();
    let failed = false;
    service.getSchedule().subscribe({ error: () => failed = true });
    http.expectOne('/api/system/mining/schedule').flush({}, { status: 401, statusText: 'Unauthorized' });
    expect(failed).toBeTrue();
  });

  it('rejects incomplete status instead of treating a request flag as ASIC confirmation', () => {
    let failed = false;
    service.getSchedule().subscribe({ error: () => failed = true });
    http.expectOne('/api/system/mining/schedule').flush({ ...response, status: { requestedPaused: true } });
    expect(failed).toBeTrue();
  });

  it('saves only the schedule, preserving overnight times and excluding hardware fields', () => {
    const draft = { enabled: true, timezone: 'Europe/London', windows: [{ days: 2, start: '23:00', end: '07:00', frequency: 650 }], coreVoltage: 1200, frequency: 650 } as unknown as MiningSchedule;
    service.saveSchedule(draft).subscribe();
    const request = http.expectOne('/api/system/mining/schedule');
    expect(request.request.method).toBe('PUT');
    expect(request.request.body).toEqual({ enabled: true, timezone: 'Europe/London', windows: [{ days: 2, start: '23:00', end: '07:00' }] });
    request.flush({ message: 'Saved' });
  });

  it('rejects enabled-empty, zero-day, equal-time, invalid-zone and over-capacity schedules before HTTP', () => {
    const window = { days: 127, start: '23:00', end: '07:00' };
    const invalid = [
      { ...schedule, enabled: true },
      { ...schedule, windows: [{ ...window, days: 0 }] },
      { ...schedule, windows: [{ ...window, end: '23:00' }] },
      { ...schedule, windows: [{ ...window, start: '24:00' }] },
      { ...schedule, timezone: 'Unknown/Zone' },
      { ...schedule, windows: Array.from({ length: 9 }, () => window) },
    ];
    invalid.forEach(draft => {
      let failed = false;
      service.saveSchedule(draft).subscribe({ error: () => failed = true });
      expect(failed).toBeTrue();
    });
    http.expectNone('/api/system/mining/schedule');
    expect(validateMiningSchedule({ ...schedule, windows: Array.from({ length: 8 }, () => window) })).toEqual([]);
    expect(miningSchedulePayload(schedule).enabled).toBeFalse();
  });

  it('clears override explicitly without changing schedule or hardware settings', () => {
    service.returnToSchedule().subscribe();
    const request = http.expectOne('/api/system/mining/schedule/override');
    expect(request.request.method).toBe('POST');
    expect(request.request.body).toEqual({ mode: 'schedule' });
    request.flush({ message: 'Returned to schedule' });
  });

  it('recovers polling after a transient read failure', fakeAsync(() => {
    const seen: Array<boolean | null> = [];
    const subscription = service.observe().subscribe(value => seen.push(value.response?.supported ?? null));
    tick(0);
    http.expectOne('/api/system/mining/schedule').flush({}, { status: 500, statusText: 'Server Error' });
    tick(5000);
    http.expectOne('/api/system/mining/schedule').flush(response);
    expect(seen).toEqual([null, true]);
    subscription.unsubscribe();
  }));

  it('reports a slow read before the next poll instead of silently canceling it forever', fakeAsync(() => {
    const errors: Array<string | null> = [];
    const subscription = service.observe().subscribe(value => errors.push(value.error));
    tick(0);
    const slow = http.expectOne('/api/system/mining/schedule');
    tick(4000);
    expect(slow.cancelled).toBeTrue();
    expect(errors[0]).toContain('Unable to read');
    tick(1000);
    http.expectOne('/api/system/mining/schedule').flush(response);
    expect(errors[1]).toBeNull();
    subscription.unsubscribe();
  }));

  it('distinguishes pending request, applied state, stale status and errors', () => {
    expect(miningStateLabel({ ...status, requestedPaused: true, transitionPending: true }, true)).toBe('Pausing ASIC…');
    expect(miningStateLabel({ ...status, requestedPaused: true, appliedPaused: true }, true)).toBe('ASIC paused');
    expect(miningStateLabel({ ...status, appliedPaused: true, transitionPending: true }, true)).toBe('Starting ASIC…');
    expect(miningStateLabel(status, false)).toBe('Mining status stale');
    expect(miningStateLabel({ ...status, error: 'ASIC failed' }, true)).toBe('Mining needs attention');
  });
});
