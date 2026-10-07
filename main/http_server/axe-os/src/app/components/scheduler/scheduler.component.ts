import { Component } from '@angular/core';

@Component({
  selector: 'app-scheduler',
  templateUrl: './scheduler.component.html',
})
export class SchedulerComponent { section: 'power' | 'pool' = 'power'; }
