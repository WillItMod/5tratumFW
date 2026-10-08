import { HttpBackend, HttpClient } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable, of, throwError } from 'rxjs';
import { map, switchMap, timeout } from 'rxjs/operators';

const CATALOG = 'https://api.github.com/repos/WillItMod/5tratumFW/releases';
const SEMANTIC_SUFFIX = '(0|[1-9]\\d*)\\.(0|[1-9]\\d*)\\.(0|[1-9]\\d*)(?:-(alpha|beta|rc)\\.(0|[1-9]\\d*))?';
interface ReleasePolicy {
  model: string;
  label: string;
  tagPrefix: string;
  versionPrefix: string;
  appName: string;
}
const POLICIES: readonly ReleasePolicy[] = [
  { model: 'NerdQAxe++', label: 'QAxe++', tagPrefix: 'qaxe-v', versionPrefix: '5tratumFW-qa-', appName: 'esp-miner-NerdQAxe++.bin' },
  { model: 'NerdOCTAXE-γ', label: 'OctAxe Gamma', tagPrefix: 'octaxe-v', versionPrefix: '5tratumFW-oct-', appName: 'esp-miner-NerdOCTAXE-Gamma.bin' },
];

export function releasePolicyForModel(model: string): Readonly<ReleasePolicy> | undefined {
  return POLICIES.find(policy => policy.model === model);
}

export interface QaxeRelease {
  tag: string;
  version: string;
  prerelease: boolean;
  releaseUrl: string;
  appUrl: string;
  webUrl: string;
}

export interface QaxeUpdateCheck {
  status: 'available' | 'up-to-date' | 'newer-build' | 'no-release' | 'unsupported';
  release: QaxeRelease | null;
}

function versionParts(value: string, pattern: RegExp): number[] | null {
  const match = pattern.exec(value);
  if (!match) return null;
  const numbers = [Number(match[1]), Number(match[2]), Number(match[3]), Number(match[5] || 0)];
  if (numbers.some(number => !Number.isSafeInteger(number))) return null;
  return [...numbers.slice(0, 3), match[4] ? ['alpha', 'beta', 'rc'].indexOf(match[4]) : 3, numbers[3]];
}

function compareVersions(first: number[], second: number[]): number {
  for (let index = 0; index < first.length; index++) {
    if (first[index] !== second[index]) return first[index] > second[index] ? 1 : -1;
  }
  return 0;
}

function canonicalUrl(value: unknown, path: string): value is string {
  // GitHub encodes '+' in asset names; admit only its exact repository paths.
  const expected = 'https://github.com' + path;
  return typeof value === 'string' && (value === expected || value === expected.replace(/\+/g, '%2B'));
}

function eligibleRelease(value: any, policy: Readonly<ReleasePolicy>): QaxeRelease | null {
  const tagPattern = new RegExp(`^${policy.tagPrefix}${SEMANTIC_SUFFIX}$`);
  if (!value || value.draft !== false || typeof value.prerelease !== 'boolean'
    || typeof value.tag_name !== 'string' || !versionParts(value.tag_name, tagPattern)
    || !canonicalUrl(value.html_url, `/WillItMod/5tratumFW/releases/tag/${value.tag_name}`)
    || !Array.isArray(value.assets)) return null;
  const asset = (name: string) => {
    const matches = value.assets.filter((item: any) => item && item.name === name);
    if (matches.length !== 1) return null;
    const item = matches[0];
    return Number.isSafeInteger(item.size) && item.size > 0
      && canonicalUrl(item.browser_download_url, `/WillItMod/5tratumFW/releases/download/${value.tag_name}/${name}`)
      ? item.browser_download_url as string : null;
  };
  const appUrl = asset(policy.appName), webUrl = asset('www.bin');
  return appUrl && webUrl ? { tag: value.tag_name, version: policy.versionPrefix + value.tag_name.slice(policy.tagPrefix.length),
    prerelease: value.prerelease, releaseUrl: value.html_url, appUrl, webUrl } : null;
}

@Injectable({ providedIn: 'root' })
export class QaxeUpdateService {
  private readonly publicHttp: HttpClient;

  constructor(backend: HttpBackend) {
    // Public catalog requests must never carry miner OTP/session/auth headers.
    this.publicHttp = new HttpClient(backend);
  }

  private releases(page = 1): Observable<unknown[]> {
    return this.publicHttp.get<unknown>(`${CATALOG}?per_page=100&page=${page}`).pipe(
      switchMap(rows => {
        if (!Array.isArray(rows)) return throwError(() => new Error('Invalid release catalog.'));
        if (rows.length < 100) return of(rows);
        if (page >= 5) return throwError(() => new Error('Release catalog exceeded its limit.'));
        return this.releases(page + 1).pipe(map(next => rows.concat(next)));
      }),
    );
  }

  check(deviceModel: string, currentVersion: string): Observable<QaxeUpdateCheck> {
    const policy = releasePolicyForModel(deviceModel);
    if (!policy) return of({ status: 'unsupported', release: null });
    const tagPattern = new RegExp(`^${policy.tagPrefix}${SEMANTIC_SUFFIX}$`);
    const versionPattern = new RegExp(`^${policy.versionPrefix}${SEMANTIC_SUFFIX}$`);
    return this.releases().pipe(timeout(15000), map(rows => {
      const releases = rows.map(row => eligibleRelease(row, policy)).filter((release): release is QaxeRelease => release !== null)
        .sort((first, second) => compareVersions(versionParts(second.tag, tagPattern)!, versionParts(first.tag, tagPattern)!));
      const release = releases[0] || null;
      if (!release) return { status: 'no-release', release: null };
      const current = versionParts(currentVersion, versionPattern);
      const comparison = current ? compareVersions(current, versionParts(release.tag, tagPattern)!) : -1;
      return { status: comparison === 0 ? 'up-to-date' : comparison > 0 ? 'newer-build' : 'available', release };
    }));
  }
}
