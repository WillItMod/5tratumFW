# Download 5tratumFW

Choose your miner below. Download **both BIN files from the same row**, then follow its installation guide. These are the current public BETA prereleases.

| Your miner | Current release | Download both files | Installation |
| --- | --- | --- | --- |
| **Gamma 601 / 602** | [Beta 10](https://github.com/WillItMod/5tratumFW/releases/tag/v0.1.0-beta.10) | [Firmware BIN](https://github.com/WillItMod/5tratumFW/releases/download/v0.1.0-beta.10/esp-miner.bin) · [Web BIN](https://github.com/WillItMod/5tratumFW/releases/download/v0.1.0-beta.10/www.bin) | [Gamma guide](docs/installation.md) |
| **NerdQAxe++** · BM1370 × 4 · standard **320×170 screen**, experimental 480×320 selector | [Beta 8](https://github.com/WillItMod/5tratumFW/releases/tag/qaxe-v0.1.0-beta.8) | [Firmware BIN](https://github.com/WillItMod/5tratumFW/releases/download/qaxe-v0.1.0-beta.8/esp-miner-NerdQAxe%2B%2B.bin) · [Web BIN](https://github.com/WillItMod/5tratumFW/releases/download/qaxe-v0.1.0-beta.8/www.bin) | [QAxe guide](firmware/nerdqaxe/docs/installation-5tratumfw-qa.md) |
| **NerdOctAxe Gamma** · BM1370 × 8 · standard **320×170 screen**, experimental 480×320 selector | [Beta 6 — EXPERIMENTAL](https://github.com/WillItMod/5tratumFW/releases/tag/octaxe-v0.1.0-beta.6) | [Firmware BIN](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.6/esp-miner-NerdOCTAXE-Gamma.bin) · [Web BIN](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.6/www.bin) | [OctAxe guide](firmware/nerdqaxe/docs/installation-5tratumfw-oct.md) |

Each model has its own `www.bin`; use the one beside its firmware download. The release page includes checksums and corresponding source.

The guides cover installation order, keeping your settings and finding the hidden manual uploader on stock Nerd firmware.

These releases retain saved tuning-profile schedules, with Gamma based on ESP-Miner v2.15.4 and both Nerd families based on shufps v1.1.1-rc1. Gamma Beta10, QAxe Beta8 and the previous OctAxe Beta5 completed short owned-device checks with their existing settings and normal screens/fans. **OctAxe Beta6 has software/build checks only; neither its standard screen nor its larger panel has been physically checked.** See the [earlier installed-device record](docs/validation-aligned-20261028.md) and [Beta6 validation](firmware/nerdqaxe/docs/validation-oct-beta6.md).

QAxe Beta8 and OctAxe Beta6 include the **experimental 480×320 RGB/BGR selector**. Install the selected model's application first, wait for its restart, then install that release's matching `www.bin` and reload. Open **Miner controls → Display → Screen**, choose **3.5-inch 480×320 RGB** or **3.5-inch 480×320 BGR**, select **Save this section**, then **Restart device**. The default remains **Standard 320×170** until an explicit save and restart. Screen size, offset and mirror orientation change together. The OctAxe larger-panel resolution is an accepted working assumption, not an identified controller or confirmed vendor layout. See [display settings](firmware/nerdqaxe/docs/display-settings.md) and [OctAxe screen instructions](firmware/nerdqaxe/docs/installation-5tratumfw-oct.md#select-the-experimental-larger-screen).

The previous physically checked standard-screen OctAxe [Beta5 release](https://github.com/WillItMod/5tratumFW/releases/tag/octaxe-v0.1.0-beta.5) remains available: [Firmware BIN](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.5/esp-miner-NerdOCTAXE-Gamma.bin) · [Web BIN](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.5/www.bin). Before downgrading, select **Standard 320×170**, save and restart on Beta6; then install both Beta5 files together.

Tuning and pool schedules use a fixed UTC offset and do not change automatically for daylight saving. Independent physical-chip routing remains unsupported and disabled.

## Hardware coverage

Gamma is checked on PCB 601 and 602. The QAxe package targets **NERDQAXEPLUS2**; the test unit's PCB revision is unidentified. The OctAxe package targets **NERDOCTAXEGAMMA**; earlier installed-device checks apply only to the owned **PCB 2.2** unit and its verified 16 MiB layout. Beta6 publication adds no physical qualification or support for a vendor's different flash layout. Other variants and revisions do not gain compatibility from a similar name. See [model compatibility](docs/compatibility.md) and [firmware selection](docs/firmware-selection.md).

[Release history and validation](docs/releases/README.md) · [All GitHub releases](https://github.com/WillItMod/5tratumFW/releases)
