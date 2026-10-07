import { provideHttpClient } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { TestBed } from '@angular/core/testing';
import { OperatingProfilesService, PoolSchedule } from './operating-profiles.service';
describe('Operating profiles HTTP contract', () => {
 let service: OperatingProfilesService; let http: HttpTestingController; const uri = 'http://miner.example.test';
 beforeEach(() => { TestBed.configureTestingModule({ providers: [provideHttpClient(), provideHttpClientTesting()] }); service = TestBed.inject(OperatingProfilesService); http = TestBed.inject(HttpTestingController); });
 afterEach(() => http.verify());
 it('keeps profile capture separate from Apply', () => { const body = { type: 'pool', slot: 3, name: 'My pool', captureCurrent: 'primary' }; service.save(body, uri).subscribe(); const request = http.expectOne(`${uri}/api/5tratum/profiles`); expect(request.request.body).toEqual(body); expect(request.request.method).toBe('POST'); request.flush({ ok: true, restartRequired: false }); http.expectNone(`${uri}/api/5tratum/profiles/apply`); });
 it('strips observed status, identity and unrelated event fields from saved schedules', () => { const draft = { schemaVersion: 1, enabled: true, utcOffsetMinutes: 60, clockValid: true, selectedSlot: 0, identity: { deviceId: 'private-record' }, events: [{ enabled: true, dayMask: 127, timeMinutes: 720, slot: 0, frequencyMHz: 625 }] } as unknown as PoolSchedule; service.saveSchedule(draft, uri).subscribe(); const request = http.expectOne(`${uri}/api/5tratum/pool-schedule`); expect(request.request.body).toEqual({ schemaVersion: 1, enabled: true, utcOffsetMinutes: 60, events: [{ enabled: true, dayMask: 127, timeMinutes: 720, slot: 0 }] }); request.flush(draft); });
});
