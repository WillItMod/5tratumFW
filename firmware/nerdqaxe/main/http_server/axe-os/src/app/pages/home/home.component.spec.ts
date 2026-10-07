import { ElementRef } from '@angular/core';
import { of, Subscription } from 'rxjs';
import { HomeComponent } from './home.component';
import { SystemService } from '../../services/system.service';
import { IDashboardV2 } from '../../models/IDashboardV2';

describe('5tratumFW dashboard telemetry', () => {
  let component: HomeComponent;
  let dashboard: IDashboardV2;
  let subscription: Subscription;
  beforeEach(() => {
    localStorage.clear();
    dashboard = SystemService.defaultDashboardV2();
    dashboard.performance.asicCount = 4;
    dashboard.performance.hashRate = 4000;
    dashboard.power.voltage = 12.08;
    dashboard.power.coreVoltageActual = 1.126;
    dashboard.power.currentA = 6.28;
    component = new HomeComponent(
      { get: () => of(null) } as any,
      {} as any,
      { getDashboardV2WithSpan: () => of(dashboard) } as any,
      { instant: (key: string) => key } as any,
      { getItem: () => null } as any,
      new ElementRef(document.createElement('div')),
      {} as any,
      { markForCheck: () => {} } as any,
      { runOutsideAngular: (fn: () => unknown) => fn() } as any,
      {} as any,
      {} as any,
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
  it('does not divide device hashrate equally to fabricate chip counters', () => {
    freshStatus([{ index: 0, hashRateGHs: 963.5, fresh: true, sampleAgeSeconds: 1, temperatureC: null }]);
    expect(component.chipHash(component.asicRows[0])).toBe('963.5');
    expect(component.asicRows.slice(1).every(row => component.chipHash(row) === '—')).toBeTrue();
  });
  it('keeps coin unknown unless positive source metadata is present', () => {
    freshStatus([]);
    component.liveStatus!.coin = { ticker: 'BTC', source: 'unavailable' };
    expect(component.coinLabel).toBe('Coin unknown');
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
