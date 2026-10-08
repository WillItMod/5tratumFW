# Next models and scoped OctAxe qualification

Research recorded on 8 October 2026. **This research document adds no compatibility beyond the separately recorded qualified families.** The current [firmware selection](firmware-selection.md) and [Gamma compatibility](compatibility.md) remain authoritative. The qualified OctAxe BETA is a separate family; existing Gamma and QAxe application/WWW pairs must not be used for it, GT800 or Gamma Duo650.

| Model | Priority and available evidence | Next step |
| --- | --- | --- |
| NerdOCTAXE-γ / NERDOCTAXEGAMMA / BM1370×8 | Scoped Beta3 BETA pair qualified on one owned PCB 2.2 unit. Actual flash geometry and private backup verified; regulator package marking unobserved | Publish the scoped pair, then further physical-screen/schedule/protection qualification; no other revision is admitted |
| Bitaxe Gamma Turbo, marketed as GT800 | Next: the owned GT requires the older 800 profile; its exact 800x/800xxx PCB and regulator revision remains unconfirmed | Confirm the physical revision and baseline, then map its power, cooling and temperature paths before preparing an exact model build |
| Gamma Duo PCB650 | After GT800: inherited source has a 650 profile and official v2.15.3 is pinned as a reference, but no 650 test unit is available | Prepare and test source/build support separately; physical qualification remains pending a matching unit |

## Scoped OctAxe BETA

The qualified OctAxe BETA is limited to **BOARD `NERDOCTAXEGAMMA`, reported model `NerdOCTAXE-γ`, ASIC BM1370×8**. Its current source/build version is `5tratumFW-oct-0.1.0-beta.3`, with tag contract `octaxe-v0.1.0-beta.3` and application filename `esp-miner-NerdOCTAXE-Gamma.bin`. It uses its own matching OctAxe `www.bin`; the shared website filename does not make other families' web images compatible.

From `firmware/nerdqaxe/`, the separate helpers are `tools/build_5tratumfw_oct.sh` and `tools/package_5tratumfw_oct.py`, using `version-oct.txt`. The build/package contract is OTA-only; it does not publish factory/NVS images or flash a device. The current qualified [Gamma Beta7](validation-beta7.md) and [QAxe Beta5](../firmware/nerdqaxe/docs/validation-qa-beta5.md) pairs are separate from OctAxe Beta3,.

The reported model, ASIC type and count are source admission criteria, not physical board identification. The owned PCB revision is 2.2 according to its owner. Its actual flash geometry and original settings were verified, and the regulator strap/stock profile were reviewed against pinned hardware source. The regulator package marking was not physically read. These observations do not qualify another assembly. Inherited board definitions, source defaults and host tests do not qualify other OctAxe models or revisions, or authorize overwriting saved clocks, voltage, cooling or settings. Native A/B sessions use a shared chain; independent physical ASIC job ownership and per-chip coin routing are not established.

Read the [OctAxe guide](nerdoctaxe.md), [build guide](../firmware/nerdqaxe/docs/build-5tratumfw-oct.md), [installation limits](../firmware/nerdqaxe/docs/installation-5tratumfw-oct.md) and [Beta2 validation](../firmware/nerdqaxe/docs/validation-oct-beta2.md) for the prior candidate's exact evidence and remaining bench work. The new [Beta3 validation](../firmware/nerdqaxe/docs/validation-oct-beta3.md) records completed CI/offline checks; the exact pair also passed a short supervised update on the owned PCB 2.2 unit.

## GT: distinguish 800 revisions from 801

The [official BitaxeGT hardware README](https://github.com/bitaxeorg/BitaxeGT/blob/4202c2533f6036cfb19e4575d7afc07fbef58f54/README.md) calls 801 the first release and describes component/header changes from 800xxx. GT uses two BM1370 ASICs, an ESP32-S3, an EMC2103 and a 12V input. A family name or two-chip count does not identify the board revision.

Official hardware tags pin different regulator populations. The following are the instantiated U1/U4 values in each pinned `Power.kicad_sch`, rather than unused library symbols:

| Hardware tag | Hardware commit | Both core regulators |
| --- | --- | --- |
| [GT-800x power schematic](https://github.com/bitaxeorg/BitaxeGT/blob/65f3e28bc5dfa510e9fbb701a9d440aacc6ca38a/Power.kicad_sch) | `65f3e28bc5dfa510e9fbb701a9d440aacc6ca38a` | TPS546D24ARVFR |
| [GT-800xxx power schematic](https://github.com/bitaxeorg/BitaxeGT/blob/8c8c548830833cd99b90e7349c35700664a2ad86/Power.kicad_sch) | `8c8c548830833cd99b90e7349c35700664a2ad86` | TPS546D24ARVFR |
| [GT-801 power schematic](https://github.com/bitaxeorg/BitaxeGT/blob/5a927a735f9bfdb557d12a39a564f156b1f3380a/Power.kicad_sch) | `5a927a735f9bfdb557d12a39a564f156b1f3380a` | TPS546D24S |

The historical [ESP-Miner v2.12.0 board table](https://github.com/bitaxeorg/ESP-Miner/blob/1ae84063891e81e97ac8615268fcc3db1b5169b7/main/device_config.h) contained board identity `800` with a -10°C temperature offset. Upstream [commit `24d1b1b7fb57b5165ad833f266bd99d95b0ad4d3` (#1479)](https://github.com/bitaxeorg/ESP-Miner/commit/24d1b1b7fb57b5165ad833f266bd99d95b0ad4d3) replaced that row with `801`, changed the offset to zero and renamed `config-800x.cvs` to `config-801.cvs`. The subsequent [“801 Strapless working” commit `eed9d5eeef37b55b7fb03b76e3532c494523c5c2` (#1478)](https://github.com/bitaxeorg/ESP-Miner/commit/eed9d5eeef37b55b7fb03b76e3532c494523c5c2) added explicit TPS546 stack, sync and compensation programming. The [v2.15.3 board table](https://github.com/bitaxeorg/ESP-Miner/blob/fb31c4fb7d975c839c827f4052ffa0415ef56793/main/device_config.h) still contains GT801 and no GT800 row.

The inherited local [board table](../main/device_config.h) likewise has GT801 only: GammaTurbo, BM1370×2, 12V, EMC2103, zero temperature offset and flipped channels. Current official hardware [commit `4202c2533f6036cfb19e4575d7afc07fbef58f54`](https://github.com/bitaxeorg/BitaxeGT/commit/4202c2533f6036cfb19e4575d7afc07fbef58f54) clarifies that EMC2103 channel 1 connects to ASIC2 and channel 2 to ASIC1; the actual candidate revision needs its own mapping checked.

The local [TPS546 driver](../main/power/TPS546.c) recognizes D24A and D24S but writes the supplied configuration for either. [GT power configuration](../main/power/vcore.c) includes the post-801 stack/sync/compensation values. Recognition of D24A does not establish that these values suit every older 800 strap/layout. An actual 800/800xxx unit needs a separate verified revision profile; changing its identity to 801 would bypass that unresolved distinction.

## Gamma Duo 650 source baseline

The current official firmware reference is [ESP-Miner v2.15.3](https://github.com/bitaxeorg/ESP-Miner/releases/tag/v2.15.3), pinned to source commit [`fb31c4fb7d975c839c827f4052ffa0415ef56793`](https://github.com/bitaxeorg/ESP-Miner/tree/fb31c4fb7d975c839c827f4052ffa0415ef56793). This is an upstream source pin, not physical qualification or support for 650 in the Gamma601/602 release. No matching 650 test unit is available.

Official hardware branch/release `gamma-duo-650` is pinned at [`bae291657152690c7e91b34dbd77a655715502ff`](https://github.com/bitaxeorg/bitaxeGamma/tree/bae291657152690c7e91b34dbd77a655715502ff). Its inherited README describes the original single-chip Gamma, so it is not evidence of the Duo configuration.

Upstream [commit `dc3ce30b6965897722c55db4a4dfa3797c7797b8` (#1537)](https://github.com/bitaxeorg/ESP-Miner/commit/dc3ce30b6965897722c55db4a4dfa3797c7797b8) added board 650. [Commit `568af2382f8de5599079c6cd8a830faca7300740` (#1557)](https://github.com/bitaxeorg/ESP-Miner/commit/568af2382f8de5599079c6cd8a830faca7300740) defines GammaDuo with two ASICs, 5V input and the `ASIC_BM1370XP` software profile at a 400MHz default; the driver identity remains BM1370. These definitions are already in the local [board table](../main/device_config.h), inherited from [ESP-Miner v2.14.2, commit `64680f8a4da0b9a3b532051f0aa18429fcf04e82`](https://github.com/bitaxeorg/ESP-Miner/tree/64680f8a4da0b9a3b532051f0aa18429fcf04e82). A source default is not permission to overwrite a device's saved frequency or voltage. The 5V Duo and 12V GT require their own board/power profiles.

## Work required before any compatibility claim

1. Establish the exact physical model/revision, reported board/ASIC identity and installed application/web versions. Privately retain the flash/partition layout and original frequency, voltage, fan policy, pool/network settings and recovery information. Confirm the regulator population, strap/layout, power supply, fans and display against the matching hardware pin.
2. Implement exact persisted-identity admission before hardware initialization, with only explicitly mapped revisions allowed. Preserve NVS and configured settings. Extend the profile binding, ASIC count, telemetry, cooling, pause/resume and restart paths for the selected model; reject other families and ambiguous identities. Keep existing Gamma and QAxe guards intact.
3. Prepare model-specific build/package contracts with matched application/WWW versions, exact supported identities, manifest, checksums and corresponding pinned source. Qualify flash/partition compatibility and update/version readback. Do not relabel or reuse an existing family pair, publish factory/NVS images, or add device flashing to builds.
4. Verify identity rejection, profile selection, power/temperature/fan mappings and settings retention with host tests, target compilation and interface checks. These establish source/build readiness, not physical qualification. For 650, physical work remains pending a matching test unit.
5. On the matching physical unit, establish the original operating baseline, then qualify cooling and protections, the available board/ASIC telemetry, mining/accepted shares, pause/resume, pool restart, reboot and paired OTA settings retention. For another OctAxe assembly, separately verify all eight ASIC counters, shared-chain A/B identity and fresh per-session coin/job information; the owned PCB 2.2 short bench does not qualify it. Record the exact revision/build and validation limits; short observations do not establish sustained soak or fault recovery.

Shared BM1370 code makes these plausible ports, but inherited definitions and successful compilation do not prove hardware operation. Multiple ASICs do not establish independent physical-chip work ownership or per-ASIC coin routing. No such claim is part of this plan.
