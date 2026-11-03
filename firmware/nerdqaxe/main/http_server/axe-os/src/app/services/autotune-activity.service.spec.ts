import { TestBed } from '@angular/core/testing';
import { provideHttpClient } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { AutotuneActivityService } from './autotune-activity.service';

describe('Passive global tuning transport',()=>{
 let service:AutotuneActivityService;let http:HttpTestingController;
 beforeEach(()=>{TestBed.configureTestingModule({providers:[provideHttpClient(),provideHttpClientTesting(),AutotuneActivityService]});service=TestBed.inject(AutotuneActivityService);http=TestBed.inject(HttpTestingController);});
 afterEach(()=>http.verify());
 it('reads only the cached activity endpoint without reading full status or authorizing a command',()=>{
  service.get().subscribe();const request=http.expectOne('/api/5tratum/autotune/activity');expect(request.request.method).toBe('GET');expect(request.request.headers.get('Cache-Control')).toBe('no-cache');request.flush({schemaVersion:1,available:false});
  http.expectNone('/api/5tratum/autotune');http.expectNone(req=>req.method!=='GET');
 });
});
