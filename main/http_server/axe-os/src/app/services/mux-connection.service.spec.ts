import { of } from 'rxjs';
import { SystemInfo } from '../generated/models';
import { SystemApiService } from './system.service';
import {
  MuxConnectionInput, MuxConnectionMonitor, MuxConnectionService,
  buildMuxConnectionPatch, defaultMuxWorker,
} from './mux-connection.service';

describe('MUX connection contract', () => {
  const target: MuxConnectionInput = { host: '198.51.100.20', port: 7331, worker: 'bitaxe-601-aabbcc', password: 'dummy-new-password' };

  it('configures the actual SV1 MUX handoff and leaves fallback, clocks and Wi-Fi out of the PATCH', () => {
    const api = jasmine.createSpyObj<SystemApiService>('SystemApiService', ['updateSystem']);
    api.updateSystem.and.returnValue(of(undefined));
    new MuxConnectionService(api).save(target).subscribe();
    const payload = api.updateSystem.calls.mostRecent().args[1];
    expect(api.updateSystem.calls.mostRecent().args[0]).toBe('');
    expect(payload.stratumURL).toBe(target.host);
    expect(payload.stratumPort).toBe(7331);
    expect(payload.stratumProtocol).toBe('SV1');
    expect(payload.stratumExtranonceSubscribe).toBeTrue();
    expect(payload.stratumSuggestedDifficulty).toBe(0);
    expect(payload.stratumTLS).toBe(0);
    expect(payload.stratumDecodeCoinbase).toBeFalse();
    expect(payload.useFallbackStratum).toBe(0);
    expect(payload.stratumPassword).toBe('dummy-new-password');
    expect(Object.keys(payload).some(key => key.startsWith('fallback'))).toBeFalse();
    expect(payload.frequency).toBeUndefined();
    expect(payload.coreVoltage).toBeUndefined();
    expect(payload.wifiPass).toBeUndefined();
  });

  it('never writes the masked password and allows an explicitly empty password', () => {
    expect(buildMuxConnectionPatch({ ...target, password: '*****' }).stratumPassword).toBeUndefined();
    expect(buildMuxConnectionPatch({ ...target, password: undefined }).stratumPassword).toBeUndefined();
    expect(buildMuxConnectionPatch({ ...target, password: '' }).stratumPassword).toBe('');
  });

  it('rejects complete URLs, embedded ports, fractional ports and blank worker identities', () => {
    for (const host of ['http://198.51.100.20', 'stratum+tcp://198.51.100.20', '198.51.100.20:7331', 'mux/path']) {
      expect(() => buildMuxConnectionPatch({ ...target, host })).toThrow();
    }
    expect(() => buildMuxConnectionPatch({ ...target, port: 7331.5 })).toThrow();
    expect(() => buildMuxConnectionPatch({ ...target, port: 0 })).toThrow();
    expect(() => buildMuxConnectionPatch({ ...target, worker: '   ' })).toThrow();
  });

  it('creates distinct worker names even when devices share the bitaxe hostname', () => {
    const first = { hostname: 'bitaxe', boardVersion: '601', macAddr: 'AA:BB:CC:12:34:56', ipv4: '198.51.100.21' };
    const second = { ...first, boardVersion: '602', macAddr: 'AA:BB:CC:65:43:21' };
    expect(defaultMuxWorker(first)).not.toBe(defaultMuxWorker(second));
    expect(defaultMuxWorker(first)).toBe('bitaxe-601-123456');
    expect(defaultMuxWorker({ ...first, macAddr: '' })).toContain('198-51-100-21');
  });
});

describe('MUX device observations', () => {
  const target: MuxConnectionInput = { host: '198.51.100.20', port: 7331, worker: 'bitaxe-601-aabbcc' };
  const now = 100000;
  const reading = {
    stratumURL: target.host, stratumPort: target.port, stratumUser: target.worker,
    stratumProtocol: 'SV1', stratumExtranonceSubscribe: true,
    stratumDecodeCoinbase: false, stratumSuggestedDifficulty: 0, stratumTLS: 0,
    isUsingFallbackStratum: 0, miningPaused: false, sharesAccepted: 100, uptimeSeconds: 600,
  } as unknown as SystemInfo;
  let monitor: MuxConnectionMonitor;

  beforeEach(() => monitor = new MuxConnectionMonitor());

  it('does not treat matching saved settings or previous accepted shares as joined proof', () => {
    monitor.observe(reading);
    const result = monitor.evaluate(reading, target, now, now);
    expect(result.configurationMatches).toBeTrue();
    expect(result.acceptedDuringObservation).toBe(0);
    expect(result.label).toBe('Waiting for accepted shares');
    expect(result.detail).toContain('do not prove');
  });

  it('reports increased device shares without claiming they verify the MUX route', () => {
    monitor.observe(reading);
    const next = { ...reading, sharesAccepted: 103, uptimeSeconds: 601 };
    monitor.observe(next);
    const result = monitor.evaluate(next, target, now, now);
    expect(result.acceptedDuringObservation).toBe(3);
    expect(result.label).toBe('Device share activity observed');
    expect(result.detail).toContain('Confirm its live worker and upstream route');
  });

  it('resets the observation when counters decrease or the miner reboots', () => {
    monitor.observe(reading);
    monitor.observe({ ...reading, sharesAccepted: 104 });
    const reset = { ...reading, sharesAccepted: 1, uptimeSeconds: 2 };
    monitor.observe(reset);
    expect(monitor.evaluate(reset, target, now, now).acceptedDuringObservation).toBe(0);
    monitor.observe({ ...reset, sharesAccepted: 2, uptimeSeconds: 3 });
    expect(monitor.evaluate(reset, target, now, now).acceptedDuringObservation).toBe(1);
  });

  it('flags mismatched settings, fallback selection and paused mining', () => {
    const mismatch = { ...reading, stratumURL: 'another-pool.local' };
    expect(monitor.evaluate(mismatch, target, now, now).label).toBe('Connection settings differ');
    expect(monitor.evaluate({ ...reading, isUsingFallbackStratum: 1 }, target, now, now).label).toBe('Fallback pool selected');
    expect(monitor.evaluate({ ...reading, miningPaused: true }, target, now, now).label).toBe('Mining paused');
  });

  it('explains mandatory handoff differences even when the endpoint and worker already match', () => {
    const mismatch = { ...reading, stratumExtranonceSubscribe: false, stratumDecodeCoinbase: true };
    const result = monitor.evaluate(mismatch, target, now, now);
    expect(result.configurationMatches).toBeFalse();
    expect(result.detail).toBe('Changes required: Extranonce subscription: disabled → enabled; Coinbase decoding: enabled → disabled.');
    expect(result.detail).not.toContain('Primary host:');
    expect(result.detail).not.toContain('Worker identity:');
    expect(result.detail.toLowerCase()).not.toContain('password');
  });

  it('keeps stale readings unknown even when accepted counters previously increased', () => {
    monitor.observe(reading);
    monitor.observe({ ...reading, sharesAccepted: 105 });
    const result = monitor.evaluate(reading, target, now - 16000, now);
    expect(result.stale).toBeTrue();
    expect(result.label).toBe('Miner telemetry stale');
    expect(result.detail).toContain('status is unknown');
    expect(monitor.evaluate(reading, target, 0, now).stale).toBeTrue();
  });

  it('separates saving and a restart request from actual share activity', () => {
    monitor.observe(reading);
    monitor.observe({ ...reading, sharesAccepted: 110 });
    expect(monitor.evaluate(reading, target, now, now, true).label).toBe('Settings saved · restart pending');
    expect(monitor.evaluate(reading, target, now, now, true, true).label).toBe('Waiting for miner restart');
  });

  it('prioritizes a fresh hardware fault over paused or pending join state', () => {
    const fault = { ...reading, miningPaused: true, power_fault: 'Power fault' };
    expect(monitor.evaluate(fault, target, now, now, true).label).toBe('Miner needs attention');
  });
});
