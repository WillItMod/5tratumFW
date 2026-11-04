# Install 5tratumFW on a Gamma 601 or 602

These application and web images are for **Bitaxe Gamma PCB revisions 601 and 602 only**. Check the physical PCB revision and the device's reported board version before updating. The firmware requires an exact stored identity of `601` or `602`; this guard is not physical board autodetection. Do not change an unrelated device's stored identity to bypass it.

GT800, NerdQAxe, NerdOctAxe, other Bitaxe boards, and custom board profiles are outside this Gamma image's supported hardware. NerdQAxe++ has a separate firmware family in this repository; use its [model-specific guide](nerdqaxe.md) and image pair. A successful build does not establish installed-device testing; consult the release's validation record for the specific revision and version.

**Current public Gamma release: [Beta15 experimental BETA](https://github.com/WillItMod/5tratumFW/releases/tag/v0.1.0-beta.15).** Read [Tuning](autotuning.md) before enabling the optional controller and [fast-error software qualification](validation-autotune-fast-errors-20261104.md) for its limits. The Beta15 pair was installed and its original settings and served assets verified on the owned Gamma 601; current GET-only form checks did not save 900 MHz or start tuning. See [current flow validation](validation-tuning-flow-20261104.md). [Earlier release evidence](validation-autotune-release-20261103.md) retains its own image/source scope. Gamma 602 tuner admission/restore remains unqualified. W5500 stays off by default; physical LAN/Ethernet-only OTA remain unqualified. Previous releases retain their own evidence.

## Choose the correct files

Use the paired files from the same 5tratumFW release or verified local build:

| File | Destination | What it changes |
| --- | --- | --- |
| `esp-miner.bin` | **Updates → Miner firmware** | Complete ESP32-S3 application with its matching embedded web interface, mining logic, power control, APIs and OLED interface. This is the normal single-file Gamma update. The device restarts after success. |
| `www.bin` | **Updates → Web interface** | Optional separate copy of the matching web interface for custom-WWW, compatibility or recovery use. It does not install application logic. |
| Source archive, manifest, checksums | Your computer | Build inputs and provenance; never upload these to either update control. |

The upload controls require the exact filenames above. Keep the files inside their versioned release directory so files from different releases do not get mixed. `esp-miner.bin` is an application OTA image, not a factory image. Do not upload a merged/factory BIN to either control. `www.bin` is the generated **3 MiB partition image** for the standard layout, even when its compressed web assets occupy much less space. Do not replace it with a ZIP, an asset directory, or a smaller raw payload.

Gamma Beta15 retains the AxeOS single-file format: the matching web assets are compiled into `esp-miner.bin`. Firmware migration disables an older custom-WWW selection, so the new application normally serves its own embedded interface. An application update leaves the separate WWW partition's bytes untouched; those bytes are not required for normal embedded-interface operation. A WWW-only update still leaves the application unchanged. QAxe and OctAxe use different builds and still need their separate application/WWW pairs.

See [single-file updates](single-file-updates.md) for the verified format and migration limits. This does not establish that an unknown third-party updater, including Harlo, accepts the application or retains a compatible partition layout and board identity.

## Before uploading

1. Confirm the physical PCB revision and the reported board version are `601` or `602`.
2. Record the exact configured frequency in MHz, core voltage in mV, fan policy, pool settings, and network settings. Follow [Settings preservation](settings-preservation.md). Keep passwords privately.
3. Verify the release's `manifest.json`, supported revisions, app/web version match, and `SHA256SUMS`. Do not use artifacts copied from another developer's build directory without provenance.
4. Confirm the existing partition layout is compatible. The intended baseline is upstream ESP-Miner v2.14.2 with its standard 16 MiB ESP32-S3 layout: 4 MiB application slots and a 3 MiB WWW partition. Devices with custom partition maps need a separate recovery plan.
5. Connect to the miner through its configured Wi-Fi network. These firmware upload endpoints refuse uploads while the miner is in AP or AP+station mode. Finish network setup first.

## Update through 5tratumOS

In **5tratumOS → Settings → Updates**, use the recommended **0.8.52 on MAIN** or **0.8.52-dev on DEV** when offered, then reload the portal. Keep your existing OS channel; a firmware beta does not require switching it. Firmware discovery requires **0.8.50 or later**. OS 0.8.52 also enables compatible native DualMUX controls from capability metadata; it does not require a fixed firmware-version list.

To start tuning after installation, choose your options and **Save options** before accepting the separate per-run waiver. A higher draft clock needs its advanced warning acknowledgement before Save; saving never starts the tuner. Then accept the run waiver and use **Start tuning** when ready.

If this miner is already running a tuning candidate, first open its Tuning page, select **Disable and restore backup**, and wait until the original settings are confirmed restored and tuning is inactive. If already paused and ordinary restoration is waiting, use the separate **Restore original settings and resume mining** action and wait for completed verification. Do this before installing another firmware pair; an accepted Stop request alone is not completed restoration.

1. Open **Fleet → Devices** in Orbit, or **5tratumFW devices** in Classic.
2. On the correct miner, select **Firmware update → BETA → Check for updates**.
3. Confirm the model-labelled version: **Bitaxe Gamma 601/602 Beta15**, **NerdQAxe++ Beta12**, or **NerdOctAxe Gamma Beta10**.
4. Select **Install firmware → Confirm install**. Keep power connected until **Firmware installation verified** appears.
5. Use **Open miner**, reload the miner page and check matching application/web versions, saved settings, temperatures, fans and mining.

The OS installs both matching images and verifies the resulting pair and saved settings. Version 0.8.50 includes the older Nerd settings-verification/redirect correction. Future compatible releases are discovered without another OS update for each firmware version. Hardware, identity, layout and API checks still apply. Initial conversion from stock firmware remains a separate manual process. NerdQAxe++ Beta6 remains selectable only when it upgrades an older installation; it is not a downgrade option.

Complete the Gamma paired update over Wi-Fi. Ethernet is not enabled by the update; use the [adapter instructions](gamma-ethernet.md) separately. Read [Tuning](autotuning.md) for opt-in setup, waiver and restore controls.

## Upload the Gamma application

Follow any order stated in the release notes. Otherwise:

1. Open the miner's **Updates** page and choose the release's `esp-miner.bin` under **Miner firmware**. Keep power connected until the upload completes and the device restarts.
2. Reconnect to the miner and confirm it reports the intended application version. Inspect any boot/storage errors before continuing.
3. Fully reload the page to load the embedded interface. Confirm the intended application and served web version; check the exact configured clock, voltage, fans, pools and network settings against your record.
4. Confirm fresh telemetry, the selected pool route, and mining behavior expected from the current schedule or manual pause. A paused miner may legitimately show no new shares. After resuming, check accepted shares and pool-side worker activity separately.

### Optional separate web image

If you deliberately use the separate custom-WWW interface or need its recovery workflow, upload the same release's `www.bin` under **Web interface** and wait for the success response. That enables the separate matching interface. Follow the reported restart requirement, fully reload, and verify the served version and original settings. Do not upload an older or another family's WWW image over the matching embedded interface. The embedded `/recovery` page provides **Disable Custom Web UI** to return to the application's own interface.

The **5tratumFW releases → Check releases** control opens the WillItMod/5tratumFW GitHub release channel, including BETA prereleases. Read the compatibility, release notes and validation record before selecting files. Obtain the paired images from that channel or build this repository yourself.

Before downgrading Gamma Beta11 to Beta10, clear **Enable wired Ethernet (W5500)**, save, power off and disconnect the adapter, then install both matching Beta10 files over Wi-Fi. Older firmware does not reserve the accessory pins for W5500.

Before downgrading Gamma Beta10 or Beta11 to Beta9, read the [authentication-journal compatibility limits](upstream-migration-20261028.md). Changing either saved pool authentication flag from its original individual-key baseline can require an extended journal that Beta9 cannot read. Restore both authentication flags to their original baseline while running Beta10 or Beta11 to regain the legacy journal format, or retain compatible firmware and the unit-specific recovery backup. An old app/WWW pair alone does not repair an extended journal. Ordinary tuning changes retain the legacy format when authentication still matches that baseline.

If the web interface is damaged, follow [Recovery](recovery.md). Do not erase flash or write a factory configuration as a routine update step.
