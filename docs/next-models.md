# Next model candidates — research and qualification

Research recorded on 8 October 2026. **This document adds no supported board or installable image.** The current [firmware selection](firmware-selection.md) and [Gamma compatibility](compatibility.md) remain authoritative; their Gamma and QAxe application/WWW pairs must not be used for these candidates.

| Candidate | Priority and available evidence | Next step |
| --- | --- | --- |
| Bitaxe Gamma Turbo, marketed as GT800 | First: the owned GT requires the older 800 profile; its exact 800x/800xxx PCB and regulator revision remains unconfirmed | Confirm the physical revision and baseline, then map its power, cooling and temperature paths before preparing an exact model build |
| Gamma Duo PCB650 | Second: inherited source has a 650 profile, but no 650 test unit is available | Prepare and test source/build support separately; physical qualification remains pending a matching unit |

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

Official hardware branch/release `gamma-duo-650` is pinned at [`bae291657152690c7e91b34dbd77a655715502ff`](https://github.com/bitaxeorg/bitaxeGamma/tree/bae291657152690c7e91b34dbd77a655715502ff). Its inherited README describes the original single-chip Gamma, so it is not evidence of the Duo configuration.

Upstream [commit `dc3ce30b6965897722c55db4a4dfa3797c7797b8` (#1537)](https://github.com/bitaxeorg/ESP-Miner/commit/dc3ce30b6965897722c55db4a4dfa3797c7797b8) added board 650. [Commit `568af2382f8de5599079c6cd8a830faca7300740` (#1557)](https://github.com/bitaxeorg/ESP-Miner/commit/568af2382f8de5599079c6cd8a830faca7300740) defines GammaDuo with two ASICs, 5V input and the `ASIC_BM1370XP` software profile at a 400MHz default; the driver identity remains BM1370. These definitions are already in the local [board table](../main/device_config.h), inherited from [ESP-Miner v2.14.2, commit `64680f8a4da0b9a3b532051f0aa18429fcf04e82`](https://github.com/bitaxeorg/ESP-Miner/tree/64680f8a4da0b9a3b532051f0aa18429fcf04e82). A source default is not permission to overwrite a device's saved frequency or voltage. The 5V Duo and 12V GT require their own board/power profiles.

## Work required before any compatibility claim

1. Establish the exact physical model/revision, reported board/ASIC identity and installed application/web versions. Privately retain the flash/partition layout and original frequency, voltage, fan policy, pool/network settings and recovery information. Confirm the regulator population, strap/layout, power supply, fans and display against the matching hardware pin.
2. Implement exact persisted-identity admission before hardware initialization, with only explicitly mapped revisions allowed. Preserve NVS and configured settings. Extend the profile binding, ASIC count, telemetry, cooling, pause/resume and restart paths for the selected model; reject other families and ambiguous identities. Keep existing Gamma and QAxe guards intact.
3. Prepare model-specific build/package contracts with matched application/WWW versions, exact supported identities, manifest, checksums and corresponding pinned source. Qualify flash/partition compatibility and update/version readback. Do not relabel or reuse an existing family pair, publish factory/NVS images, or add device flashing to builds.
4. Verify identity rejection, profile selection, power/temperature/fan mappings and settings retention with host tests, target compilation and interface checks. For 650, this can establish source/build readiness only while no matching test unit is available.
5. On the matching physical unit, establish the original operating baseline, then qualify cooling and protections, both ASICs' telemetry, mining/accepted shares, pause/resume, pool restart, reboot and paired OTA settings retention. Record the exact revision/build and validation limits; short observations do not establish sustained soak or fault recovery.

Shared BM1370 code makes these plausible ports, but inherited definitions and successful compilation do not prove hardware operation. Multiple ASICs do not establish independent physical-chip work ownership or per-ASIC coin routing. No such claim is part of this plan.
