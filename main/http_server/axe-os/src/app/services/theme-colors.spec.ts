import { THEME_COLORS, themeAccentColors } from './theme-colors';

function contrast(first: string, second: string): number {
  const luminance = (hex: string) => {
    const channels = [1, 3, 5].map(i => parseInt(hex.slice(i, i + 2), 16) / 255)
      .map(c => c <= .04045 ? c / 12.92 : ((c + .055) / 1.055) ** 2.4);
    return .2126 * channels[0] + .7152 * channels[1] + .0722 * channels[2];
  };
  const values = [luminance(first), luminance(second)].sort((a, b) => a - b);
  return (values[1] + .05) / (values[0] + .05);
}

describe('Theme colours', () => {
  it('uses one derived accent for controls without redefining warning or fault colours', () => {
    for (const theme of THEME_COLORS) {
      const colors = themeAccentColors(theme.primaryColor, 'dark');
      expect(colors['--button-bg']).toBe(colors['--primary-color']);
      expect(colors['--slider-range-bg']).toBe(colors['--primary-color']);
      expect(colors['--progressbar-value-bg']).toBe(colors['--primary-color']);
      expect(Object.keys(colors).some(key => /warning|danger|success/.test(key))).toBeFalse();
    }
  });
  it('keeps all palette accents readable against actual page and card surfaces in both schemes', () => {
    for (const scheme of ['light', 'dark']) for (const theme of THEME_COLORS) {
      const colors = themeAccentColors(theme.primaryColor, scheme);
      const accent = colors['--primary-color'];
      expect(accent).toMatch(/^#[0-9a-f]{6}$/);
      for (const surface of scheme === 'light' ? ['#e4eaf2','#f1f4f9'] : ['#101925','#0b121e']) {
        expect(contrast(accent, surface)).withContext(`${scheme} ${theme.name} ${surface}`).toBeGreaterThanOrEqual(4.5);
      }
    }
  });
  it('keeps normal and hovered button labels readable for every colour and scheme', () => {
    for (const scheme of ['light', 'dark']) for (const theme of THEME_COLORS) {
      const colors = themeAccentColors(theme.primaryColor, scheme);
      const foreground = colors['--primary-color-text'];
      expect(contrast(colors['--button-bg'], foreground)).withContext(`${scheme} ${theme.name} normal`).toBeGreaterThanOrEqual(4.5);
      expect(contrast(colors['--button-hover-bg'], foreground)).withContext(`${scheme} ${theme.name} hover`).toBeGreaterThanOrEqual(4.5);
    }
  });
});
