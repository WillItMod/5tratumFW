# QAxe Beta6 validation

Paired version: **`5tratumFW-qa-0.1.0-beta.6`**, BETA tag **`qaxe-v0.1.0-beta.6`**. Exact compiled/source-archive commit: **`689b2c8fcd01c4b507be102bb1caed22f4552738`**.

Compatibility remains **NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4 / ESP32-S3 with the original 320×170 display**. The owned QAxe's physical PCB revision remains unidentified. The exact matched pair completed supervised OTA, configuration/served-asset checks and the short observations below on **14 October 2026**. The [previously qualified pair](validation-qa-beta5.md) retains its own historical evidence.

## Change and retained behavior

The physical screen replaces the plain candidate notice with six approved glazed-donut designs and short captions. Each new event chooses its design once, avoids an immediate repeat and keeps it through redraws. One bounded bob, shine and confetti burst finishes on a static card. **Block candidate / Awaiting verification** distinguishes this local notice from pool acceptance, canonical-chain proof and a confirmed win. A physical button dismisses it.

Errors, thermal protection, shutdown, enrollment and identification outrank or cancel the celebration. No extra animation timers retain deleted widgets. Mining drivers, regulator sequencing, clocks/voltage, NVS keys, both fans, pools, twenty named slots, both schedulers and protection behavior remain unchanged. Native A/B sessions still share the 4-chip chain; independent physical-chip work assignment remains unsupported.

## Matched package and build

| Asset | Bytes | SHA256 |
| --- | ---: | --- |
| `esp-miner-NerdQAxe++.bin` | 3,576,896 | `1d91ac330daf2c2a9941535431524ae542d5c470a105af0536da1cf13507014f` |
| `www.bin` | 3,145,728 | `a7b468b41eb28c2cd26634d17c27bc3417bcce0c3d1bbbd5e0dbd75b15be1c8a` |
| `5tratumFW-NerdQAxe++-5tratumFW-qa-0.1.0-beta.6-source.zip` | 24,515,550 | `7ec21d141872cfc47596f4051741aab6d91098ef55babbda3fc41fccf7b23e20` |

Use this application's matching `www.bin`, manifest, SHA256SUMS, web-budget report and corresponding GPL source. These are the checked/uploaded local package bytes; CI is an independent same-source build. Later documentation commits do not change the compiled/source-archive receipt above or these package bytes. No factory, NVS or experimental larger-screen image is included.

The pinned full helper passed **103 frontend tests, 20 web-control checks and all 23 host entry points**, including 145 unittest cases plus direct production harness checks. The optional external production-MUX-source class skipped when no reviewed MUX source was supplied; this is not a live MUX pass. Actual LVGL 8.3.11 rendering passed **54 layout pages at each of 320×170 and 480×320**, and all six candidate variants, static/priority cases, 12 production method bodies, three priority-state cases and **1,024 replacement events per viewport**. Address/Undefined/LeakSanitizer checks passed in Linux CI; the fixture explicitly releases LVGL's driver-owned draw context at teardown.

[Full QAxe CI passed](https://github.com/WillItMod/5tratumFW/actions/runs/38625384990) at exact source `689b2c8fcd01c4b507be102bb1caed22f4552738`. Node **24.14.0** and pinned ESP-IDF **5.5.3** were used. Target/configuration checks select only the correct board, `TDISPLAY_S3_320X170` / `FIVETRATUM_LCD_PROFILE=0`, with experimental, diagnostic-driver and capture modes off. ESP descriptor, segment checksum, appended SHA256, package checksums, all **1,258 archived source files** and all **94 web asset records** were verified; reconstructed WWW matched byte-for-byte.

The application is **3,576,896 bytes**, leaving **617,408 bytes** in its 4 MiB slot. WWW is **3,145,728 bytes** with a **1,683,235-byte payload**. The 480×320 native views test responsive layout only: those profiles remain private/experimental, excluded from public helpers/package admission, with **no public 480×320 binaries** or physical compatibility claim.

## Supervised installed-device observation — 14 October 2026

The exact application/WWW versions and source-bound web build matched. All **94 served web records** matched the immutable family package, comparing compressed records to gzip wire bytes and raw records to decoded bytes where a gzip sibling is served. Every exported setting, ten named pool slots, ten named tuning slots, both schedules and original manual override matched the fresh private pre-update baseline. Retained operation remained **500 MHz / 1130 mV**, Dual pool **50/50**. These are the owned unit's saved values, not recommended presets.

| Observation | First sample | Second sample |
| --- | ---: | ---: |
| Same-boot uptime | 177 s | 235 s |
| Accepted shares | 12 | 15 |
| Rejected shares | 0 | 0 |
| Rate | 3.961 TH/s | 4.064 TH/s |
| Shared ASIC board | 55.75 °C | 55.44 °C |
| Shared regulator external / internal | 54.88 / 97.62 °C | 54.88 / 97.62 °C |
| Fan 1 / fan 2 | 1928 / 3050 RPM | 1934 / 3036 RPM |

Accepted shares increased by **3 over 58 seconds** without a reboot or pause in that window. All 4 hardware-counter estimates were fresh in both samples; unavailable individual chip temperatures remained null, without copying the shared board/regulator temperatures into chips. The scheduler clock was valid, with no error or pending transition, and the unit reported neither shutdown nor paused mining.

Both native A/B MUX advertisements were acknowledged, transport-connected and unexpired. Both supplied the actual **5TRAT** ticker and fresh forwarded-job context. Observed block context: **25790 → 25793**. This verifies the received route/coin/job feed, not canonical-chain confirmation, payout or assignment of an individual ASIC to a stream.

## Power outage and cold boot

The same-boot share/served-asset observation above finished before a later user-reported power outage. After power returned, the cold-boot capture confirmed matching application/web versions, no exported settings/slot/schedule differences, valid clock, no pending transition/error, manual override `none` and requested/applied pause false. The owner confirmed normal physical screens and fans. The cold-boot read occurred at 65 seconds uptime; it is separate from the preceding same-boot observation.

A later simultaneous power-on reset across the four bench miners interrupted the first OctAxe power-return test. The completed QAxe observation and manual-control windows are separate evidence; no continuous-soak claim spans either interruption.

## Manual power check

A separate manual pause/resume test completed after power returned. Requested/applied pause settled true, explicit resume settled false, and the temporary override was cleared back to `none`; exported settings, all slots and both schedules still matched. The retained operating point remained 500 MHz / 1130 mV.

Firmware-reported power changed **91.00 → 0.0625 → 91.38 W** (running → paused → resumed). These are board/regulator telemetry estimates, **not wall-meter measurements or a claim that the complete miner consumes only the paused reading**. Controller, fans and networking remain distinct loads. This short manual test does not qualify a scheduled power-window boundary.

With a disabled schedule, clearing an override alone retains the current request. The test explicitly restored running mode before clearing the override; no power-policy code change was made.

## Physical evidence and limits

The owner confirmed normal physical screens and fans after power returned. No natural block candidate was observed or manufactured during this bench. Six-design candidate graphics/motion and priority handling remain **simulated native fixture evidence**, separate from actual physical candidate-celebration/readability qualification. No fake block, candidate endpoint or payout claim was used.

Short OTA, cold-boot, mining and manual-control observations do not establish sustained soak, all protection/thermal faults, complete recovery or scheduled-boundary operation. Exported settings comparisons do not back up raw NVS or unexported password bytes. Network/device identifiers, worker names, credentials and private recovery material remain outside public records. Immutable pre-bench manifest flags stay unchanged; this dated record adds evidence without rewriting the package.

No other PCB revision, regulator population, flash geometry, larger display or independent ASIC mining is newly qualified. Central OS/MUX release admission remains a separate exact-pair/source check; earlier admission does not automatically admit this version/source.
