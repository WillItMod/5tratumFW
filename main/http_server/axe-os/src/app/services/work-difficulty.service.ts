import { Injectable } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable, timeout } from 'rxjs';

export interface PoolWorkDifficulty { index: 0|1; selected: boolean; connected: boolean; available: boolean; shareDifficulty: number|null; networkDifficulty: number|null; jobAgeSeconds: number|null; source: 'active-stratum-job'; }
export interface WorkDifficultyReport { schemaVersion: 1; normalization: 'bitcoin-diff1'; pools: PoolWorkDifficulty[]; }
/** Passive current-work metadata only. No external chain lookup or state-changing request. */
@Injectable({providedIn:'root'})
export class WorkDifficultyService {
  constructor(private http:HttpClient) {}
  get():Observable<unknown> { return this.http.get<unknown>('/api/5tratum/work-difficulty',{headers:{'Cache-Control':'no-cache'}}).pipe(timeout(6000)); }
}
