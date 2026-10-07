import { SystemInfo } from 'src/app/generated/models';
import { createOperatingSettingsSnapshot } from './operating-settings-export.service';

describe('Operating settings export', () => {
  const observed = new Date('2026-10-07T09:00:00Z');
  const info = {
    boardVersion: '601', ASICModel: 'BM1370', hostname: 'miner', version: 'test',
    frequency: 625.125, coreVoltage: 1173, coreVoltageActual: 1144,
    autofanspeed: 0, manualFanSpeed: 0, displayTimeout: -1,
    stratumURL: 'mux.local', stratumPort: 7331, stratumUser: 'gamma601',
    stratumDecodeCoinbase: false, fallbackStratumURL: '',
    stratumPassword: 'dummy-pool-password', wifiPassword: 'dummy-wifi-password',
    unrelatedSecret: 'do-not-export',
  } as unknown as SystemInfo;

  it('preserves exact configured clocks, millivolts, zero values and empty settings', () => {
    const snapshot = createOperatingSettingsSnapshot(info, observed);
    expect(snapshot.settings['frequency']).toBe(625.125);
    expect(snapshot.settings['coreVoltage']).toBe(1173);
    expect(snapshot.settings['manualFanSpeed']).toBe(0);
    expect(snapshot.settings['displayTimeout']).toBe(-1);
    expect(snapshot.settings['fallbackStratumURL']).toBe('');
    expect(snapshot.settings['stratumDecodeCoinbase']).toBeFalse();
    expect(snapshot.observedAtUtc).toBe(observed.toISOString());
  });

  it('exports only known operating settings and excludes credentials and measured voltages', () => {
    const snapshot = createOperatingSettingsSnapshot(info, observed);
    expect(JSON.stringify(snapshot)).not.toContain('do-not-export');
    expect(snapshot.settings['coreVoltageActual']).toBeUndefined();
    expect(snapshot.scope).toContain('not included');
  });

  it('rejects incomplete or unsupported operating settings instead of producing a recovery record', () => {
    expect(() => createOperatingSettingsSnapshot({ ...info, frequency: NaN }, observed)).toThrow();
    expect(() => createOperatingSettingsSnapshot({ ...info, boardVersion: '800' }, observed)).toThrow();
    expect(() => createOperatingSettingsSnapshot({ ...info, coreVoltage: 0 }, observed)).toThrow();
  });
});
