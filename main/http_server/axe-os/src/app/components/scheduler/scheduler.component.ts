import { Component, ViewChild } from '@angular/core';
import { TuningScheduleComponent } from '../tuning-schedule/tuning-schedule.component';

@Component({
  selector: 'app-scheduler',
  templateUrl: './scheduler.component.html',
})
export class SchedulerComponent {
  @ViewChild(TuningScheduleComponent) tuningScheduler?: TuningScheduleComponent;
  section: 'power' | 'tuning' | 'pool' = 'power';
  canLeave(): boolean { return !this.tuningScheduler?.hasUnsavedChanges || window.confirm('Leave this page and discard its unsaved tuning schedule changes?'); }
  selectSection(section: 'power' | 'tuning' | 'pool'): void {
    if (section !== this.section && this.canLeave()) this.section = section;
  }
}
export function leaveScheduler(component: SchedulerComponent): boolean { return component.canLeave(); }
