# 5tratumFW for NerdQAxe++ — BETA

This source builds the QAxe-specific paired OTA images for **NerdQAxe++ / NERDQAXEPLUS2 with four BM1370 ASICs**. It is a GPLv3 derivative of [shufps ESP-Miner-NerdQAxePlus](https://github.com/shufps/ESP-Miner-NerdQAxePlus). Upstream authors and licenses are retained; the original documentation is in [upstream-readme](docs/upstream-readme.md).

Current QAxe BETA pair: `5tratumFW-qa-0.1.0-beta.8`, with the standard **320×170 display** and an explicit experimental **480×320 RGB/BGR screen selector**. This image is separate from Gamma 601/602 and OctAxe. The exact standard-screen pair completed a short supervised bench on the owned test unit, whose PCB revision remains unidentified. Read [Beta8 release notes](docs/releases/qaxe-v0.1.0-beta.8.md) and the [28 October validation](../../docs/validation-aligned-20261028.md). Earlier records retain their own release evidence.

The current QAxe Beta8 and OctAxe Beta5 pairs were compiled and packaged from clean source `1a34b24100e1238e331d8145a70f6b08f85f943d`. Both use the shufps **v1.1.1-rc1** base (`bfd07393918ce1b464faf1b7bd1ecd11343197f4`), pinned ESP-IDF 5.5.3 and the pinned certificate-bundle update. Family CI and offline checks passed. Both standard 320×170 pairs completed supervised OTA, all 108 packaged web-record and retained-settings checks on 28 October 2026. Pause/resume and a temporary tuning schedule using each miner's existing operating point passed; original settings, slots and policies were restored. Accepted shares progressed, and the owner confirmed normal screens and both fans. The Oct scope remains the owned PCB 2.2 unit. These short checks do not qualify new operating points, sustained thermal stability or every protection/recovery case. Six animated donut candidate celebrations and existing connection/slot controls are retained.

QAxe Beta8 includes the [saved screen selector](docs/display-settings.md) in its public standard-default image: **Miner controls → Display → Screen**, choose **3.5-inch RGB** or **3.5-inch BGR**, click **Save this section**, then **Restart device** and refresh. The larger profiles correct viewport size, offset and mirroring together. The [480×320 profiles](docs/experimental-480-display.md) remain **experimental and physically unqualified**; the owned standard-screen bench does not qualify another panel or vendor flash layout. OctAxe retains only its standard 320×170 screen.

## Choose the correct firmware

| Device | Image family | Evidence |
| --- | --- | --- |
| Bitaxe Gamma PCB 601/602 | Gamma images in the parent 5tratumFW repository; never these QAxe images | Beta10 on ESP-Miner v2.15.4; separate short checks on both models. [Release notes](../../docs/releases/v0.1.0-beta.10.md) · [Validation](../../docs/validation-aligned-20261028.md) |
| NerdQAxe++ / NERDQAXEPLUS2 / BM1370 ×4 | This QAxe paired BETA | Beta8; standard 320×170 display bench completed, experimental saved 480×320 selector remains physically unqualified. PCB revision remains unknown. [Release notes](docs/releases/qaxe-v0.1.0-beta.8.md) · [Validation](../../docs/validation-aligned-20261028.md) |
| NerdOCTAXE-γ / NERDOCTAXEGAMMA / BM1370 ×8 | Separate OctAxe pair in this subtree | Beta5; standard 320×170 display, limited to one owned PCB 2.2 unit. No other revision qualified. [Release notes](docs/releases/octaxe-v0.1.0-beta.5.md) · [Validation](../../docs/validation-aligned-20261028.md) · [OctAxe guide](../../docs/nerdoctaxe.md) |
| QAxe+, other QAxe/OctAxe revisions, NerdQX, GT800 and other devices | No supported image in this QAxe release | Do not infer compatibility from shared upstream driver definitions |

## Included

- Gamma-style six-page navigation: Overview, Miner controls, Scheduler, Pool routing, Network and Updates. More keeps Security, Alerts, InfluxDB, System, Network miners and capability-gated CAN fleet.
- Both fan controllers, thermal shutdown limits, SV1/SV2 connection settings, optional verification, named tuning slots and ten named pool slots.
- Explicit 1MHz/1mV manual requests and nearby operating-point Review/Apply. Existing saved operating points are retained; these are requests subject to hardware quantization, not automatic tuning.
- Weekly power pause windows and independent pool-switch time points, saved and executed on the miner. Pool schedules currently select connection A/primary only; manual profile Apply can target A or B.
- [Saved tuning-profile schedules](docs/tuning-profile-schedule.md), selecting whole-miner frequency/voltage from ten saved slots at up to sixteen weekly time points. Network time and a fixed UTC offset are required; tuning and pool schedules do not automatically adjust for daylight saving time. Pauses and hardware protection retain priority.
- Separate native A/B network sessions through 5tratMUX, with independent MUX routing/accounting and one physical-parent card.
- Per-session fresh MUX coin identity, candidate height, nBits, bdiff and current forwarded job. Mixed dual mining has separate coin/job information; no single aggregate chain is invented.
- Physical screen branding, scrolling startup progress and recurring large 5tratumFW logo, plus route/job and shared board/regulator readings. Thermal, shutdown and enrollment overlays take priority.
- [Saved QAxe screen selection](docs/display-settings.md) for the standard screen and explicit experimental 480×320 RGB/BGR choices. Save and restart to apply; OctAxe remains standard 320×170 only.

The two sessions share the ASIC chain's existing job selector. This release does **not** prove or implement independent physical-ASIC work ownership; `independentWorkAssignment` remains false. Advertised controller/session identity is a peer report rather than authentication.

## Install, operate and build

Use both `esp-miner-NerdQAxe++.bin` and `www.bin` from the same QAxe package. Confirm the manifest model and hashes. OTA retains existing NVS settings; never erase flash or write a factory image as a routine upgrade.

- [Installation and bootloader recovery](docs/installation-5tratumfw-qa.md)
- [Native dual-MUX sessions](docs/native-mux-pool-streams.md)
- [Named slots and pool scheduler](docs/miner-profiles-and-pool-schedule.md)
- [Saved tuning-profile schedule](docs/tuning-profile-schedule.md)
- [Power saving and weekly power scheduler](docs/mining-power.md)
- [QAxe screen size, position and colour selection](docs/display-settings.md)
- [Pinned build and packaging](docs/build-5tratumfw-qa.md)
- [Current QAxe Beta8 release notes](docs/releases/qaxe-v0.1.0-beta.8.md)
- [Current OctAxe Beta5 release notes](docs/releases/octaxe-v0.1.0-beta.5.md)
- [Current aligned build and owned-device validation](../../docs/validation-aligned-20261028.md)
- [Earlier QAxe Beta6 validation](docs/validation-qa-beta6.md)
- [Earlier OctAxe Beta4 validation](docs/validation-oct-beta4.md)
- [Earlier QAxe Beta5 validation](docs/validation-qa-beta5.md)
- [Earlier OctAxe Beta3 validation](docs/validation-oct-beta3.md)
- [Earlier Beta 4 validation](docs/validation-qa-beta4.md)
- [Earlier Beta 3 validation](docs/validation-qa-beta3.md)
- [Earlier Beta 2 updater correction](docs/validation-qa-beta2.md)
- [Earlier Beta 1 installed-device observations](docs/validation-qa-beta1.md)
- [Earlier a10 test-unit observations](docs/validation-a10.md)

[GPLv3](LICENSE). This firmware is separate from the licensing used by other WillItMod products.
