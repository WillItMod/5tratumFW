import { Component, Input, OnDestroy, OnInit } from '@angular/core';
import { ToastrService } from 'ngx-toastr';
import { EMPTY, Subject, Subscription, catchError, exhaustMap, filter, finalize, forkJoin, switchMap, takeUntil, tap, throwError, timeout, timer } from 'rxjs';
import { OperatingProfilesService, Profiles, PoolEvent, TuningSlot } from '../../services/operating-profiles.service';
import { TuningSchedule, TuningScheduleReport } from 'src/models/ITuningSchedule';

@Component({
  selector: 'app-tuning-schedule',
  templateUrl: './tuning-schedule.component.html',
  styleUrls: ['./tuning-schedule.component.scss'],
})
export class TuningScheduleComponent implements OnInit, OnDestroy {
  @Input() uri = '';
  public schedule: TuningSchedule | null = null;
  public report: TuningScheduleReport | null = null;
  public profiles: Profiles | null = null;
  public loading = false;
  public busy = false;
  public readError = '';
  public saveError = '';
  public savedMessage = '';
  public readonly weekDays = [{label:'Sun',bit:1},{label:'Mon',bit:2},{label:'Tue',bit:4},{label:'Wed',bit:8},{label:'Thu',bit:16},{label:'Fri',bit:32},{label:'Sat',bit:64}];
  public readonly utcOffsets = Array.from({length:105}, (_, i) => (i - 48) * 15);
  private baseline = '';
  private submitted: TuningSchedule | null = null;
  private readEpoch = 0;
  private readonly destroyed = new Subject<void>();
  private statusWatch?: Subscription;

  constructor(private service: OperatingProfilesService, private toastr: ToastrService) {}

  ngOnInit(): void {
    this.load();
    this.statusWatch = timer(5000, 5000).pipe(filter(() => !!this.report && !this.busy && !this.loading),
      exhaustMap(() => {
        const epoch = this.readEpoch;
        return this.service.getTuningSchedule(this.uri).pipe(
        timeout(10000),
        tap({next: report => { if (epoch === this.readEpoch) this.acceptStatus(report); },
          error: () => { if (epoch === this.readEpoch) this.readError = 'Could not refresh tuning status. The last reported state is shown.'; }}),
        // Polling remains read-only, and a failed refresh must not stop later reads.
        catchError(() => EMPTY),
      ); }), takeUntil(this.destroyed)).subscribe();
  }

  ngOnDestroy(): void { this.destroyed.next(); this.destroyed.complete(); this.statusWatch?.unsubscribe(); }

  load(): void {
    if (this.busy || this.loading) return;
    this.readEpoch++;
    this.loading = true;
    forkJoin({report:this.service.getTuningSchedule(this.uri), profiles:this.service.get(this.uri)})
      .pipe(timeout(10000), takeUntil(this.destroyed), finalize(() => this.loading = false)).subscribe({
        next: ({report, profiles}) => {
          if (!this.validReport(report) || !this.validProfiles(profiles)) {
            this.readError = 'Tuning schedules require a matching 5tratumFW application and web interface.';
            return;
          }
          this.profiles = profiles;
          this.acceptStatus(report);
          if (!this.schedule) {
            this.schedule = this.configuration(report);
            this.baseline = JSON.stringify(this.schedule);
          }
          if (report.storageValid !== false && this.saveError && this.submitted && JSON.stringify(this.configuration(report)) === JSON.stringify(this.submitted)) {
            this.confirmSave(this.submitted);
          }
        },
        error: error => this.readError = [404,501].includes(error.status)
          ? 'Tuning schedules are unavailable in this application. Install its matching application and web update.'
          : 'Could not read the tuning schedule and saved profiles. Check the connection and retry.',
      });
  }

  private validReport(report: TuningScheduleReport): boolean {
    const validSlot = (slot:number|null) => slot === null || Number.isInteger(slot) && slot >= 0 && slot < 10;
    return report?.schemaVersion === 1 && report.supported === true && report.timezoneMode === 'fixed-utc-offset'
      && report.maxEvents === 16 && typeof report.enabled === 'boolean' && typeof report.clockValid === 'boolean'
      && Number.isInteger(report.utcOffsetMinutes) && this.utcOffsets.includes(report.utcOffsetMinutes)
      && Array.isArray(report.events) && report.events.length <= 16 && report.events.every(event => this.validEvent(event))
      && (!report.enabled || report.events.some(event => event.enabled))
      && !report.events.some((event,i) => event.enabled && report.events.slice(i + 1).some(other => other.enabled && event.timeMinutes === other.timeMinutes && (event.dayMask & other.dayMask)))
      && validSlot(report.selectedSlot) && validSlot(report.appliedSlot) && (report.error === null || typeof report.error === 'string')
      && (report.storageValid === undefined || typeof report.storageValid === 'boolean')
      && ['disabled','waiting-clock','waiting-runtime','paused','pending','applied','error'].includes(report.status);
  }

  private validEvent(event: PoolEvent): boolean {
    return !!event && typeof event.enabled === 'boolean' && Number.isInteger(event.dayMask) && event.dayMask >= 1 && event.dayMask <= 127
      && Number.isInteger(event.timeMinutes) && event.timeMinutes >= 0 && event.timeMinutes < 1440
      && Number.isInteger(event.slot) && event.slot >= 0 && event.slot < 10;
  }

  private validProfiles(profiles: Profiles): boolean {
    return profiles?.schemaVersion === 1 && Array.isArray(profiles.tuning) && profiles.tuning.length === 10
      && new Set(profiles.tuning.map(profile => profile?.slot)).size === 10
      && profiles.tuning.every(profile => !!profile && Number.isInteger(profile.slot) && profile.slot >= 0 && profile.slot < 10
        && typeof profile.configured === 'boolean' && (profile.name === null || typeof profile.name === 'string')
        && (!profile.configured || Number.isFinite(profile.frequencyMHz) && Number.isFinite(profile.coreVoltageMv)));
  }

  private acceptStatus(report: TuningScheduleReport): void {
    if (!this.validReport(report)) { this.readError = 'Tuning status could not be read from this application.'; return; }
    this.report = report;
    this.readError = '';
  }

  private configuration(schedule: TuningSchedule): TuningSchedule {
    return {schemaVersion:1,enabled:schedule.enabled,utcOffsetMinutes:schedule.utcOffsetMinutes,
      events:schedule.events.map(event => ({enabled:event.enabled,dayMask:event.dayMask,timeMinutes:event.timeMinutes,slot:event.slot}))};
  }

  get hasUnsavedChanges(): boolean { return !!this.schedule && (this.report?.storageValid === false || JSON.stringify(this.configuration(this.schedule)) !== this.baseline); }
  get configuredProfiles(): TuningSlot[] { return this.profiles?.tuning.filter(profile => profile.configured) || []; }

  get validationError(): string {
    const schedule = this.schedule;
    if (!schedule || !this.report || !this.profiles || !Array.isArray(schedule.events)) return 'Read the schedule and saved tuning profiles before saving.';
    if (typeof schedule.enabled !== 'boolean') return 'Choose whether the tuning schedule is enabled.';
    if (!Number.isInteger(schedule.utcOffsetMinutes) || !this.utcOffsets.includes(schedule.utcOffsetMinutes)) return 'Choose a valid time zone offset.';
    if (schedule.events.length > 16) return 'Use no more than 16 time points.';
    if (schedule.enabled && !schedule.events.some(event => event?.enabled)) return 'Add and enable at least one time point.';
    for (const event of schedule.events) {
      if (!event || typeof event.enabled !== 'boolean') return 'Choose whether every time point is enabled.';
      if (!Number.isInteger(event.dayMask) || event.dayMask < 1 || event.dayMask > 127) return 'Choose at least one day for every time point.';
      if (!Number.isInteger(event.timeMinutes) || event.timeMinutes < 0 || event.timeMinutes >= 1440) return 'Choose a valid time for every time point.';
      if (!Number.isInteger(event.slot) || !this.configuredProfiles.some(profile => profile.slot === event.slot)) return 'Choose a saved tuning profile for every time point.';
    }
    for (let i = 0; i < schedule.events.length; i++) {
      const event = schedule.events[i];
      if (event.enabled && schedule.events.slice(i + 1).some(other => other.enabled && other.timeMinutes === event.timeMinutes && (other.dayMask & event.dayMask))) {
        return 'Enabled time points cannot share the same time on the same day.';
      }
    }
    return '';
  }

  addEvent(): void {
    if (!this.schedule || this.busy || this.schedule.events.length >= 16 || !this.configuredProfiles.length) return;
    this.schedule.events.push({enabled:true,dayMask:127,timeMinutes:540,slot:this.configuredProfiles[0].slot});
  }
  removeEvent(index: number): void { if (!this.busy) this.schedule?.events.splice(index,1); }
  hasDay(event: PoolEvent, bit: number): boolean { return (event.dayMask & bit) !== 0; }
  toggleDay(event: PoolEvent, bit: number): void { if (!this.busy) event.dayMask ^= bit; }
  timeText(minutes: number): string { return Number.isInteger(minutes) && minutes >= 0 && minutes < 1440 ? `${String(Math.floor(minutes / 60)).padStart(2,'0')}:${String(minutes % 60).padStart(2,'0')}` : ''; }
  changeTime(event: PoolEvent, value: string): void {
    if (this.busy) return;
    const match = /^(?:([01]\d|2[0-3])):([0-5]\d)$/.exec(value);
    event.timeMinutes = match ? Number(match[1]) * 60 + Number(match[2]) : NaN;
  }
  offsetText(minutes: number): string { return minutes === 0 ? 'UTC' : `UTC${minutes < 0 ? '−' : '+'}${this.timeText(Math.abs(minutes))}`; }
  profileLabel(slot: number | null): string {
    if (slot === null || slot === undefined) return 'None';
    const profile = this.profiles?.tuning.find(item => item.slot === slot);
    return `Slot ${slot + 1}${profile?.name ? ' · ' + profile.name : ''}`;
  }
  get statusText(): string {
    return ({disabled:'Schedule disabled', 'waiting-clock':'Waiting for network time', 'waiting-runtime':'Waiting for the miner to be ready',
      paused:'Mining is paused; tuning waits until mining resumes', pending:'Waiting to apply the selected tuning profile',
      applied:'Last scheduled tuning profile activated', error:'The selected tuning profile could not be activated'} as Record<string,string>)[this.report?.status || ''] || '';
  }

  private confirmSave(payload: TuningSchedule): void {
    this.baseline = JSON.stringify(payload);
    this.saveError = '';
    this.submitted = null;
    this.savedMessage = 'Tuning schedule saved on miner. Last activation status is shown below.';
    this.toastr.success(this.savedMessage,'Tuning schedule');
  }

  save(): void {
    if (this.busy || this.loading || this.readError || this.validationError || !this.hasUnsavedChanges) return;
    const payload = this.configuration(this.schedule!);
    this.busy = true;
    this.readEpoch++;
    this.saveError = this.savedMessage = '';
    this.submitted = payload;
    this.service.saveTuningSchedule(payload,this.uri).pipe(timeout(10000),
      switchMap(result => result?.ok === true ? this.service.getTuningSchedule(this.uri).pipe(timeout(10000)) : throwError(() => new Error('unconfirmed'))),
      takeUntil(this.destroyed), finalize(() => this.busy = false),
    ).subscribe({
      next: report => {
        if (!this.validReport(report) || report.storageValid === false || JSON.stringify(this.configuration(report)) !== JSON.stringify(payload)) {
          this.saveError = 'Save could not be confirmed. Your edits are retained. Check the miner before saving again.';
          return;
        }
        this.acceptStatus(report);
        this.confirmSave(payload);
      },
      error: () => this.saveError = 'Save could not be confirmed. Your edits are retained. Check the miner before saving again.',
    });
  }
}
