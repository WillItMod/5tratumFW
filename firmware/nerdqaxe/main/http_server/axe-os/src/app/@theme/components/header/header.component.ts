import { Component, EventEmitter, Input, OnDestroy, OnInit, Output } from '@angular/core';
import { NbThemeService } from '@nebular/theme';
import { Subject, takeUntil } from 'rxjs';
import { SystemService } from '../../../services/system.service';

@Component({ selector: 'ngx-header', styleUrls: ['./header.component.scss'], templateUrl: './header.component.html' })
export class HeaderComponent implements OnInit, OnDestroy {
  private destroy$ = new Subject<void>();
  @Input() navigationOpen = false;
  @Output() navigationToggle = new EventEmitter<void>();
  currentTheme = 'dark';
  deviceModel = 'Miner';
  apActive = false;
  readonly themes = [{ value: 'dark', name: 'Dark' }, { value: 'default', name: 'Light' }];
  constructor(private theme: NbThemeService, private system: SystemService) {}
  ngOnInit(): void {
    const saved = this.readPreference('selectedTheme');
    this.changeTheme(this.themes.some(t => t.value === saved) ? saved! : 'dark');
    this.theme.onThemeChange().pipe(takeUntil(this.destroy$)).subscribe(({ name }) => this.currentTheme = name);
    this.system.getIdentifyV2().pipe(takeUntil(this.destroy$)).subscribe({
      next: info => { this.deviceModel = info?.deviceModel || 'Miner'; this.apActive = info?.apActive === true; }, error: () => {} });
  }
  changeTheme(value: string): void {
    if (!this.themes.some(t => t.value === value)) return;
    this.currentTheme = value;
    this.theme.changeTheme(value);
    try { localStorage.setItem('selectedTheme', value); } catch {}
  }
  private readPreference(key: string): string | null { try { return localStorage.getItem(key); } catch { return null; } }
  ngOnDestroy(): void { this.destroy$.next(); this.destroy$.complete(); }
}
