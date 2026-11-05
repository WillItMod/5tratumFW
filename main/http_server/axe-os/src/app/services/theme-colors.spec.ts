import { THEME_COLORS, themeAccentColors } from './theme-colors';

describe('Theme colours', () => {
  it('uses each selected dark accent consistently for controls without redefining fault colours', () => {
    for (const theme of THEME_COLORS) {
      const colors = themeAccentColors(theme.primaryColor, 'dark');
      expect(colors['--primary-color']).toBe(theme.primaryColor);
      expect(colors['--button-bg']).toBe(theme.primaryColor);
      expect(colors['--slider-range-bg']).toBe(theme.primaryColor);
      expect(colors['--progressbar-value-bg']).toBe(theme.primaryColor);
      expect(Object.keys(colors).some(key => /warning|danger|success/.test(key))).toBeFalse();
    }
  });
  it('makes light accents readable and keeps derived colours valid hex strings', () => {
    for (const theme of THEME_COLORS) {
      const colors = themeAccentColors(theme.primaryColor, 'light');
      const hex = colors['--primary-color'];
      expect(hex).toMatch(/^#[0-9a-f]{6}$/);
      const channels = [1, 3, 5].map(i => parseInt(hex.slice(i, i+2),16)/255)
        .map(c => c <= .04045 ? c / 12.92 : ((c + .055)/1.055)**2.4);
      const luminance = .2126*channels[0]+.7152*channels[1]+.0722*channels[2];
      expect(1.05/(luminance+.05)).toBeGreaterThanOrEqual(4.5);
    }
  });
});
