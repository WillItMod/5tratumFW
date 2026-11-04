# 5tratumFW — choose your device

**Adaptive tuning BETA:** Gamma **Beta13**, NerdQAxe++ **Beta10** and NerdOctAxe Gamma **Beta8** add the [Tuning page](docs/autotuning.md), explicit per-run waiver and verified original-settings backup/restore. Choose the model-specific pair below. The [dated validation](docs/validation-autotune-release-20261103.md) separates final-build software checks from the limited earlier hardware tests.

**[Download firmware — Gamma, QAxe or OctAxe](DOWNLOADS.md)**

5tratumFW has separate firmware image families. **Choose by the actual model and PCB/ASIC information; application and WWW images are not interchangeable between families.** The firmware families retain their upstream GPLv3 licensing and saved device settings during compatible OTA updates.

**Gamma Beta13 supports single-file OTA:** `esp-miner.bin` includes its complete matching web interface. The separate Gamma `www.bin` is optional for custom-WWW, compatibility and recovery. QAxe and OctAxe still require both their application and matching WWW images. See [single-file updates](docs/single-file-updates.md).

| Your device | Firmware pair | Qualification and guide |
| --- | --- | --- |
| Bitaxe Gamma PCB 601 or 602 | Gamma `esp-miner.bin` includes web UI; separate Gamma `www.bin` optional | Beta13 **experimental** adaptive tuner. Gamma 602 remains unqualified for tuner admission/restore; W5500 hardware untested. [Gamma guide](docs/installation.md) · [Validation](docs/validation-autotune-release-20261103.md) |
| NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4 | QAxe `esp-miner-NerdQAxe++.bin` + matching QAxe `www.bin` | Beta10 **experimental** adaptive tuner and ASIC errors; standard 320×170 default, larger panel unqualified. [QAxe guide](docs/nerdqaxe.md) · [Validation](docs/validation-autotune-release-20261103.md) |
| NerdOCTAXE-γ / NERDOCTAXEGAMMA / BM1370×8 | OctAxe `esp-miner-NerdOCTAXE-Gamma.bin` + matching OctAxe `www.bin` | Beta8 **experimental** adaptive tuner and ASIC errors; PCB 2.2 observations only, larger panel unqualified. [OctAxe guide](docs/nerdoctaxe.md) · [Validation](docs/validation-autotune-release-20261103.md) |
| QAxe+, other OctAxe revisions, GT800, other Gamma PCBs or other miners | No supported package in this repository | Do not use a similarly named board's image |

Start with the [three-model download page](DOWNLOADS.md) or [firmware selection and repository layout](docs/firmware-selection.md). Use a complete matching image pair, manifest, checksums and corresponding source. [All GitHub releases](https://github.com/WillItMod/5tratumFW/releases) remain available for history. Recovery backups/NVS/factory images are never published.

The root source/build remains Gamma. QAxe source is in [`firmware/nerdqaxe`](firmware/nerdqaxe/readme.md), with its own build/package helpers. The unified interface has Overview, Miner controls, Tuning, Scheduler, Pool routing, Network and Updates; QAxe retains its extra hardware, protocol/security and integration settings.

The current public BETA prereleases are Gamma **Beta13**, NerdQAxe++ **Beta10** and NerdOctAxe Gamma **Beta8**, with matched Gamma files built from `06d2bae5f1a45352a60b9deaf8dc0df34d2d2b89` and Nerd files from `06d2bae5f1a45352a60b9deaf8dc0df34d2d2b89`. Gamma retains ESP-Miner v2.15.4; both Nerd families retain shufps v1.1.1-rc1. The new [Live dashboard and Adjust this tune views](docs/autotuning.md) show measured tuning decisions, reported J/TH and gap-aware charts; the header shows activity across pages. Overview and Tuning also show each pool's share difficulty and network/job difficulty separately. [Update through 5tratumOS 0.8.50 on your existing MAIN/DEV channel](DOWNLOADS.md#update-through-5tratumos); future compatible firmware releases are discovered without an OS release for every beta.

Earlier release provenance remains unchanged. Gamma Beta11's published pair was compiled from clean source `e73cbc69b815b3f7e0a3a5af9266f605bb95a33b`; the previous Beta10 pair from `1645747dfc46a83c52e27c567d43be3f0b53a035`; QAxe Beta8 and the previous OctAxe Beta5 from `1a34b24100e1238e331d8145a70f6b08f85f943d`; OctAxe Beta6 from `5567a2d6c1f59808f197062e565fa6525a93f97b`. Full local builds and package checks passed; [Gamma Beta11 validation](docs/validation-gamma-ethernet.md) records its separate software evidence. The six earlier GitHub family runs apply to Gamma Beta10, QAxe Beta8 and OctAxe Beta5. All three Beta6 publication checks also passed at `9aa01916`; [the dated record](firmware/nerdqaxe/docs/validation-oct-beta6.md#github-publication-checks) preserves its separate image/source receipts. Later documentation commits do not relabel these images or their corresponding source. QAxe Beta8 and OctAxe Beta6 expose saved 480×320 RGB/BGR choices that change size, offset and mirror orientation; both larger-panel paths remain **experimental and physically unqualified**. OctAxe uses 480×320 as the accepted working assumption, with Standard 320×170 as its default. See [screen setup and downgrade instructions](firmware/nerdqaxe/docs/installation-5tratumfw-oct.md#select-the-experimental-larger-screen). The physically checked standard-screen [OctAxe Beta5 pair](https://github.com/WillItMod/5tratumFW/releases/tag/octaxe-v0.1.0-beta.5) remains available.

Pool routing now places Primary and Secondary connection editors above the ten named slots. **Save pool settings** saves that connection; **Save to slot** captures its saved configuration into the selected named slot. Each configured slot has explicit **Apply to Primary** and **Apply to Secondary** actions. Opening the page or selecting a slot does not apply it. Invalid, missing or ambiguous acknowledgments leave the operation unconfirmed, with no automatic write retry. Clocks, voltage, cooling, network, credentials and schedules are not changed by this layout update.

QAxe's native A/B MUX connections have independently configured routes and fresh per-session coin/job information. They use the shared-chain job selector; this does not establish independent physical-chip ownership.

## Gamma family

**Gamma BETA — root-source images accept Gamma 601/602 only. The current pair is experimental `5tratumFW-0.1.0-beta.13`. Read the exact-source validation before enabling tuning; Gamma 602 has not passed tuner admission/restore qualification. Earlier pairs retain their own records.**

Firmware and a compact web interface for **Bitaxe Gamma PCB revisions 601 and 602 only**.
A GPLv3 derivative of [Bitaxe ESP-Miner v2.15.4](https://github.com/bitaxeorg/ESP-Miner/tree/v2.15.4), maintained by [WillItMod](https://github.com/WillItMod).

## Compatibility

| Hardware | This image | Validation |
| --- | --- | --- |
| Bitaxe Gamma PCB 601 | Supported by the explicit stored-identity guard | Beta13 qualification is recorded separately; Gamma 601 final evidence is in the dated tuning record. Previous Beta10 matching app/web and all 11 served records verified; original settings, 20 slots and policies retained; normal display/fan confirmation, pause/resume, same-point tuning schedule and normal software-restart checks. |
| Bitaxe Gamma PCB 602 | Supported by the explicit stored-identity guard | Gamma 602 tuner admission/restore remains unqualified because of its strict reported fault. Separate previous Beta10 matching app/web and all 11 served records verified; original settings, 20 slots and policies retained; normal display/fan confirmation, pause/resume, same-point tuning schedule and normal software-restart checks. |
| Other Bitaxe PCBs, GT800, NerdQAxe, NerdOctAxe and other miners (use their separately qualified family, where available) | Incompatible | Do not flash these Gamma images. |

The guard validates an existing NVS board identity; it does not physically identify a PCB. Confirm the physical board revision and existing identity before installing. A missing, unreadable or unsupported identity stops startup before hardware initialization.

## Features

- Redesigned 5tratumFW dashboard, mining settings, pool routing and responsive navigation.
- Experimental W5500 wired Ethernet for Gamma 601/602, off by default, with explicit save/restart and Wi-Fi fallback; physical adapter qualification remains pending.
- Weekly Scheduler navigation item, manual ASIC pause/resume and power saving.
- Existing clock, voltage, fan, network and pool settings retained during compatible OTA updates.
- Coinbase decoding off by default; explicitly saved choices preserved.
- Direct SV1/SV2 pool configuration and miner-side 5tratMUX setup in **Pool routing**.
- Gamma OLED mining/health views and fresh peer-advertised MUX status, exposed read-only to OS inventory at `/api/5tratum/status`.
- Ten named pool slots with direct Apply, ten named tuning slots, and fine manual clock/voltage requests.
- Weekly pool-switch time points saved on the miner alongside the independent power scheduler.
- Weekly tuning-profile time points select saved clock/voltage slots; tuning and pool schedules use an explicit fixed UTC offset without automatic daylight-saving changes.

The current experimental pairs add opt-in whole-miner adaptive tuning with explicit acknowledgement before every Start. They do not implement independent per-chip tuning/routing, guaranteed overclocks or fixed priority presets. Hardware protection remains active. [Read the complete Tuning guidance](docs/autotuning.md), including 60°C ASIC /70°C VRM starting targets, immutable original backup, pending restoration on faults and no automatic restart of tuning after boot.

## Install and operate

Current **experimental BETA**: [Gamma Beta13](docs/releases/v0.1.0-beta.13.md), [QAxe Beta10](firmware/nerdqaxe/docs/releases/qaxe-v0.1.0-beta.10.md) and [OctAxe Beta8](firmware/nerdqaxe/docs/releases/octaxe-v0.1.0-beta.8.md). The W5500 driver remains off by default and physically unqualified. Before a downgrade, disable tuning and verify restoration, then review the exact journal, Ethernet and screen-profile compatibility requirements.

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

For the current images, see [final tuning release validation](docs/validation-autotune-release-20261103.md). The [memory-repair history](docs/validation-autotune-memory-20261103.md), [Beta11 Ethernet validation](docs/validation-gamma-ethernet.md) and [earlier aligned Beta10/QA8/Oct5 checks](docs/validation-aligned-20261028.md) remain evidence for those exact earlier sources. They do not qualify later builds, all physical panels, every protection fault or sustained operation. Keep a private unit-specific recovery backup.

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
