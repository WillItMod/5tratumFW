export type TuningPriority = 'efficiency' | 'balanced' | 'performance';
export interface AutotuneConfig {
  priority: TuningPriority;
  continuous: boolean;
  /** Explicitly saved experimental clock permission; absent on legacy reports. */
  advancedClockOverride?: boolean;
  maxAsicTempC: number;
  maxVrmTempC: number;
  minFrequencyMHz: number;
  maxFrequencyMHz: number;
  minVoltageMv: number;
  maxVoltageMv: number;
  maxPowerW: number;
}
export interface AutotunePoint { frequencyMHz: number; coreVoltageMv: number; }
export interface AutotuneLimits {
  minFrequencyMHz: number; maxFrequencyMHz: number;
  /** Experimental project ceiling, not a hardware or safe operating rating. */
  advancedMaxFrequencyMHz?: number;
  minVoltageMv: number; maxVoltageMv: number;
  maxAsicTempC: number; maxVrmTempC: number; maxPowerW: number;
  frequencyStepMHz: number; voltageStepMv: number;
}
export interface AutotuneTelemetry {
  valid: boolean; asicTempC: number | null; vrmTempC: number | null;
  powerW: number | null; hashRateGHs: number | null; errorPercentage: number | null;
  sampleAgeSeconds: number | null; fanOk: boolean;
  miningActive: boolean; fault: boolean; worstErrorPercentage: number | null; voltageReadbackMv: number | null;
}
export interface AutotuneMeans { hashRateGHs: number; powerW: number; }
export interface AutotuneSearch {
  phaseElapsedSeconds: number | null; minimumSeconds: number | null;
  samples: number; minimumSamples: number; roundExperiments: number; roundExperimentLimit: number;
  rejectedWindows: number; reference: AutotuneMeans | null; best: AutotuneMeans | null;
}
export interface AutotuneConditions {asicTempC:number;vrmTempC:number;powerW:number;hashRateGHs:number;errorPercentage:number;worstErrorPercentage:number;}
export interface AutotuneRecovery {requested:boolean;phase:'idle'|'validating-off'|'restoring'|'resuming'|'verifying'|'complete'|'blocked';reason:string;}
export interface AutotuneEvent {
  id: number; atSeconds: number; kind: string; reason: string; outcome: string;
  from: AutotunePoint | null; point: AutotunePoint;
  measurement: {hashRateGHs: number; powerW: number; errorPercentage: number; worstErrorPercentage: number} | null;
  score: number | null; requiredScore: number | null;
  conditions?: AutotuneConditions | null;
}
export interface AutotuneReadiness { key: string; label: string; ready: boolean; detail: string; }
export interface AutotuneReport {
  schemaVersion: 1; supported: boolean; waiverVersion: 1;
  hardware: { model: string; asicCount: number; asicSensor: string; vrmSensor: string; powerSource: string };
  config: AutotuneConfig; limits: AutotuneLimits;
  guidance: {asic: string; vrm: string; power: string; experimental: true};
  status: {
    state: string; reason: string; active: boolean; ownsSettings: boolean; runId: number; experiments: number;
    baseline: AutotunePoint | null; current: AutotunePoint | null; best: AutotunePoint | null;
    telemetry: AutotuneTelemetry;
    /** Additive diagnostics; existing sensor and controller guards still authorize Start. */
    readiness?: AutotuneReadiness[];
    /** Additive presentation data; never required to authorize Restore. */
    search?: AutotuneSearch;
    eventCapacity?: number; events?: AutotuneEvent[];
    recovery?: AutotuneRecovery;
  };
  backup: { available: boolean; restored: boolean; frequencyMHz: number | null; coreVoltageMv: number | null };
}
export interface AutotuneAck { ok: boolean; restartRequired: boolean; }
