# 5tratumFW — choose your device

**[Download firmware — Gamma, QAxe or OctAxe](DOWNLOADS.md)**

5tratumFW has separate firmware image families. **Choose by the actual model and PCB/ASIC information; application and WWW images are not interchangeable between families.** The firmware families retain their upstream GPLv3 licensing and saved device settings during compatible OTA updates.

| Your device | Firmware pair | Qualification and guide |
| --- | --- | --- |
| Bitaxe Gamma PCB 601 or 602 | Gamma `esp-miner.bin` + matching Gamma `www.bin` | Beta10 public BETA pair; separate short checks on both models. [Gamma guide](docs/installation.md) · [Validation](docs/validation-aligned-20261028.md) |
| NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4 | QAxe `esp-miner-NerdQAxe++.bin` + matching QAxe `www.bin` | Beta8 public BETA pair; standard 320×170 display checked, experimental 480×320 selector, test PCB revision unknown. [QAxe guide](docs/nerdqaxe.md) · [Validation](docs/validation-aligned-20261028.md) |
| NerdOCTAXE-γ / NERDOCTAXEGAMMA / BM1370×8 | OctAxe `esp-miner-NerdOCTAXE-Gamma.bin` + matching OctAxe `www.bin` | Beta6 public **EXPERIMENTAL** pair; standard-default display with saved 480×320 choices. Later standard-screen paired check on one owned PCB 2.2 unit; larger panel unqualified. [OctAxe guide](docs/nerdoctaxe.md) · [Beta6 validation](firmware/nerdqaxe/docs/validation-oct-beta6.md) |
| QAxe+, other OctAxe revisions, GT800, other Gamma PCBs or other miners | No supported package in this repository | Do not use a similarly named board's image |

Start with the [three-model download page](DOWNLOADS.md) or [firmware selection and repository layout](docs/firmware-selection.md). Use a complete matching image pair, manifest, checksums and corresponding source. [All GitHub releases](https://github.com/WillItMod/5tratumFW/releases) remain available for history. Recovery backups/NVS/factory images are never published.

The root source/build remains Gamma. QAxe source is in [`firmware/nerdqaxe`](firmware/nerdqaxe/readme.md), with its own build/package helpers. The unified interface has Overview, Miner controls, Scheduler, Pool routing, Network and Updates; QAxe retains its extra hardware, protocol/security and integration settings.

The current public BETA prereleases are Gamma `5tratumFW-0.1.0-beta.10` on ESP-Miner v2.15.4, QAxe `5tratumFW-qa-0.1.0-beta.8` and experimental OctAxe `5tratumFW-oct-0.1.0-beta.6` on shufps v1.1.1-rc1. They retain saved tuning-profile schedules, the existing interface and candidate celebrations. Gamma Beta10, QAxe Beta8 and the previous OctAxe Beta5 completed supervised updates, preserved-settings checks, pause/resume and a temporary schedule at their existing operating point; the owner confirmed normal physical screens and fans. **Later OctAxe Beta6 check:** production 5tratumOS updater modules invoked by a local QA driver verified the Beta5 → Beta6 application/web pair and retained settings on the owned PCB 2.2 unit; the owner confirmed its standard screen and both fans normal. The installed OS was not upgraded for this test. The larger panel remains physically unqualified, and the original manifest flags remain unchanged. See the [5tratumOS 0.8.48 validation](https://github.com/WillItMod/5tratum/releases/download/v0.8.48/validation-0.8.48.md) and [one-click instructions](DOWNLOADS.md#update-through-5tratumos). See [Beta6 software validation](firmware/nerdqaxe/docs/validation-oct-beta6.md), [earlier owned-device validation](docs/validation-aligned-20261028.md), the [release index](docs/releases/README.md) and [upstream migration](docs/upstream-migration-20261028.md).

Gamma's published pair was compiled from clean source `1645747dfc46a83c52e27c567d43be3f0b53a035`; QAxe Beta8 and the previous OctAxe Beta5 from `1a34b24100e1238e331d8145a70f6b08f85f943d`; OctAxe Beta6 from `5567a2d6c1f59808f197062e565fa6525a93f97b`. Full local builds and package checks passed; the six earlier GitHub family runs apply to Gamma Beta10, QAxe Beta8 and OctAxe Beta5. All three Beta6 publication checks also passed at `9aa01916`; [the dated record](firmware/nerdqaxe/docs/validation-oct-beta6.md#github-publication-checks) preserves its separate image/source receipts. Later documentation commits do not relabel these images or their corresponding source. QAxe Beta8 and OctAxe Beta6 expose saved 480×320 RGB/BGR choices that change size, offset and mirror orientation; both larger-panel paths remain **experimental and physically unqualified**. OctAxe uses 480×320 as the accepted working assumption, with Standard 320×170 as its default. See [screen setup and downgrade instructions](firmware/nerdqaxe/docs/installation-5tratumfw-oct.md#select-the-experimental-larger-screen). The physically checked standard-screen [OctAxe Beta5 pair](https://github.com/WillItMod/5tratumFW/releases/tag/octaxe-v0.1.0-beta.5) remains available.

Pool routing now places Primary and Secondary connection editors above the ten named slots. **Save pool settings** saves that connection; **Save to slot** captures its saved configuration into the selected named slot. Each configured slot has explicit **Apply to Primary** and **Apply to Secondary** actions. Opening the page or selecting a slot does not apply it. Invalid, missing or ambiguous acknowledgments leave the operation unconfirmed, with no automatic write retry. Clocks, voltage, cooling, network, credentials and schedules are not changed by this layout update.

QAxe's native A/B MUX connections have independently configured routes and fresh per-session coin/job information. They use the shared-chain job selector; this does not establish independent physical-chip ownership.

## Gamma family

**Gamma BETA — root-source images accept Gamma 601/602 only. The current public pair is `5tratumFW-0.1.0-beta.10`. Earlier pairs retain their own validation records.**

Firmware and a compact web interface for **Bitaxe Gamma PCB revisions 601 and 602 only**.
A GPLv3 derivative of [Bitaxe ESP-Miner v2.15.4](https://github.com/bitaxeorg/ESP-Miner/tree/v2.15.4), maintained by [WillItMod](https://github.com/WillItMod).

## Compatibility

| Hardware | This image | Validation |
| --- | --- | --- |
| Bitaxe Gamma PCB 601 | Supported by the explicit stored-identity guard | Beta10 matching app/web and all 11 served records verified; original settings, 20 slots and policies retained; normal display/fan confirmation, pause/resume, same-point tuning schedule and normal software-restart checks. |
| Bitaxe Gamma PCB 602 | Supported by the explicit stored-identity guard | Separate Beta10 matching app/web and all 11 served records verified; original settings, 20 slots and policies retained; normal display/fan confirmation, pause/resume, same-point tuning schedule and normal software-restart checks. |
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
- Weekly tuning-profile time points select saved clock/voltage slots; tuning and pool schedules use an explicit fixed UTC offset without automatic daylight-saving changes.

No autotuning, clock presets, authenticated native MUX management or independent per-chip mining are implemented or validated. Existing thermal protection remains active and may change operating settings after overheating.

## Install and operate

Development branch: [Gamma Beta11 W5500 Ethernet candidate](docs/gamma-ethernet.md), off by default and pending physical adapter qualification. This does not change the current public Beta10 release.

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

See [aligned Beta10 validation](docs/validation-aligned-20261028.md) for the separate completed 601/602 checks. [Beta9](docs/validation-beta9.md), [Beta7](docs/validation-beta7.md), [Beta6](docs/validation-beta6.md), [Beta5](docs/validation-beta5.md), [Beta4](docs/validation-beta4.md) and [Beta3](docs/validation-beta3.md) remain historical records for their own images. Candidate animations were checked in native fixtures; controlled cold power-cycles, different operating points, sustained operation and complete fault recovery remain outside the current short checks. Gamma's new pool-authentication settings have a [documented older-firmware journal limit](docs/upstream-migration-20261028.md); keep a unit-specific recovery backup.

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
