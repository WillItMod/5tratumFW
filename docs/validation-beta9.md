# Gamma Beta9 candidate-screen validation

Paired version: **`5tratumFW-0.1.0-beta.9`**, BETA tag **`v0.1.0-beta.9`**. Exact compiled/source-archive commit: **`689b2c8fcd01c4b507be102bb1caed22f4552738`**.

Scope: **Bitaxe Gamma PCB 601/602, BM1370×1, ESP32-S3, stock 128×32 OLED**. The exact matched pair completed supervised OTA, settings/served-asset checks and the short observations below on **14 October 2026**. [Beta8 validation](validation-beta8.md) remains historical evidence for its own pair.

## Change and retained behavior

The local block-candidate page uses one of six monochrome glazed donuts with a short caption, recorded difficulty and **BLOCK CANDIDATE / Awaiting pool**. Artwork stays consistent during redraws and safety interruptions. A new event chooses another variant without an immediate repeat. Sprinkle/bob movement stops after six seconds; the existing acknowledgement still clears the notice. The six 26×26 palette/bitmap assets total 672 flash bytes and need no runtime PNG decoder. Identification has its own branded view; thermal/hardware faults, self-test, update and setup pages retain priority. See [display implementation and limits](display-celebrations.md).

Candidate target checks, counters and acknowledgement semantics remain unchanged. This notice does not assert pool acceptance or canonical-chain proof. No NVS key, operating point, hardware/partition profile, mining driver, pool routing, scheduler or MUX protocol changes. Gamma retains Primary/fallback routing; no native Dual A/B or independent ASIC mining is added.

## Matched package

| Asset | Bytes | SHA256 |
| --- | ---: | --- |
| `esp-miner.bin` | 1,719,632 | `ea0d7a8e3df265dd2f54bafd57375dabea8fbc4c82b885a80d4e2c246553d1e3` |
| `www.bin` | 3,145,728 | `b2c6ebda390701e79b6d0b39e49bb8cced6e99bc68b8363ed591d420485b3e16` |
| `5tratumFW-0.1.0-beta.9-source.zip` | 28,075,454 | `9d80492c47dd8f6a7dbfa2e8abd797e3cbfd8a91416d04d98b375231c5ef97e5` |

Use the matched application/WWW, manifest, SHA256SUMS, web-budget report and corresponding GPL source. No factory, bootloader, partition-table or NVS image is distributed. These are the local checked/uploaded package hashes; same-source CI builds are separate evidence. Later documentation commits do not change the compiled/source-archive receipt above or the package bytes.

## Build and offline evidence

The pinned full Beta9 build passed **140 frontend tests, ten host entry points, 36 actual LVGL 9.3 OLED fixture views and production `screen.c` lifecycle checks**. Native cases cover six variants, event stability, no immediate repeat, bounded motion, separate identification and priority handling. The ESP32-S3 application descriptor and package hashes passed verification; WWW recreation matched byte-for-byte. All **2,027 corresponding-source inventory hashes** and all six original artwork hashes were verified, with no private or factory artifacts. Node **24.14.0** and pinned ESP-IDF **5.5.3** were used.

The application is **1,719,632 bytes** in its 4 MiB OTA slot, leaving **2,474,672 bytes (59%)**. WWW is **3,145,728 bytes**; its payload is **621,837 bytes**, within the recorded web budget. [Gamma CI passed](https://github.com/WillItMod/5tratumFW/actions/runs/38625385017) at exact source `689b2c8fcd01c4b507be102bb1caed22f4552738`. CI builds/tests/packages; it does not flash a miner.

Earlier development source `4131bc9bf3747ad41296346a27c25e861553d22d` and its [CI](https://github.com/WillItMod/5tratumFW/actions/runs/38606688230) retained the old Beta8 marker. They are design evidence only, separate from this uniquely versioned Beta9 pair.

## Supervised paired update and short observation — 14 October 2026

Both units returned with matching Beta9 application/web versions and **all 11 served web asset records** matching the package. Their exported settings, ten pool slots, ten tuning slots, pool/power schedules and original manual-override state matched fresh pre-update baselines. The 601 retained **580 MHz / 1155 mV**, and the 602 retained **600 MHz / 1150 mV** including its saved overclock choice. These are retained owner settings, not presets or recommended operating points.

| Unit | Same-boot sample interval | Accepted shares | Rate observed | ASIC board / regulator external | Fan |
| --- | ---: | --- | --- | --- | --- |
| Owned Gamma 601 | 52 seconds | 14 → 19, +5; zero rejected | 1.086 → 1.273 TH/s | 59.9 → 60.1 °C / 49 → 50 °C | 5953 → 5980 RPM |
| Owned Gamma 602 | 52 seconds | 5 → 6, +1; zero rejected | 1.336 → 1.170 TH/s | 59.9 → 60.1 °C / 59 → 60 °C | 5177 → 5258 RPM |

Both observations had advancing same-boot uptime, valid scheduler clocks, no pending power transition and no scheduler error or reported overheating. Primary MUX-v1 advertisements were transport-connected, acknowledged and unexpired; their ages changed from 11 to 3 seconds. Gamma's legacy coin/job fields remained null. This is fresh MUX connection evidence, not enhanced coin/block injection or payout verification.

## Power outage, cold boot and manual power control

A user-reported power outage occurred after both paired updates and the same-boot observations had completed. It interrupted an earlier 601 power-return attempt before a resume write; that attempt is not counted as a completed pause/resume check. After normal power returned, both units retained the exact paired versions and exported settings/slots/schedules. Their scheduler clocks were valid, manual override was `none`, requested/applied pause was false and no transition/error remained. The 601 API reported a power-on reset; short cold-boot reads showed mining/share progress on both units. The owner confirmed the normal physical screens and fans after power returned.

A fresh manual pause/resume test then completed separately on each unit. Pause applied; explicit resume applied; the temporary override was cleared back to `none`, with all exported settings and schedules unchanged. With a disabled schedule, clearing an override alone correctly retains the current request, so the test explicitly resumes before clearing it; no firmware power-policy change was made.

| Unit | Before pause | Paused | Resumed |
| --- | ---: | ---: | ---: |
| Gamma 601 | 17.65 W | 4.99 W | 17.05 W |
| Gamma 602 | 20.76 W | 4.99 W | 19.86 W |

These are **firmware-reported board power readings**, not wall-meter measurements or total-system energy guarantees. The observations show a lower reported load during pause; they do not qualify a scheduled power-window boundary.

A later simultaneous power-on reset across the four bench miners interrupted the first OctAxe power-return test. The completed Gamma update, observation and manual-control windows above remain separate evidence; no continuous-soak claim spans either interruption.

## Physical evidence and limits

The owner confirmed normal screens/fans; no natural block candidate was observed or manufactured during this bench. Six-design candidate graphics, motion, readability and safety-priority cases remain **simulated native fixture evidence**, not physical candidate-celebration qualification. No fake block, proof, candidate API or payout claim was used.

Short OTA, cold-boot and manual power observations do not qualify sustained soak, complete thermal/protection faults, all recovery cases, unexported NVS/password bytes or upstream payout verification. Public records exclude device/network identifiers, worker names, credentials and private backups. The immutable manifest's `hardware_tested: false` remains its pre-bench audit fact; this dated record adds later evidence without rewriting the package.

The **480×320 QAxe profiles remain experimental/private**, with no public 480×320 binaries and no compatibility claim from this Gamma release. Central OS/MUX release admission remains a separate exact-pair/source check. [Installation](installation.md) and [settings preservation](settings-preservation.md) describe the compatible update path.
