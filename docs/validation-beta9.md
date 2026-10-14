# Gamma Beta9 candidate-screen validation

Paired version: **`5tratumFW-0.1.0-beta.9`**, BETA tag **`v0.1.0-beta.9`**.
Scope: **Bitaxe Gamma PCB 601/602, BM1370×1, ESP32-S3, stock 128×32 OLED**.

**Release preparation: the new Beta9 build, package audit and supervised checks
on both Gamma revisions are pending.** Record the exact compiled/source commit,
package hashes and each unit's observations here after those checks. Earlier
Beta8 installations and version-retaining development builds do not qualify
this new Beta9 pair. [Beta8 validation](validation-beta8.md) remains unchanged.

## Change

The local block-candidate page uses one of six monochrome glazed donuts with a
short caption, recorded difficulty and **BLOCK CANDIDATE / Awaiting pool**.
Artwork stays consistent during redraws and safety interruptions. A new event
chooses another variant without an immediate repeat. Sprinkle/bob movement
stops after six seconds; the existing acknowledgement still clears the notice.

The six 26×26 palette/bitmap assets total 672 flash bytes and need no runtime
PNG decoder. Identify has its own branded view. Thermal/hardware faults,
self-test, firmware update and connection/setup pages retain their priority.
See [display implementation and limits](display-celebrations.md).

Candidate target checks, counters and acknowledgement semantics are retained.
This notice does not assert pool acceptance or canonical-chain verification.
No NVS key, operating point, hardware/partition profile, mining driver, pool
routing, scheduler or MUX protocol is changed by this display work. Gamma keeps
its Primary/fallback model; no native Dual A/B or independent chip work is added.

## Beta9 evidence to record

| Check | Status |
| --- | --- |
| Unique application marker and generated WWW version | Focused production-generator test passed on 14 October 2026 with Node 24.14.0; no target image built yet |
| Exact clean compiled/source commit | Pending source freeze |
| Pinned full frontend/host/ESP-IDF build | Pending Beta9 build |
| Native OLED rendering and production candidate lifecycle | Pending Beta9 run |
| Application descriptor, size, checksums and matched WWW recreation | Pending Beta9 package audit |
| Corresponding-source inventory, original art and no factory/NVS artifacts | Pending Beta9 package audit |
| Gamma 601 paired installation and settings retention | Pending supervised check |
| Gamma 602 paired installation and settings retention | Pending separate supervised check |
| Physical OLED readability and candidate presentation | Pending scoped observation |

For each installed unit, record matching application/web versions and served
web assets; exported settings, 10 pool slots, 10 tuning slots and both schedules;
its own clock, voltage and fan policy; and short mining/telemetry observations.
Keep owner changes separate from update-time preservation results. Do not turn
a synthetic candidate display check into an actual block or payout claim.

The immutable package manifest's `hardware_tested: false` records the pre-bench
build. Later dated installation evidence belongs in this document and does not
rewrite the package bytes or that flag. Exclude network addresses, identifiers,
worker names, credentials and private recovery material from public evidence.

## Earlier design evidence

Source `4131bc9bf3747ad41296346a27c25e861553d22d` passed Gamma CI
[38606688230](https://github.com/WillItMod/5tratumFW/actions/runs/38606688230)
with 140 frontend tests, ten host entry points, ESP32-S3 compilation, 36 actual
LVGL 9.3 OLED fixture views and production `screen.c` lifecycle checks. Those
artifacts retained the old Beta8 marker and are development evidence only.
Their checks covered six variants, same-event stability, no immediate repeat,
bounded motion, separate identify, priority handling and paired packaging.
They are not the new Beta9 images or installed-device evidence for Beta9.

## Package and limits

The Beta9 package must contain `esp-miner.bin`, its matching `www.bin`,
`5tratumFW-0.1.0-beta.9-source.zip`, `manifest.json`, `SHA256SUMS` and
`web-budget.json`. Record their exact size/hash receipt after the new build.
These are application/WWW OTA partition images, without factory, bootloader,
partition-table or NVS images. [Installation](installation.md) and
[settings preservation](settings-preservation.md) describe the compatible path.

Firmware compilation, host fixtures and short installation checks remain
separate from sustained soak, complete thermal/protection fault testing,
unexported NVS/credential preservation and upstream payout verification.
The experimental QAxe 480×320 profiles are a separate family and are not
included in Gamma compatibility or this release.
