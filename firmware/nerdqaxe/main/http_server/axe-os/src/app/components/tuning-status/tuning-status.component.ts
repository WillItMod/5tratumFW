import { CommonModule } from '@angular/common';
import { Component, Input, OnDestroy, OnInit } from '@angular/core';
import { RouterModule } from '@angular/router';
import { EMPTY, Subject, catchError, exhaustMap, takeUntil, timer } from 'rxjs';
import { AutotuneActivityService } from '../../services/autotune-activity.service';

type Activity = {schemaVersion:1;available:true;supported:boolean;active:boolean;ownsSettings:boolean;fault:boolean;
  state:string;reason:string;priority:'efficiency'|'balanced'|'performance';continuous:boolean;runId:number;controllerAgeSeconds:number|null};
type Badge = {label:string;phase:string;tone:string;description:string;goal:string};
const STATES=['disabled','baseline','settling','measuring','holding','observing','suspended','faulted','applying','restoring'];

@Component({selector:'app-tuning-status',standalone:true,imports:[CommonModule,RouterModule],host:{'[class.tuning-legacy-hidden]':'hiddenLegacy'},
  templateUrl:'./tuning-status.component.html',styleUrls:['./tuning-status.component.scss']})
export class TuningStatusComponent implements OnInit,OnDestroy {
  @Input() tuningRoute='/tuning';
  private destroyed=new Subject<void>();
  private last:Activity|null=null;
  private requestedAt=-Infinity;
  private unavailable=true;
  private everEngaged=false;
  hiddenLegacy=false;
  private problem='Checking current activity';
  constructor(private activity:AutotuneActivityService) {}
  ngOnInit():void {
    timer(0,5000).pipe(exhaustMap(()=>{
      const at=performance.now();
      return this.activity.get().pipe(exhaustMap(value=>{this.accept(value,at);return EMPTY;}),catchError(error=>{
        this.unavailable=true;
        this.hiddenLegacy=error?.status===404&&!this.everEngaged;
        this.problem=error?.status===404?'Matching application and web interface are required':'Activity status is unavailable; check the connection';
        return EMPTY;
      }));
    }),takeUntil(this.destroyed)).subscribe();
    // Refresh the stale indicator even when a browser throttles network polling.
    timer(0,1000).pipe(takeUntil(this.destroyed)).subscribe(()=>{});
  }
  ngOnDestroy():void {this.destroyed.next();this.destroyed.complete();}
  private valid(value:any):value is Activity {
    return !!value&&typeof value==='object'&&!Array.isArray(value)&&value.schemaVersion===1&&value.available===true
      &&['supported','active','ownsSettings','fault','continuous'].every(key=>typeof value[key]==='boolean')
      &&STATES.includes(value.state)&&typeof value.reason==='string'
      &&['efficiency','balanced','performance'].includes(value.priority)
      &&Number.isInteger(value.runId)&&value.runId>=0
      &&(value.controllerAgeSeconds===null||typeof value.controllerAgeSeconds==='number'&&Number.isFinite(value.controllerAgeSeconds)&&value.controllerAgeSeconds>=0);
  }
  private snapshot(value:Activity):Activity {
    return {schemaVersion:1,available:true,supported:value.supported,active:value.active,ownsSettings:value.ownsSettings,
      fault:value.fault,state:value.state,reason:value.reason.slice(0,256),priority:value.priority,continuous:value.continuous,
      runId:value.runId,controllerAgeSeconds:value.controllerAgeSeconds};
  }
  private accept(value:unknown,at:number):void {
    this.hiddenLegacy=false;
    if(!this.valid(value)) {
      this.unavailable=true;
      this.problem=(value as any)?.schemaVersion===1&&(value as any)?.available===false?'Controller snapshot is busy; checking again':'Unrecognised activity report; matching application and web interface are required';
      return;
    }
    this.everEngaged=this.everEngaged||value.active||value.ownsSettings||value.fault;
    const transit=(performance.now()-at)/1000;
    if(value.controllerAgeSeconds===null||transit<0||value.controllerAgeSeconds+transit>15) {
      const evidence=(report:Activity|null)=>report?.fault?2:report&&(report.active||report.ownsSettings)?1:0;
      if(!this.last||evidence(value)>evidence(this.last))this.last=this.snapshot(value);
      this.unavailable=true;this.problem='Controller heartbeat is stale or unavailable; checking again';return;
    }
    this.last=this.snapshot(value);this.requestedAt=at;this.unavailable=false;this.problem='';
  }
  get fresh():boolean {
    const elapsed=(performance.now()-this.requestedAt)/1000;
    return !!this.last&&!this.unavailable&&this.last.controllerAgeSeconds!==null&&elapsed>=0&&this.last.controllerAgeSeconds+elapsed<=15;
  }
  get badge():Badge {
    const current=this.last;
    if(!this.fresh||!current) {
      const label=current?.fault?'Tuning last blocked':current?.active?'Tuning last active':current?.ownsSettings?'Tuning last held':'Tuning checking';
      return {label,phase:'Current state unknown',tone:'unknown',goal:'',description:`${this.problem||'Controller heartbeat is stale'}. ${current&&(current.active||current.ownsSettings)?'The last reported tuner still held control; do not assume it stopped. ':''}Open Tuning for fresh details.`};
    }
    const goal={efficiency:'Efficiency',balanced:'Balanced',performance:'Performance'}[current.priority];
    const result=(label:string,phase:string,tone:string):Badge=>({label,phase,tone,goal,description:`${label}. ${phase}. ${current.reason.replace(/[-_]/g,' ')}. Open Tuning for details.`});
    if(current.fault)return result('Tuning blocked','Protection reported','fault');
    if(!current.active&&!current.ownsSettings)return result('Tuning off','Control released','off');
    if(current.state==='faulted')return result('Tuning faulted','Check Tuning','fault');
    if(current.state==='restoring')return result('Tuning restoring','Backup restore','restoring');
    if(current.state==='suspended')return result('Tuning suspended',current.ownsSettings?'Control retained':'Adjustments paused','suspended');
    if(current.state==='observing'&&current.active)return result('Tuning monitoring','Watching conditions','observing');
    if(current.active) {
      const phase=({baseline:'Measuring reference',settling:'Settling',measuring:'Measuring',applying:'Adjusting',holding:'Holding result',disabled:'Checking controller'} as Record<string,string>)[current.state]||'Checking controller';
      return result('Tuning active',phase,'active');
    }
    if(current.ownsSettings)return result('Tuning holding','Control retained','holding');
    return result('Tuning off','Explicit Start required','off');
  }
}
