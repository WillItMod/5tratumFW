# Download 5tratumFW

Choose your miner below. Download **both BIN files from the same row**, then follow its installation guide. These are the current public BETA prereleases.

| Your miner | Current release | Download both files | Installation |
| --- | --- | --- | --- |
| **Gamma 601 / 602** | [Beta 10](https://github.com/WillItMod/5tratumFW/releases/tag/v0.1.0-beta.10) | [Firmware BIN](https://github.com/WillItMod/5tratumFW/releases/download/v0.1.0-beta.10/esp-miner.bin) · [Web BIN](https://github.com/WillItMod/5tratumFW/releases/download/v0.1.0-beta.10/www.bin) | [Gamma guide](docs/installation.md) |
| **NerdQAxe++** · BM1370 × 4 · standard **320×170 screen**, experimental 480×320 selector | [Beta 8](https://github.com/WillItMod/5tratumFW/releases/tag/qaxe-v0.1.0-beta.8) | [Firmware BIN](https://github.com/WillItMod/5tratumFW/releases/download/qaxe-v0.1.0-beta.8/esp-miner-NerdQAxe%2B%2B.bin) · [Web BIN](https://github.com/WillItMod/5tratumFW/releases/download/qaxe-v0.1.0-beta.8/www.bin) | [QAxe guide](firmware/nerdqaxe/docs/installation-5tratumfw-qa.md) |
| **NerdOctAxe Gamma** · BM1370 × 8 · **320×170 screen** | [Beta 5](https://github.com/WillItMod/5tratumFW/releases/tag/octaxe-v0.1.0-beta.5) | [Firmware BIN](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.5/esp-miner-NerdOCTAXE-Gamma.bin) · [Web BIN](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.5/www.bin) | [OctAxe guide](firmware/nerdqaxe/docs/installation-5tratumfw-oct.md) |

Each model has its own `www.bin`; use the one beside its firmware download. The release page includes checksums and corresponding source.

The guides cover installation order, keeping your settings and finding the hidden manual uploader on stock Nerd firmware.

These releases add saved tuning-profile schedules, with Gamma based on ESP-Miner v2.15.4 and both Nerd families based on shufps v1.1.1-rc1. All four owned miners completed short checks with their existing settings; the owner confirmed normal screens and fans. See the [dated validation](docs/validation-aligned-20261028.md) for the exact scope.

QAxe Beta8 includes the **experimental 480×320 RGB/BGR selector** under **Miner controls → Display → Screen**. Save the section and restart to activate a choice. It changes screen size, offset and mirror orientation together; physical testing on the larger panel is still required. The standard 320×170 screen was checked locally. OctAxe remains 320×170 only. See [display settings](firmware/nerdqaxe/docs/display-settings.md).

Tuning and pool schedules use a fixed UTC offset and do not change automatically for daylight saving. Independent physical-chip routing remains unsupported and disabled.

## Hardware coverage

Gamma is checked on PCB 601 and 602. The QAxe package targets **NERDQAXEPLUS2**; the test unit's PCB revision is unidentified. The OctAxe package targets **NERDOCTAXEGAMMA** and is checked on the owned **PCB 2.2** unit. Other variants and revisions do not gain compatibility from a similar name. See [model compatibility](docs/compatibility.md) and [firmware selection](docs/firmware-selection.md).

[Release history and validation](docs/releases/README.md) · [All GitHub releases](https://github.com/WillItMod/5tratumFW/releases)
