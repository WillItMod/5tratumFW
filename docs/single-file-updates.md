# Single-file firmware updates

Gamma Beta14 retains the update structure used by [AxeOS 2.15.4](https://github.com/bitaxeorg/ESP-Miner/blob/v2.15.4/readme.md#unified-firmware--rollbacks): the normal ESP32-S3 application contains its matching web assets. Compressed assets retain their gzip flags and are served from the application after boot. This is not a concatenation of the application and SPIFFS images, a ZIP, or a merged factory flash image.

| Model | Normal manual update | Separate `www.bin` |
| --- | --- | --- |
| Gamma 601/602, Beta14 | `esp-miner.bin` contains application and matching web interface | Optional custom-WWW, compatibility and recovery copy |
| NerdQAxe++, Beta11 | Application followed by the matching QAxe web image | Required |
| NerdOctAxe Gamma, Beta9 | Application followed by the matching OctAxe web image | Required |

Current [Gamma Beta14](releases/v0.1.0-beta.14.md), [QAxe Beta11](releases/qaxe-v0.1.0-beta.11.md) and [OctAxe Beta9](releases/octaxe-v0.1.0-beta.9.md) use unique release versions. Verify the current version's manifest and checksums for its exact files. The original Beta13 receipt below remains historical evidence for that unchanged file, not a checksum for Beta14. No new-release physical installation is claimed.

The Nerd builds still serve their normal interface from SPIFFS. Combining their downloads into one file would require implementation and validation; joining the BINs or renaming a file does not add that support.

## Earlier verified Gamma Beta13 file

[Download the existing single-file Gamma application](https://github.com/WillItMod/5tratumFW/releases/download/v0.1.0-beta.13/esp-miner.bin).

- Version: `5tratumFW-0.1.0-beta.13`.
- Build source: `06d2bae5f1a45352a60b9deaf8dc0df34d2d2b89`.
- Size: **2,925,488 bytes**, within the standard 4 MiB application slot.
- SHA256: `3ee6343165530c7e34c07fdeb444ff6f1a3de66fa26635b8f1635bf548a51db3`.
- Standard ESP application header, ESP32-S3 target and application descriptor.
- The application descriptor matches the selected build ELF. All **11 embedded web assets**, totalling **657,618 payload bytes**, were independently checked against the tested production bundle.

This describes the original published file. No executable, release asset, checksum or version has been replaced. The retained release manifest describes the package containing both app and WWW; it already records the embedded-web verification.

## Install and verify

Use the [Gamma installation guide](installation.md) to check the physical 601/602 identity, compatible partition layout, saved settings and upload prerequisites. Stop active tuning and verify original restoration before an update.

Upload `esp-miner.bin` through a compatible raw application OTA control, wait for its restart, then fully reload the page. Gamma firmware migration disables the previous custom-WWW selection when the build changes, allowing the new application to serve its own matching interface. It does not erase the separate WWW partition or replace NVS, the bootloader or partition table.

Verify the application version, served web version, original operating settings, fans and fresh mining telemetry. The existing 5tratumOS 0.8.50 updater continues to install and verify both release images; its current transport is unchanged.

The separate Gamma WWW image remains useful for custom-WWW and compatibility workflows. `/recovery` is embedded independently of SPIFFS and provides **Disable Custom Web UI** plus the matching WWW upload. See [recovery](recovery.md) before downgrading to firmware that requires a separate web partition.

## Harlo and other third-party firmware

Matching the AxeOS single-file format does not prove another product accepts that image. A third-party updater can use a different package format, partition layout or stored board identity. Harlo-to-5tratumFW migration has not been physically tested. Obtain the exact Harlo version and rejection message before choosing its update or USB recovery route.

The published `esp-miner.bin` is not a USB factory/wipe image and must not be written at address zero. Preserve a private backup and use the exact board's documented recovery process if the existing layout needs replacement. No miner write was performed for this format verification.
