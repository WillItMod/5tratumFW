import { TestBed } from '@angular/core/testing';
import { provideHttpClient } from '@angular/common/http';
import { provideHttpClientTesting, HttpTestingController } from '@angular/common/http/testing';
import { WorkDifficultyService } from './work-difficulty.service';
describe('Current work difficulty transport',()=>{
 it('only performs the canonical passive GET without cached browser replies',()=>{TestBed.configureTestingModule({providers:[provideHttpClient(),provideHttpClientTesting(),WorkDifficultyService]});const service=TestBed.inject(WorkDifficultyService),http=TestBed.inject(HttpTestingController);service.get().subscribe();const request=http.expectOne('/api/5tratum/work-difficulty');expect(request.request.method).toBe('GET');expect(request.request.headers.get('Cache-Control')).toBe('no-cache');request.flush({schemaVersion:1,normalization:'bitcoin-diff1',pools:[]});http.verify();});
});
