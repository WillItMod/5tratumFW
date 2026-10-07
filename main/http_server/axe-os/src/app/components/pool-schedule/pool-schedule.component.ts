import { Component, OnDestroy, OnInit } from '@angular/core';
import { forkJoin, finalize, Subject, takeUntil } from 'rxjs';
import { OperatingProfilesService, PoolSchedule, PoolSlot, PoolEvent } from 'src/app/services/operating-profiles.service';
@Component({ selector: 'app-pool-schedule', templateUrl: './pool-schedule.component.html', styleUrls: ['./pool-schedule.component.scss'] })
export class PoolScheduleComponent implements OnInit, OnDestroy {
  schedule?: PoolSchedule;
  slots: PoolSlot[] = [];
  busy = false;
  dirty = false;
  error = '';
  message = '';
  readonly days = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
  readonly offsets = Array.from({ length: 105 }, (_, index) => index * 15 - 720);
  private destroy$ = new Subject<void>();
  constructor(private service: OperatingProfilesService) {}
  ngOnInit() { this.load(); }
  ngOnDestroy() { this.destroy$.next(); this.destroy$.complete(); }
  load() {
    this.busy = true;
    forkJoin({ schedule: this.service.getSchedule(), profiles: this.service.get() }).pipe(takeUntil(this.destroy$), finalize(() => this.busy = false)).subscribe({ next: result => {
      this.schedule = result.schedule; this.slots = result.profiles.pools; this.dirty = false; this.error = '';
    }, error: () => this.error = 'Pool schedule unavailable. Check that the application and web interface versions match.' });
  }
  offsetLabel(minutes: number) { const absolute = Math.abs(minutes); return `UTC${minutes < 0 ? '−' : '+'}${String(Math.floor(absolute / 60)).padStart(2, '0')}:${String(absolute % 60).padStart(2, '0')}`; }
  time(minutes: number) { return `${String(Math.floor(minutes / 60)).padStart(2, '0')}:${String(minutes % 60).padStart(2, '0')}`; }
  setTime(event: PoolEvent, text: string) {
    if (!/^\d{2}:\d{2}$/.test(text)) event.timeMinutes = -1;
    else { const [hours, minutes] = text.split(':').map(Number); event.timeMinutes = hours < 24 && minutes < 60 ? hours * 60 + minutes : -1; }
    this.dirty = true;
  }
  daySelected(event: PoolEvent, day: number) { return !!(event.dayMask & (1 << day)); }
  toggleDay(event: PoolEvent, day: number) { event.dayMask ^= 1 << day; this.dirty = true; }
  add() {
    const slot = this.slots.find(slot => slot.configured);
    if (!this.schedule || this.schedule.events.length >= 16 || !slot) return;
    this.schedule.events.push({ enabled: true, dayMask: 127, timeMinutes: 720, slot: slot.slot }); this.dirty = true;
  }
  remove(index: number) { this.schedule?.events.splice(index, 1); this.dirty = true; }
  get canAdd() { return !!this.schedule && this.schedule.events.length < 16 && this.slots.some(slot => slot.configured); }
  get valid() {
    return !!this.schedule && this.schedule.events.length <= 16 && Number.isInteger(this.schedule.utcOffsetMinutes) && this.offsets.includes(this.schedule.utcOffsetMinutes) && (!this.schedule.enabled || this.schedule.events.some(event => event.enabled)) && this.schedule.events.every(event => Number.isInteger(event.dayMask) && Number.isInteger(event.timeMinutes) && Number.isInteger(event.slot) && event.dayMask >= 1 && event.dayMask <= 127 && event.timeMinutes >= 0 && event.timeMinutes <= 1439 && this.slots.some(slot => slot.slot === event.slot && slot.configured));
  }
  get selectedName() { return this.slots.find(slot => slot.slot === this.schedule?.selectedSlot)?.name || 'None'; }
  save() {
    if (!this.schedule || !this.valid || this.busy) return;
    this.busy = true; this.error = '';
    this.service.saveSchedule(this.schedule).pipe(takeUntil(this.destroy$), finalize(() => this.busy = false)).subscribe({ next: schedule => { this.schedule = schedule; this.dirty = false; this.message = 'Pool schedule saved on this miner.'; }, error: err => { this.error = err.error?.error === 'invalid-profile' || err.status === 400 || err.status === 409 ? 'Check event times, days and existing profile slots. Previous schedule retained.' : 'Schedule commit failed. Previous schedule retained.'; } });
  }
}
