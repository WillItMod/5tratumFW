# 5tratumFW — choose your device

**[Download firmware — Gamma, QAxe or OctAxe](DOWNLOADS.md)**

5tratumFW has separate firmware image families. **Choose by the actual model and PCB/ASIC information; application and WWW images are not interchangeable between families.** The firmware families retain their upstream GPLv3 licensing and saved device settings during compatible OTA updates.

| Your device | Firmware pair | Qualification and guide |
| --- | --- | --- |
| Bitaxe Gamma PCB 601 or 602 | Gamma `esp-miner.bin` + matching Gamma `www.bin` | Qualified Beta8 BETA pair; separate short checks on both models. [Gamma guide](docs/installation.md) · [Beta8 validation](docs/validation-beta8.md) |
| NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4 | QAxe `esp-miner-NerdQAxe++.bin` + matching QAxe `www.bin` | Qualified Beta5 BETA pair; test PCB revision unknown. [QAxe guide](docs/nerdqaxe.md) · [Beta5 validation](firmware/nerdqaxe/docs/validation-qa-beta5.md) |
| NerdOCTAXE-γ / NERDOCTAXEGAMMA / BM1370×8 | OctAxe `esp-miner-NerdOCTAXE-Gamma.bin` + matching OctAxe `www.bin` | Qualified Beta3 BETA pair; limited to one owned PCB 2.2 unit. [OctAxe guide](docs/nerdoctaxe.md) · [Beta3 validation](firmware/nerdqaxe/docs/validation-oct-beta3.md) |
| QAxe+, other OctAxe revisions, GT800, other Gamma PCBs or other miners | No supported package in this repository | Do not use a similarly named board's image |

Start with the [three-model download page](DOWNLOADS.md) or [firmware selection and repository layout](docs/firmware-selection.md). Use a complete matching image pair, manifest, checksums and corresponding source. [All GitHub releases](https://github.com/WillItMod/5tratumFW/releases) remain available for history. Recovery backups/NVS/factory images are never published.

The root source/build remains Gamma. QAxe source is in [`firmware/nerdqaxe`](firmware/nerdqaxe/readme.md), with its own build/package helpers. The unified interface has Overview, Miner controls, Scheduler, Pool routing, Network and Updates; QAxe retains its extra hardware, protocol/security and integration settings.

The qualified Gamma pair is `5tratumFW-0.1.0-beta.8`, compiled from source `eff5e633f372bb46a86803580c453645600f5fcd`. Its full pinned build, offline package checks and exact-source CI passed. Separate supervised paired checks on 601 and 602 completed on 8 October 2026; installed strict OS31 inspections completed on both models, with later owner tuning changes recorded separately. It corrects the successful Content-Type of both schedule APIs so strict central clients can inspect the existing JSON. QAxe `5tratumFW-qa-0.1.0-beta.5` and OctAxe `5tratumFW-oct-0.1.0-beta.3` retain their qualified published pairs and source `3ffbe84f43e265e61d5bae3eca1cd985def2bb3f`; their code and packages are unchanged. See [Gamma Beta8](docs/validation-beta8.md), [QAxe Beta5](firmware/nerdqaxe/docs/validation-qa-beta5.md), [OctAxe Beta3](firmware/nerdqaxe/docs/validation-oct-beta3.md) and the [release index](docs/releases/README.md).

Pool routing now places Primary and Secondary connection editors above the ten named slots. **Save pool settings** saves that connection; **Save to slot** captures its saved configuration into the selected named slot. Each configured slot has explicit **Apply to Primary** and **Apply to Secondary** actions. Opening the page or selecting a slot does not apply it. Invalid, missing or ambiguous acknowledgments leave the operation unconfirmed, with no automatic write retry. Clocks, voltage, cooling, network, credentials and schedules are not changed by this layout update.

QAxe's native A/B MUX connections have independently configured routes and fresh per-session coin/job information. They use the shared-chain job selector; this does not establish independent physical-chip ownership.

## Gamma family

**Gamma BETA — root-source images accept Gamma 601/602 only. The current qualified pair is `5tratumFW-0.1.0-beta.8`. Beta7 retains its own validation record.**

Firmware and a compact web interface for **Bitaxe Gamma PCB revisions 601 and 602 only**.
A GPLv3 derivative of [Bitaxe ESP-Miner v2.14.2](https://github.com/bitaxeorg/ESP-Miner/tree/v2.14.2), maintained by [WillItMod](https://github.com/WillItMod).

## Compatibility

| Hardware | This image | Validation |
| --- | --- | --- |
| Bitaxe Gamma PCB 601 | Supported by the explicit stored-identity guard | Beta8 matching app/web and all 11 served records verified; exported settings, 20 slots and both schedules retained; both schedule responses declare JSON; accepted shares increased by one. |
| Bitaxe Gamma PCB 602 | Supported by the explicit stored-identity guard | Separate Beta8 pair and all 11 served records verified; exported settings, 20 slots and both schedules retained; both schedule responses declare JSON; accepted shares increased by one. |
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

See [Beta8 validation](docs/validation-beta8.md) for the qualified schedule-header pair and separate completed 601/602 checks. [Beta7](docs/validation-beta7.md), [Beta6](docs/validation-beta6.md), [Beta5](docs/validation-beta5.md), [Beta4](docs/validation-beta4.md) and [Beta3](docs/validation-beta3.md) remain historical records for their own images. Physical OLED readability, sustained operation and complete fault recovery remain unqualified by these short checks.

## Build

With Node 24.14.0, npm, Python 3, a native C compiler, Chrome/Chromium, Git and Docker:

```sh
git clone --recurse-submodules https://github.com/WillItMod/5tratumFW.git
cd 5tratumFW
bash tools/build_5tratumfw.sh
```

The build pins ESP-IDF v5.5.3 by container digest, runs the checks described in the build guide and packages paired OTA files, checksums, a manifest and corresponding source under `artifacts/<version>/`. It does not flash a miner.

## License

[GPLv3](LICENSE). Upstream authors and third-party notices are retained. This repository does not apply the Business Source License used by separate WillItMod projects. See [attribution](docs/attribution.md).
