import { ElementRef } from '@angular/core';
import { EMPTY, of, Subject, Subscription } from 'rxjs';
import { HomeComponent } from './home.component';
import { SystemService } from '../../services/system.service';
import { IDashboardV2 } from '../../models/IDashboardV2';

describe('5tratumFW dashboard telemetry', () => {
  let component: HomeComponent;
  let dashboard: IDashboardV2;
  let subscription: Subscription;
  let reset: jasmine.Spy;
  let auth: jasmine.Spy;
  let openDialog: jasmine.Spy;
  let getDashboard: jasmine.Spy;
  beforeEach(() => {
    localStorage.clear();
    dashboard = SystemService.defaultDashboardV2();
    dashboard.performance.asicCount = 4;
    dashboard.performance.hashRate = 4000;
    dashboard.power.voltage = 12.08;
    dashboard.power.coreVoltageActual = 1.126;
    dashboard.power.currentA = 6.28;
    reset = jasmine.createSpy('resetStats').and.returnValue(of('ok'));
    auth = jasmine.createSpy('ensureOtp$').and.returnValue(of({totp:'123456'}));
    openDialog = jasmine.createSpy('open');
    getDashboard = jasmine.createSpy('getDashboardV2WithSpan').and.callFake(() => of(dashboard));
    component = new HomeComponent(
      { get: () => of(null) } as any,
      {} as any,
      { getDashboardV2WithSpan: getDashboard, resetStats:reset } as any,
      { instant: (key: string) => key } as any,
      { getItem: () => null } as any,
      new ElementRef(document.createElement('div')),
      {} as any,
      { markForCheck: () => {} } as any,
      { runOutsideAngular: (fn: () => unknown) => fn() } as any,
      {open:openDialog} as any,
      {success:()=>{},danger:()=>{}} as any,
      {ensureOtp$:auth} as any,
    );
    subscription = component.info$.subscribe();
  });
  afterEach(() => { subscription.unsubscribe(); component.ngOnDestroy(); });
  function freshStatus(asics: any[], pools: any[] = []) {
    component.liveStatus = { schemaVersion: 1, asics, mux: { pools } };
    (component as any).statusReceivedAt = Date.now();
  }
  it('shows current pool job context and expires the advertised age',()=>{
    const work={jobId:'dgb-job',height:123456,nBits:'1d00ffff',networkDifficulty:1,ageSeconds:89,source:'forwarded-stratum-job'};
    freshStatus([],[{index:0,connected:true,serverId:null,coin:{ticker:'DGB'},workContext:work}]);expect(component.poolWorkContext(0)?.height).toBe(123456);expect(component.poolCoinLabel(0)).toBe('DGB');expect(component.poolWorkContext(1)).toBeNull();
    (component as any).statusReceivedAt=Date.now()-1001;expect(component.poolWorkContext(0)).toBeNull();
  });
  it('keeps mixed dual work separate and hides disconnected work',()=>{
    const base={jobId:'one',height:null,nBits:'1d00ffff',networkDifficulty:null,ageSeconds:0,source:'forwarded-stratum-job'};
    freshStatus([],[{index:0,connected:true,workContext:base},{index:1,connected:true,workContext:{...base,jobId:'two',height:50}}]);expect(component.poolWorkContext(0)?.height).toBeNull();expect(component.poolWorkContext(1)?.jobId).toBe('two');
    component.liveStatus!.mux!.pools![1].connected=false;expect(component.poolWorkContext(1)).toBeNull();
  });
  it('preserves backend V/A and actual core voltage precision', () => {
    expect(component.metric(dashboard.power.voltage, 2)).toBe('12.08');
    expect(component.metric(dashboard.power.currentA, 2)).toBe('6.28');
    expect(component.metric(dashboard.power.coreVoltageActual, 3)).toBe('1.126');
    expect(dashboard.power.voltage).toBe(12.08);
  });
  it('shows missing and non-finite readings as unavailable without inventing zero', () => {
    expect(component.metric(null)).toBe('—');
    expect(component.metric(undefined)).toBe('—');
    expect(component.metric(NaN)).toBe('—');
    expect(component.metric(0)).toBe('0.0');
  });
  function freshErrorSamples(count = 4, percentage = 2) {
    dashboard.performance.asicCount = count;
    dashboard.performance.errorPercentage = percentage;
    dashboard.performance.chips = Array.from({length: count}, () => ({
      hashRate: 980, errorHashRate: 20, errorPercentage: percentage,
      errorSampleFresh: true, sampleAgeSeconds: 0,
    }));
  }
  it('keeps ASIC errors unavailable with an older app or startup placeholders', () => {
    expect(component.asicErrorPercentage).toBe('—');
    expect(component.chipErrorPercentage(component.asicRows[0])).toBe('—');
    delete dashboard.performance.errorPercentage;
    delete dashboard.performance.chips;
    dashboard.performance.sharesRejected = 999;
    expect(component.asicErrorPercentage).toBe('—');
    expect(component.chipErrorPercentage(component.asicRows[0])).toBe('—');
  });
  it('shows measured zero ASIC errors independently of pool-rejected shares', () => {
    freshErrorSamples(4, 0);
    dashboard.performance.chips!.forEach(chip => chip.errorHashRate = 0);
    dashboard.performance.sharesRejected = 999;
    expect(component.asicErrorPercentage).toBe('0.00');
    expect(component.chipErrorPercentage(component.asicRows[0])).toBe('0.00');
  });
  it('shows all four QAxe and eight OctAxe error readings without manufacturing missing chips', () => {
    for (const count of [4, 8]) {
      freshErrorSamples(count);
      expect(component.asicRows.length).toBe(count);
      expect(component.asicRows.every(row => component.chipErrorPercentage(row) === '2.00')).toBeTrue();
      expect(component.asicErrorPercentage).toBe('2.00');
      dashboard.performance.chips!.pop();
      expect(component.asicErrorPercentage).toBe('—');
      expect(component.chipErrorPercentage(component.asicRows[count - 1])).toBe('—');
      expect(component.chipErrorPercentage(component.asicRows[0])).toBe('2.00');
    }
  });
  it('uses the device weighted board percentage instead of averaging chip percentages', () => {
    freshErrorSamples();
    dashboard.performance.chips = [
      {hashRate:900,errorHashRate:100,errorPercentage:10,errorSampleFresh:true,sampleAgeSeconds:0},
      ...Array.from({length:3}, () => ({hashRate:100,errorHashRate:0,errorPercentage:0,errorSampleFresh:true,sampleAgeSeconds:0})),
    ];
    dashboard.performance.errorPercentage = 100 * 100 / 1300;
    expect(component.asicErrorPercentage).toBe('7.69');
    expect(component.chipErrorPercentage(component.asicRows[0])).toBe('10.00');
    expect(component.chipErrorPercentage(component.asicRows[1])).toBe('0.00');
  });
  it('expires a hardware sample using its age plus monotonic request age even with a fresh dashboard', () => {
    freshErrorSamples();
    const requestedAt = performance.now();
    (component as any).dashboardErrorRequestStartedAt = requestedAt;
    const now = spyOn(performance, 'now').and.returnValue(requestedAt + 500);
    dashboard.performance.chips![0].sampleAgeSeconds = 14.5;
    expect(component.asicErrorPercentage).toBe('2.00');
    expect(component.chipErrorPercentage(component.asicRows[0])).toBe('2.00');
    now.and.returnValue(requestedAt + 501);
    expect(component.dashboardIsFresh).toBeTrue();
    expect(component.asicErrorPercentage).toBe('—');
    expect(component.chipErrorPercentage(component.asicRows[0])).toBe('—');
    expect(component.chipErrorPercentage(component.asicRows[1])).toBe('2.00');
  });
  it('counts delayed response transit time and never revives an expired hardware sample on receipt', () => {
    subscription.unsubscribe();
    let monotonicNow = 1000;
    spyOn(performance, 'now').and.callFake(() => monotonicNow);
    let response = new Subject<IDashboardV2>();
    getDashboard.and.returnValue(response);
    subscription = component.info$.subscribe();
    freshErrorSamples();
    dashboard.performance.chips![0].sampleAgeSeconds = 14.5;
    monotonicNow = 2000; // One-second round trip makes the 14.5s sample too old.
    response.next(dashboard);
    expect(component.dashboardIsFresh).toBeTrue();
    expect(component.asicErrorPercentage).toBe('—');
    expect(component.chipErrorPercentage(component.asicRows[0])).toBe('—');
    expect(component.chipErrorPercentage(component.asicRows[1])).toBe('2.00');

    subscription.unsubscribe();
    response = new Subject<IDashboardV2>();
    getDashboard.and.returnValue(response);
    monotonicNow = 10000;
    subscription = component.info$.subscribe();
    freshErrorSamples();
    monotonicNow = 26000; // Even a zero-age server snapshot has expired in transit.
    response.next(dashboard);
    expect(component.dashboardIsFresh).toBeTrue();
    expect(component.asicErrorPercentage).toBe('—');
    expect(component.asicRows.every(row => component.chipErrorPercentage(row) === '—')).toBeTrue();
  });
  it('rejects missing, invalid or out-of-range error percentages and sample ages', () => {
    const invalid: unknown[] = [null, undefined, NaN, Infinity, -1, 101, '0'];
    for (const value of invalid) {
      freshErrorSamples();
      dashboard.performance.errorPercentage = value as any;
      expect(component.asicErrorPercentage).withContext(`board percentage ${value}`).toBe('—');
      dashboard.performance.chips![0].errorPercentage = value as any;
      expect(component.chipErrorPercentage(component.asicRows[0])).withContext(`chip percentage ${value}`).toBe('—');
    }
    for (const age of [null, undefined, NaN, Infinity, -1, 15.001, '0']) {
      freshErrorSamples();
      dashboard.performance.chips![0].sampleAgeSeconds = age as any;
      expect(component.asicErrorPercentage).withContext(`sample age ${age}`).toBe('—');
      expect(component.chipErrorPercentage(component.asicRows[0])).withContext(`sample age ${age}`).toBe('—');
    }
  });
  it('hides paused, powered-down, failed and stale error telemetry rather than showing zero', () => {
    freshErrorSamples(8, 0);
    dashboard.performance.chips!.forEach(chip => chip.errorSampleFresh = false); // Paused rail / no new sample.
    expect(component.asicErrorPercentage).toBe('—');
    expect(component.chipErrorPercentage(component.asicRows[0])).toBe('—');
    freshErrorSamples(8, 0);
    dashboard.system.shutdown = true;
    expect(component.asicErrorPercentage).toBe('—');
    expect(component.chipErrorPercentage(component.asicRows[0])).toBe('—');
    dashboard.system.shutdown = false;
    component.dashboardUnavailable = true;
    expect(component.asicErrorPercentage).toBe('—');
    expect(component.chipErrorPercentage(component.asicRows[0])).toBe('—');
    component.dashboardUnavailable = false;
    (component as any).dashboardReceivedAt = Date.now() - 15001;
    expect(component.asicErrorPercentage).toBe('—');
    expect(component.chipErrorPercentage(component.asicRows[0])).toBe('—');
  });
  it('retains a measured board total when a fresh chip has zero work and no defined percentage', () => {
    freshErrorSamples();
    const chip = dashboard.performance.chips![0];
    chip.hashRate = chip.errorHashRate = 0;
    chip.errorPercentage = null;
    expect(component.chipErrorPercentage(component.asicRows[0])).toBe('—');
    expect(component.asicErrorPercentage).toBe('2.00');
    dashboard.performance.chips!.forEach(sample => sample.hashRate = sample.errorHashRate = 0);
    expect(component.asicErrorPercentage).toBe('—');
  });
  it('hides invalid rate and future-dated samples', () => {
    for (const rates of [[NaN,1], [1,Infinity], [-1,1], [1,-1]]) {
      freshErrorSamples();
      const chip = dashboard.performance.chips![0];
      [chip.hashRate, chip.errorHashRate] = rates;
      expect(component.asicErrorPercentage).toBe('—');
      expect(component.chipErrorPercentage(component.asicRows[0])).toBe('—');
    }
    freshErrorSamples();
    (component as any).dashboardReceivedAt = Date.now() + 1000;
    expect(component.asicErrorPercentage).toBe('—');
    expect(component.chipErrorPercentage(component.asicRows[0])).toBe('—');
  });
  it('shows four placeholders when chip counter endpoint is unavailable', () => {
    expect(component.asicRows.length).toBe(4);
    expect(component.asicRows.every(row => component.chipHash(row) === '—')).toBeTrue();
    expect(component.asicRows.every(row => component.chipTemperature(row) === '—')).toBeTrue();
  });
  it('distinguishes a valid measured zero from a missing counter', () => {
    freshStatus([{ index: 0, hashRateGHs: 0, fresh: true, sampleAgeSeconds: 1, temperatureC: null }]);
    expect(component.chipHash(component.asicRows[0])).toBe('0.0');
    expect(component.chipIsFresh(component.asicRows[0])).toBeTrue();
    expect(component.chipHash(component.asicRows[1])).toBe('—');
  });
  it('hides stale chip counters and expired MUX acknowledgements', () => {
    freshStatus([{ index: 0, hashRateGHs: 1000, fresh: true, sampleAgeSeconds: 1, temperatureC: null }], [{ index: 0, connected: true, serverId: 'test' }]);
    expect(component.muxConnected(0)).toBeTrue();
    (component as any).statusReceivedAt = Date.now() - 16000;
    expect(component.chipHash(component.asicRows[0])).toBe('—');
    expect(component.muxConnected(0)).toBeFalse();
  });
  it('maps a fallback-only dashboard row to physical pool2 rather than pool1', () => {
    dashboard.stratum.usingFallback = true;
    dashboard.stratum.pools[0].connected = true;
    freshStatus([], [{ index: 0, connected: false, serverId: null }, { index: 1, connected: true, serverId: 'test' }]);
    expect(component.physicalPoolIndex(0)).toBe(1);
    expect(component.muxConnected(component.physicalPoolIndex(0))).toBeTrue();
    expect(component.routingAllocation(0)).toBe(100);
  });
  it('uses RC1 active flags when failover reports both pool connections', () => {
    const primary=dashboard.stratum.pools[0];primary.connected=true;primary.active=false;
    dashboard.stratum.pools.push({...primary,active:true});dashboard.stratum.usingFallback=true;
    expect(component.physicalPoolIndex(1)).toBe(1);expect(component.routingAllocation(0)).toBe(0);expect(component.routingAllocation(1)).toBe(100);
  });
  it('keeps all-time reset opt-in and requests OTP only after an explicit all-time reset confirmation', () => {
    component.resetAllTime=true;component.openResetStatsDialog('dialog');
    expect(component.resetAllTime).toBeFalse();expect(reset).not.toHaveBeenCalled();expect(auth).not.toHaveBeenCalled();
    const ref={close:jasmine.createSpy('close')};component.confirmResetStats(ref);
    expect(reset.calls.mostRecent().args).toEqual([]);expect(auth).not.toHaveBeenCalled();
    component.resetAllTime=true;component.confirmResetStats(ref);
    expect(auth).toHaveBeenCalledTimes(1);expect(reset.calls.mostRecent().args).toEqual(['',true,'123456']);expect(component.resetStatsBusy).toBeFalse();
  });
  it('does not reset statistics when the all-time OTP prompt is cancelled', () => {
    auth.and.returnValue(EMPTY);component.resetAllTime=true;component.confirmResetStats({close:()=>{}});
    expect(reset).not.toHaveBeenCalled();expect(component.resetStatsBusy).toBeFalse();
  });
  it('does not divide device hashrate equally to fabricate chip counters', () => {
    freshStatus([{ index: 0, hashRateGHs: 963.5, fresh: true, sampleAgeSeconds: 1, temperatureC: null }]);
    expect(component.chipHash(component.asicRows[0])).toBe('963.5');
    expect(component.asicRows.slice(1).every(row => component.chipHash(row) === '—')).toBeTrue();
  });
  it('keeps coin unknown unless positive source metadata is present', () => {
    freshStatus([]);
    component.liveStatus!.coin = { ticker: 'BTC', source: 'unavailable' };
    expect(component.coinLabel).toBe('Direct pool');
    expect(component.coinLabel).not.toBe('BTC');
    component.liveStatus!.coin = { ticker: 'BCH', source: 'mux-route' };
    expect(component.coinLabel).toBe('BCH');
  });
  it('marks retained values unavailable after a failed or stale dashboard poll', () => {
    component.dashboardUnavailable = true;
    expect(component.hashValue(4000)).toBe('—');
    expect(component.miningState).toBe('Telemetry unavailable');
    component.dashboardUnavailable = false;
    (component as any).dashboardReceivedAt = Date.now() - 16000;
    expect(component.metric(75)).toBe('—');
  });
});
