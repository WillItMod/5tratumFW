# Install 5tratumFW on a Gamma 601 or 602

These application and web images are for **Bitaxe Gamma PCB revisions 601 and 602 only**. Check the physical PCB revision and the device's reported board version before updating. The firmware requires an exact stored identity of `601` or `602`; this guard is not physical board autodetection. Do not change an unrelated device's stored identity to bypass it.

GT800, NerdQAxe, NerdOctAxe, other Bitaxe boards, and custom board profiles are outside this image's supported hardware. QAxe development belongs to a separate project. A successful build does not establish installed-device testing; consult the release's validation record for the specific revision and version.

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

## Upload the paired release

Follow any order stated in the release notes. Otherwise:

1. Open the miner's **Updates** page and choose the release's `esp-miner.bin` under **Miner firmware**. Keep power connected until the upload completes and the device restarts.
2. Reconnect to the miner and confirm it reports the intended application version. Inspect any boot/storage errors before continuing.
3. Choose the matching `www.bin` under **Web interface**. Keep power connected and wait for the success response and browser reload.
4. Reload the page fully if old assets remain cached. Confirm the application and web versions match; check the exact configured clock, voltage, fans, pools, and network settings against your record.
5. Confirm fresh telemetry, the selected pool route, and mining behavior expected from the current schedule or manual pause. A paused miner may legitimately show no new shares. After resuming, check accepted shares and pool-side worker activity separately.

The **5tratumFW releases → Check releases** control opens the WillItMod/5tratumFW GitHub release channel, including BETA prereleases. Read the compatibility, release notes and validation record before selecting files. Obtain the paired images from that channel or build this repository yourself.

If the web interface is damaged, follow [Recovery](recovery.md). Do not erase flash or write a factory configuration as a routine update step.
