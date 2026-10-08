import { HttpEventType } from '@angular/common/http';
import { Component, OnDestroy, OnInit, ViewChild } from '@angular/core';
import { ActivatedRoute, Router } from '@angular/router';
import { Observable, Subscription } from 'rxjs';
import { shareReplay, switchMap } from 'rxjs/operators';
import { NbToastrService } from '@nebular/theme';
import { LoadingService } from '../../services/loading.service';
import { SystemService, UpdateDeviceIdentity } from '../../services/system.service';
import { OtpAuthService, EnsureOtpResult } from '../../services/otp-auth.service';
import { EditComponent } from '../edit/edit.component';
import { combineLatest } from 'rxjs';
import { getAppVersion } from '../../app.module';
import { QaxeRelease, QaxeUpdateCheck, QaxeUpdateService } from '../../services/qaxe-update.service';

@Component({
  selector: 'app-settings',
  templateUrl: './settings.component.html',
  styleUrls: ['./settings.component.scss']
})
export class SettingsComponent implements OnInit, OnDestroy {
  @ViewChild(EditComponent) editor?: EditComponent;
  public readonly sections = [{id:'performance',label:'Performance'},{id:'cooling',label:'Cooling'},{id:'display',label:'Display'},{id:'advanced',label:'Integrations'}];
  public page='controls';
  public get title():string {return ({controls:'Miner controls',pool:'Pool routing',scheduler:'Scheduler',network:'Network',update:'Updates'} as Record<string,string>)[this.page] || 'Miner controls';}
  public canLeave():boolean {return !this.editor?.hasUnsavedChanges || window.confirm('Leave this page and discard its unsaved changes?');}
  public section = 'performance';
  public deviceModel = '';
  public currentVersion = '';
  public currentWebVersion = getAppVersion();
  public expectedFileName = '';
  public identityLoading = true;
  public identityError = '';
  public selectedFirmwareFile: File | null = null;
  public selectedWebsiteFile: File | null = null;
  public firmwareUpdateProgress = 0;
  public websiteUpdateProgress = 0;
  public isFirmwareUploading = false;
  public isWebsiteUploading = false;
  public firmwareRestartPending = false;
  public releaseCheckState: 'idle' | 'checking' | 'error' | QaxeUpdateCheck['status'] = 'idle';
  public availableRelease: QaxeRelease | null = null;
  public info$: Observable<UpdateDeviceIdentity>;
  private subscriptions = new Subscription();
  private releaseSubscription?: Subscription;

  constructor(
    private systemService: SystemService,
    private toastrService: NbToastrService,
    private loadingService: LoadingService,
    private otpAuth: OtpAuthService,
    private route: ActivatedRoute,
    private router: Router,
    private qaxeUpdates: QaxeUpdateService,
  ) {
    this.info$ = this.systemService.getUpdateInfo().pipe(shareReplay({ refCount: true, bufferSize: 1 }));
  }

  ngOnInit(): void {
    this.subscriptions.add(combineLatest([this.route.data,this.route.queryParamMap]).subscribe(([data,params])=>{
      const legacy=params.get('section');
      if(data['section']==='controls' && legacy && ['pool','scheduler','network','update','security'].includes(legacy)) {
        this.router.navigate(['/pages',legacy],{replaceUrl:true});return;
      }
      this.page=data['section'] || 'controls';
      this.section=this.page==='controls' ? (this.sections.some(item=>item.id===legacy)?legacy!:'performance') : this.page;
    }));
    this.loadDeviceIdentity();
  }

  loadDeviceIdentity(): void {
    if (this.isFirmwareUploading || this.isWebsiteUploading || this.firmwareRestartPending) return;
    this.releaseSubscription?.unsubscribe();
    this.releaseCheckState = 'idle';
    this.availableRelease = null;
    this.identityLoading = true;
    this.identityError = '';
    this.deviceModel = this.currentVersion = this.expectedFileName = '';
    this.subscriptions.add(this.info$.pipe(this.loadingService.lockUIUntilComplete()).subscribe({
      next: info => {
        this.deviceModel = info.deviceModel;
        this.currentVersion = info.version;
        this.expectedFileName = `esp-miner-${info.deviceModel.replace(/γ/g, 'Gamma').replace(/\s+/g, '')}.bin`;
        this.identityLoading = false;
      },
      error: () => {
        this.identityLoading = false;
        this.identityError = 'Could not read the device model. Check the connection and retry.';
      },
    }));
  }

  ngOnDestroy(): void { this.subscriptions.unsubscribe(); }

  get releaseCheckMessage(): string {
    const version = this.availableRelease?.version || '';
    switch (this.releaseCheckState) {
      case 'checking': return 'Checking public QAxe releases…';
      case 'error': return 'Could not check GitHub releases. Check the connection and retry.';
      case 'available': return `${version} is available.`;
      case 'up-to-date': return 'Your application is up to date.';
      case 'newer-build': return 'Your application is newer than the published release.';
      case 'no-release': return 'No complete QAxe firmware pair is published.';
      case 'unsupported': return 'This release check supports NerdQAxe++ only.';
      default: return '';
    }
  }

  checkUpdates(): void {
    if (this.identityLoading || this.identityError || !this.deviceModel || this.releaseCheckState === 'checking'
      || this.isFirmwareUploading || this.isWebsiteUploading || this.firmwareRestartPending) return;
    this.releaseCheckState = 'checking';
    this.availableRelease = null;
    this.releaseSubscription = this.qaxeUpdates.check(this.deviceModel, this.currentVersion).subscribe({
      next: result => { this.releaseCheckState = result.status; this.availableRelease = result.release; },
      error: () => { this.releaseCheckState = 'error'; this.availableRelease = null; },
    });
    this.subscriptions.add(this.releaseSubscription);
  }

  selectSection(section: string): void {
    this.router.navigate([], { relativeTo: this.route, queryParams: { section }, queryParamsHandling: 'merge' });
  }

  get firmwareFileValid(): boolean {
    return !this.identityLoading && !this.identityError && !!this.expectedFileName
      && !!this.selectedFirmwareFile && this.selectedFirmwareFile.name === this.expectedFileName;
  }

  get websiteFileValid(): boolean {
    return !!this.selectedWebsiteFile && this.selectedWebsiteFile.name === 'www.bin';
  }

  onFirmwareFileSelected(event: Event): void {
    this.selectedFirmwareFile = (event.target as HTMLInputElement).files?.[0] ?? null;
  }

  onWebsiteFileSelected(event: Event): void {
    this.selectedWebsiteFile = (event.target as HTMLInputElement).files?.[0] ?? null;
  }

  uploadFirmwareFile(): void {
    if (!this.firmwareFileValid || !this.selectedFirmwareFile || this.isFirmwareUploading || this.isWebsiteUploading || this.firmwareRestartPending) return;
    const file = this.selectedFirmwareFile;
    this.otpAuth.ensureOtp$('', 'Authentication', 'Enter the code to update firmware.').pipe(
      switchMap(({ totp }: EnsureOtpResult) => {
        this.isFirmwareUploading = true;
        this.firmwareUpdateProgress = 0;
        return this.systemService.performOTAUpdate(file, totp).pipe(this.loadingService.lockUIUntilComplete());
      })
    ).subscribe({
      next: event => {
        if (event?.type === HttpEventType.UploadProgress && event.total) {
          this.firmwareUpdateProgress = Math.round(100 * event.loaded / event.total);
        } else if (event?.type === HttpEventType.Response) {
          this.firmwareUpdateProgress = 100;
          this.firmwareRestartPending = true;
          this.selectedFirmwareFile = null;
          this.toastrService.success('Application uploaded. The miner is restarting.', 'Firmware');
        }
      },
      error: error => {
        this.isFirmwareUploading = false;
        this.firmwareUpdateProgress = 0;
        this.toastrService.danger(error.message || 'Upload failed.', 'Firmware');
      },
      complete: () => { this.isFirmwareUploading = false; }
    });
  }

  uploadWebsiteFile(): void {
    if (!this.websiteFileValid || !this.selectedWebsiteFile || this.isFirmwareUploading || this.isWebsiteUploading || this.firmwareRestartPending) return;
    const file = this.selectedWebsiteFile;
    this.otpAuth.ensureOtp$('', 'Authentication', 'Enter the code to update the web interface.').pipe(
      switchMap(({ totp }: EnsureOtpResult) => {
        this.isWebsiteUploading = true;
        this.websiteUpdateProgress = 0;
        return this.systemService.performWWWOTAUpdate(file, totp).pipe(this.loadingService.lockUIUntilComplete());
      })
    ).subscribe({
      next: event => {
        if (event?.type === HttpEventType.UploadProgress && event.total) {
          this.websiteUpdateProgress = Math.round(100 * event.loaded / event.total);
        } else if (event?.type === HttpEventType.Response) {
          this.websiteUpdateProgress = 100;
          this.selectedWebsiteFile = null;
          this.toastrService.success('Web interface uploaded. Reloading…', 'Firmware');
          setTimeout(() => window.location.reload(), 1000);
        }
      },
      error: error => {
        this.isWebsiteUploading = false;
        this.websiteUpdateProgress = 0;
        this.toastrService.danger(error.message || 'Upload failed.', 'Web interface');
      },
      complete: () => { this.isWebsiteUploading = false; }
    });
  }
}

export function leaveSettings(component: SettingsComponent): boolean {return component.canLeave();}
