import { provideHttpClient } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { TestBed } from '@angular/core/testing';
import { OperatingProfilesService, PoolSchedule, PoolSlot } from './operating-profiles.service';
import { TuningSchedule } from 'src/models/ITuningSchedule';
describe('Operating profiles HTTP contract', () => {
 let service: OperatingProfilesService; let http: HttpTestingController; const uri = 'http://miner.example.test';
 beforeEach(() => { TestBed.configureTestingModule({ providers: [provideHttpClient(), provideHttpClientTesting()] }); service = TestBed.inject(OperatingProfilesService); http = TestBed.inject(HttpTestingController); });
 afterEach(() => http.verify());
 it('keeps profile capture separate from Apply', () => { const body = { type: 'pool', slot: 3, name: 'My pool', captureCurrent: 'primary' }; service.save(body, uri).subscribe(); const request = http.expectOne(`${uri}/api/5tratum/profiles`); expect(request.request.body).toEqual(body); expect(request.request.method).toBe('POST'); request.flush({ ok: true, restartRequired: false }); http.expectNone(`${uri}/api/5tratum/profiles/apply`); });
 it('preserves saved pool authentication flags and leaves legacy slots unspecified', () => {
   const pools: PoolSlot[] = [
     {slot:0,configured:true,name:'Authenticated',sv2RequireAuth:true},
     {slot:1,configured:true,name:'Optional',sv2RequireAuth:false},
     {slot:2,configured:true,name:'Legacy'}
   ];
   service.get(uri).subscribe(profiles => {
     expect(profiles.pools).toEqual(pools);
     expect(Object.prototype.hasOwnProperty.call(profiles.pools[2],'sv2RequireAuth')).toBeFalse();
   });
   http.expectOne(`${uri}/api/5tratum/profiles`).flush({schemaVersion:1,pools,tuning:[]});
   const rename = {type:'pool',slot:2,name:'Legacy renamed'};
   service.save(rename,uri).subscribe();
   const write = http.expectOne(`${uri}/api/5tratum/profiles`);
   expect(write.request.body).toEqual(rename);expect(write.request.body.sv2RequireAuth).toBeUndefined();
   write.flush({ok:true,restartRequired:false});
   service.apply({type:'pool',slot:2,poolTarget:'primary'},uri).subscribe();
   const apply = http.expectOne(`${uri}/api/5tratum/profiles/apply`);
   expect(apply.request.body).toEqual({type:'pool',slot:2,poolTarget:'primary'});
   apply.flush({ok:true,restartRequired:false});
 });
 it('strips observed status, identity and unrelated event fields from saved schedules', () => { const draft = { schemaVersion: 1, enabled: true, utcOffsetMinutes: 60, clockValid: true, selectedSlot: 0, identity: { deviceId: 'private-record' }, events: [{ enabled: true, dayMask: 127, timeMinutes: 720, slot: 0, frequencyMHz: 625 }] } as unknown as PoolSchedule; service.saveSchedule(draft, uri).subscribe(); const request = http.expectOne(`${uri}/api/5tratum/pool-schedule`); expect(request.request.body).toEqual({ schemaVersion: 1, enabled: true, utcOffsetMinutes: 60, events: [{ enabled: true, dayMask: 127, timeMinutes: 720, slot: 0 }] }); request.flush(draft); });
 it('reads tuning status and writes only tuning schedule fields for the selected miner', () => {
   const configuration = {schemaVersion:1 as const,enabled:true,utcOffsetMinutes:0,events:[{enabled:true,dayMask:127,timeMinutes:540,slot:0}]};
   service.getTuningSchedule(uri).subscribe();
   const read = http.expectOne(`${uri}/api/5tratum/tuning-schedule`);expect(read.request.method).toBe('GET');read.flush(configuration);
   const draft = {...configuration,selectedSlot:0,appliedSlot:0,events:[{...configuration.events[0],frequencyMHz:625}]} as unknown as TuningSchedule;
   service.saveTuningSchedule(draft,uri).subscribe();
   const write = http.expectOne(`${uri}/api/5tratum/tuning-schedule`);expect(write.request.method).toBe('POST');expect(write.request.body).toEqual(configuration);
   write.flush({ok:true,restartRequired:false});http.expectNone('/api/5tratum/profiles/apply');http.expectNone('/api/system');
 });
 it('keeps development tuning schedule reads detached and does not simulate hardware activation on save', () => {
   let first:any,second:any;
   service.getTuningSchedule().subscribe(report => first = report);first.events.push({enabled:true,dayMask:127,timeMinutes:540,slot:0});
   service.getTuningSchedule().subscribe(report => second = report);expect(second.events.length).toBe(0);
   service.saveTuningSchedule({schemaVersion:1,enabled:true,utcOffsetMinutes:0,events:first.events}).subscribe();
   service.getTuningSchedule().subscribe(report => second = report);
   expect(second.status).toBe('waiting-clock');expect(second.appliedSlot).toBeNull();expect(second.selectedSlot).toBeNull();
   http.expectNone('/api/5tratum/tuning-schedule');
 });
});
