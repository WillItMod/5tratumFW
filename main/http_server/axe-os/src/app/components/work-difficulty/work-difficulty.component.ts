import { CommonModule } from '@angular/common';
import { Component, Input, OnDestroy, OnInit } from '@angular/core';
import { EMPTY, Subject, catchError, exhaustMap, takeUntil, timer } from 'rxjs';
import { PoolWorkDifficulty, WorkDifficultyReport, WorkDifficultyService } from '../../services/work-difficulty.service';

@Component({selector:'app-work-difficulty',standalone:true,imports:[CommonModule],templateUrl:'./work-difficulty.component.html',styleUrls:['./work-difficulty.component.scss']})
export class WorkDifficultyComponent implements OnInit,OnDestroy {
  @Input() compact=false;
  private destroyed=new Subject<void>();
  private requestedAt=-Infinity;
  private failed=true;
  pools:PoolWorkDifficulty[]=[];
  constructor(private service:WorkDifficultyService) {}
  ngOnInit():void {
    timer(0,5000).pipe(exhaustMap(()=>{
      const at=performance.now();
      return this.service.get().pipe(exhaustMap(value=>{this.accept(value,at);return EMPTY;}),catchError(()=>{this.failed=true;return EMPTY;}));
    }),takeUntil(this.destroyed)).subscribe();
    timer(0,1000).pipe(takeUntil(this.destroyed)).subscribe(()=>{});
  }
  ngOnDestroy():void {this.destroyed.next();this.destroyed.complete();}
  private finite(value:unknown):value is number {return typeof value==='number'&&Number.isFinite(value);}
  private valid(value:any):value is WorkDifficultyReport {
    const positive=(n:unknown)=>n===null||this.finite(n)&&n>0;
    return !!value&&value.schemaVersion===1&&value.normalization==='bitcoin-diff1'&&Array.isArray(value.pools)&&value.pools.length===2
      &&value.pools.every((pool:any)=>pool&&[0,1].includes(pool.index)&&['selected','connected','available'].every(key=>typeof pool[key]==='boolean')
        &&pool.source==='active-stratum-job'&&positive(pool.shareDifficulty)&&positive(pool.networkDifficulty)
        &&(pool.jobAgeSeconds===null||this.finite(pool.jobAgeSeconds)&&pool.jobAgeSeconds>=0)
        &&(!pool.available||pool.connected&&this.finite(pool.shareDifficulty)&&pool.shareDifficulty>0&&this.finite(pool.networkDifficulty)&&pool.networkDifficulty>0&&this.finite(pool.jobAgeSeconds)))
      &&new Set(value.pools.map((pool:any)=>pool.index)).size===2;
  }
  private accept(value:unknown,at:number):void {
    if(!this.valid(value)){this.failed=true;return;}
    this.pools=value.pools.map(pool=>({index:pool.index,selected:pool.selected,connected:pool.connected,available:pool.available,shareDifficulty:pool.shareDifficulty,networkDifficulty:pool.networkDifficulty,jobAgeSeconds:pool.jobAgeSeconds,source:pool.source})).sort((a,b)=>a.index-b.index);
    this.requestedAt=at;this.failed=false;
  }
  get fresh():boolean {const age=performance.now()-this.requestedAt;return !this.failed&&age>=0&&age<=12000;}
  get rows():PoolWorkDifficulty[] {return this.pools.length?this.pools:[0,1].map(index=>({index:index as 0|1,selected:false,connected:false,available:false,shareDifficulty:null,networkDifficulty:null,jobAgeSeconds:null,source:'active-stratum-job'}));}
  value(pool:PoolWorkDifficulty,key:'shareDifficulty'|'networkDifficulty'):string {
    const value=pool[key];if(!this.fresh||!pool.connected||!pool.available||!this.finite(value)||value<=0)return '—';
    return new Intl.NumberFormat('en-GB',{notation:value>=1000000?'compact':'standard',maximumSignificantDigits:6}).format(value);
  }
  exact(pool:PoolWorkDifficulty,key:'shareDifficulty'|'networkDifficulty'):string {
    const value=pool[key];return this.value(pool,key)==='—'?'No current job value available':new Intl.NumberFormat('en-GB',{maximumSignificantDigits:15}).format(value!);
  }
  state(pool:PoolWorkDifficulty):string {return !this.fresh?'Unavailable':!pool.connected?'Disconnected':pool.available?'Current work':'Waiting for work';}
  jobAge(pool:PoolWorkDifficulty):string {
    return this.fresh&&pool.connected&&pool.available&&this.finite(pool.jobAgeSeconds)?`${Math.floor(pool.jobAgeSeconds+(performance.now()-this.requestedAt)/1000)} s`:'—';
  }
}
