import { Component, Input, OnInit, OnDestroy } from '@angular/core';
import { FormBuilder, FormGroup, Validators, ValidatorFn, ValidationErrors, AbstractControl, FormControl } from '@angular/forms';
import { ToastrService } from 'ngx-toastr';
import { LoadingService } from 'src/app/services/loading.service';
import { SystemApiService } from 'src/app/services/system.service';
import { LiveDataService } from 'src/app/services/live-data.service';
import { Subject, takeUntil, finalize, timer, exhaustMap, catchError, EMPTY } from 'rxjs';
import { SystemInfo } from 'src/app/generated/models';
import { MuxConnectionInput, MuxConnectionMonitor, MuxConnectionPatch, MuxObservation, buildMuxConnectionPatch, validMuxHost } from 'src/app/services/mux-connection.service';

type PoolType = 'stratum' | 'fallbackStratum';
type ConnectionMode = 'direct' | 'mux';

interface ITlsOption {
  value: number;
  label: string;
}

interface IProtocolOption {
  value: 'SV1' | 'SV2';
  label: string;
}

interface IChannelOption {
  value: 'standard' | 'extended';
  label: string;
}

@Component({
  selector: 'app-pool',
  templateUrl: './pool.component.html',
  styleUrls: ['./pool.component.scss']
})
export class PoolComponent implements OnInit, OnDestroy {
  public form!: FormGroup;
  public savedChanges: boolean = false;
  public connectionMode: ConnectionMode = 'direct';
  public reviewedPatch?: MuxConnectionPatch;
  public busy = false;
  public restartPending = false;
  public restartRequested = false;
  public restartObserved = false;
  public error = '';
  public info?: SystemInfo;
  private observedAt = 0;
  private now = Date.now();
  private destroy$ = new Subject<void>();
  private monitor = new MuxConnectionMonitor();
  private freshnessTimer?: ReturnType<typeof setInterval>;
  private restartBaselineUptime = 0;
  private restartRequestedAt = 0;
  private confirmedMuxConnection?: MuxConnectionInput;

  public readonly DEFAULT_BITCOIN_ADDRESS = 'bc1qnp980s5fpp8l94p5cvttmtdqy8rvrq74qly2yrfmzkdsntqzlc5qkc4rkq';

  public pools: PoolType[] = ['stratum', 'fallbackStratum'];
  public selectedPool: PoolType = 'stratum';
  public showPassword = { 'stratum': false, 'fallbackStratum': false };
  public showAdvancedOptions = { 'stratum': false, 'fallbackStratum': false };

  public tlsOptions: ITlsOption[] = [
    { value: 0, label: 'No TLS' },
    { value: 1, label: 'TLS (System certificate)' },
    { value: 2, label: 'TLS (Custom CA certificate)' }
  ];

  public protocolOptions: IProtocolOption[] = [
    { value: 'SV1', label: 'Stratum V1' },
    { value: 'SV2', label: 'Stratum V2' }
  ];

  public sv2ChannelOptions: IChannelOption[] = [
    { value: 'extended', label: 'Extended Channels' },
    { value: 'standard', label: 'Standard Channels' }
  ];

  public asicModel: string = '';

  @Input() uri = '';

  constructor(
    private fb: FormBuilder,
    private systemService: SystemApiService,
    private liveDataService: LiveDataService,
    private toastr: ToastrService,
    private loadingService: LoadingService
  ) { }

  ngOnInit(): void {
    const readings$ = this.uri ? timer(0, 5000).pipe(
      exhaustMap(() => this.systemService.getInfo(this.uri).pipe(catchError(() => EMPTY)))
    ) : this.liveDataService.info$;
    readings$.pipe(takeUntil(this.destroy$)).subscribe(info => {
        this.receiveInfo(info, this.uri ? Date.now() : this.liveDataService.lastUpdateAt);
        // A telemetry update must never replace an edited connection draft.
        if (this.form) return;
        this.asicModel = info.ASICModel || '';
        this.form = this.fb.group({
          stratumURL: [info.stratumURL, [
            Validators.required,
            Validators.pattern(/^(?!.*stratum\+tcp:\/\/)(?!.*:[1-9]\d{0,4}$).*$/),
          ]],
          stratumPort: [info.stratumPort, [
            Validators.required,
            Validators.pattern(/^[^:]*$/),
            Validators.min(0),
            Validators.max(65535)
          ]],
          stratumProtocol: [info.stratumProtocol || 'SV1'],
          stratumV2AuthorityPubkey: [info.stratumV2AuthorityPubkey || '', [this.base58Validator()]],
          stratumExtranonceSubscribe: [info.stratumExtranonceSubscribe == true, [Validators.required]],
          stratumSuggestedDifficulty: [info.stratumSuggestedDifficulty, [Validators.required]],
          stratumUser: [info.stratumUser, [Validators.required]],
          stratumPassword: ['*****', [Validators.required]],
          stratumTLS: [info.stratumTLS || 0],
          stratumCert: [info.stratumCert],
          stratumDecodeCoinbase: [info.stratumDecodeCoinbase == true, [Validators.required]],
          fallbackStratumURL: [info.fallbackStratumURL, [
            Validators.pattern(/^(?!.*stratum\+tcp:\/\/)(?!.*:[1-9]\d{0,4}$).*$/),
          ]],
          fallbackStratumPort: [info.fallbackStratumPort, [
            Validators.required,
            Validators.pattern(/^[^:]*$/),
            Validators.min(0),
            Validators.max(65535)
          ]],
          fallbackStratumExtranonceSubscribe: [info.fallbackStratumExtranonceSubscribe == true, [Validators.required]],
          fallbackStratumSuggestedDifficulty: [info.fallbackStratumSuggestedDifficulty, [Validators.required]],
          fallbackStratumTLS: [info.fallbackStratumTLS || 0],
          fallbackStratumCert: [info.fallbackStratumCert],
          fallbackStratumDecodeCoinbase: [info.fallbackStratumDecodeCoinbase == true, [Validators.required]],
          fallbackStratumUser: [info.fallbackStratumUser, [Validators.required]],
          fallbackStratumPassword: ['*****', [Validators.required]],
          fallbackStratumProtocol: [info.fallbackStratumProtocol || 'SV1'],
          fallbackStratumV2AuthorityPubkey: [info.fallbackStratumV2AuthorityPubkey || '', [this.base58Validator()]],
          stratumV2ChannelType: [info.stratumV2ChannelType || 'standard'],
          fallbackStratumV2ChannelType: [info.fallbackStratumV2ChannelType || 'standard']
        });

        for (const pool of this.pools) {
          this.form.get(pool + 'TLS')!.valueChanges.pipe(takeUntil(this.destroy$)).subscribe(() => this.updateAdvancedValidation(pool));
          this.form.get(pool + 'Protocol')!.valueChanges.pipe(takeUntil(this.destroy$)).subscribe(() => this.updateAdvancedValidation(pool));
          this.updateAdvancedValidation(pool);
        }
        this.form.valueChanges.pipe(takeUntil(this.destroy$)).subscribe(() => {
          this.reviewedPatch = undefined;
          this.error = '';
        });
      });
    this.freshnessTimer = setInterval(() => this.now = Date.now(), 1000);
  }

  ngOnDestroy(): void {
    clearInterval(this.freshnessTimer);
    this.destroy$.next();
    this.destroy$.complete();
  }

  selectConnectionMode(mode: ConnectionMode): void {
    if (this.busy || this.connectionMode === mode) return;
    this.connectionMode = mode;
    if (mode === 'mux') this.selectedPool = 'stratum';
    this.reviewedPatch = undefined;
    this.error = '';
  }

  get editablePools(): PoolType[] { return this.connectionMode === 'mux' ? ['stratum'] : this.pools; }

  get muxConnection(): MuxConnectionInput {
    const value = this.form?.getRawValue() || {};
    return { host: String(value.stratumURL || ''), port: Number(value.stratumPort), worker: String(value.stratumUser || ''), password: value.stratumPassword };
  }

  get muxObservation(): MuxObservation | null {
    return this.info ? this.monitor.evaluate(this.info, this.confirmedMuxConnection || this.muxConnection,
      this.observedAt, this.now, this.restartPending, this.restartRequested) : null;
  }

  get telemetryFresh(): boolean { return !!this.info && this.observedAt > 0 && this.now - this.observedAt <= 15000; }

  get deviceState(): string {
    if (!this.info) return 'Waiting for device readings';
    if (!this.telemetryFresh) return 'Device telemetry stale';
    if (this.info.power_fault || this.info.hardware_fault || this.info.overheat_mode) return 'Miner needs attention';
    if (this.restartPending) return this.restartRequested ? 'Waiting for miner restart' : 'Settings saved · restart pending';
    if (this.connectionMode === 'mux') return this.muxObservation!.label;
    if (this.info.miningPaused) return 'Mining paused';
    return this.info.isUsingFallbackStratum ? 'Fallback route reported' : 'Primary route reported';
  }

  get savedPrimaryEndpoint(): string { return this.info?.stratumURL ? `${this.info.stratumURL}:${this.info.stratumPort}` : 'Unavailable'; }
  get savedFallbackEndpoint(): string { return this.info?.fallbackStratumURL ? `${this.info.fallbackStratumURL}:${this.info.fallbackStratumPort}` : 'Not configured'; }
  get fallbackDraftDirty(): boolean { return !!this.form && Object.entries(this.form.controls).some(([key, control]) => key.startsWith('fallbackStratum') && control.dirty); }
  get fallbackUsesFactoryAddress(): boolean { return !!this.info?.fallbackStratumUser?.includes(this.DEFAULT_BITCOIN_ADDRESS); }
  get muxConsoleUrl(): string | null { const host = this.muxConnection.host.trim(); return validMuxHost(host) ? `http://${host}:13050` : null; }

  receiveInfo(info: SystemInfo, receivedAt: number): void {
    if (receivedAt < this.observedAt) return;
    this.now = Date.now();
    this.info = info;
    this.observedAt = receivedAt;
    if (this.restartRequested && receivedAt > this.restartRequestedAt && Number.isFinite(info.uptimeSeconds)
      && info.uptimeSeconds < this.restartBaselineUptime) {
      this.restartPending = false;
      this.restartRequested = false;
      this.restartObserved = true;
      this.monitor.reset();
    }
    this.monitor.observe(info);
  }

  reviewMuxConnection(): void {
    if (this.busy || this.connectionMode !== 'mux' || !this.form) return;
    this.form.markAllAsTouched();
    try {
      this.reviewedPatch = buildMuxConnectionPatch(this.muxConnection);
      this.error = '';
    } catch (error) {
      this.reviewedPatch = undefined;
      this.error = error instanceof Error ? error.message : 'Check the MUX connection fields.';
    }
  }

  public updateSystem(): void {
    if (this.busy || !this.form) return;
    const isMux = this.connectionMode === 'mux';
    if (isMux && !this.reviewedPatch) return;
    if (!isMux && (!this.form.dirty || this.form.invalid)) return;
    const submittedValues = this.form.getRawValue();
    const patch = isMux ? { ...this.reviewedPatch! } : { ...submittedValues };
    if (patch.stratumPassword === '*****') delete patch.stratumPassword;
    if ('fallbackStratumPassword' in patch && patch.fallbackStratumPassword === '*****') delete patch.fallbackStratumPassword;
    const savedMuxTarget = isMux ? { ...this.muxConnection } : undefined;
    this.busy = true;
    this.error = '';
    this.systemService.updateSystem(this.uri, patch).pipe(takeUntil(this.destroy$), finalize(() => this.busy = false)).subscribe({
      next: () => {
        this.savedChanges = true;
        this.restartPending = true;
        this.restartRequested = false;
        this.restartObserved = false;
        this.confirmedMuxConnection = savedMuxTarget ? { host: savedMuxTarget.host, port: savedMuxTarget.port, worker: savedMuxTarget.worker } : undefined;
        // Confirm only the submitted fields. In particular a MUX save does not
        // save or discard a separate fallback draft.
        for (const [key, value] of Object.entries(patch)) {
          const control = this.form.get(key);
          if (control && control.value === submittedValues[key]) {
            control.setValue(key.endsWith('Password') ? '*****' : value, { emitEvent: false });
            control.markAsPristine();
          }
        }
        this.updateAdvancedValidation('stratum');
        this.updateAdvancedValidation('fallbackStratum');
        this.reviewedPatch = undefined;
        this.toastr.success(isMux ? 'MUX connection saved. Restart required.' : 'Pool settings saved. Restart required.');
      },
      error: () => {
        this.error = 'The device did not confirm saving this connection. Your draft has been retained.';
        this.toastr.error(this.error);
      }
    });
  }

  public restart(): void {
    if (!this.restartPending || this.busy || this.restartRequested || !this.telemetryFresh) return;
    this.busy = true;
    this.error = '';
    const baseline = this.info!.uptimeSeconds;
    this.systemService.restart(this.uri).pipe(takeUntil(this.destroy$), finalize(() => this.busy = false)).subscribe({
      next: () => {
        this.restartBaselineUptime = baseline;
        this.restartRequestedAt = Date.now();
        this.restartRequested = true;
        this.toastr.info('Restart requested. Waiting for a new boot in device telemetry.');
      },
      error: () => {
        this.error = 'The device did not confirm the restart request.';
        this.toastr.error(this.error);
      }
    });
  }

  private updateAdvancedValidation(pool: PoolType): void {
    const isV2 = this.form.get(pool + 'Protocol')!.value === 'SV2';
    const cert = this.form.get(pool + 'Cert')!;
    cert.setValidators(!isV2 && this.form.get(pool + 'TLS')!.value === 2 ? [Validators.required, this.pemCertificateValidator()] : []);
    cert.updateValueAndValidity({ emitEvent: false });
    const authority = this.form.get(pool + 'V2AuthorityPubkey')!;
    authority.setValidators(isV2 ? [this.base58Validator()] : []);
    authority.updateValueAndValidity({ emitEvent: false });
  }

  private extractPort(url: string): { cleanUrl: string, port?: number } {
    const match = url.match(/:(\d{1,5})$/);
    if (match) {
      const port = parseInt(match[1], 10);
      return { cleanUrl: url.slice(0, match.index), port };
    }
    return { cleanUrl: url };
  }

  public onUrlChange(poolType: PoolType) {
    const urlControl = this.form.get(`${poolType}URL`);
    const portControl = this.form.get(`${poolType}Port`);
    const tlsControl = this.form.get(`${poolType}TLS`);
    if (!urlControl || !portControl || !tlsControl) return;

    if (this.connectionMode === 'mux') return;
    let urlValue = String(urlControl.value || '').trim();

    if (!urlValue) return;

    const prefixes = [
      { prefix: 'stratum+tcp://', tlsMode: false },
      { prefix: 'stratum+tls://', tlsMode: true },
      { prefix: 'stratum+ssl://', tlsMode: true }
    ] as const;

    let isTlsMode = Number(tlsControl.value) || 0;
    const matched = prefixes.find(({ prefix }) => urlValue.startsWith(prefix));
    if (matched) {
      urlValue = urlValue.slice(matched.prefix.length);
      isTlsMode = +matched.tlsMode;
    }

    const { cleanUrl, port } = this.extractPort(urlValue);

    if (port !== undefined) {
      portControl.setValue(port);
    }
    urlControl.setValue(cleanUrl);
    tlsControl.setValue(isTlsMode);
  }

  onCertFileSelected(event: Event, formControlName: string): void {
    const fileInput = event.target as HTMLInputElement;

    if (fileInput.files && fileInput.files.length > 0) {
      const file = fileInput.files[0];
      const reader = new FileReader();

      reader.onload = () => {
        const fileContent = reader.result as string;
        // Update the corresponding certificate field in the form
        this.form.get(formControlName)?.setValue(fileContent);
        this.form.get(formControlName)?.markAsDirty();

        // Reset file input so the same file can be selected again
        fileInput.value = '';
      };

      reader.onerror = () => {
        // Error handling when reading the certificate file
        this.toastr.error('Failed to read certificate file');
        fileInput.value = '';
      };

      // Read the file as text
      reader.readAsText(file);
    }
  }

  private pemCertificateValidator(): ValidatorFn {
    return (control: AbstractControl): ValidationErrors | null => {
      const value = control.value?.trim();
      if (!value) return null;

      const pemChainRegex =
        /^(?:-----BEGIN CERTIFICATE-----[\s\S]*?-----END CERTIFICATE-----\s*)+$/;

      return pemChainRegex.test(value) ? null : { invalidCertificate: true };
    };
  }

  private base58Validator(): ValidatorFn {
    return (control: AbstractControl): ValidationErrors | null => {
      const value = control.value?.trim();
      if (!value) return null;

      // Base58 alphabet (no 0, O, I, l)
      const base58Regex = /^[123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz]+$/;
      if (!base58Regex.test(value)) {
        return { invalidBase58: true };
      }

      // SV2 authority pubkeys are typically 51-52 characters
      if (value.length < 40 || value.length > 52) {
        return { invalidBase58Length: true };
      }

      return null;
    };
  }

  trackByFn(index: number, option: { value: string | number }): string | number {
    return option.value;
  }

  isUsingDefaultAddress(pool: PoolType): boolean {
    const userValue = this.form?.get(pool + 'User')?.value || '';
    return userValue.includes(this.DEFAULT_BITCOIN_ADDRESS);
  }

  isAnyPoolUsingDefaultAddress(): boolean {
    return this.pools.some(pool => this.isUsingDefaultAddress(pool));
  }

  isStratumV2Enabled(): boolean {
    return this.form?.get('stratumProtocol')?.value === 'SV2';
  }

  isFallbackStratumV2Enabled(): boolean {
    return this.form?.get('fallbackStratumProtocol')?.value === 'SV2';
  }

  isPoolV2Enabled(pool: PoolType): boolean {
    return pool === 'stratum' ? this.isStratumV2Enabled() : this.isFallbackStratumV2Enabled();
  }

  isStandardChannelDisabled(): boolean {
    return this.asicModel === 'BM1397';
  }

  isPoolV2Extended(pool: PoolType): boolean {
    if (!this.isPoolV2Enabled(pool)) return false;
    const key = pool === 'stratum' ? 'stratumV2ChannelType' : 'fallbackStratumV2ChannelType';
    return this.form?.get(key)?.value === 'extended';
  }
}
