import { HttpClient, HttpErrorResponse } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable, Subject, catchError, map, merge, of, shareReplay, switchMap, throwError, timeout, timer } from 'rxjs';

export interface MiningWindow { days: number; start: string; end: string; }
export interface MiningSchedule { enabled: boolean; timezone: string; windows: MiningWindow[]; }
export interface MiningScheduleStatus {
  clockValid: boolean; localTime: string | null; scheduledPause: boolean;
  manualOverride: 'none' | 'paused' | 'running'; requestedPaused: boolean;
  appliedPaused: boolean; transitionPending: boolean; error: string | null;
}
export interface MiningScheduleResponse { supported: boolean; schedule?: MiningSchedule; status?: MiningScheduleStatus; }
export interface MiningObservation { response: MiningScheduleResponse | null; receivedAt: number; error: string | null; }
export const MINING_TIMEZONES = ['UTC', 'Europe/London', 'Europe/Berlin', 'America/New_York', 'America/Chicago', 'America/Denver', 'America/Los_Angeles', 'Asia/Tokyo', 'Asia/Shanghai', 'Australia/Sydney'];
const VALID_TIME = /^(?:[01]\d|2[0-3]):[0-5]\d$/;

export function validateMiningSchedule(schedule: MiningSchedule): string[] {
  const errors: string[] = [];
  if (typeof schedule.enabled !== 'boolean') errors.push('Choose whether the schedule is enabled.');
  if (!MINING_TIMEZONES.includes(schedule.timezone)) errors.push('Choose a supported timezone.');
  if (!Array.isArray(schedule.windows)) return [...errors, 'Schedule windows must be a list.'];
  if (schedule.windows.length > 8) errors.push('Use at most eight windows.');
  if (schedule.enabled && !schedule.windows.length) errors.push('Add a pause window before enabling the schedule.');
  schedule.windows.forEach((window, index) => {
    const name = `Window ${index + 1}`;
    if (!Number.isInteger(window.days) || window.days < 1 || window.days > 127) errors.push(`${name}: select at least one start day.`);
    if (!VALID_TIME.test(window.start) || !VALID_TIME.test(window.end)) errors.push(`${name}: enter valid start and end times.`);
    else if (window.start === window.end) errors.push(`${name}: start and end times must differ.`);
  });
  return errors;
}

export function miningSchedulePayload(schedule: MiningSchedule): MiningSchedule {
  const errors = validateMiningSchedule(schedule);
  if (errors.length) throw new Error(errors[0]);
  return { enabled: schedule.enabled, timezone: schedule.timezone, windows: schedule.windows.map(window => ({ days: window.days, start: window.start, end: window.end })) };
}

function validResponse(response: MiningScheduleResponse): boolean {
  if (response?.supported === false) return true;
  const status = response?.status;
  return response?.supported === true && !!response.schedule && !!status
    && validateMiningSchedule(response.schedule).length === 0
    && ['clockValid', 'scheduledPause', 'requestedPaused', 'appliedPaused', 'transitionPending'].every(key => typeof status[key as keyof MiningScheduleStatus] === 'boolean')
    && ['none', 'paused', 'running'].includes(status.manualOverride)
    && (status.localTime === null || typeof status.localTime === 'string')
    && (!status.clockValid || (typeof status.localTime === 'string' && status.localTime.length > 0))
    && (status.error === null || typeof status.error === 'string');
}

export function miningStateLabel(status: MiningScheduleStatus | null, fresh: boolean): string {
  if (!status) return 'Waiting for mining status';
  if (!fresh) return 'Mining status stale';
  if (status.error) return 'Mining needs attention';
  if (status.transitionPending || status.requestedPaused !== status.appliedPaused) return status.requestedPaused ? 'Pausing ASIC…' : 'Starting ASIC…';
  return status.appliedPaused ? 'ASIC paused' : 'ASIC running';
}

@Injectable({ providedIn: 'root' })
export class MiningControlsService {
  private refreshRequested = new Subject<void>();
  private observations = new Map<string, Observable<MiningObservation>>();
  constructor(private http: HttpClient) {}

  getSchedule(uri = ''): Observable<MiningScheduleResponse> {
    return this.http.get<MiningScheduleResponse>(`${uri}/api/system/mining/schedule`).pipe(
      timeout(4000),
      map(response => {
        if (!validResponse(response)) throw new Error('The device returned incomplete mining status.');
        return response;
      }),
      catchError(error => error instanceof HttpErrorResponse && error.status === 404 ? of({ supported: false }) : throwError(() => error))
    );
  }

  observe(uri = ''): Observable<MiningObservation> {
    let observation = this.observations.get(uri);
    if (!observation) {
      observation = merge(timer(0, 5000), this.refreshRequested).pipe(
        switchMap(() => this.getSchedule(uri).pipe(
          map(response => ({ response, receivedAt: Date.now(), error: null } as MiningObservation)),
          catchError(() => of({ response: null, receivedAt: 0, error: 'Unable to read mining status from the device.' } as MiningObservation))
        )),
        shareReplay({ bufferSize: 1, refCount: true })
      );
      this.observations.set(uri, observation);
    }
    return observation;
  }

  refresh(): void { this.refreshRequested.next(); }
  saveSchedule(schedule: MiningSchedule, uri = ''): Observable<unknown> {
    let payload: MiningSchedule;
    try { payload = miningSchedulePayload(schedule); }
    catch (error) { return throwError(() => error); }
    return this.http.put(`${uri}/api/system/mining/schedule`, payload).pipe(timeout(10000));
  }
  returnToSchedule(uri = ''): Observable<unknown> {
    return this.http.post(`${uri}/api/system/mining/schedule/override`, { mode: 'schedule' }).pipe(timeout(10000));
  }
}
