export interface TuningScheduleEvent { enabled: boolean; dayMask: number; timeMinutes: number; slot: number; }
export interface TuningSchedule { schemaVersion: 1; enabled: boolean; utcOffsetMinutes: number; events: TuningScheduleEvent[]; }
export interface TuningScheduleReport extends TuningSchedule {
  supported: boolean;
  timezoneMode: 'fixed-utc-offset';
  clockValid: boolean;
  maxEvents: number;
  selectedSlot: number | null;
  appliedSlot: number | null;
  status: 'disabled' | 'waiting-clock' | 'waiting-runtime' | 'paused' | 'pending' | 'applied' | 'error';
  error: string | null;
  storageValid?: boolean;
}
