# 5tratumFW for NerdQAxe++ — BETA

This source builds the QAxe-specific paired OTA images for **NerdQAxe++ / NERDQAXEPLUS2 with four BM1370 ASICs**. It is a GPLv3 derivative of [shufps ESP-Miner-NerdQAxePlus](https://github.com/shufps/ESP-Miner-NerdQAxePlus). Upstream authors and licenses are retained; the original documentation is in [upstream-readme](docs/upstream-readme.md).

Current qualified QAxe BETA pair: `5tratumFW-qa-0.1.0-beta.6`, standard **320×170 display**. This image is separate from Gamma 601/602 and OctAxe. The exact pair was installed on the owned test unit, whose PCB revision remains unidentified. Read [Beta6 validation](docs/validation-qa-beta6.md). Earlier records retain their own release evidence.

The current QAxe Beta6 and OctAxe Beta4 pairs are built from clean source `689b2c8fcd01c4b507be102bb1caed22f4552738`, with family CI and offline checks passed. Both standard 320×170 pairs completed supervised OTA, 94-asset and exported-settings checks on 14 October 2026. QAxe has four fresh hardware counters, OctAxe eight, and both retained Dual pool 50/50 with A/B coin/job context. The Oct scope remains the owned PCB 2.2 unit. Six randomized animated donut candidate celebrations are included; the 480×320 QAxe profiles remain experimental and are not published. Model compatibility and existing connection/slot controls are unchanged. Read [QAxe Beta6 validation](docs/validation-qa-beta6.md) and [OctAxe Beta4 validation](docs/validation-oct-beta4.md).

The [480×320 QAxe profiles](docs/experimental-480-display.md) remain experimental and have no published binaries or automatic-update path. The current standard packages require the original 320×170 screen.

## Choose the correct firmware

| Device | Image family | Evidence |
| --- | --- | --- |
| Bitaxe Gamma PCB 601/602 | Gamma images in the parent 5tratumFW repository; never these QAxe images | Qualified Beta9 BETA pair. Separate short checks on both models; [Beta9 validation](../../docs/validation-beta9.md) |
| NerdQAxe++ / NERDQAXEPLUS2 / BM1370 ×4 | This QAxe paired BETA | Qualified Beta6 BETA pair; standard 320×170 display. Exported settings and fresh A/B context checked; PCB revision remains unknown. [Beta6 validation](docs/validation-qa-beta6.md) |
| NerdOCTAXE-γ / NERDOCTAXEGAMMA / BM1370 ×8 | Separate OctAxe pair in this subtree | Qualified Beta4 BETA pair; standard 320×170 display, limited to one owned PCB 2.2 unit. No other revision qualified; [Beta4 validation](docs/validation-oct-beta4.md). See the [OctAxe guide](../../docs/nerdoctaxe.md) |
| QAxe+, other QAxe/OctAxe revisions, NerdQX, GT800 and other devices | No supported image in this QAxe release | Do not infer compatibility from shared upstream driver definitions |

## Included

- Gamma-style six-page navigation: Overview, Miner controls, Scheduler, Pool routing, Network and Updates. More keeps Security, Alerts, InfluxDB, System, Network miners and capability-gated CAN fleet.
- Both fan controllers, thermal shutdown limits, SV1/SV2 connection settings, optional verification, named tuning slots and ten named pool slots.
- Explicit 1MHz/1mV manual requests and nearby operating-point Review/Apply. Existing saved operating points are retained; these are requests subject to hardware quantization, not automatic tuning.
- Weekly power pause windows and independent pool-switch time points, saved and executed on the miner. Pool schedules currently select connection A/primary only; manual profile Apply can target A or B.
- Separate native A/B network sessions through 5tratMUX, with independent MUX routing/accounting and one physical-parent card.
- Per-session fresh MUX coin identity, candidate height, nBits, bdiff and current forwarded job. Mixed dual mining has separate coin/job information; no single aggregate chain is invented.
- Physical screen branding, scrolling startup progress and recurring large 5tratumFW logo, plus route/job and shared board/regulator readings. Thermal, shutdown and enrollment overlays take priority.

The two sessions share the ASIC chain's existing job selector. This release does **not** prove or implement independent physical-ASIC work ownership; `independentWorkAssignment` remains false. Advertised controller/session identity is a peer report rather than authentication.

## Install, operate and build

Use both `esp-miner-NerdQAxe++.bin` and `www.bin` from the same QAxe package. Confirm the manifest model and hashes. OTA retains existing NVS settings; never erase flash or write a factory image as a routine upgrade.

- [Installation and bootloader recovery](docs/installation-5tratumfw-qa.md)
- [Native dual-MUX sessions](docs/native-mux-pool-streams.md)
- [Named slots and pool scheduler](docs/miner-profiles-and-pool-schedule.md)
- [Power saving and weekly power scheduler](docs/mining-power.md)
- [Pinned build and packaging](docs/build-5tratumfw-qa.md)
- [Current QAxe Beta6 validation](docs/validation-qa-beta6.md)
- [Current OctAxe Beta4 validation](docs/validation-oct-beta4.md)
- [Earlier QAxe Beta5 validation](docs/validation-qa-beta5.md)
- [Earlier OctAxe Beta3 validation](docs/validation-oct-beta3.md)
- [Earlier Beta 4 validation](docs/validation-qa-beta4.md)
- [Earlier Beta 3 validation](docs/validation-qa-beta3.md)
- [Earlier Beta 2 updater correction](docs/validation-qa-beta2.md)
- [Earlier Beta 1 installed-device observations](docs/validation-qa-beta1.md)
- [Earlier a10 test-unit observations](docs/validation-a10.md)

[GPLv3](LICENSE). This firmware is separate from the licensing used by other WillItMod products.
