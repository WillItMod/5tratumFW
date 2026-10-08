# 5tratumFW — choose your device

5tratumFW has separate firmware image families. **Choose by the actual model and PCB/ASIC information; application and WWW images are not interchangeable between families.** Both families retain their upstream GPLv3 licensing and saved device settings during compatible OTA updates.

| Your device | Firmware pair | Qualification and guide |
| --- | --- | --- |
| Bitaxe Gamma PCB601 or602 | Gamma `esp-miner.bin` + matching Gamma `www.bin` | Gamma601 Beta4 installed;602 compiled, unflashed. [Gamma guides](docs/installation.md) · [compatibility](docs/compatibility.md) |
| NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4 | QAxe `esp-miner-NerdQAxe++.bin` + matching QAxe `www.bin` | Separate QAxe BETA; test PCB revision unknown. [QAxe guide](docs/nerdqaxe.md) · [Beta 2 validation](firmware/nerdqaxe/docs/validation-qa-beta2.md) |
| QAxe+, NerdOctAxe/OctAxe revisions, GT800, other Gamma PCBs or other miners | No supported package in this repository | Do not use a similarly named board's image |

Start with [firmware selection and repository layout](docs/firmware-selection.md). Downloads are [model-labelled GitHub prereleases](https://github.com/WillItMod/5tratumFW/releases); use a complete matching image pair, manifest, checksums and corresponding source. Recovery backups/NVS/factory images are never published.

The root source/build remains Gamma. QAxe source is in [`firmware/nerdqaxe`](firmware/nerdqaxe/readme.md), with its own build/package helpers. The unified interface has Overview, Miner controls, Scheduler, Pool routing, Network and Updates; QAxe retains its extra hardware, protocol/security and integration settings.

QAxe's native A/B MUX connections have independently configured routes and fresh per-session coin/job information. They use the shared-chain job selector; this does not establish independent physical-chip ownership.

## Gamma family

**Gamma BETA — these root-source images accept Gamma601/602 only. The current source is `5tratumFW-0.1.0-beta.4`.**

Firmware and a compact web interface for **Bitaxe Gamma PCB revisions 601 and 602 only**.
A GPLv3 derivative of [Bitaxe ESP-Miner v2.14.2](https://github.com/bitaxeorg/ESP-Miner/tree/v2.14.2), maintained by [WillItMod](https://github.com/WillItMod).

## Compatibility

| Hardware | This image | Validation |
| --- | --- | --- |
| Bitaxe Gamma PCB 601 | Supported by the explicit stored-identity guard | Beta 4 paired app/WWW installed; both versions match, saved settings/slots/schedules retained, mining and accepted shares observed. Inline Review placement verified on the device web interface. See the validation record below. |
| Bitaxe Gamma PCB 602 | Supported by the explicit stored-identity guard | Compiled support. Device still runs stock v2.14.2; this firmware has not been installed/tested there. |
| Other Bitaxe PCBs, GT800, NerdQAxe, NerdOctAxe and other miners (use their separately qualified family, where available) | Incompatible | Do not flash these Gamma images. |

The guard validates an existing NVS board identity; it does not physically identify a PCB. Confirm the physical board revision and existing identity before installing. A missing, unreadable or unsupported identity stops startup before hardware initialization.

## Features

- Redesigned 5tratumFW dashboard, mining settings, pool routing and responsive navigation.
- Weekly Scheduler navigation item, manual ASIC pause/resume and power saving.
- Existing clock, voltage, fan, network and pool settings retained during compatible OTA updates.
- Coinbase decoding off by default; explicitly saved choices preserved.
- Direct SV1/SV2 pool configuration and miner-side 5tratMUX setup in **Pool routing**.
- Gamma OLED mining/health views and optional fresh peer-advertised MUX status.
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

See [Beta 4 validation](docs/validation-beta4.md) for this build and [Beta 3 validation](docs/validation-beta3.md) for the preceding physical-device checks. Gamma 602 remains unflashed; source/build compatibility is not hardware qualification.

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
