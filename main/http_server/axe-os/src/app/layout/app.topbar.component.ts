import { Component, ElementRef, Input, ViewChild, OnInit, OnDestroy } from '@angular/core';
import { Observable, Subject, takeUntil } from 'rxjs';
import { ToastrService } from 'ngx-toastr';
import { SystemApiService } from 'src/app/services/system.service';
import { LiveDataService } from 'src/app/services/live-data.service';
import { LayoutService } from './service/app.layout.service';
import { SensitiveData } from 'src/app/services/sensitive-data.service';
import { DashboardEditService } from 'src/app/services/dashboard-edit.service';
import { SystemInfo as ISystemInfo } from 'src/app/generated/models';
import { MenuItem } from 'primeng/api';
import { MiningControlsService, MiningScheduleStatus } from 'src/app/services/mining-controls.service';

@Component({
  selector: 'app-topbar',
  templateUrl: './app.topbar.component.html'
})
export class AppTopBarComponent implements OnInit, OnDestroy {
  private destroy$ = new Subject<void>();

  public info$: Observable<ISystemInfo>;
  public sensitiveDataHidden: boolean = false;
  public isMiningPaused: boolean = false;
  public items!: MenuItem[];
  public miningRequestBusy = false;
  private miningStatus: MiningScheduleStatus | null = null;
  private miningStatusAt = 0;
  private scheduleUnsupported = false;
  private scheduleReadFailed = false;
  private reportedPause: boolean | null = null;
  private pendingPauseTarget: boolean | null = null;
  private pauseRequestedAt = 0;

  @Input() isAPMode: boolean = false;

  @ViewChild('menubutton') menuButton!: ElementRef;

  constructor(
    public layoutService: LayoutService,
    private systemService: SystemApiService,
    private liveDataService: LiveDataService,
    private toastr: ToastrService,
    private sensitiveData: SensitiveData,
    public dashboardEdit: DashboardEditService,
    private miningControls: MiningControlsService,
  ) {
    this.info$ = this.liveDataService.info$;
  }

  ngOnInit() {
    this.sensitiveData.hidden
      .pipe(takeUntil(this.destroy$))
      .subscribe((hidden: boolean) => {
        this.sensitiveDataHidden = hidden;
      });

    this.info$.pipe(takeUntil(this.destroy$)).subscribe((info: ISystemInfo) => {
      if (typeof info.miningPaused === 'boolean') {
        this.reportedPause = info.miningPaused;
        if (this.scheduleUnsupported) {
          this.isMiningPaused = info.miningPaused;
          if (!this.miningRequestBusy && this.pendingPauseTarget !== null && this.liveDataService.lastUpdateAt > this.pauseRequestedAt && info.miningPaused === this.pendingPauseTarget) this.pendingPauseTarget = null;
        }
      }
    });
    if (!this.isAPMode) this.miningControls.observe().pipe(takeUntil(this.destroy$)).subscribe(observation => {
      this.scheduleReadFailed = !!observation.error;
      if (!observation.response) return;
      this.scheduleUnsupported = !observation.response.supported;
      if (this.scheduleUnsupported) {
        if (this.reportedPause !== null) this.isMiningPaused = this.reportedPause;
        return;
      }
      if (!observation.response.status) return;
      this.miningStatus = observation.response.status;
      this.miningStatusAt = observation.receivedAt;
      this.isMiningPaused = this.miningStatus.appliedPaused;
      const stable = !this.miningStatus.transitionPending && this.miningStatus.requestedPaused === this.miningStatus.appliedPaused;
      if (!this.miningRequestBusy && this.pendingPauseTarget !== null && observation.receivedAt > this.pauseRequestedAt
        && (this.miningStatus.error || (stable && (this.miningStatus.appliedPaused === this.pendingPauseTarget || this.miningStatus.manualOverride === 'none')))) this.pendingPauseTarget = null;
    });
  }

  ngOnDestroy() {
    this.destroy$.next();
    this.destroy$.complete();
  }

  public toggleSensitiveData() {
    this.sensitiveData.toggle();
  }

  public toggleMiningPaused() {
    if (this.miningActionDisabled) return;
    const targetPaused = !this.isMiningPaused;
    const action = targetPaused
      ? this.systemService.pauseMining()
      : this.systemService.resumeMining();
    this.miningRequestBusy = true;
    this.pendingPauseTarget = targetPaused;
    this.pauseRequestedAt = Date.now();
    action.subscribe({
      next: () => {
        this.miningRequestBusy = false;
        this.pauseRequestedAt = Date.now();
        this.miningControls.refresh();
        this.toastr.info(targetPaused ? 'Pause requested. Waiting for device confirmation.' : 'Resume requested. Waiting for device confirmation.');
      },
      error: () => {
        this.miningRequestBusy = false;
        this.pendingPauseTarget = null;
        this.toastr.error('Failed to change mining state');
      }
    });
  }

  get miningActionDisabled(): boolean {
    if (this.miningRequestBusy || this.pendingPauseTarget !== null || this.scheduleReadFailed) return true;
    if (this.scheduleUnsupported) return this.reportedPause === null || Date.now() - this.liveDataService.lastUpdateAt >= 15000;
    return !this.miningStatus || Date.now() - this.miningStatusAt >= 15000 || this.miningStatus.transitionPending
      || this.miningStatus.requestedPaused !== this.miningStatus.appliedPaused;
  }

  get miningActionText(): string {
    if (this.pendingPauseTarget !== null) return this.pendingPauseTarget ? 'Pausing…' : 'Starting…';
    if (this.miningStatus?.transitionPending) return this.miningStatus.requestedPaused ? 'Pausing…' : 'Starting…';
    return this.isMiningPaused ? 'Resume' : 'Pause';
  }

  get miningActionDescription(): string {
    if (this.scheduleReadFailed) return 'Mining state unavailable. Waiting for the device.';
    if (this.pendingPauseTarget !== null) return 'Request acknowledged only after the device responds; applied state awaits fresh telemetry.';
    if (this.scheduleUnsupported) return 'Uses the device pause request. ASIC state confirmation requires 5tratumFW firmware.';
    if (!this.miningStatus || Date.now() - this.miningStatusAt >= 15000) return 'Waiting for fresh mining status.';
    if (this.miningStatus.error) return this.miningStatus.error;
    return this.miningStatus.appliedPaused ? 'Resume ASIC mining. Manual override lasts until the next schedule boundary.' : 'Pause ASIC mining and save power. The web interface stays available.';
  }

  public restart() {
    this.systemService.restart().subscribe({
      next: () => this.toastr.success('Device restarted'),
      error: () => this.toastr.error('Restart failed')
    });
  }
}
