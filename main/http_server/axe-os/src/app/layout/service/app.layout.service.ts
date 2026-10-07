import { Injectable, effect, signal } from '@angular/core';
import { BehaviorSubject, Subject } from 'rxjs';
import { ThemeService } from '../../services/theme.service';
import { LocalStorageService } from '../../local-storage.service';

const STATIC_MENU_DESKTOP_INACTIVE = 'STATIC_MENU_DESKTOP_INACTIVE'

export interface AppConfig {
    inputStyle: string;
    colorScheme: string;
    ripple: boolean;
    menuMode: string;
    scale: number;
}

interface LayoutState {
    staticMenuDesktopInactive: boolean;
    overlayMenuActive: boolean;
    profileSidebarVisible: boolean;
    configSidebarVisible: boolean;
    staticMenuMobileActive: boolean;
    menuHoverActive: boolean;
}

@Injectable({
    providedIn: 'root',
})
export class LayoutService {
    private darkTheme = {
        '--surface-a': '#101925',
        '--surface-b': '#0b121e',
        '--surface-c': 'rgba(255,255,255,0.03)',
        '--surface-d': '#293747',
        '--surface-e': '#101925',
        '--surface-f': '#101925',
        '--surface-ground': '#0b121e',
        '--surface-section': '#0b121e',
        '--surface-card': '#101925',
        '--surface-overlay': '#101925',
        '--surface-border': '#293747',
        '--surface-0': '#0b121e',
        '--surface-50': '#293747',
        '--surface-100': '#3d4a5d',
        '--surface-200': '#59697e',
        '--surface-300': '#7e90a6',
        '--surface-400': '#96a4b8',
        '--surface-500': '#a7b5c8',
        '--surface-600': '#bdcbdc',
        '--surface-700': '#cedaeb',
        '--surface-800': '#e0e8f3',
        '--surface-900': '#e8eef8',
        '--surface-hover': 'rgba(255,255,255,0.03)',
        '--text-color': '#e8eef8',
        '--text-color-secondary': '#96a4b8',
        '--maskbg': 'rgba(0,0,0,0.4)'
    };

    private lightTheme = {
        '--surface-a': '#e4eaf2',
        '--surface-b': '#f1f4f9',
        '--surface-c': 'rgba(19,33,51,0.04)',
        '--surface-d': '#cbd5e2',
        '--surface-e': '#e4eaf2',
        '--surface-f': '#e4eaf2',
        '--surface-ground': '#f1f4f9',
        '--surface-section': '#f1f4f9',
        '--surface-card': '#e4eaf2',
        '--surface-overlay': '#e4eaf2',
        '--surface-border': '#cbd5e2',
        '--surface-0': '#f1f4f9',
        '--surface-50': '#e4eaf2',
        '--surface-100': '#cbd5e2',
        '--surface-200': '#adbaca',
        '--surface-300': '#8494a9',
        '--surface-400': '#536379',
        '--surface-500': '#465a70',
        '--surface-600': '#35485f',
        '--surface-700': '#273b51',
        '--surface-800': '#1d2f44',
        '--surface-900': '#132133',
        '--surface-hover': 'rgba(19,33,51,0.04)',
        '--text-color': '#132133',
        '--text-color-secondary': '#536379',
        '--maskbg': 'rgba(0,0,0,0.2)'
    };

    _config: AppConfig = {
        ripple: false,
        inputStyle: 'outlined',
        menuMode: 'static',
        colorScheme: 'dark',
        scale: 14,
    };

    config = signal<AppConfig>(this._config);

    state: LayoutState = {
        staticMenuDesktopInactive: false,
        overlayMenuActive: false,
        profileSidebarVisible: false,
        configSidebarVisible: false,
        staticMenuMobileActive: false,
        menuHoverActive: false,
    };

    isWideView = signal<boolean>(this.localStorageService.getBool('DASHBOARD_WIDE_VIEW'));

    private overlayOpen = new Subject<any>();
    overlayOpen$ = this.overlayOpen.asObservable();

    private staticMenuDesktopInactive$ = new BehaviorSubject<boolean>(this.state.staticMenuDesktopInactive);

    constructor(
      private themeService: ThemeService,
      private localStorageService: LocalStorageService
    ) {
        // Load saved theme settings from NVS
        this.themeService.getThemeSettings().subscribe(
            settings => {
                if (settings) {
                    this._config = {
                        ...this._config,
                        colorScheme: settings.colorScheme,
                    };
                    // Apply accent colors if they exist
                    if (settings.accentColors) {
                        Object.entries(settings.accentColors).forEach(([key, value]) => {
                            document.documentElement.style.setProperty(key, value);
                        });
                    }
                }
                // Update signal with config
                this.config.set(this._config);
                // Apply initial theme
                this.changeTheme();
            },
            error => {
                console.error('Error loading theme settings:', error);
                // Use default theme on error
                this.config.set(this._config);
                this.changeTheme();
            }
        );

        effect(() => {
            const config = this.config();
            this.changeTheme();
            this.changeScale(config.scale);
            this.handleStaticMenuDesktopInactivity();
        });
    }

    onMenuToggle() {
        if (this.isOverlay()) {
            this.state.overlayMenuActive = !this.state.overlayMenuActive;
            if (this.state.overlayMenuActive) {
                this.overlayOpen.next(null);
            }
        }

        if (this.isDesktop()) {
            this.state.staticMenuDesktopInactive =
                !this.state.staticMenuDesktopInactive;

            this.localStorageService.setBool(STATIC_MENU_DESKTOP_INACTIVE, this.state.staticMenuDesktopInactive);
            this.staticMenuDesktopInactive$.next(this.state.staticMenuDesktopInactive);
        } else {
            this.state.staticMenuMobileActive =
                !this.state.staticMenuMobileActive;

            if (this.state.staticMenuMobileActive) {
                this.overlayOpen.next(null);
            }
        }
    }

    showProfileSidebar() {
        this.state.profileSidebarVisible = !this.state.profileSidebarVisible;
        if (this.state.profileSidebarVisible) {
            this.overlayOpen.next(null);
        }
    }

    showConfigSidebar() {
        this.state.configSidebarVisible = true;
    }

    isOverlay() {
        return this.config().menuMode === 'overlay';
    }

    isDesktop() {
        return window.innerWidth > 991;
    }

    isMobile() {
        return !this.isDesktop();
    }

    changeTheme() {
        const config = this.config();

        // Apply light/dark theme variables
        const themeVars = config.colorScheme === 'light' ? this.lightTheme : this.darkTheme;
        Object.entries(themeVars).forEach(([key, value]) => {
            document.documentElement.style.setProperty(key, value);
        });

        // Orbit defaults are presentation-only. Never write device preferences on load.
        const light = config.colorScheme === 'light';
        const accent = light ? '#006681' : '#20cfff';
        const button = light ? '#006681' : '#17677f';
        const hover = light ? '#00516a' : '#207e99';
        const accentVars = {
            '--primary-color': accent,
            '--primary-color-text': '#ffffff',
            '--highlight-bg': light ? '#d4eaf5' : '#103044',
            '--highlight-text-color': light ? '#132133' : '#e8eef8',
            '--focus-ring': `0 0 0 0.2rem ${light ? 'rgba(0,102,129,.25)' : 'rgba(32,207,255,.25)'}`,
            '--slider-bg': themeVars['--surface-d'],
            '--slider-range-bg': accent,
            '--slider-handle-bg': accent,
            '--progressbar-bg': themeVars['--surface-d'],
            '--progressbar-value-bg': accent,
            '--checkbox-border': button,
            '--checkbox-bg': button,
            '--checkbox-hover-bg': hover,
            '--button-bg': button,
            '--button-hover-bg': hover,
            '--button-focus-shadow': `0 0 0 2px ${themeVars['--surface-card']}, 0 0 0 4px ${accent}`,
            '--togglebutton-bg': button,
            '--togglebutton-border': `1px solid ${button}`,
            '--togglebutton-hover-bg': hover,
            '--togglebutton-hover-border': `1px solid ${hover}`,
            '--togglebutton-text-color': '#ffffff',
        };
        Object.entries(accentVars).forEach(([key, value]) => document.documentElement.style.setProperty(key, value));

        // Load theme settings from NVS
        this.themeService.getThemeSettings().subscribe(
            settings => {
                if (settings && settings.accentColors) {
                    Object.entries(settings.accentColors).forEach(([key, value]) => {
                        document.documentElement.style.setProperty(key, value);
                    });
                }
            },
            error => console.error('Error loading accent colors:', error)
        );
    }

    changeScale(value: number) {
        document.documentElement.style.fontSize = `${value}px`;
    }

    handleStaticMenuDesktopInactivity() {
        if (!this.isDesktop()) {
            return;
        }

        this.state.staticMenuDesktopInactive = this.localStorageService.getBool(STATIC_MENU_DESKTOP_INACTIVE);
        this.staticMenuDesktopInactive$.next(this.state.staticMenuDesktopInactive);
    }

    toggleWideView() {
        this.isWideView.set(!this.isWideView());
        this.localStorageService.setBool('DASHBOARD_WIDE_VIEW', this.isWideView());
    }

    getStaticMenuDesktopInactive$() {
      return this.staticMenuDesktopInactive$.asObservable();
    }
}
