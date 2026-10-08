# 5tratumFW for NerdQAxe++ — BETA

This source builds the QAxe-specific paired OTA images for **NerdQAxe++ / NERDQAXEPLUS2 with four BM1370 ASICs**. It is a GPLv3 derivative of [shufps ESP-Miner-NerdQAxePlus](https://github.com/shufps/ESP-Miner-NerdQAxePlus). Upstream authors and licenses are retained; the original documentation is in [upstream-readme](docs/upstream-readme.md).

Published QAxe version: `5tratumFW-qa-0.1.0-beta.3`. This QAxe image is separate from the Gamma 601/602 image. Beta 3 enlarges the approved full-logo header and adds a QAxe-only public release check. It retains Beta 2's legacy update-page model detection and OTP lookup. Its exact pair was installed on the test unit, whose physical PCB revision remains unidentified. Read the [Beta3 release validation asset](https://github.com/WillItMod/5tratumFW/releases/download/qaxe-v0.1.0-beta.3/validation-qa-beta3.md) for the recorded scope; earlier Beta1 observations remain historical.

## Choose the correct firmware

| Device | Image family | Evidence |
| --- | --- | --- |
| Bitaxe Gamma PCB 601/602 | Gamma images in the parent 5tratumFW repository; never these QAxe images | Published Beta6 pair installed and checked separately on both models |
| NerdQAxe++ / NERDQAXEPLUS2 / BM1370 ×4 | This QAxe paired BETA | Published Beta3 pair installed; exported settings and both native A/B accepted-share progress checked. PCB revision remains unknown |
| NerdOCTAXE-γ / NERDOCTAXEGAMMA / BM1370 ×8 | Separate OctAxe source/build candidate in this subtree | Not installed or physically qualified. See the [OctAxe guide](../../docs/nerdoctaxe.md) |
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
- [Beta 3 validation](docs/validation-qa-beta3.md)
- [Earlier Beta 2 updater correction](docs/validation-qa-beta2.md)
- [Earlier Beta 1 installed-device observations](docs/validation-qa-beta1.md)
- [Earlier a10 test-unit observations](docs/validation-a10.md)

[GPLv3](LICENSE). This firmware is separate from the licensing used by other WillItMod products.
