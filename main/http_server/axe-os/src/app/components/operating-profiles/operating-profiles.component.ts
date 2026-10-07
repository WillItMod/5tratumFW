import { Component, EventEmitter, Input, OnDestroy, OnInit, Output } from '@angular/core';
import { Subject, finalize, takeUntil, concatMap, tap } from 'rxjs';
import { OperatingProfilesService, Profiles, TuningSlot, PoolSlot } from 'src/app/services/operating-profiles.service';
import { LiveDataService } from 'src/app/services/live-data.service';
@Component({ selector: 'app-operating-profiles', templateUrl: './operating-profiles.component.html', styleUrls: ['./operating-profiles.component.scss'] })
export class OperatingProfilesComponent implements OnInit, OnDestroy {
  @Input() type: 'tuning' | 'pool' = 'tuning';
  @Input() uri = '';
  @Output() applied = new EventEmitter<{ frequencyMHz?: number; coreVoltageMv?: number; restartRequired: boolean }>();
  data?: Profiles;
  names: string[] = Array(10).fill('');
  manual = false;
  frequencyMHz = 0;
  coreVoltageMv = 0;
  actualFrequencyMHz?: number;
  poolTarget: 'primary' | 'fallback' = 'primary';
  captureCurrent: 'primary' | 'fallback' = 'primary';
  busy = false;
  error = '';
  message = '';
  review?: { body: Record<string, unknown>; label: string; frequencyMHz?: number; coreVoltageMv?: number; endpoint?: string };
  private destroy$ = new Subject<void>();
  constructor(private service: OperatingProfilesService, private live: LiveDataService) {}
  ngOnInit() {
    this.load();
    if (!this.uri) this.live.info$.pipe(takeUntil(this.destroy$)).subscribe(info => { this.actualFrequencyMHz = info.actualFrequency; });
  }
  ngOnDestroy() { this.destroy$.next(); this.destroy$.complete(); }
  load() {
    this.busy = true;
    this.service.get(this.uri).pipe(takeUntil(this.destroy$), finalize(() => this.busy = false)).subscribe({ next: data => {
      this.data = data; this.names = this.slots.map(slot => slot.name || '');
      this.frequencyMHz = data.current.frequencyMHz; this.coreVoltageMv = data.current.coreVoltageMv;
      this.error = '';
    }, error: () => this.error = 'Profiles unavailable. The application and web interface must support the same profile API.' });
  }
  get slots(): (TuningSlot | PoolSlot)[] { return this.data ? this.type === 'tuning' ? this.data.tuning : this.data.pools : []; }
  get pointChanged() { return !!this.data && (this.frequencyMHz !== this.data.current.frequencyMHz || this.coreVoltageMv !== this.data.current.coreVoltageMv); }
  get pointValid() {
    if (!this.data || !Number.isFinite(this.frequencyMHz) || !Number.isInteger(this.coreVoltageMv)) return false;
    const { limits, current } = this.data;
    return (this.frequencyMHz === current.frequencyMHz || (this.frequencyMHz >= limits.frequency.min && this.frequencyMHz <= limits.frequency.max && Math.abs(this.frequencyMHz / limits.frequency.step - Math.round(this.frequencyMHz / limits.frequency.step)) < 0.00001)) &&
      (this.coreVoltageMv === current.coreVoltageMv || (this.coreVoltageMv >= limits.coreVoltage.min && this.coreVoltageMv <= limits.coreVoltage.max));
  }
  step(field: 'frequencyMHz' | 'coreVoltageMv', direction: number) {
    if (!this.data) return;
    const limits = field === 'frequencyMHz' ? this.data.limits.frequency : this.data.limits.coreVoltage;
    this[field] = Number(Math.max(limits.min, Math.min(limits.max, this[field] + direction * limits.step)).toFixed(3));
    this.review = undefined;
  }
  save(slot: number) {
    const name = this.names[slot]?.trim(); if (!name || name.length > 32 || (this.type === 'tuning' && !this.pointValid)) return;
    const body = this.type === 'pool' ? { type: this.type, slot, name, captureCurrent: this.captureCurrent } : { type: this.type, slot, name, frequencyMHz: this.frequencyMHz, coreVoltageMv: this.coreVoltageMv };
    this.execute(() => this.service.save(body, this.uri), () => { this.message = `Saved ${name} on this miner.`; });
  }
  clear(slot: number) {
    this.execute(() => this.service.save({ type: this.type, slot, clear: true }, this.uri), () => { this.message = 'Profile cleared.'; });
  }
  reviewManual() {
    if (!this.pointChanged || !this.pointValid) return;
    this.review = { body: { type: 'tuning', frequencyMHz: this.frequencyMHz, coreVoltageMv: this.coreVoltageMv }, label: 'Manual operating point', frequencyMHz: this.frequencyMHz, coreVoltageMv: this.coreVoltageMv };
  }
  reviewSlot(slot: TuningSlot | PoolSlot) {
    if (!slot.configured) return;
    this.review = this.type === 'tuning' ? { body: { type: 'tuning', slot: slot.slot }, label: slot.name!, frequencyMHz: (slot as TuningSlot).frequencyMHz, coreVoltageMv: (slot as TuningSlot).coreVoltageMv } :
      { body: { type: 'pool', slot: slot.slot, poolTarget: this.poolTarget }, label: `${slot.name} → ${this.poolTarget}`, endpoint: `${(slot as PoolSlot).host}:${(slot as PoolSlot).port}` };
  }
  applyPoolSlot(slot: TuningSlot | PoolSlot) {
    if (this.type !== 'pool' || !slot.configured || this.busy) return;
    this.reviewSlot(slot);
    this.apply();
  }
  apply() {
    const review = this.review; if (!review) return;
    this.execute(() => this.service.apply(review.body, this.uri), result => {
      this.review = undefined; this.message = result.restartRequired ? 'Saved. Restart the miner to apply this route.' : 'Settings committed. The miner is applying the change.';
      this.applied.emit({ frequencyMHz: review.frequencyMHz, coreVoltageMv: review.coreVoltageMv, restartRequired: result.restartRequired });
    });
  }
  private execute(request: () => any, success: (result: any) => void) {
    if (this.busy) return; this.busy = true; this.error = ''; this.review = undefined;
    request().pipe(tap(success), concatMap(() => this.service.get(this.uri)), takeUntil(this.destroy$), finalize(() => this.busy = false)).subscribe({ next: (data: Profiles) => { this.data = data; this.names = this.slots.map(slot => slot.name || ''); this.frequencyMHz = data.current.frequencyMHz; this.coreVoltageMv = data.current.coreVoltageMv; }, error: (err: any) => {
      const code = err.error?.error;
      this.error = code === 'profile-in-use' ? 'This profile is used by the pool schedule. Remove those events before editing or clearing it.' : code === 'storage-unavailable' ? 'Storage commit failed. Previous settings retained.' : code === 'invalid-profile' ? 'Check the profile values and the model’s software request bounds.' : code === 'empty-slot' ? 'This slot is empty.' : 'Could not commit this change. Previous settings retained.';
    } });
  }
}
