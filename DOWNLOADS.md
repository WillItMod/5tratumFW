# Download 5tratumFW

Choose your miner below. On **5tratumOS 0.8.48**, use the [one-click instructions](#update-through-5tratumos) to install the matched application and web interface together. For manual installation, download **both BIN files from the same row** and follow its guide. These are the current public BETA prereleases.

| Your miner | Current release | Download both files | Installation |
| --- | --- | --- | --- |
| **Bitaxe Gamma 601 / 602** | [Beta 10](https://github.com/WillItMod/5tratumFW/releases/tag/v0.1.0-beta.10) | [Firmware BIN](https://github.com/WillItMod/5tratumFW/releases/download/v0.1.0-beta.10/esp-miner.bin) · [Web BIN](https://github.com/WillItMod/5tratumFW/releases/download/v0.1.0-beta.10/www.bin) | [Gamma guide](docs/installation.md) |
| **NerdQAxe++** · BM1370 × 4 · standard **320×170 screen**, experimental 480×320 selector | [Beta 8](https://github.com/WillItMod/5tratumFW/releases/tag/qaxe-v0.1.0-beta.8) | [Firmware BIN](https://github.com/WillItMod/5tratumFW/releases/download/qaxe-v0.1.0-beta.8/esp-miner-NerdQAxe%2B%2B.bin) · [Web BIN](https://github.com/WillItMod/5tratumFW/releases/download/qaxe-v0.1.0-beta.8/www.bin) | [QAxe guide](firmware/nerdqaxe/docs/installation-5tratumfw-qa.md) |
| **NerdOctAxe Gamma** · BM1370 × 8 · standard **320×170 screen**, experimental 480×320 selector | [Beta 6 — EXPERIMENTAL](https://github.com/WillItMod/5tratumFW/releases/tag/octaxe-v0.1.0-beta.6) | [Firmware BIN](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.6/esp-miner-NerdOCTAXE-Gamma.bin) · [Web BIN](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.6/www.bin) | [OctAxe guide](firmware/nerdqaxe/docs/installation-5tratumfw-oct.md) |

Each model has its own `www.bin`; use the one beside its firmware download. The release page includes checksums and corresponding source.

The guides cover installation order, keeping your settings and finding the hidden manual uploader on stock Nerd firmware. [NerdQAxe++ Beta 6](https://github.com/WillItMod/5tratumFW/releases/tag/qaxe-v0.1.0-beta.6) is also available as an upgrade choice for older NerdQAxe++ installations; Beta 8 remains the newest default. The central updater does not downgrade a miner already running Beta 6 or newer to Beta 6.

These releases retain saved tuning-profile schedules, with Bitaxe Gamma based on ESP-Miner v2.15.4 and both Nerd families based on shufps v1.1.1-rc1. Bitaxe Gamma Beta 10, NerdQAxe++ Beta 8 and the previous NerdOctAxe Gamma Beta 5 completed short owned-device checks with their existing settings and normal screens/fans. On **28 October 2026**, the owned **PCB 2.2 NerdOctAxe Gamma** also completed **Beta 5 → Beta 6** through the production 5tratumOS updater modules invoked by a local QA driver, with verified application/web and retained-settings checks. The installed 5tratumOS system was not upgraded for this test; browser interactions were checked separately with fictional miners. The owner confirmed the **standard screen and both fans looked normal**. **The larger panel remains physically unqualified.** See the [earlier installed-device record](docs/validation-aligned-20261028.md), [original Beta 6 build validation](firmware/nerdqaxe/docs/validation-oct-beta6.md) and [5tratumOS 0.8.48 release](https://github.com/WillItMod/5tratum/releases/tag/v0.8.48).

## Update through 5tratumOS

For an existing compatible 5tratumFW installation:

1. Update 5tratumOS to **0.8.48** through **Settings → Updates**, then reload the portal.
2. Open **Fleet → Devices** in Orbit, or **5tratumFW devices** in Classic.
3. On the correct miner, select **Firmware update**, choose **BETA**, then **Check for updates**.
4. Check the model-labelled **Firmware version**. The newest compatible default is **Bitaxe Gamma 601/602 Beta 10**, **NerdQAxe++ Beta 8**, or **NerdOctAxe Gamma Beta 6 — experimental**, respectively. NerdQAxe++ Beta 6 is an additional choice when it upgrades the installed version.
5. Select **Install firmware → Confirm install**. Keep the miner powered and wait for **Firmware installation verified**. The OS downloads and installs both matching images, handles the required restarts and checks the resulting pair and saved settings.

Future compatible firmware releases are discovered from the selected channel without another OS update for each beta number. The **BETA** channel also includes compatible stable releases. Model, layout, image and API compatibility checks still apply; new hardware or a breaking format can require an adapter. The one-click updater does not perform the initial conversion from stock firmware.

On older **5tratumOS 0.8.47**, use **Open miner** from the same Devices page and follow the miner's manual update guide: install the correct application first, wait for restart, then install its matching `www.bin` and refresh.

## Screen selection and earlier firmware

NerdQAxe++ Beta 8 and NerdOctAxe Gamma Beta 6 include the **experimental 480×320 RGB/BGR selector**. After the matched pair is installed, use **Open miner**, then **Miner controls → Display → Screen**. Choose **3.5-inch 480×320 RGB** or **3.5-inch 480×320 BGR**, select **Save this section**, then **Restart device**. The default remains **Standard 320×170** until an explicit save and restart. Screen size, offset and mirror orientation change together. The NerdOctAxe Gamma larger-panel resolution is an accepted working assumption, not an identified controller or confirmed vendor layout. See [display settings](firmware/nerdqaxe/docs/display-settings.md) and [OctAxe screen instructions](firmware/nerdqaxe/docs/installation-5tratumfw-oct.md#select-the-experimental-larger-screen).

The previous physically checked standard-screen OctAxe [Beta5 release](https://github.com/WillItMod/5tratumFW/releases/tag/octaxe-v0.1.0-beta.5) remains available: [Firmware BIN](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.5/esp-miner-NerdOCTAXE-Gamma.bin) · [Web BIN](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.5/www.bin). Before downgrading, select **Standard 320×170**, save and restart on Beta6; then install both Beta5 files together.

Tuning and pool schedules use a fixed UTC offset and do not change automatically for daylight saving. Independent physical-chip routing remains unsupported and disabled.

## Hardware coverage

Bitaxe Gamma is checked on PCB 601 and 602. The NerdQAxe++ package targets **NERDQAXEPLUS2**; the test unit's PCB revision is unidentified. The NerdOctAxe Gamma package targets **NERDOCTAXEGAMMA**; installed-device checks apply only to the owned **PCB 2.2** unit and its verified 16 MiB layout. The later Beta 6 standard-screen check does not qualify the larger panel or a vendor's different flash layout. Other variants and revisions do not gain compatibility from a similar name. See [model compatibility](docs/compatibility.md) and [firmware selection](docs/firmware-selection.md).

[Release history and validation](docs/releases/README.md) · [All GitHub releases](https://github.com/WillItMod/5tratumFW/releases)
