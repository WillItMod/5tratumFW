# 5tratumFW — choose your device

**[Download firmware — Gamma, QAxe or OctAxe](DOWNLOADS.md)**

5tratumFW has separate firmware image families. **Choose by the actual model and PCB/ASIC information; application and WWW images are not interchangeable between families.** The firmware families retain their upstream GPLv3 licensing and saved device settings during compatible OTA updates.

| Your device | Firmware pair | Qualification and guide |
| --- | --- | --- |
| Bitaxe Gamma PCB 601 or 602 | Gamma `esp-miner.bin` + matching Gamma `www.bin` | Qualified Beta9 BETA pair; separate short checks on both models. [Gamma guide](docs/installation.md) · [Beta9 validation](docs/validation-beta9.md) |
| NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4 | QAxe `esp-miner-NerdQAxe++.bin` + matching QAxe `www.bin` | Qualified Beta6 BETA pair; standard 320×170 display, test PCB revision unknown. [QAxe guide](docs/nerdqaxe.md) · [Beta6 validation](firmware/nerdqaxe/docs/validation-qa-beta6.md) |
| NerdOCTAXE-γ / NERDOCTAXEGAMMA / BM1370×8 | OctAxe `esp-miner-NerdOCTAXE-Gamma.bin` + matching OctAxe `www.bin` | Qualified Beta4 BETA pair; standard 320×170 display, limited to one owned PCB 2.2 unit. [OctAxe guide](docs/nerdoctaxe.md) · [Beta4 validation](firmware/nerdqaxe/docs/validation-oct-beta4.md) |
| QAxe+, other OctAxe revisions, GT800, other Gamma PCBs or other miners | No supported package in this repository | Do not use a similarly named board's image |

Start with the [three-model download page](DOWNLOADS.md) or [firmware selection and repository layout](docs/firmware-selection.md). Use a complete matching image pair, manifest, checksums and corresponding source. [All GitHub releases](https://github.com/WillItMod/5tratumFW/releases) remain available for history. Recovery backups/NVS/factory images are never published.

The root source/build remains Gamma. QAxe source is in [`firmware/nerdqaxe`](firmware/nerdqaxe/readme.md), with its own build/package helpers. The unified interface has Overview, Miner controls, Scheduler, Pool routing, Network and Updates; QAxe retains its extra hardware, protocol/security and integration settings.

The current pairs are Gamma `5tratumFW-0.1.0-beta.9`, QAxe `5tratumFW-qa-0.1.0-beta.6` and OctAxe `5tratumFW-oct-0.1.0-beta.4`, compiled from clean source `689b2c8fcd01c4b507be102bb1caed22f4552738`. Full pinned builds, package checks and exact-source CI passed. All four owned miners completed paired OTA and exported-settings checks on 14 October 2026, with normal physical screens/fans confirmed by the owner. Six randomized animated donut candidate celebrations are included; their graphics and lifecycle were tested in native fixtures without fabricated live blocks. See the [release index](docs/releases/README.md) for scoped evidence. The 480×320 QAxe profiles remain experimental and have no public download or automatic-update path.

The development candidates are Gamma Beta10 on ESP-Miner v2.15.4, QAxe Beta8 and OctAxe Beta5 on shufps v1.1.1-rc1. They add saved tuning-profile schedules; QAxe includes the experimental larger-screen selector. They have separate build evidence and still need installed-device checks. See the [aligned update](docs/aligned-tuning-update.md) and [upstream migration](docs/upstream-migration-20261028.md).

Pool routing now places Primary and Secondary connection editors above the ten named slots. **Save pool settings** saves that connection; **Save to slot** captures its saved configuration into the selected named slot. Each configured slot has explicit **Apply to Primary** and **Apply to Secondary** actions. Opening the page or selecting a slot does not apply it. Invalid, missing or ambiguous acknowledgments leave the operation unconfirmed, with no automatic write retry. Clocks, voltage, cooling, network, credentials and schedules are not changed by this layout update.

QAxe's native A/B MUX connections have independently configured routes and fresh per-session coin/job information. They use the shared-chain job selector; this does not establish independent physical-chip ownership.

## Gamma family

**Gamma BETA — root-source images accept Gamma 601/602 only. The current qualified pair is `5tratumFW-0.1.0-beta.9`. Earlier pairs retain their own validation records.**

Firmware and a compact web interface for **Bitaxe Gamma PCB revisions 601 and 602 only**.
A GPLv3 derivative of [Bitaxe ESP-Miner v2.15.4](https://github.com/bitaxeorg/ESP-Miner/tree/v2.15.4), maintained by [WillItMod](https://github.com/WillItMod).

## Compatibility

| Hardware | This image | Validation |
| --- | --- | --- |
| Bitaxe Gamma PCB 601 | Supported by the explicit stored-identity guard | Beta9 matching app/web and all 11 served records verified; exported settings, 20 slots and both schedules retained; normal display/fan confirmation, cold-start and pause/resume checks. |
| Bitaxe Gamma PCB 602 | Supported by the explicit stored-identity guard | Separate Beta9 pair and all 11 served records verified; exported settings, 20 slots and both schedules retained; normal display/fan confirmation, cold-start and pause/resume checks. |
| Other Bitaxe PCBs, GT800, NerdQAxe, NerdOctAxe and other miners (use their separately qualified family, where available) | Incompatible | Do not flash these Gamma images. |

The guard validates an existing NVS board identity; it does not physically identify a PCB. Confirm the physical board revision and existing identity before installing. A missing, unreadable or unsupported identity stops startup before hardware initialization.

## Features

- Redesigned 5tratumFW dashboard, mining settings, pool routing and responsive navigation.
- Weekly Scheduler navigation item, manual ASIC pause/resume and power saving.
- Existing clock, voltage, fan, network and pool settings retained during compatible OTA updates.
- Coinbase decoding off by default; explicitly saved choices preserved.
- Direct SV1/SV2 pool configuration and miner-side 5tratMUX setup in **Pool routing**.
- Gamma OLED mining/health views and fresh peer-advertised MUX status, exposed read-only to OS inventory at `/api/5tratum/status`.
- Ten named pool slots with direct Apply, ten named tuning slots, and fine manual clock/voltage requests.
- Weekly pool-switch time points saved on the miner alongside the independent power scheduler.

No autotuning, clock presets, authenticated native MUX management or independent per-chip mining are implemented or validated. Existing thermal protection remains active and may change operating settings after overheating.

## Install and operate

Use the matched **application** `esp-miner.bin` and **web interface** `www.bin` from the same build. These are OTA images, not a factory image. The compressed web payload and the full 3 MiB WWW partition image have different sizes.

- [Installation: app BIN versus WWW BIN](docs/installation.md)
- [Preserve settings](docs/settings-preservation.md)
- [Weekly scheduler and manual power saving](docs/scheduler.md)
- [Pool routing and 5tratMUX](docs/pool-routing.md)
- [Saved pools, tuning slots and pool switching](docs/profiles-api.md)
- [Recovery](docs/recovery.md)
- [Build and test](docs/build.md)
- [Validation and known limits](docs/validation.md)
- [Attribution and licensing](docs/attribution.md)

The reserved `mining.5tratum.status` receiver is informational: it cannot confirm payouts or make routing decisions. A MUX label requires a valid advertisement received from the current peer within 90 seconds. Direct pools and older MUX servers remain ordinary Stratum connections. The test MUX candidate advertises live status to the Gamma 601. This Gamma receiver currently supports the legacy status acknowledgment; the QAxe's richer coin/block metadata protocol is a separate firmware capability.

See [Beta9 validation](docs/validation-beta9.md) for the candidate-celebration pair and separate completed 601/602 checks. [Beta7](docs/validation-beta7.md), [Beta6](docs/validation-beta6.md), [Beta5](docs/validation-beta5.md), [Beta4](docs/validation-beta4.md) and [Beta3](docs/validation-beta3.md) remain historical records for their own images. The owner confirmed normal physical screens and fans. Candidate animations were checked in native fixtures; sustained operation and complete fault recovery remain outside these short checks.

## Build

With Node 24.14.0, npm, Python 3, a native C compiler, Chrome/Chromium, Git and Docker:

```sh
git clone --recurse-submodules https://github.com/WillItMod/5tratumFW.git
cd 5tratumFW
bash tools/build_5tratumfw.sh
```

The Gamma build pins ESP-IDF v6.0.2 by container digest, runs the checks described in the build guide and packages paired OTA files, checksums, a manifest and corresponding source under `artifacts/<version>/`. It does not flash a miner.

## License

[GPLv3](LICENSE). Upstream authors and third-party notices are retained. This repository does not apply the Business Source License used by separate WillItMod projects. See [attribution](docs/attribution.md).
