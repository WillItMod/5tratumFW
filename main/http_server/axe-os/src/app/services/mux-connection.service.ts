import { Injectable } from '@angular/core';
import { Observable } from 'rxjs';
import { SystemInfo } from '../generated/models';
import { SystemApiService } from './system.service';

export const MUX_STRATUM_PORT = 7331;
export const MUX_TELEMETRY_FRESH_MS = 15000;

export interface MuxConnectionInput {
  host: string;
  port: number;
  worker: string;
  password?: string;
}

export interface MuxConnectionPatch {
  stratumURL: string;
  stratumPort: number;
  stratumUser: string;
  stratumPassword?: string;
  stratumProtocol: 'SV1';
  stratumExtranonceSubscribe: true;
  stratumSuggestedDifficulty: 0;
  stratumTLS: 0;
  stratumDecodeCoinbase: false;
  useFallbackStratum: 0;
}

/** Host and port are separate firmware settings; never put a URI into the host. */
export function validMuxHost(value: string): boolean {
  return !!value && value.length <= 253 && /^[a-zA-Z0-9](?:[a-zA-Z0-9._-]*[a-zA-Z0-9])?$/.test(value);
}

export function buildMuxConnectionPatch(input: MuxConnectionInput): MuxConnectionPatch {
  const host = input.host.trim();
  const worker = input.worker.trim();
  const port = Number(input.port);
  if (!validMuxHost(host)) throw new Error('Enter the MUX IP address or hostname without a protocol, path or port.');
  if (!Number.isInteger(port) || port < 1 || port > 65535) throw new Error('Enter a port from 1 to 65535.');
  if (!worker || worker.length > 128 || /\s/.test(worker)) throw new Error('Enter a worker name without spaces (128 characters maximum).');
  const patch: MuxConnectionPatch = {
    stratumURL: host,
    stratumPort: port,
    stratumUser: worker,
    stratumProtocol: 'SV1',
    stratumExtranonceSubscribe: true,
    stratumSuggestedDifficulty: 0,
    stratumTLS: 0,
    stratumDecodeCoinbase: false,
    useFallbackStratum: 0,
  };
  // ***** is a masked readback, not an actual password. Omission preserves it.
  if (input.password !== undefined && input.password !== '*****') patch.stratumPassword = input.password;
  return patch;
}

export function defaultMuxWorker(info: Pick<SystemInfo, 'hostname' | 'boardVersion' | 'macAddr' | 'ipv4'>): string {
  const name = (info.hostname || 'miner').replace(/[^a-zA-Z0-9_-]/g, '-').slice(0, 80);
  const board = (info.boardVersion || 'device').replace(/[^a-zA-Z0-9_-]/g, '-');
  const mac = (info.macAddr || '').replace(/[^a-fA-F0-9]/g, '');
  const identity = mac.length >= 6 ? mac.slice(-6).toLowerCase() : (info.ipv4 || 'unknown').replace(/[^a-zA-Z0-9]/g, '-');
  return `${name}-${board}-${identity}`.slice(0, 128);
}

export function muxConfigurationMatches(info: SystemInfo, target: MuxConnectionInput): boolean {
  return muxConfigurationDifferences(info, target).length === 0;
}

function muxConfigurationDifferences(info: SystemInfo, target: MuxConnectionInput): string[] {
  const differences: string[] = [];
  const display = (value: unknown): string => value === undefined || value === null || value === '' ? 'not reported' : String(value);
  const enabled = (value: unknown): string => value === true ? 'enabled' : value === false ? 'disabled' : 'not reported';
  if (info.stratumURL?.trim().toLowerCase() !== target.host.trim().toLowerCase()) {
    differences.push(`Primary host: ${display(info.stratumURL)} → ${target.host.trim() || 'not entered'}`);
  }
  if (info.stratumPort !== Number(target.port)) {
    differences.push(`Stratum port: ${display(info.stratumPort)} → ${target.port}`);
  }
  if (info.stratumUser !== target.worker.trim()) {
    differences.push(`Worker identity: ${display(info.stratumUser)} → ${target.worker.trim() || 'not entered'}`);
  }
  if (info.stratumProtocol !== 'SV1') differences.push(`Protocol: ${display(info.stratumProtocol)} → Stratum V1`);
  if (info.stratumExtranonceSubscribe !== true) differences.push(`Extranonce subscription: ${enabled(info.stratumExtranonceSubscribe)} → enabled`);
  if (info.stratumTLS) differences.push('TLS: on → off');
  if (info.stratumDecodeCoinbase !== false) differences.push(`Coinbase decoding: ${enabled(info.stratumDecodeCoinbase)} → disabled`);
  if (info.stratumSuggestedDifficulty !== 0) differences.push(`Suggested difficulty: ${display(info.stratumSuggestedDifficulty)} → 0 (MUX chooses)`);
  return differences;
}

export interface MuxObservation {
  label: string;
  detail: string;
  tone: 'neutral' | 'warning';
  configurationMatches: boolean;
  stale: boolean;
  acceptedDuringObservation: number;
}

/** Device counters are evidence of miner activity, never proof of a MUX route. */
export class MuxConnectionMonitor {
  private accepted?: number;
  private uptime?: number;
  private selection?: string;
  private acceptedDuringObservation = 0;

  reset(): void {
    this.accepted = undefined;
    this.uptime = undefined;
    this.selection = undefined;
    this.acceptedDuringObservation = 0;
  }

  observe(info: SystemInfo): void {
    if (!Number.isFinite(info.sharesAccepted) || !Number.isFinite(info.uptimeSeconds)) return;
    const selection = `${info.isUsingFallbackStratum ? 'fallback' : 'primary'}:${info.stratumURL}:${info.stratumPort}`;
    if (this.selection !== selection || (this.uptime !== undefined && info.uptimeSeconds < this.uptime)
      || (this.accepted !== undefined && info.sharesAccepted < this.accepted)) {
      this.acceptedDuringObservation = 0;
      this.accepted = undefined;
    }
    if (this.accepted !== undefined) {
      this.acceptedDuringObservation += Math.max(0, info.sharesAccepted - this.accepted);
    }
    this.accepted = info.sharesAccepted;
    this.uptime = info.uptimeSeconds;
    this.selection = selection;
  }

  evaluate(info: SystemInfo, target: MuxConnectionInput, lastUpdateAt: number, now: number,
    restartPending = false, restartRequested = false): MuxObservation {
    const stale = lastUpdateAt <= 0 || now - lastUpdateAt > MUX_TELEMETRY_FRESH_MS;
    const configurationMatches = muxConfigurationMatches(info, target);
    let label = 'Waiting for accepted shares';
    let detail = 'Matching saved settings alone do not prove a live MUX connection.';
    let tone: 'neutral' | 'warning' = 'neutral';
    if (stale) {
      label = 'Miner telemetry stale';
      detail = 'These are the last received readings. Live connection status is unknown.';
      tone = 'warning';
    } else if (info.power_fault || info.hardware_fault || info.overheat_mode) {
      label = 'Miner needs attention';
      detail = 'Resolve the device fault before checking MUX mining activity.';
      tone = 'warning';
    } else if (restartPending) {
      label = restartRequested ? 'Waiting for miner restart' : 'Settings saved · restart pending';
      detail = 'The mining session may still use its previous pool until the miner restarts.';
    } else if (info.miningPaused) {
      label = 'Mining paused';
      detail = 'Resume mining from the device controls to send work through the configured connection.';
      tone = 'warning';
    } else if (!configurationMatches) {
      label = 'Connection settings differ';
      detail = `Changes required: ${muxConfigurationDifferences(info, target).join('; ')}.`;
      tone = 'warning';
    } else if (info.isUsingFallbackStratum) {
      label = 'Fallback pool selected';
      detail = 'The miner reports fallback selection. Confirm MUX access and a healthy SHA-256 destination.';
      tone = 'warning';
    } else if (this.acceptedDuringObservation > 0) {
      label = 'Device share activity observed';
      detail = 'Accepted shares increased on this device. Confirm its live worker and upstream route in MUX Miners.';
    }
    return { label, detail, tone, stale, configurationMatches, acceptedDuringObservation: this.acceptedDuringObservation };
  }
}

@Injectable({ providedIn: 'root' })
export class MuxConnectionService {
  constructor(private systemApi: SystemApiService) {}

  save(input: MuxConnectionInput): Observable<void> {
    // No fallback, Wi-Fi, clocks or fan fields are included in this patch.
    return this.systemApi.updateSystem('', buildMuxConnectionPatch(input));
  }
}
