import { Injectable } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { environment } from '../../environments/environment';
import { BehaviorSubject, Observable, of } from 'rxjs';
import { map, switchMap, tap } from 'rxjs/operators';
import { themeAccentColors } from './theme-colors';

export interface ThemeSettings {
  colorScheme: string;
  primaryColor?: string;
  accentColors?: { [key: string]: string };
}

const DEFAULT_SETTINGS: ThemeSettings = { colorScheme: 'dark', primaryColor: '#20cfff' };

/** The device persists a scheme and one colour, never an arbitrary CSS map. */
function normalized(settings: Partial<ThemeSettings> | null, fallback = DEFAULT_SETTINGS): ThemeSettings {
  const source = settings && typeof settings === 'object' && !Array.isArray(settings) ? settings : {};
  const candidate = source.primaryColor || source.accentColors?.['--primary-color'];
  const primaryColor = typeof candidate === 'string' && /^#[0-9a-f]{6}$/i.test(candidate)
    ? candidate.toLowerCase() : fallback.primaryColor!;
  const colorScheme = source.colorScheme && ['dark', 'light'].includes(source.colorScheme) ? source.colorScheme : fallback.colorScheme;
  return { colorScheme, primaryColor, accentColors: themeAccentColors(primaryColor, colorScheme) };
}

function completeStoredSettings(settings: ThemeSettings | null): boolean {
  return !!settings && typeof settings === 'object' && !Array.isArray(settings)
    && ['dark', 'light'].includes(settings.colorScheme)
    && typeof settings.primaryColor === 'string' && /^#[0-9a-f]{6}$/i.test(settings.primaryColor);
}

@Injectable({ providedIn: 'root' })
export class ThemeService {
  private themeSettingsSubject = new BehaviorSubject<ThemeSettings>(normalized(DEFAULT_SETTINGS));
  private loadStateSubject = new BehaviorSubject({ loading: false, error: '', needsRepair: false });
  readonly loadState$ = this.loadStateSubject.asObservable();

  constructor(private http: HttpClient) {
    if (environment.production) this.reload();
  }

  getThemeSettings(): Observable<ThemeSettings> { return this.themeSettingsSubject.asObservable(); }

  reload(): void {
    if (!environment.production) return;
    this.loadStateSubject.next({ ...this.loadStateSubject.value, loading: true, error: '' });
    this.http.get<ThemeSettings>('/api/theme').subscribe({
      next: settings => {
        // An incomplete saved preference is repairable, unlike a failed request.
        this.themeSettingsSubject.next(normalized(settings));
        this.loadStateSubject.next({ loading: false, error: '', needsRepair: !completeStoredSettings(settings) });
      },
      error: () => this.loadStateSubject.next({ ...this.loadStateSubject.value, loading: false,
        error: 'Saved appearance could not be loaded. Retry before changing it.' }),
    });
  }

  saveThemeSettings(settings: Partial<ThemeSettings>): Observable<void> {
    const next = normalized({ ...this.themeSettingsSubject.value, ...settings } as ThemeSettings, this.themeSettingsSubject.value);
    // Match the firmware API and retain the accent when only the scheme changes.
    const payload = { colorScheme: next.colorScheme, primaryColor: next.primaryColor };
    const saved$ = environment.production
      ? this.http.post<void>('/api/theme', payload).pipe(
        switchMap(() => this.http.get<ThemeSettings>('/api/theme')),
        map(saved => {
          if (!completeStoredSettings(saved) || saved.colorScheme !== next.colorScheme || saved.primaryColor?.toLowerCase() !== next.primaryColor) {
            throw new Error('The miner did not confirm the selected appearance.');
          }
          return normalized(saved);
        })
      )
      : of(next);
    return saved$.pipe(tap(saved => {
      this.themeSettingsSubject.next(saved);
      this.loadStateSubject.next({ loading: false, error: '', needsRepair: false });
    }), map(() => void 0));
  }
}
