import { Injectable } from '@angular/core';
import { first, timeout } from 'rxjs';
import { ToastrService } from 'ngx-toastr';
import { SystemInfo } from 'src/app/generated/models';
import { SystemApiService } from './system.service';

const SETTING_KEYS: (keyof SystemInfo)[] = [
  'frequency', 'coreVoltage', 'autofanspeed', 'manualFanSpeed', 'minFanSpeed', 'temptarget',
  'stratumURL', 'stratumPort', 'stratumUser', 'stratumProtocol', 'stratumTLS',
  'stratumExtranonceSubscribe', 'stratumDecodeCoinbase', 'stratumSuggestedDifficulty',
  'fallbackStratumURL', 'fallbackStratumPort', 'fallbackStratumUser', 'fallbackStratumProtocol',
  'fallbackStratumTLS', 'fallbackStratumExtranonceSubscribe', 'fallbackStratumDecodeCoinbase',
  'fallbackStratumSuggestedDifficulty', 'display', 'rotation', 'invertscreen', 'displayTimeout',
  'statsFrequency', 'overclockEnabled',
];

export function createOperatingSettingsSnapshot(info: SystemInfo, observedAt: Date) {
  if (!['601', '602'].includes(info.boardVersion) ||
      !Number.isFinite(info.frequency) || info.frequency <= 0 ||
      !Number.isFinite(info.coreVoltage) || info.coreVoltage <= 0) {
    throw new Error('The device did not return complete Gamma operating settings.');
  }
  const settings: Record<string, string | number | boolean> = {};
  for (const key of SETTING_KEYS) {
    const value = info[key];
    if (typeof value === 'string' || typeof value === 'boolean' ||
        (typeof value === 'number' && Number.isFinite(value))) settings[key] = value;
  }
  return {
    format: '5tratumFW-operating-settings', formatVersion: 1,
    observedAtUtc: observedAt.toISOString(),
    device: { boardVersion: info.boardVersion, ASICModel: info.ASICModel, hostname: info.hostname },
    sourceFirmware: info.version,
    scope: 'Operating settings record. Passwords and other NVS contents are not included.',
    units: { frequency: 'MHz', coreVoltage: 'mV', temptarget: 'degrees Celsius', fanSpeed: 'percent' },
    settings,
  };
}

@Injectable({ providedIn: 'root' })
export class OperatingSettingsExportService {
  constructor(private system: SystemApiService, private toastr: ToastrService) {}

  exportSettings(): void {
    this.system.getInfo().pipe(first(), timeout(8000)).subscribe({
      next: info => {
        try {
          const snapshot = createOperatingSettingsSnapshot(info, new Date());
          const url = URL.createObjectURL(new Blob([JSON.stringify(snapshot, null, 2) + '\n'], { type: 'application/json' }));
          const link = document.createElement('a');
          link.href = url;
          link.download = `5tratumFW-Gamma-${info.boardVersion}-operating-settings.json`;
          link.click();
          setTimeout(() => URL.revokeObjectURL(url), 1000);
          this.toastr.success('Operating settings exported. Passwords are not included.');
        } catch (error) {
          this.toastr.error(error instanceof Error ? error.message : 'Could not export settings.');
        }
      },
      error: () => this.toastr.error('Could not read current device settings for export.'),
    });
  }
}
