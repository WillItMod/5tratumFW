import { Injectable } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable, of, timeout } from 'rxjs';
import { environment } from 'src/environments/environment';

export interface TuningSlot { slot: number; configured: boolean; name: string | null; frequencyMHz?: number; coreVoltageMv?: number; }
export interface PoolSlot { slot: number; configured: boolean; name: string | null; host?: string; port?: number; user?: string; protocol?: string; passwordConfigured?: boolean; }
export interface Profiles { schemaVersion: number; tuning: TuningSlot[]; pools: PoolSlot[]; limits: { frequency: { min: number; max: number; step: number; quantization: string }; coreVoltage: { min: number; max: number; step: number } }; current: { frequencyMHz: number; coreVoltageMv: number; overclockEnabled?: boolean }; }
export interface ProfileResult { ok: boolean; restartRequired: boolean; }
export interface PoolEvent { enabled: boolean; dayMask: number; timeMinutes: number; slot: number; }
export interface PoolSchedule { schemaVersion: number; enabled: boolean; utcOffsetMinutes: number; events: PoolEvent[]; clockValid?: boolean; selectedSlot?: number | null; timezoneMode?: string; poolTarget?: string; maxEvents?: number; storageValid?: boolean; }
@Injectable({ providedIn: 'root' })
export class OperatingProfilesService {
  private mockProfiles: Profiles = { schemaVersion: 1, tuning: Array.from({ length: 10 }, (_, slot) => ({ slot, configured: false, name: null })), pools: Array.from({ length: 10 }, (_, slot) => ({ slot, configured: false, name: null })), limits: { frequency: { min: 400, max: 625, step: .25, quantization: 'nearest-pll' }, coreVoltage: { min: 1000, max: 1250, step: 1 } }, current: { frequencyMHz: 525, coreVoltageMv: 1150 } };
  private mockSchedule: PoolSchedule = { schemaVersion: 1, enabled: false, utcOffsetMinutes: 0, events: [], clockValid: false, selectedSlot: null, timezoneMode: 'fixed-utc-offset', poolTarget: 'primary', maxEvents: 16 };
  constructor(private http: HttpClient) {}
  get(uri = ''): Observable<Profiles> { return environment.production || uri ? this.http.get<Profiles>(`${uri}/api/5tratum/profiles`).pipe(timeout(5000)) : of(structuredClone(this.mockProfiles)); }
  save(body: Record<string, unknown>, uri = ''): Observable<ProfileResult> {
    if (environment.production || uri) return this.http.post<ProfileResult>(`${uri}/api/5tratum/profiles`, body).pipe(timeout(6000));
    const list = body['type'] === 'pool' ? this.mockProfiles.pools : this.mockProfiles.tuning;
    const slot = Number(body['slot']);
    list[slot] = body['clear'] ? { slot, configured: false, name: null } : { ...(body['type'] === 'pool' ? { host: 'pool.example.test', port: 3333, user: 'preview.worker', protocol: 'SV1', passwordConfigured: true } : {}), ...body, slot, name: String(body['name']), configured: true } as any;
    return of({ ok: true, restartRequired: false });
  }
  apply(body: Record<string, unknown>, uri = ''): Observable<ProfileResult> {
    if (environment.production || uri) return this.http.post<ProfileResult>(`${uri}/api/5tratum/profiles/apply`, body).pipe(timeout(6000));
    if (body['type'] === 'tuning') {
      const point = body['slot'] === undefined ? body : this.mockProfiles.tuning[Number(body['slot'])];
      this.mockProfiles.current = { frequencyMHz: Number(point['frequencyMHz']), coreVoltageMv: Number(point['coreVoltageMv']) };
    }
    return of({ ok: true, restartRequired: body['poolTarget'] === 'fallback' });
  }
  getSchedule(uri = ''): Observable<PoolSchedule> { return environment.production || uri ? this.http.get<PoolSchedule>(`${uri}/api/5tratum/pool-schedule`).pipe(timeout(5000)) : of(structuredClone(this.mockSchedule)); }
  saveSchedule(schedule: PoolSchedule, uri = ''): Observable<PoolSchedule> {
    const body = { schemaVersion: 1, enabled: schedule.enabled, utcOffsetMinutes: schedule.utcOffsetMinutes, events: schedule.events.map(({ enabled, dayMask, timeMinutes, slot }) => ({ enabled, dayMask, timeMinutes, slot })) };
    if (environment.production || uri) return this.http.post<PoolSchedule>(`${uri}/api/5tratum/pool-schedule`, body).pipe(timeout(6000));
    this.mockSchedule = { ...body, clockValid: false, selectedSlot: null }; return of(structuredClone(this.mockSchedule));
  }
}
