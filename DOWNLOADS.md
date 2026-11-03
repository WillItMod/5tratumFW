# Download 5tratumFW

Choose by the actual miner model. These are **experimental BETA prereleases**. Use both BIN files from the same row; their shared `www.bin` name does not make them interchangeable.

| Your miner | Current release | Download both files | Installation |
| --- | --- | --- | --- |
| **Bitaxe Gamma 601 / 602** | [Beta13 — experimental](https://github.com/WillItMod/5tratumFW/releases/tag/v0.1.0-beta.13) | [Firmware BIN](https://github.com/WillItMod/5tratumFW/releases/download/v0.1.0-beta.13/esp-miner.bin) · [Web BIN](https://github.com/WillItMod/5tratumFW/releases/download/v0.1.0-beta.13/www.bin) | [Gamma guide](docs/installation.md) |
| **NerdQAxe++** · BM1370 ×4 | [Beta10 — experimental](https://github.com/WillItMod/5tratumFW/releases/tag/qaxe-v0.1.0-beta.10) | [Firmware BIN](https://github.com/WillItMod/5tratumFW/releases/download/qaxe-v0.1.0-beta.10/esp-miner-NerdQAxe%2B%2B.bin) · [Web BIN](https://github.com/WillItMod/5tratumFW/releases/download/qaxe-v0.1.0-beta.10/www.bin) | [QAxe guide](firmware/nerdqaxe/docs/installation-5tratumfw-qa.md) |
| **NerdOctAxe Gamma** · BM1370 ×8 | [Beta8 — experimental](https://github.com/WillItMod/5tratumFW/releases/tag/octaxe-v0.1.0-beta.8) | [Firmware BIN](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.8/esp-miner-NerdOCTAXE-Gamma.bin) · [Web BIN](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.8/www.bin) | [OctAxe guide](firmware/nerdqaxe/docs/installation-5tratumfw-oct.md) |

Each release includes its manifest, checksums and corresponding source. Gamma retains the ESP-Miner v2.15.4 base; both Nerd families retain shufps v1.1.1-rc1. See [exact source and qualification](docs/validation-autotune-release-20261103.md). These versions add the [opt-in adaptive tuner](docs/autotuning.md), not an automatic overclock on installation.

## Update through 5tratumOS

In **5tratumOS → Settings → Updates**, install **0.8.50 on MAIN**, or **0.8.50-dev if already on DEV**, then reload the portal. Keep your existing OS channel; a firmware beta does not require changing it.

If this miner is already running a tuning candidate, first open its Tuning page, select **Disable and restore backup**, and wait until the original settings are confirmed restored and tuning is inactive. If already paused and ordinary restoration is waiting, use the separate **Restore original settings and resume mining** action and wait for completed verification. Do this before installing another firmware pair; an accepted Stop request alone is not completed restoration.

1. Open **Fleet → Devices** in Orbit, or **5tratumFW devices** in Classic.
2. On the correct miner, select **Firmware update → BETA → Check for updates**.
3. Confirm the model-labelled version: **Bitaxe Gamma 601/602 Beta13**, **NerdQAxe++ Beta10**, or **NerdOctAxe Gamma Beta8**.
4. Select **Install firmware → Confirm install**. Keep power connected until **Firmware installation verified** appears.
5. Use **Open miner**, reload the miner page and check matching application/web versions, saved settings, temperatures, fans and mining.

The OS installs both matching images and verifies the resulting pair and saved settings. Version 0.8.50 includes the older Nerd settings-verification/redirect correction. Future compatible releases are discovered without another OS update for each firmware version. Hardware, identity, layout and API checks still apply. Initial conversion from stock firmware remains a separate manual process. NerdQAxe++ Beta6 remains selectable only when it upgrades an older installation; it is not a downgrade option.

If an update reports **settings-verification-unavailable**, update 5tratumOS first, reload and check again. If it still fails, leave the miner running and report the OS version, miner model and installed firmware version. Do not erase settings or bypass the verification.

## Use adaptive tuning

1. Open the miner's **Tuning** page. Choose the goal and compatible power, clock and voltage limits. The initial upper clock/voltage bounds match the saved operating point; Performance cannot explore above its saved clock ceiling.
2. Start with the recommended **60°C ASIC / 70°C VRM** targets, or lower where the installed protection requires it. The page warns above 60°C ASIC and above 70°C VRM, with stronger warnings above 65°C ASIC or at 80°C VRM. Sensor sources and hardware limits differ; these are starting guidelines, not certified universal limits.
3. Save the options, read the experimental disclaimer and explicitly accept the risk acknowledgement for this run. Then select **Start tuning**. No waiver is preselected and no update enables the tuner.
4. Watch **Live dashboard**. Use **Adjust this tune** to edit an admitted session and save changes; this does not require a second Start. Normal search probes are 6.25MHz on Gamma, 10MHz on Nerd, or 5mV. Safety backoff can be larger. A completed measurement must beat the noise margin; retaining the original point is a valid result.
5. Select **Disable and restore backup** whenever you want to stop and restore the original settings. Keep the page open until saved settings and live state confirm restoration. A hot, faulted or unavailable miner may leave restoration pending; the controller never clears a fault or forces unsafe recovery.

For an already paused miner, **Restore original settings and resume mining** is a separate explicit action: it validates the powered-off state, restores and independently verifies the durable original configuration, requests normal startup, then verifies live settings, fans and mining. It keeps tuning off, never clears a hardware fault and never bypasses startup protection. New manual/scheduled control or an update cancels a pending recovery; a blocked attempt requires another explicit request.

Before the first admitted run, the firmware verifies an immutable, identity-bound backup of the actual pre-tuning settings, not factory defaults. Later runs do not replace it. The backup survives disabling and restart; experimental points remain in RAM. A restart disables tuning, including when Continuous On was saved. Manual/scheduled operating changes can take ownership without erasing the backup. Preserve your separate private recovery backup as well.

- **Tuning → Live dashboard / Adjust this tune** separates live operation from editable options. A persistent header shows active, observing, holding, restoring, blocked or stale status across pages.
- Three measured priorities: **Standard Glazed** (efficiency), **Jam Filled** (balanced) and **Cream Filled, Cherry on Top** (performance). These are dynamic objectives, not fixed clock/voltage presets. Continuous On keeps reassessing; Off completes one search and holds its result. Neither setting starts tuning automatically after boot.
- Live useful TH/s, J/TH, ASIC/VRM temperatures, reported power and ASIC errors, with six bounded, gap-aware browser trend charts. J/TH uses reported miner power, not wall-meter power. Real controller decisions distinguish requested, applied, accepted and rejected changes; missing data stays unavailable.
- Separate **Pool share difficulty** and **Network difficulty** for each pool on Overview and Tuning, taken from current dispatched work and normalized to Bitcoin difficulty-1 units. Disconnects or invalidated work show unavailable; these values are not best-share difficulty or external chain estimates.
- A read-only Tuning page on the physical display. It cannot accept a waiver, start tuning or change limits.


Full guidance, sensor limits, backup behaviour and qualification limits are in [Adaptive tuning](docs/autotuning.md).

## Manual update and screen selection

For a manual update, verify checksums, upload the correct family's **application first**, wait for its restart, then upload **that same release's WWW** and reload. Follow the model guide's restart and readback steps. These are OTA images, without replacement NVS, factory settings or partition tables. Keep enabled authentication. Stock conversion uses the separate model guide.

Nerd defaults remain **Standard 320×170**. For a supported larger assembly, use **Miner controls → Display → Screen**, select **3.5-inch 480×320 RGB** or **BGR**, **Save this section**, then **Restart device**. The profile changes size, offset and mirror orientation together; rotation is separate. The physical panel/controller still needs its own qualification. OctAxe's 480×320 geometry remains an accepted working assumption, not proof of a vendor/controller match.

Gamma's W5500 option stays off by default. Complete the paired update over Wi-Fi; read the [Ethernet instructions](docs/gamma-ethernet.md) before wiring or enabling an adapter. Physical LAN and Ethernet-only OTA remain unqualified.

## Hardware and earlier releases

Gamma accepts the stored 601/602 identity, QAxe targets **NERDQAXEPLUS2 / BM1370×4**, and OctAxe targets **NERDOCTAXEGAMMA / BM1370×8**. A software name is not physical board identification. OctAxe observations apply to the owned PCB 2.2 assembly and verified 16MiB layout; other revisions need their own checks. The final-pair checks are recorded in [the dated validation](docs/validation-autotune-release-20261103.md). The older source `35719b8e` QAxe/OctAxe bench was limited and does not qualify later UI or firmware changes. Gamma 602 has not passed tuner admission/backup/restore qualification; its strict reported fault was not bypassed. The larger Nerd panels and Gamma W5500 hardware/Ethernet-only updates remain physically unqualified.

The previous [Gamma Beta11](https://github.com/WillItMod/5tratumFW/releases/tag/v0.1.0-beta.11), [QAxe Beta8](https://github.com/WillItMod/5tratumFW/releases/tag/qaxe-v0.1.0-beta.8) and [OctAxe Beta6](https://github.com/WillItMod/5tratumFW/releases/tag/octaxe-v0.1.0-beta.6) retain their original files and evidence. The updater does not offer downgrades. Before any manual downgrade, disable tuning, verify restoration, and read the relevant journal and screen-profile compatibility notes; an older image cannot necessarily read every later settings format.

Independent per-chip tuning/routing remains unsupported. Pool/tuning schedules use their configured fixed UTC offset without automatic daylight-saving adjustment.

[Release history](docs/releases/README.md) · [Model compatibility](docs/firmware-selection.md) · [All releases](https://github.com/WillItMod/5tratumFW/releases)


When an ordinary measured trial is rejected, the controller returns to its better measured point when headroom permits. If protective backoff exhausts the permitted clock range, or repeated error-correction windows are exhausted, the failed point is retained in the decision history. A clock-floor rejection records the initiating readings; error exhaustion records the actual completed window means, without inventing an improvement score. These two soft tuning failures can make one guarded automatic recovery attempt: pause and cool, validate protection and sensors, restore and independently verify the immutable pre-tune settings, then resume and verify normal mining with tuning off. Cooling admission requires the applicable temperature headroom; Gamma holds mining work until fresh powered ASIC/regulator/fan readings pass. Nerd applies the extra 5°C margin before startup and continues to enforce user ceilings during powered startup. A real hardware fault, invalid backup, missing telemetry, failed verification or newer manual/scheduled control blocks or cancels recovery. There is no automatic fault clearing, MCU reboot or repeated retry. The original point can differ from tighter tuning targets selected later; improve cooling or review the permitted range before another Start.
