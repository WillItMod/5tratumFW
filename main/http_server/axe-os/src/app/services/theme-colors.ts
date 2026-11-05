export const THEME_COLORS = [
  { name: 'Cyan', primaryColor: '#20cfff' },
  { name: 'Orange', primaryColor: '#f7931a' },
  { name: 'Red', primaryColor: '#f80421' },
  { name: 'Blue', primaryColor: '#2196f3' },
  { name: 'Green', primaryColor: '#4caf50' },
  { name: 'Purple', primaryColor: '#b340fa' },
];

function rgb(color: string): number[] {
  return [1, 3, 5].map(index => parseInt(color.slice(index, index + 2), 16));
}
function hex(channels: number[]): string {
  return '#' + channels.map(value => Math.round(value).toString(16).padStart(2, '0')).join('');
}
function luminance(color: string): number {
  const values = rgb(color).map(value => value / 255).map(value => value <= 0.04045 ? value / 12.92 : ((value + 0.055) / 1.055) ** 2.4);
  return values[0] * 0.2126 + values[1] * 0.7152 + values[2] * 0.0722;
}

function contrast(first: string, second: string): number {
  const light = Math.max(luminance(first), luminance(second));
  const dark = Math.min(luminance(first), luminance(second));
  return (light + 0.05) / (dark + 0.05);
}

/** Use the actual card surfaces, preserving the user's saved base colour. */
export function themeAccentColors(primaryColor: string, scheme: string): { [key: string]: string } {
  let accent = primaryColor;
  const light = scheme === 'light';
  const surface = light ? '#e4eaf2' : '#101925';
  for (let attempt = 0; attempt < 256 && contrast(accent, surface) < 4.5; attempt++) {
    accent = hex(rgb(accent).map(value => light ? Math.max(0, Math.min(value - 1, Math.round(value * 0.95)))
      : Math.min(255, Math.max(value + 1, Math.round(value + (255 - value) * 0.05)))));
  }
  const text = contrast(accent, '#07111b') >= 4.5 ? '#07111b'
    : contrast(accent, '#ffffff') >= 4.5 ? '#ffffff' : '#000000';
  // Keep the same readable foreground when the button is hovered.
  const hover = hex(rgb(accent).map(value => text === '#ffffff' ? value * 0.85 : value + (255 - value) * 0.12));
  const channels = rgb(accent).join(',');
  return {
    '--primary-color': accent, '--primary-color-text': text,
    '--highlight-bg': accent, '--highlight-text-color': text,
    '--focus-ring': `0 0 0 0.2rem rgba(${channels},0.25)`,
    '--slider-range-bg': accent, '--slider-handle-bg': accent,
    '--progressbar-value-bg': accent,
    '--checkbox-border': accent, '--checkbox-bg': accent, '--checkbox-hover-bg': hover,
    '--button-bg': accent, '--button-hover-bg': hover,
    '--button-focus-shadow': `0 0 0 2px var(--surface-card), 0 0 0 4px ${accent}`,
    '--togglebutton-bg': accent, '--togglebutton-border': `1px solid ${accent}`,
    '--togglebutton-hover-bg': hover, '--togglebutton-hover-border': `1px solid ${hover}`,
    '--togglebutton-text-color': text,
  };
}
