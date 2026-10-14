# Miner display celebrations and the larger QAxe screen

Gamma Beta9, NerdQAxe++ Beta6 and NerdOctAxe Gamma Beta4 update the local
block-candidate presentation. Their standard matched pairs were built from
clean source `689b2c8fcd01c4b507be102bb1caed22f4552738` and installed on the
owned Gamma 601, Gamma 602, QAxe and PCB 2.2 OctAxe on 14 October 2026.
Paired versions, served assets, retained settings, mining and cold starts were
checked; the owner confirmed normal physical screens and fans. See the
[release index](releases/README.md) for each family's exact validation scope.
The new candidate artwork and animation were exercised in native fixtures;
no fabricated block event was sent to a live miner.

## Candidate celebration

Six approved glazed donut images accompany six short captions. The firmware
chooses one variant for each new locally detected candidate, avoids an immediate
repeat, and keeps that choice during redraws. This changes presentation only:
local target checks, candidate counters and existing acknowledgements are not
changed. A candidate is not pool acceptance or canonical-chain verification.

On the 320×170 Nerd displays, the artwork sits alongside the caption. A 480×320
viewport uses a larger centered illustration. The artwork has a matching halo,
bounded bob/shine/sprinkle animation, then becomes a still dismissible card.
The text remains **Block candidate / Awaiting verification**. The existing
candidate-alert switch still applies. Faults, thermal/shutdown conditions,
enrollment and identification take priority; the celebration cannot cover them.
The brief animation creates no additional LVGL timers.

Gamma's qualified compact display is **128×32**, not 128×64. Its version uses a
26×26 monochrome donut, a short caption, recorded difficulty and **Awaiting pool**.
Sprinkle/bob movement stops after six seconds. Identification has its own view
and does not accidentally display candidate artwork. No new persistent setting
or tuning behavior is introduced.

The Nerd images occupy 115,200 RGB565-alpha pixel bytes in flash. Only the active
LVGL byte order is compiled. The OLED palette/bitmap payload is 672 bytes. There
is no runtime PNG decoder. The unchanged original PNGs, hashes and generation
prompts are in `firmware/nerdqaxe/branding/candidate-donuts/`. Regenerate both
formats from the full repository with `python3 tools/generate_candidate_assets.py`.
The standalone Nerd source archive can regenerate its own images using its
included `tools/generate_candidate_assets.py` and `branding/candidate-donuts/`.

## 480×320 QAxe support

The existing public QAxe and OctAxe profile stays **320×170**. The larger QAxe
layout uses the actual LVGL viewport, larger fonts and full-screen warnings,
while keeping A/B jobs, shared board temperatures, ASIC readings and setup pages.

Two explicitly selected **experimental QAxe-only display profiles** retain the
existing bus pins, board/regulator behavior and stock 16 MB partition table.
They differ in panel colour order, clock and mirroring. Neither is automatically
selected from the model name or reported board version. Public build/package
helpers reject these profiles. They have no published OTA pair and are not
admitted by the existing model-only updater policy.

“Rev 6.1 with a 3.5-inch screen” is not enough to choose between them or establish
the controller, wiring and flash geometry. A seller offers both screen sizes and
explicitly warns about compatibility. Hardware qualification and updater asset
selection are required before exposing an experimental installation path. See
the [profile details and pinned upstream references](../firmware/nerdqaxe/docs/experimental-480-display.md).

## Validation

The native fixtures run the actual LVGL libraries and production layout/runtime
code against synthetic readings; they do not contact or flash a miner.

```sh
python3 test/host/render_oled_native.py
cd firmware/nerdqaxe
python3 test/host/test_display_profiles.py
python3 test/host/render_display_native.py
python3 test/host/render_candidate_native.py
```

Coverage includes all six variants, repeated-event stability, animation settling,
stock geometry, both larger-display colour profiles, 320×170 and 480×320 pages,
button dismissal, safety/enrollment precedence and repeated allocation cleanup.
The Nerd family build helpers include the new profile tests and candidate
renderer, so those checks also run in their existing family CI jobs. Firmware
compilation and flash-size checks are separate from physical-display validation.
