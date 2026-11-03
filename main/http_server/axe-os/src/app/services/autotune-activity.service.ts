import { Injectable } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable, timeout } from 'rxjs';

/** Passive cached activity only. Never reads sensors or changes tuner settings. */
@Injectable({providedIn:'root'})
export class AutotuneActivityService {
  constructor(private http: HttpClient) {}
  get(): Observable<unknown> {
    return this.http.get<unknown>('/api/5tratum/autotune/activity',{headers:{'Cache-Control':'no-cache'}}).pipe(timeout(6000));
  }
}
