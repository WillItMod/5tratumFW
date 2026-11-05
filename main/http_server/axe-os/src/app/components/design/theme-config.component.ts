import { Component, OnInit, OnDestroy } from '@angular/core';
import { Subject } from 'rxjs';
import { takeUntil } from 'rxjs/operators';
import { ThemeService } from '../../services/theme.service';
import { THEME_COLORS } from '../../services/theme-colors';

@Component({
  selector: 'app-theme-config',
  template: `
    <section class="card appearance-settings" aria-labelledby="appearance-title">
      <h2 id="appearance-title">Appearance</h2>
      <p>Choose a scheme and accent colour, then apply them to this miner. They stay saved after a reload.</p>
      <p *ngIf="loading" role="status">Loading saved appearance…</p>
      <div *ngIf="loadError" role="alert" class="appearance-error">
        <p>{{loadError}}</p><button type="button" (click)="reload()">Retry loading</button>
      </div>
      <fieldset [disabled]="saving || loading || !!loadError">
        <legend>Colour scheme</legend>
        <div class="appearance-schemes">
          <label><input type="radio" name="colorScheme" value="dark" [(ngModel)]="selectedScheme" (ngModelChange)="selectionChanged()"> Dark</label>
          <label><input type="radio" name="colorScheme" value="light" [(ngModel)]="selectedScheme" (ngModelChange)="selectionChanged()"> Light</label>
        </div>
      </fieldset>
      <fieldset [disabled]="saving || loading || !!loadError">
        <legend>Accent colour</legend>
        <div class="appearance-colors">
          <button *ngFor="let theme of themes" type="button" class="appearance-swatch"
            [attr.aria-pressed]="theme.primaryColor === currentColor" (click)="selectColor(theme.primaryColor)">
            <span class="appearance-dot" [style.backgroundColor]="theme.primaryColor">
              <i *ngIf="theme.primaryColor === currentColor" class="pi pi-check" aria-hidden="true"></i>
            </span><span>{{theme.name}}</span>
          </button>
        </div>
      </fieldset>
      <div class="appearance-actions">
        <button pButton type="button" (click)="applyAppearance()" [disabled]="saving || loading || !!loadError || !hasChanges">
          {{saving ? 'Applying…' : 'Apply appearance'}}
        </button>
        <span *ngIf="!saving && !message && !saveError && !loading && !loadError" role="status">{{hasChanges ? 'Changes are ready to apply.' : 'This appearance is saved on the miner.'}}</span>
        <span *ngIf="message" role="status">{{message}}</span>
        <span *ngIf="saveError" class="appearance-error" role="alert">{{saveError}}</span>
      </div>
    </section>
  `,
  styleUrls: ['./design-component.scss']
})
export class ThemeConfigComponent implements OnInit, OnDestroy {
  readonly themes = THEME_COLORS;
  selectedScheme = 'dark';
  currentColor = '#20cfff';
  saving = false;
  loading = false;
  loadError = '';
  saveError = '';
  message = '';
  private savedScheme = 'dark';
  private savedColor = '#20cfff';
  private destroy$ = new Subject<void>();

  constructor(private themeService: ThemeService) {}

  ngOnInit(): void {
    this.themeService.getThemeSettings().pipe(takeUntil(this.destroy$)).subscribe(settings => {
      this.selectedScheme = this.savedScheme = settings.colorScheme;
      this.currentColor = this.savedColor = settings.primaryColor || '#20cfff';
    });
    this.themeService.loadState$.pipe(takeUntil(this.destroy$)).subscribe(state => {
      this.loading = state.loading; this.loadError = state.error;
    });
  }
  get hasChanges(): boolean { return this.selectedScheme !== this.savedScheme || this.currentColor !== this.savedColor; }
  selectionChanged(): void { this.message = ''; this.saveError = ''; }
  selectColor(color: string): void { this.currentColor = color; this.selectionChanged(); }
  reload(): void { this.themeService.reload(); }
  applyAppearance(): void {
    if (this.saving || this.loading || this.loadError || !this.hasChanges) return;
    this.saving = true; this.message = ''; this.saveError = '';
    this.themeService.saveThemeSettings({ colorScheme: this.selectedScheme, primaryColor: this.currentColor })
      .pipe(takeUntil(this.destroy$)).subscribe({
        next: () => { this.saving = false; this.message = 'Appearance saved and confirmed on the miner.'; },
        error: () => { this.saving = false; this.saveError = 'Could not confirm the saved appearance. Your selection is still here; try Apply appearance again.'; }
      });
  }
  ngOnDestroy(): void { this.destroy$.next(); this.destroy$.complete(); }
}
