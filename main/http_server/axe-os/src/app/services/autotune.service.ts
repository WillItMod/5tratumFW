import { Injectable } from '@angular/core';
import { HttpClient, HttpHeaders } from '@angular/common/http';
import { Observable, of, timeout } from 'rxjs';
import { AutotuneAck, AutotuneReport } from '../models/IAutotune';

@Injectable({ providedIn: 'root' })
export class AutotuneService {
  constructor(private http: HttpClient) {}
  get(): Observable<AutotuneReport> {
    return this.http.get<AutotuneReport>('/api/5tratum/autotune').pipe(timeout(6000));
  }
  authorize(): Observable<{totp?: string}> { return of({}); }
  command(body: Record<string, unknown>, totp?: string): Observable<AutotuneAck> {
    const headers = totp ? new HttpHeaders({'X-TOTP': totp}) : new HttpHeaders();
    return this.http.post<AutotuneAck>('/api/5tratum/autotune', body, {headers}).pipe(timeout(8000));
  }
}
