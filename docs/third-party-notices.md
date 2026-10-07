# Third-party notices

5tratumFW retains the upstream ESP-Miner GPLv3 license in [LICENSE](../LICENSE). Individual dependencies and assets retain their own licenses. Keep the notices below, their referenced license texts and the original source headers with redistributed source and builds.

## Portfolio 6x8 display font

`main/lv_font_portfolio-6x8.c` is the upstream LVGL C conversion of **Mx437_Portfolio_6x8.ttf**, from **The Ultimate Oldschool PC Font Pack v2.2** by **VileR**. Its source header records the input font and the LVGL conversion options, including 1-bit rendering at size 8. Conversion from the source font to LVGL C glyph data is an adaptation; this notice marks that conversion. No new font authorship is claimed by 5tratumFW.

Original material: [VileR's Oldschool PC Font Pack](https://int10h.org/oldschool-pc-fonts/). The author's [license and attribution notes](https://int10h.org/oldschool-pc-fonts/readme/#legal) identify **Creative Commons Attribution-ShareAlike 4.0 International** for the font files. The converted font data retains that license. Its full legal text is included in [LICENSES/CC-BY-SA-4.0.txt](../LICENSES/CC-BY-SA-4.0.txt), with the [official license](https://creativecommons.org/licenses/by-sa/4.0/) as the primary reference.

`test/host/generate_oled_assets.py` further adapts the existing Portfolio glyphs by doubling selected digit, decimal-point and dash bitmaps. The generated `main/oled_display_assets.h` font arrays and descriptors retain the same font attribution and CC BY-SA 4.0 license. This notice marks the scaling and conversion as adaptations. The established 5tratum emblem image data in that generated header is separate from the Portfolio font data.

## PrimeIcons

The web interface bundles `main/http_server/axe-os/src/assets/fonts/primeicons.woff2` and `src/app/layout/styles/modules/primeicons.css`, inherited from the upstream interface. PrimeIcons is **MIT licensed**, with **Copyright (c) 2018–2021 PrimeTek** in the [upstream license](https://github.com/primefaces/primeicons/blob/master/LICENSE). The complete notice is included in [LICENSES/PrimeIcons-MIT.txt](../LICENSES/PrimeIcons-MIT.txt).

## libbase58

The inherited `components/stratum/base58.c` and `include/libbase58.h` carry the **Luke Dashjr, 2012–2014** copyright notice and identify the **MIT license**. The [original libbase58 COPYING file](https://github.com/luke-jr/libbase58/blob/master/COPYING) is included in [LICENSES/libbase58-MIT.txt](../LICENSES/libbase58-MIT.txt), preserving its original **Copyright (c) 2014 Luke Dashjr** statement.

## libsecp256k1 and other Bitcoin utilities

[bitcoin-core/secp256k1](https://github.com/bitcoin-core/secp256k1) remains pinned as a Git submodule at commit `0cdc758a56360bf58a851fe91085a327ec97685a` (v0.6.0). Its original MIT text is retained in `components/libsecp256k1/libsecp256k1/COPYING`; its examples and test data have additional notices retained in their original directories. Corresponding source packages include the pinned source and these files.

`components/stratum/segwit_addr.c` retains its in-file MIT notice and Pieter Wuille copyright. Keep source headers for these and other imported Bitcoin utility code intact.

## Build dependencies and web package notices

ESP-IDF, LVGL, `esp_lvgl_port`, the display driver, Angular, PrimeNG and the remaining build dependencies retain their own package licenses. `main/idf_component.yml`, the generated component lockfile and the npm lockfile identify their build inputs. Managed component sources are downloaded during the build; they are not copied from a local device workspace. Preserve the upstream package notices when redistributing those dependencies.

Angular's production build emits a `3rdpartylicenses.txt` notice file for bundled JavaScript dependencies; gzip processing stores its compressed form in the WWW bundle. Keep that generated notice in packaged web images. The bundled PrimeIcons font is listed separately above because it is a copied asset rather than an npm dependency in this checkout.

See [attribution](attribution.md) for the upstream ESP-Miner baseline and the scope of the 5tratumFW changes.
