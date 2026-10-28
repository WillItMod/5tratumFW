# Install 5tratumFW on a Gamma 601 or 602

These application and web images are for **Bitaxe Gamma PCB revisions 601 and 602 only**. Check the physical PCB revision and the device's reported board version before updating. The firmware requires an exact stored identity of `601` or `602`; this guard is not physical board autodetection. Do not change an unrelated device's stored identity to bypass it.

GT800, NerdQAxe, NerdOctAxe, other Bitaxe boards, and custom board profiles are outside this Gamma image's supported hardware. NerdQAxe++ has a separate firmware family in this repository; use its [model-specific guide](nerdqaxe.md) and image pair. A successful build does not establish installed-device testing; consult the release's validation record for the specific revision and version.

**Current public Gamma release: [Beta11 — EXPERIMENTAL BETA](https://github.com/WillItMod/5tratumFW/releases/tag/v0.1.0-beta.11).** It has software/build evidence only and has not been installed on a miner. The original W5500 Ethernet option is off by default; physical LAN and Ethernet-only OTA remain unqualified. See [Ethernet setup and qualification](gamma-ethernet.md). The previous physically checked [Beta10 pair](https://github.com/WillItMod/5tratumFW/releases/tag/v0.1.0-beta.10) remains available.

## Choose the correct files

Use the paired files from the same 5tratumFW release or verified local build:

| File | Destination | What it changes |
| --- | --- | --- |
| `esp-miner.bin` | **Updates → Miner firmware** | ESP32-S3 application, mining logic, power control, APIs, and OLED interface. The device restarts after a successful update. |
| `www.bin` | **Updates → Web interface** | The web interface in the `www` SPIFFS partition. It does not install application logic. The browser reloads after success. |
| Source archive, manifest, checksums | Your computer | Build inputs and provenance; never upload these to either update control. |

The upload controls require the exact filenames above. Keep the files inside their versioned release directory so files from different releases do not get mixed. `esp-miner.bin` is an application OTA image, not a factory image. Do not upload a merged/factory BIN to either control. `www.bin` is the generated **3 MiB partition image** for the standard layout, even when its compressed web assets occupy much less space. Do not replace it with a ZIP, an asset directory, or a smaller raw payload.

Both images are needed to install a complete release when its application and web interface have changed. An app-only update leaves the installed web interface in place; a WWW-only update leaves the installed application in place. Temporary version mismatch can make controls unavailable or incomplete.

## Before uploading

1. Confirm the physical PCB revision and the reported board version are `601` or `602`.
2. Record the exact configured frequency in MHz, core voltage in mV, fan policy, pool settings, and network settings. Follow [Settings preservation](settings-preservation.md). Keep passwords privately.
3. Verify the release's `manifest.json`, supported revisions, app/web version match, and `SHA256SUMS`. Do not use artifacts copied from another developer's build directory without provenance.
4. Confirm the existing partition layout is compatible. The intended baseline is upstream ESP-Miner v2.14.2 with its standard 16 MiB ESP32-S3 layout: 4 MiB application slots and a 3 MiB WWW partition. Devices with custom partition maps need a separate recovery plan.
5. Connect to the miner through its configured Wi-Fi network. These firmware upload endpoints refuse uploads while the miner is in AP or AP+station mode. Finish network setup first.

## Update through 5tratumOS

For an existing compatible **Bitaxe Gamma 601 or 602** running 5tratumFW, update 5tratumOS to **0.8.48 or later** through **Settings → Updates** and reload the portal. Open **Fleet → Devices** in Orbit, or **5tratumFW devices** in Classic. On the correct Gamma, select **Firmware update → BETA → Check for updates** and confirm the model-labelled **Bitaxe Gamma 601/602 Beta11 — experimental** release. Select **Install firmware → Confirm install** and keep the miner powered until **Firmware installation verified** appears. The OS installs both matching images, handles restarts and verifies versions and retained settings. See the [complete one-click instructions](../DOWNLOADS.md#update-through-5tratumos).

For Beta11, install and verify both images over Wi-Fi with the adapter disconnected. The update does not enable Ethernet by default. Open **Network → Enable wired Ethernet (W5500)** only when ready to follow [the powered-off wiring and save/restart procedure](gamma-ethernet.md#enable-ethernet). Ethernet-only update qualification remains pending.

Future compatible releases are discovered without another OS update for each firmware version; BETA also includes compatible stable releases. Firmware-update eligibility is independent of separate central miner-control qualification, including Power. Hardware, identity, package and API checks still apply; new hardware or a breaking contract can require an adapter. The one-click updater does not perform the initial conversion from stock firmware.

## Upload the paired release

Follow any order stated in the release notes. Otherwise:

1. Open the miner's **Updates** page and choose the release's `esp-miner.bin` under **Miner firmware**. Keep power connected until the upload completes and the device restarts.
2. Reconnect to the miner and confirm it reports the intended application version. Inspect any boot/storage errors before continuing.
3. Choose the matching `www.bin` under **Web interface**. Keep power connected and wait for the success response and browser reload.
4. Restart the device once after the web upload, then reconnect. The application reads the web version at boot, so its mismatch warning can persist until this restart even after a successful WWW upload. Reload the page fully if old assets remain cached. Confirm the application and web versions match; check the exact configured clock, voltage, fans, pools, and network settings against your record.
5. Confirm fresh telemetry, the selected pool route, and mining behavior expected from the current schedule or manual pause. A paused miner may legitimately show no new shares. After resuming, check accepted shares and pool-side worker activity separately.

The **5tratumFW releases → Check releases** control opens the WillItMod/5tratumFW GitHub release channel, including BETA prereleases. Read the compatibility, release notes and validation record before selecting files. Obtain the paired images from that channel or build this repository yourself.

Before downgrading Gamma Beta11 to Beta10, clear **Enable wired Ethernet (W5500)**, save, power off and disconnect the adapter, then install both matching Beta10 files over Wi-Fi. Older firmware does not reserve the accessory pins for W5500.

Before downgrading Gamma Beta10 or Beta11 to Beta9, read the [authentication-journal compatibility limits](upstream-migration-20261028.md). Changing either saved pool authentication flag from its original individual-key baseline can require an extended journal that Beta9 cannot read. Restore both authentication flags to their original baseline while running Beta10 or Beta11 to regain the legacy journal format, or retain compatible firmware and the unit-specific recovery backup. An old app/WWW pair alone does not repair an extended journal. Ordinary tuning changes retain the legacy format when authentication still matches that baseline.

If the web interface is damaged, follow [Recovery](recovery.md). Do not erase flash or write a factory configuration as a routine update step.
