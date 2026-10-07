export interface ProfileSlot { slot: number; configured: boolean; name: string | null; frequencyMHz?: number; coreVoltageMv?: number; host?: string; port?: number; user?: string; passwordConfigured?: boolean; protocol?: number; tls?: boolean; extranonceSubscribe?: boolean; authorityPubkey?: string; channelType?: number; }
export interface FiveTratumProfiles {
  schemaVersion: 1;
  tuning: ProfileSlot[]; pools: ProfileSlot[];
  limits: { frequency: { min: number; max: number; step: number; quantization: string }; coreVoltage: { min: number; max: number; step: number } };
  current: { frequencyMHz: number; coreVoltageMv: number }; independentWorkAssignment: false;
}
export interface PoolScheduleEvent { enabled: boolean; dayMask: number; timeMinutes: number; slot: number; }
export interface PoolSchedule { schemaVersion: 1; enabled: boolean; utcOffsetMinutes: number; events: PoolScheduleEvent[]; clockValid?: boolean; selectedSlot?: number | null; }
export interface PowerWindow { days: number; start: string; end: string; }
export interface PowerSchedule { enabled: boolean; timezone: string; windows: PowerWindow[]; }
export interface PowerReport {
  supported: boolean; schedule: PowerSchedule;
  status: { clockValid: boolean; localTime: string | null; scheduledPause: boolean; manualOverride: 'none' | 'paused' | 'running'; requestedPaused: boolean; appliedPaused: boolean; transitionPending: boolean; error: string | null };
  limits: { maxWindows: number; timezones: string[] };
}
export interface MiningWorkContext { jobId: string; height: number | null; nBits: string; networkDifficulty: number | null; ageSeconds: number; source: 'forwarded-stratum-job'; }
export interface FiveTratumMiningStatus { schemaVersion: 1; workContext: MiningWorkContext | null; mux: { pools: { poolIndex: number; connected: boolean; coin: { id: string | null; ticker: string | null; name: string | null }; workContext: MiningWorkContext | null }[] }; }
