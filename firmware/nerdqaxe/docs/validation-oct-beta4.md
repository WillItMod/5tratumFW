# OctAxe Beta4 validation

Paired version: **`5tratumFW-oct-0.1.0-beta.4`**, BETA tag **`octaxe-v0.1.0-beta.4`**. Exact compiled/source-archive commit: **`689b2c8fcd01c4b507be102bb1caed22f4552738`**.

Compatibility remains **NerdOCTAXE-γ / NERDOCTAXEGAMMA / BM1370×8 / ESP32-S3 with the original 320×170 display**. Physical evidence remains limited to the owned PCB 2.2 assembly; no other revision is newly qualified. The exact matched pair completed supervised OTA, configuration/served-asset checks and the short observations below on **14 October 2026**. The [previously qualified pair](validation-oct-beta3.md) retains its own historical evidence.

## Change and retained behavior

The physical screen replaces the plain candidate notice with six approved glazed-donut designs and short captions. Each new event chooses its design once, avoids an immediate repeat and keeps it through redraws. One bounded bob, shine and confetti burst finishes on a static card. **Block candidate / Awaiting verification** distinguishes this local notice from pool acceptance, canonical-chain proof and a confirmed win. A physical button dismisses it.

Errors, thermal protection, shutdown, enrollment and identification outrank or cancel the celebration. No extra animation timers retain deleted widgets. Mining drivers, regulator sequencing, clocks/voltage, NVS keys, both fans, pools, twenty named slots, both schedulers and protection behavior remain unchanged. Native A/B sessions still share the 8-chip chain; independent physical-chip work assignment remains unsupported.

## Matched package and build

| Asset | Bytes | SHA256 |
| --- | ---: | --- |
| `esp-miner-NerdOCTAXE-Gamma.bin` | 3,582,720 | `603c6041bd5f7cfa521b6a026d1fdbe2ec8e2e3e48003168ab2e1507e47bfe8d` |
| `www.bin` | 3,145,728 | `f200a2a350eef3a66fbe1d42ab37162c695a392776572b51f6df51168896cfd0` |
| `5tratumFW-NerdOCTAXE-Gamma-5tratumFW-oct-0.1.0-beta.4-source.zip` | 24,515,550 | `a341adc561af306e6afe3659aca0037970d2f5aabe4808614c55bf49938b8c1f` |

Use this application's matching `www.bin`, manifest, SHA256SUMS, web-budget report and corresponding GPL source. These are the checked/uploaded local package bytes; CI is an independent same-source build. Later documentation commits do not change the compiled/source-archive receipt above or these package bytes. No factory, NVS or experimental larger-screen image is included.

The pinned full helper passed **103 frontend tests, 20 web-control checks and all 23 host entry points**, including 145 unittest cases plus direct production harness checks. The optional external production-MUX-source class skipped when no reviewed MUX source was supplied; this is not a live MUX pass. Actual LVGL 8.3.11 rendering passed **54 layout pages at each of 320×170 and 480×320**, and all six candidate variants, static/priority cases, 12 production method bodies, three priority-state cases and **1,024 replacement events per viewport**. Address/Undefined/LeakSanitizer checks passed in Linux CI; the fixture explicitly releases LVGL's driver-owned draw context at teardown.

[Full OctAxe CI passed](https://github.com/WillItMod/5tratumFW/actions/runs/38625384974) at exact source `689b2c8fcd01c4b507be102bb1caed22f4552738`. Node **24.14.0** and pinned ESP-IDF **5.5.3** were used. Target/configuration checks select only the correct board, `TDISPLAY_S3_320X170` / `FIVETRATUM_LCD_PROFILE=0`, with experimental, diagnostic-driver and capture modes off. ESP descriptor, segment checksum, appended SHA256, package checksums, all **1,258 archived source files** and all **94 web asset records** were verified; reconstructed WWW matched byte-for-byte.

The application is **3,582,720 bytes**, leaving **611,584 bytes** in its 4 MiB slot. WWW is **3,145,728 bytes** with a **1,683,238-byte payload**. The 480×320 native views test responsive layout only: those profiles remain private/experimental, excluded from public helpers/package admission, with **no public 480×320 binaries** or physical compatibility claim.

## Supervised installed-device observation — 14 October 2026

The exact application/WWW versions and source-bound web build matched. All **94 served web records** matched the immutable family package, comparing compressed records to gzip wire bytes and raw records to decoded bytes where a gzip sibling is served. Every exported setting, ten named pool slots, ten named tuning slots, both schedules and original manual override matched the fresh private pre-update baseline. Retained operation remained **700 MHz / 1180 mV**, Dual pool **50/50**. These are the owned unit's saved values, not recommended presets.

| Observation | First sample | Second sample |
| --- | ---: | ---: |
| Same-boot uptime | 94 s | 150 s |
| Accepted shares | 7 | 11 |
| Rejected shares | 0 | 0 |
| Rate | 11.321 TH/s | 11.411 TH/s |
| Shared ASIC board | 62.38 °C | 62.31 °C |
| Shared regulator external / internal | 51.19 / 63.38 °C | 53.62 / 66.00 °C |
| Fan 1 / fan 2 | 2351 / 2367 RPM | 2581 / 2610 RPM |

Accepted shares increased by **4 over 56 seconds** without a reboot or pause in that window. All 8 hardware-counter estimates were fresh in both samples; unavailable individual chip temperatures remained null, without copying the shared board/regulator temperatures into chips. The scheduler clock was valid, with no error or pending transition, and the unit reported neither shutdown nor paused mining.

Both native A/B MUX advertisements were acknowledged, transport-connected and unexpired. Both supplied the actual **5TRAT** ticker and fresh forwarded-job context. Observed block context: **25793 in both samples; forwarded work ages 25–26 → 21 seconds**. This verifies the received route/coin/job feed, not canonical-chain confirmation, payout or assignment of an individual ASIC to a stream.

## Power outage and cold boot

The user-reported power outage occurred after the paired OTA/settings verification and before the first later read-only observation completed. That interrupted attempt makes no soak, same-boot share-growth or resume claim. Once the owner confirmed power/screens/fans had returned, the cold-boot capture confirmed matching application/web versions, no exported settings/slot/schedule differences, valid clock, no pending transition/error, manual override `none` and requested/applied pause false. The complete two-sample observation above was then taken in the new boot. The initial cold-boot read occurred at 66 seconds uptime.

## Manual power check

The first manual attempt captured applied pause and a firmware-reported change from **183.25 to 0.0625 W**, but a later simultaneous power-on reset across the four bench miners interrupted restoration. Its resume state was not observed, so it is not counted as a completed pause/resume test. The preceding two-sample mining window remains separate evidence; no continuous-soak claim spans either interruption.

A fresh retry then completed: requested/applied pause settled true, explicit resume settled false, and the temporary override was cleared back to `none`. Exported settings, twenty slots and both schedules still matched the original baseline. The retained operating point remained **700 MHz / 1180 mV**. Firmware-reported power changed **183.25 → 0.0625 → 184.75 W** (running → paused → resumed).

These are board/regulator telemetry estimates, **not wall-meter measurements or a claim that the complete miner consumes only the paused reading**. Controller, fans and networking remain distinct loads. This short manual test does not qualify a scheduled power-window boundary. With a disabled schedule, clearing an override alone retains the current request; the test explicitly restored running mode before clearing it, without changing firmware power policy.

## Physical evidence and limits

The owner confirmed normal physical screens and fans after power returned. No natural block candidate was observed or manufactured during this bench. Six-design candidate graphics/motion and priority handling remain **simulated native fixture evidence**, separate from actual physical candidate-celebration/readability qualification. No fake block, candidate endpoint or payout claim was used.

Short OTA, cold-boot, mining and manual-control observations do not establish sustained soak, all protection/thermal faults, complete recovery or scheduled-boundary operation. Exported settings comparisons do not back up raw NVS or unexported password bytes. Network/device identifiers, worker names, credentials and private recovery material remain outside public records. Immutable pre-bench manifest flags stay unchanged; this dated record adds evidence without rewriting the package.

No other PCB revision, regulator population, flash geometry, larger display or independent ASIC mining is newly qualified. Central OS/MUX release admission remains a separate exact-pair/source check; earlier admission does not automatically admit this version/source.
