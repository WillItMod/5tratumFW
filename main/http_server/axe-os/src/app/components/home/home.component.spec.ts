// Match the production AppModule's Chart.js time-axis adapter.
import 'chartjs-adapter-moment';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { HomeComponent } from './home.component';
import { WorkDifficultyComponent } from '../work-difficulty/work-difficulty.component';
import { WorkDifficultyService } from '../../services/work-difficulty.service';
import { provideHttpClient } from '@angular/common/http';
import { provideToastr } from 'ngx-toastr';
import { ReactiveFormsModule, FormsModule } from '@angular/forms';
import { NoopAnimationsModule } from '@angular/platform-browser/animations';
import { Title } from '@angular/platform-browser';
import { provideRouter } from '@angular/router';
import { MessageModule } from 'primeng/message';
import { DropdownModule } from 'primeng/dropdown';
import { ChartModule } from 'primeng/chart';
import { ProgressBarModule } from 'primeng/progressbar';
import { TooltipModule } from 'primeng/tooltip';

import { HashSuffixPipe } from 'src/app/pipes/hash-suffix.pipe';
import { DiffSuffixPipe } from 'src/app/pipes/diff-suffix.pipe';
import { DateAgoPipe } from 'src/app/pipes/date-ago.pipe';
import { AddressPipe } from 'src/app/pipes/address.pipe';
import { SatsPipe } from 'src/app/pipes/sats.pipe';
import { ByteSuffixPipe } from 'src/app/pipes/byte-suffix.pipe';

import { TooltipTextIconComponent } from 'src/app/components/tooltip-text-icon/tooltip-text-icon.component';
import { TooltipIconComponent } from 'src/app/components/tooltip-icon/tooltip-icon.component';
import { ConfettiComponent } from 'src/app/components/confetti/confetti.component';
import { SnowflakesComponent } from 'src/app/components/snowflakes/snowflakes.component';

import { SystemApiService } from 'src/app/services/system.service';
import { ThemeService } from 'src/app/services/theme.service';
import { themeAccentColors } from 'src/app/services/theme-colors';
import { QuicklinkService } from 'src/app/services/quicklink.service';
import { LoadingService } from 'src/app/services/loading.service';
import { ShareRejectionExplanationService } from 'src/app/services/share-rejection-explanation.service';
import { LocalStorageService } from 'src/app/local-storage.service';
import { DashboardEditService } from 'src/app/services/dashboard-edit.service';
import { LayoutService } from 'src/app/layout/service/app.layout.service';
import { SystemInfo } from 'src/app/generated/models';
import { firstValueFrom, of } from 'rxjs';

describe('HomeComponent', () => {
  let component: HomeComponent;
  let fixture: ComponentFixture<HomeComponent>;

  beforeEach(() => {
    localStorage.removeItem('5TRATUMFW_SHOW_COINBASE');
    TestBed.configureTestingModule({
      declarations: [
        HomeComponent,
        TooltipTextIconComponent,
        TooltipIconComponent,
        ConfettiComponent,
        SnowflakesComponent,
        HashSuffixPipe,
        DiffSuffixPipe,
        DateAgoPipe,
        AddressPipe,
        SatsPipe,
        ByteSuffixPipe
      ],
      imports: [
        WorkDifficultyComponent,
        ReactiveFormsModule,
        FormsModule,
        NoopAnimationsModule,
        MessageModule,
        DropdownModule,
        ChartModule,
        ProgressBarModule,
        TooltipModule
      ],
      providers: [
        provideRouter([]),
        {provide:WorkDifficultyService,useValue:{get:()=>of({schemaVersion:1,normalization:'bitcoin-diff1',pools:[0,1].map(index=>({index,selected:false,connected:false,available:false,shareDifficulty:null,networkDifficulty:null,jobAgeSeconds:null,source:'active-stratum-job'}))})}},
        provideHttpClient(),
        provideToastr(),
        SystemApiService,
        ThemeService,
        QuicklinkService,
        Title,
        LoadingService,
        ShareRejectionExplanationService,
        LocalStorageService,
        DashboardEditService,
        LayoutService
      ]
    });
    fixture = TestBed.createComponent(HomeComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  afterEach(() => localStorage.removeItem('5TRATUMFW_SHOW_COINBASE'));

  it('updates the hash chart when the saved appearance changes', () => {
    TestBed.inject(ThemeService).saveThemeSettings({colorScheme:'dark',primaryColor:'#f7931a'}).subscribe();
    fixture.detectChanges();
    expect(component.chartData.datasets[0].borderColor).toBe('#f7931a');
    expect(component.chartOptions.scales.y.ticks.color).toBe('#f7931a');
    TestBed.inject(ThemeService).saveThemeSettings({colorScheme:'dark',primaryColor:'#f80421'}).subscribe();
    fixture.detectChanges();
    expect(component.chartData.datasets[0].borderColor).toBe(themeAccentColors('#f80421','dark')['--primary-color']);
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });

  it('accepts the matching active embedded interface version without a pair warning', () => {
    const info={hashRate:1200,temp:55,frequency:525,version:'5tratumFW-0.1.0-beta.16',axeOSVersion:'5tratumFW-0.1.0-beta.16'} as SystemInfo;
    component.handleSystemMessages(info,{duration:0,startTime:null});
    expect(component.messages.some(message=>message.type==='VERSION_MISMATCH'||message.type==='VERSION_UNVERIFIED')).toBeFalse();
  });

  it('keeps a genuine custom-interface mismatch visible with source-aware update guidance', () => {
    const info={hashRate:1200,temp:55,frequency:525,version:'5tratumFW-0.1.0-beta.16',axeOSVersion:'5tratumFW-0.1.0-beta.15'} as SystemInfo;
    component.handleSystemMessages(info,{duration:0,startTime:null});
    const message=component.messages.find(value=>value.type==='VERSION_MISMATCH');
    expect(message?.severity).toBe('warn');expect(message?.text).toContain('beta.16');expect(message?.text).toContain('beta.15');
    expect(message?.text).toContain('bundled interface');expect(message?.text).toContain('custom www.bin');
    expect(message?.text).not.toContain('Update both');expect(component.messages.some(value=>value.type==='VERSION_UNVERIFIED')).toBeFalse();
  });

  it('reports missing or placeholder web metadata as unverified and never treats Unified as proof of a matching pair', () => {
    for(const web of [undefined,null,'','   ','Unknown','Unified',' unified ','unavailable']) {
      const info={hashRate:1200,temp:55,frequency:525,version:'5tratumFW-0.1.0-beta.16',axeOSVersion:web} as unknown as SystemInfo;
      component.handleSystemMessages(info,{duration:0,startTime:null});
      expect(component.messages.some(value=>value.type==='VERSION_MISMATCH')).toBeFalse();
      const message=component.messages.find(value=>value.type==='VERSION_UNVERIFIED');
      expect(message?.severity).toBe('warn');expect(message?.text).toContain('could not be verified');
    }
  });

  it('clears stale mismatch and unverified warnings only after a known matching report arrives', () => {
    const info={hashRate:1200,temp:55,frequency:525,version:'5tratumFW-0.1.0-beta.16',axeOSVersion:'5tratumFW-0.1.0-beta.15'} as SystemInfo;
    component.handleSystemMessages(info,{duration:0,startTime:null});
    expect(component.messages.some(value=>value.type==='VERSION_MISMATCH')).toBeTrue();
    component.handleSystemMessages({...info,axeOSVersion:'Unknown'},{duration:0,startTime:null});
    expect(component.messages.some(value=>value.type==='VERSION_MISMATCH')).toBeFalse();
    expect(component.messages.filter(value=>value.type==='VERSION_UNVERIFIED').length).toBe(1);
    component.handleSystemMessages({...info,axeOSVersion:info.version},{duration:0,startTime:null});
    expect(component.messages.some(value=>value.type==='VERSION_MISMATCH'||value.type==='VERSION_UNVERIFIED')).toBeFalse();
    component.handleSystemMessages({...info,version:undefined as any},{duration:0,startTime:null});
    expect(component.messages.some(value=>value.type==='VERSION_UNVERIFIED')).toBeTrue();
  });

  it('does not lock the interface behind a loader while initial telemetry is unavailable', () => {
    expect(TestBed.inject(LoadingService).loading$.value).toBeFalse();
    expect(fixture.nativeElement.querySelector('.fw-hash-value')).toBeNull();
  });

  it('should describe an unmatched coinbase allocation as an unverified payout', () => {
    component.setCoinbaseDisplay(true);
    const info = {
      hashRate: 1200, temp: 55, frequency: 525,
      coinbaseOutputs: [{ address: 'pool-address', value: 100 }],
      coinbaseValueTotalSatoshis: 100, coinbaseValueUserSatoshis: 0,
    } as SystemInfo;
    component.handleSystemMessages(info, { duration: 0, startTime: null });
    const message = component.messages.find(item => item.type === 'NO_MINING_REWARD');

    expect(message?.severity).toBe('info');
    expect(message?.text).toBe('Payout could not be verified on this device. Check your pool or MUX.');
  });

  it('should keep payout diagnostics off until the browser explicitly opts in', () => {
    const info = {
      hashRate: 1200, temp: 55, frequency: 525,
      coinbaseOutputs: [{ address: 'pool-address', value: 100 }],
      coinbaseValueTotalSatoshis: 100, coinbaseValueUserSatoshis: 0,
    } as SystemInfo;
    component['latestInfo'] = info;
    component.handleSystemMessages(info, { duration: 0, startTime: null });
    expect(component.showCoinbaseData).toBeFalse();
    expect(component.messages.some(item => item.type === 'NO_MINING_REWARD')).toBeFalse();

    component.setCoinbaseDisplay(true);
    expect(localStorage.getItem('5TRATUMFW_SHOW_COINBASE')).toBe('true');
    expect(component.messages.some(item => item.type === 'NO_MINING_REWARD')).toBeTrue();

    component.setCoinbaseDisplay(false);
    expect(localStorage.getItem('5TRATUMFW_SHOW_COINBASE')).toBe('false');
    expect(component.messages.some(item => item.type === 'NO_MINING_REWARD')).toBeFalse();
  });

  it('should warn about a low measured supply and clear the warning after recovery', () => {
    const info = { hashRate: 1200, temp: 55, frequency: 525, nominalVoltage: 5, voltage: 4.7 } as SystemInfo;
    component.handleSystemMessages(info, { duration: 0, startTime: null });
    expect(component.operatingState).toBe('Input voltage low');
    expect(component.operatingSeverity).toBe('warning');
    expect(component.messages.find(item => item.type === 'LOW_INPUT_VOLTAGE')?.text).toContain('4.7 V');

    component.handleSystemMessages({ ...info, voltage: 5.1 }, { duration: 0, startTime: null });
    expect(component.lowSupplyVoltage).toBeFalse();
    expect(component.operatingState).toBe('Hashing');
    expect(component.messages.some(item => item.type === 'LOW_INPUT_VOLTAGE')).toBeFalse();

    component.handleSystemMessages({ ...info, voltage: NaN }, { duration: 0, startTime: null });
    expect(component.lowSupplyVoltage).toBeFalse();
  });

  it('should show a block candidate with reward diagnostics off and Advanced closed', async () => {
    const info = await firstValueFrom(TestBed.inject(SystemApiService).getInfo());
    component.info$ = of({ ...info, showNewBlock: true });
    fixture.detectChanges();
    expect(component.showCoinbaseData).toBeFalse();
    expect(fixture.nativeElement.querySelector('.fw-diagnostics').open).toBeFalse();
    expect(fixture.nativeElement.querySelector('.fw-job-details')).toBeNull();
    expect(fixture.nativeElement.querySelector('.fw-candidate-notice').textContent).toContain('Block candidate detected. Verify acceptance with your pool.');
  });

  describe('stale data and visibility state', () => {
    it('should set stale data error when visible and last message is old', () => {
      spyOnProperty(document, 'visibilityState', 'get').and.returnValue('visible');

      component['lastMessageTime'] = Date.now() - 10000;
      component.systemInfoError$.next({ duration: 0, startTime: null });

      component['checkStaleData']();

      expect(component.systemInfoError$.value.duration).toBe(10);
    });

    it('should NOT set stale data error when hidden and last message is old', () => {
      spyOnProperty(document, 'visibilityState', 'get').and.returnValue('hidden');

      component['lastMessageTime'] = Date.now() - 10000;
      component.systemInfoError$.next({ duration: 0, startTime: null });

      component['checkStaleData']();

      expect(component.systemInfoError$.value.duration).toBe(0);
    });

    it('should preserve stale telemetry when the page becomes visible without a device response', () => {
      spyOnProperty(document, 'visibilityState', 'get').and.returnValue('visible');
      spyOn(component as any, 'updateChart').and.stub();
      spyOn(component as any, 'loadPreviousData').and.stub();

      const initialTime = Date.now() - 10000;
      component['lastMessageTime'] = initialTime;
      component.systemInfoError$.next({ duration: 10, startTime: initialTime });

      component.onVisibilityChange();

      expect(component.systemInfoError$.value.duration).toBeGreaterThanOrEqual(10);
      expect(component.systemInfoError$.value.startTime).toBe(initialTime);
      expect(component['lastMessageTime']).toBe(initialTime);
    });

    it('should retain critical hardware status while also flagging stale readings', () => {
      const info = { hashRate: 1200, temp: 55, power_fault: 'Power fault' } as SystemInfo;
      component.handleSystemMessages(info, { duration: 10, startTime: Date.now() - 10000 });

      expect(component.telemetryStale).toBeTrue();
      expect(component.operatingSeverity).toBe('error');
      expect(component.operatingState).toBe('Hardware needs attention');
    });
  });
});
