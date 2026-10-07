#!/usr/bin/env python3
"""Encode the approved artwork for LVGL and the browser; no runtime decoder."""
from pathlib import Path
import hashlib
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'branding/5tratumfw-logo.png'
EXPECTED = '90b7f3f9fee2d758f974d07cbfc6ba386614f387fc4c0855fb12957479f15158'


def encode(name, image):
    raw = bytearray()
    pixels = image.tobytes()
    for offset in range(0, len(pixels), 4):
        r, g, b, a = pixels[offset:offset+4]
        value = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        raw.extend((value & 255, value >> 8, a))
    rows = [', '.join(f'0x{x:02x}' for x in raw[i:i+24]) for i in range(0, len(raw), 24)]
    pixels = f'static const uint8_t {name}Pixels[] = {{\n    ' + ',\n    '.join(rows) + '\n};\n'
    descriptor = (f'static const lv_img_dsc_t {name} = {{\n'
                  f'    {{LV_IMG_CF_TRUE_COLOR_ALPHA, 0, 0, {image.width}, {image.height}}},\n'
                  f'    sizeof({name}Pixels), {name}Pixels\n}};\n')
    return pixels + descriptor


def main():
    if hashlib.sha256(SOURCE.read_bytes()).hexdigest() != EXPECTED:
        raise SystemExit('Approved logo source does not match')
    image = Image.open(SOURCE).convert('RGBA')
    full = image.resize((240, 147), Image.Resampling.LANCZOS)
    # Extract the original lettering/underline for narrow telemetry headers.
    # Startup and the recurring slide retain the complete original artwork.
    wordmark = image.crop((37, 550, 1544, 963)).resize((144, 40), Image.Resampling.LANCZOS)
    source = ('#pragma once\n#include "lvgl.h"\n\n'
              '// Approved full 5tratumFW artwork, encoded RGB565 + alpha.\n'
              '// Source: branding/5tratumfw-logo.png\n'
              f'// Source SHA256: {EXPECTED}\n'
              '// 123,120 bytes in flash; no decoder or image allocation at runtime.\n'
              'namespace FiveTratumDisplay {\n')
    source += encode('firmwareLogo', full) + encode('firmwareWordmark', wordmark)
    (ROOT/'main/displays/display_logo.h').write_text(source + '} // namespace FiveTratumDisplay\n')
    web = image.resize((320, 197), Image.Resampling.LANCZOS)
    web.save(ROOT/'main/http_server/axe-os/src/assets/5tratumfw-logo.png', optimize=True)
    print('Encoded full logo and compact wordmark; browser asset generated from the same approved source.')


if __name__ == '__main__':
    main()
