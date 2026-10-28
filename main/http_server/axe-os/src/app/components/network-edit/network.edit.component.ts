import { HttpClient, HttpErrorResponse } from '@angular/common/http';
import { Component, Input, OnDestroy, OnInit } from '@angular/core';
import { FormBuilder, FormGroup, Validators } from '@angular/forms';
import { ToastrService } from 'ngx-toastr';
import { finalize } from 'rxjs/operators';
import { BehaviorSubject, Observable, Subscription, timer } from 'rxjs';
import { DialogService } from 'src/app/services/dialog.service';
import { LoadingService } from 'src/app/services/loading.service';
import { LiveDataService } from 'src/app/services/live-data.service';
import { SystemApiService } from 'src/app/services/system.service';
import { WifiNetwork } from 'src/app/generated/models';
import { first } from 'rxjs/operators';

type EthernetStatus = 'disabled' | 'starting' | 'not-detected' | 'link-down' | 'connecting' | 'connected' | 'error';
interface EthernetReport { enabled: boolean; driverEnabled: boolean; status: EthernetStatus; ip: string; active: 'wifi' | 'ethernet' | 'none'; }

@Component({
  selector: 'app-network-edit',
  templateUrl: './network.edit.component.html',
  styleUrls: ['./network.edit.component.scss']
})
export class NetworkEditComponent implements OnInit, OnDestroy {
  private formSubject = new BehaviorSubject<FormGroup | null>(null);
  public form$: Observable<FormGroup | null> = this.formSubject.asObservable();

  public form!: FormGroup;
  public savedChanges: boolean = false;
  public scanning: boolean = false;
  public savedHostnameUrl: string | null = null;
  public ethernet: EthernetReport | null = null;
  public ethernetReadError = false;
  public busy = false;
  public restartRequested = false;
  private wifiRestartPending = false;
  private refreshingEthernet = false;
  private lastUptime: number | null = null;
  private restartUptime: number | null = null;
  private ethernetPendingAtRestart = false;
  private subscriptions = new Subscription();
  private mutationGeneration = 0;

  @Input() uri = '';

  constructor(
    private fb: FormBuilder,
    private systemService: SystemApiService,
    private liveDataService: LiveDataService,
    private toastr: ToastrService,
    private loadingService: LoadingService,
    private http: HttpClient,
    private dialogService: DialogService
  ) {

  }
  ngOnInit(): void {
    this.subscriptions.add((this.uri ? this.systemService.getInfo(this.uri) : this.liveDataService.info$)
      .pipe(first(), this.loadingService.lockUIUntilComplete())
      .subscribe(info => {
        this.form = this.fb.group({
          hostname: [info.hostname, [Validators.required]],
          ssid: [info.ssid, [Validators.required]],
          wifiPass: ['*****'],
        });
        this.observeEthernet(info);
        this.formSubject.next(this.form);
        if (this.ethernet) this.subscriptions.add(timer(5000, 5000).subscribe(() => this.refreshEthernet()));
      }));
  }


  public updateSystem() {
    if (!this.networkSettingsValid || this.busy || this.restartRequested) return;
    const ethernetControl = this.form.get('ethernetEnabled');
    const ethernetChanged = !!ethernetControl?.dirty;
    const wifiChanged = ['ssid', 'wifiPass'].some(key => this.form.get(key)?.dirty);
    const ethernetOnly = this.ethernetOnlyChange;
    const restartRequired = this.isRestartRequired;
    const draft = this.form.getRawValue();
    const form = ethernetOnly ? { ethernetEnabled: ethernetControl!.value } : { ...draft };
    if (!ethernetChanged) delete form.ethernetEnabled;
    if (!ethernetOnly) {
      form.wifiPass = form.wifiPass == null ? '' : form.wifiPass;
      if (form.wifiPass === '*****') delete form.wifiPass;
      if (form.ssid) form.ssid = form.ssid.trim();
    }
    this.busy = true;
    this.mutationGeneration++;
    this.subscriptions.add(this.systemService.updateSystem(this.uri, form)
      .pipe(this.loadingService.lockUIUntilComplete(), finalize(() => this.busy = false))
      .subscribe({
        next: response => {
          if (response?.status !== 'success') {
            this.toastr.error('Network settings could not be confirmed. Check the device before saving again.');
            return;
          }
          this.toastr.success('Saved network settings');
          this.savedHostnameUrl = this.hostnameUrl(response?.redirect?.url, form.hostname);
          if (ethernetChanged && this.ethernet) this.ethernet = { ...this.ethernet, enabled: form.ethernetEnabled };
          this.wifiRestartPending ||= wifiChanged;
          this.updateRestartPending();
          if (restartRequired && this.savedChanges) this.toastr.warning('You must restart this device after saving for changes to take effect.');
          for (const [key, value] of Object.entries(draft)) {
            const control = this.form.get(key);
            if (control && control.value === value) control.markAsPristine();
          }
        },
        error: (err: HttpErrorResponse) => this.toastr.error(`Could not save. ${err.message}`)
      }));
  }

  private get ethernetOnlyChange(): boolean {
    return !!this.form?.get('ethernetEnabled')?.dirty
      && ['hostname', 'ssid', 'wifiPass'].every(key => !this.form.get(key)?.dirty);
  }

  get networkSettingsValid(): boolean {
    if (!this.form) return false;
    const ethernetControl = this.form.get('ethernetEnabled');
    if (ethernetControl?.dirty && (!this.ethernet || this.ethernetReadError || typeof ethernetControl.value !== 'boolean')) return false;
    return this.ethernetOnlyChange || !this.form.invalid;
  }

  private readEthernet(info: any): EthernetReport | null {
    const statuses: EthernetStatus[] = ['disabled', 'starting', 'not-detected', 'link-down', 'connecting', 'connected', 'error'];
    if (!info || !['601', '602'].includes(info.boardVersion) || info.ASICModel !== 'BM1370'
      || (info.deviceModel !== undefined && info.deviceModel !== 'Gamma') || (info.asicCount !== undefined && info.asicCount !== 1)
      || typeof info.ethernetEnabled !== 'boolean' || typeof info.ethernetDriverEnabled !== 'boolean'
      || !statuses.includes(info.ethernetStatus) || !['wifi', 'ethernet', 'none'].includes(info.activeNetwork)
      || typeof info.ethernetIP !== 'string' || (info.ethernetIP !== '' && !this.validIPv4(info.ethernetIP))) return null;
    return { enabled: info.ethernetEnabled, driverEnabled: info.ethernetDriverEnabled,
      status: info.ethernetStatus, ip: info.ethernetIP, active: info.activeNetwork };
  }

  private validIPv4(value: string): boolean {
    return /^(?:0|[1-9]\d{0,2})(?:\.(?:0|[1-9]\d{0,2})){3}$/.test(value) && value.split('.').every(part => Number(part) <= 255);
  }

  private observeEthernet(info: any): void {
    const report = this.readEthernet(info);
    if (!report && this.ethernet) { this.ethernetReadError = true; return; }
    this.ethernet = report;
    this.ethernetReadError = false;
    if (!report) {
      this.form.removeControl('ethernetEnabled');
      this.updateRestartPending();
      return;
    }
    const current = this.form.get('ethernetEnabled');
    if (!current) this.form.addControl('ethernetEnabled', this.fb.control(report.enabled));
    else if (!current.dirty) current.setValue(report.enabled, { emitEvent: false });
    const uptime = typeof info.uptimeSeconds === 'number' && Number.isFinite(info.uptimeSeconds) && info.uptimeSeconds >= 0 ? info.uptimeSeconds : null;
    if (this.restartRequested && ((uptime !== null && this.restartUptime !== null && uptime < this.restartUptime)
      || (this.ethernetPendingAtRestart && report.driverEnabled === report.enabled))) {
      this.restartRequested = false;
      this.wifiRestartPending = false;
    }
    this.lastUptime = uptime;
    this.updateRestartPending();
  }

  private updateRestartPending(): void {
    this.savedChanges = this.wifiRestartPending || this.ethernetRestartPending;
  }

  public refreshEthernet(): void {
    if (!this.ethernet || this.refreshingEthernet || this.busy) return;
    this.refreshingEthernet = true;
    const generation = this.mutationGeneration;
    this.subscriptions.add(this.systemService.getInfo(this.uri).pipe(first(), finalize(() => this.refreshingEthernet = false)).subscribe({
      next: info => { if (generation === this.mutationGeneration) this.observeEthernet(info); },
      error: () => { if (generation === this.mutationGeneration) this.ethernetReadError = true; }
    }));
  }

  get ethernetRestartPending(): boolean { return !!this.ethernet && this.ethernet.enabled !== this.ethernet.driverEnabled; }
  get ethernetStatusLabel(): string {
    if (this.ethernetReadError) return 'Ethernet status unavailable';
    return this.ethernet ? { disabled: 'Disabled', starting: 'Starting', 'not-detected': 'Adapter not detected',
      'link-down': 'Cable disconnected', connecting: 'Getting an address', connected: 'Connected', error: 'Ethernet unavailable' }[this.ethernet.status] : '';
  }
  get activeNetworkLabel(): string { return this.ethernet ? { wifi: 'Wi-Fi', ethernet: 'Ethernet', none: 'No network connection' }[this.ethernet.active] : ''; }

  ngOnDestroy(): void { this.subscriptions.unsubscribe(); }

  private hostnameUrl(value: unknown, hostname: unknown): string | null {
    if (typeof value !== 'string' || typeof hostname !== 'string' || !/^[a-z\d](?:[a-z\d-]{0,61}[a-z\d])?$/i.test(hostname)) return null;
    try {
      const url = new URL(value);
      return url.protocol === 'http:' && url.hostname.toLowerCase() === `${hostname}.local`.toLowerCase()
        && !url.username && !url.password && !url.port && url.pathname === '/' && !url.search && !url.hash ? url.href : null;
    } catch { return null; }
  }

  showWifiPassword: boolean = false;
  toggleWifiPasswordVisibility() {
    this.showWifiPassword = !this.showWifiPassword;
  }

  public scanWifi() {
    this.scanning = true;
    this.http.get<{networks: WifiNetwork[]}>(`${this.uri}/api/system/wifi/scan`)
      .pipe(
        finalize(() => this.scanning = false)
      )
      .subscribe({
        next: (response) => {
          // Sort networks by signal strength (highest first)
          const networks = response.networks.sort((a, b) => b.rssi - a.rssi);

          // filter out poor Wi-Fi connections
          const poorNetworks = networks.filter(network => network.rssi >= -80);

          // Remove duplicate Network Names and show highest signal strength only
          const uniqueNetworks = poorNetworks.reduce((acc, network) => {
            if (!acc[network.ssid] || acc[network.ssid].rssi < network.rssi) {
              acc[network.ssid] = network;
            }
            return acc;
          }, {} as { [key: string]: WifiNetwork });

          // Convert the object back to an array
          const filteredNetworks = Object.values(uniqueNetworks);

          // Create dialog data
          const dialogData = filteredNetworks.map(n => ({
            label: n.ssid,
            rssi: n.rssi,
            value: n.ssid
          }));

          // Show dialog with network list
          this.dialogService.open('Select Wi-Fi Network', dialogData)
            .subscribe((selectedSsid: string) => {
              if (selectedSsid) {
                this.form.patchValue({ ssid: selectedSsid });
                this.form.get('ssid')?.markAsDirty();
              }
            });
        },
        error: (err) => {
          this.toastr.error('Failed to scan Wi-Fi networks');
        }
      });
  }

  public restart() {
    if (this.busy || this.restartRequested || !this.savedChanges) return;
    this.busy = true;
    this.mutationGeneration++;
    this.restartUptime = this.lastUptime;
    this.ethernetPendingAtRestart = this.ethernetRestartPending;
    this.subscriptions.add(this.systemService.restart(this.uri)
      .pipe(this.loadingService.lockUIUntilComplete(), finalize(() => this.busy = false))
      .subscribe({
        next: () => {
          this.toastr.success('Restart requested');
          if (this.ethernet) this.restartRequested = true;
          else { this.wifiRestartPending = false; this.savedChanges = false; }
        },
        error: (err: HttpErrorResponse) => this.toastr.error(`Could not restart. ${err.message}`)
      }));
  }

  get noRestartFields(): string[] {
    return [
      'hostname'
    ];
  }

  get isRestartRequired(): boolean {
    return Object.entries(this.form.controls)
      .some(([field, control]) => control.dirty && !this.noRestartFields.includes(field));
  }
}
