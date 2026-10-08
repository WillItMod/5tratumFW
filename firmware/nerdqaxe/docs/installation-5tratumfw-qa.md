# QAxe paired OTA installation and recovery

## Identify before choosing

This BETA targets NerdQAxe++ (`NERDQAXEPLUS2`, BM1370 ×4). Confirm the actual board/model, ASIC type/count, existing firmware settings and partition layout. Physical PCB revisions have different operating points; the test unit's revision remains unknown. A similar product name does not identify the wiring. Gamma, QAxe+ and OctAxe images are different families.

The inherited QAxe++ driver uses one `NERDQAXEPLUS2` application target; there is no separate 6.0/6.1 filename or compile profile. This is not a claim that both PCB revisions have been physically tested. The reported model and power connector do not independently verify the PCB/regulator wiring. The separate inherited rev7 regulator probe also does not extend this BETA's hardware qualification.

Download one **QAxe-specific matched pair** from the [model-labelled prereleases](https://github.com/WillItMod/5tratumFW/releases), with its `manifest.json`, `SHA256SUMS` and corresponding source. The Beta5 pair passed its scoped installed qualification; read [Beta5 validation](validation-qa-beta5.md) before selecting that version. On macOS/Linux, verify with `shasum -a 256 esp-miner-NerdQAxe++.bin www.bin`, comparing both entries against the published checksums. Keep the original firmware, current settings and a private full-flash backup for recovery.

## Installation FAQ

### Can I install from the original Nerd web interface?

Yes, for a qualified NerdQAxe++ unit that still boots and has a working web interface. Use the model-specific application and matching web image from the **same QAxe release**. In stock v1.0.36, open **Settings → Manual Firmware Update**, upload `esp-miner-NerdQAxe++.bin` first, and wait for the application to finish and restart. Then use **Manual WWW Update** to upload that release's `www.bin`. Reopen the miner, bypass stale browser cache if needed, and check that application/web versions match and the original settings and mining operation remain intact. Follow the identification and retention checks in [Routine OTA upgrade](#routine-ota-upgrade).

In stock **v1.1.0.1**, manual uploads are hidden until **Danger Zone** is enabled:

1. Open **Settings** and find the **Mining Settings** card. Single-click the faint warning-triangle button at the right of its heading; its tooltip is **Danger Zone**. Enabling this UI mode does not require changing or saving frequency/voltage settings.
2. On the same **Settings** page, scroll to **Release & Update** and below the GitHub updater to the newly visible **Legacy Update** card. It contains **Manual Firmware Upload** and **Manual WWW Upload**, each with **Browse** and **Flash** buttons.
3. Under **Manual Firmware Upload**, browse to `esp-miner-NerdQAxe++.bin` from the selected QAxe release and flash the application first. Wait for the miner to restart, then upload that release's matching `www.bin` through its web-image updater and verify the pair as described below.

The **Install from GitHub** control selects upstream Nerd firmware; it is not the selector for this 5tratumFW pair. The factory-image filename shown there is not the manual uploader's expected application filename. A changed menu location does not qualify a different PCB or establish that stock v1.1.0.1 migration has been physically tested here. These instructions follow the pinned [Mining settings toggle location](https://github.com/shufps/ESP-Miner-NerdQAxePlus/blob/762092dab9c53f1b257411759ce4d5b0e1bc9462/main/http_server/axe-os/src/app/pages/edit/edit.component.html#L325-L329), [Danger Zone control](https://github.com/shufps/ESP-Miner-NerdQAxePlus/blob/762092dab9c53f1b257411759ce4d5b0e1bc9462/main/http_server/axe-os/src/app/components/advanced-toggle/advanced-toggle.component.html) and [Legacy upload card](https://github.com/shufps/ESP-Miner-NerdQAxePlus/blob/762092dab9c53f1b257411759ce4d5b0e1bc9462/main/http_server/axe-os/src/app/pages/settings/settings.component.html#L133-L178). The older [stock v1.0.36 update controls](https://github.com/shufps/ESP-Miner-NerdQAxePlus/blob/v1.0.36/main/http_server/axe-os/src/app/pages/settings/settings.component.html) remain a separate UI version.

### Why does the Nerd browser flasher use one file?

The upstream USB browser flasher selects a merged `esp-miner-factory-…` image containing bootloader, partition table, application, web filesystem and OTA selection data. It is a different format from these two OTA files; the web assets are not embedded in our application. The upstream v1.1.0.1 release still provides separate model-specific application and `www.bin` files alongside its factory images. See the [upstream release assets](https://github.com/shufps/ESP-Miner-NerdQAxePlus/releases/tag/v1.1.0.1) and [USB flasher implementation](https://github.com/shufps/nerdqaxe-web-flasher/blob/main/src/components/LandingHero.tsx).

The USB flasher's **keep configuration** option skips its expected NVS region; a full merged-image write also covers that region. It still writes other factory-image regions when configuration is kept. Its catalog and address-zero factory write cannot install our published application/WWW pair. Do not upload an OTA application at address zero or select a similar upstream model as a substitute. A future 5tratumFW USB flasher needs a separately qualified recovery package and model/layout manifest. This project does not publish factory/NVS images or anyone's full-flash backup.

The upstream miner-side one-click updater is separate from the USB flasher: it extracts application/WWW sections from its factory container and updates the existing partitions. This does not make the two OTA files interchangeable with a merged image. See the [pinned v1.0.36 factory-container updater](https://github.com/shufps/ESP-Miner-NerdQAxePlus/blob/v1.0.36/main/http_server/handler_ota_factory.cpp).

### Will HashWatcher recognise the miner?

Recognition is expected from inspection of the public HashWatcher gateway, but the complete HashWatcher application has not been tested with this BETA. The gateway reads `/api/system/info`, uses `deviceModel` to recognise names containing `qaxe` or `nerdq`, and records the firmware version without an upstream-only version allowlist. 5tratumFW retains that API, the `NerdQAxe++` model, MAC identity and legacy telemetry fields. This is evidence for discovery/recognition, not a claim that HashWatcher exposes the new slots, schedules or MUX A/B controls. See the [HashWatcher gateway detector](https://github.com/gpena208777/HashWatcherGateway_Desktop/blob/main/app/gateway/hub_agent.py) and [retained system-information handler](../main/http_server/handler_system.cpp).

## Check for releases

Open **Updates → Check updates**. The browser checks the public WillItMod/5tratumFW release catalog for a QAxe-specific release containing both `esp-miner-NerdQAxe++.bin` and `www.bin`. Gamma and incomplete image pairs are excluded. Download both from the same release, read its model/qualification notes and verify checksums. The check does not install or restart the miner. Manual OTA and OTP authentication are unchanged.

## Routine OTA upgrade

1. Confirm normal power, working network access and cooling. Record configured frequency, voltage, version rolling, fan settings, both pool connections, ten pool/tuning slots and both schedules. Masked passwords are not an exported credential backup.
2. Open the running firmware's update page. Upload `esp-miner-NerdQAxe++.bin` through the application uploader **first**. Let the miner finish/restart; do not interrupt power during writes. The v1 API remains available to the older web interface.
3. Upload the matching QAxe `www.bin` through the web-interface uploader. Refresh after completion. Installing the application first avoids loading a newer web interface against an older application without its v2 API.
4. Reopen the miner and confirm application and web versions match. If cached browser assets remain, reload with cache bypass. Check the device model, retained operating point, fans, temperatures, shares and both schedules.
5. If connected to MUX, confirm the physical parent and both sessions independently. Check fresh coin/job data and actual accepted shares on the intended target for A and B.

These files are application and WWW **OTA** images. They are not a complete factory image and do not include bootloader, partition table, OTA selection or NVS. Do not overwrite another board's partition layout.

### If the update page cannot identify the model

In the published QAxe Beta 1 web interface, a selected application can show **Filename does not match this device** while **Expected: Loading device model…** is still displayed. This means no expected filename was obtained; it does not prove a different PCB revision or an incompatible file. The page reads the newer `/api/v2/settings` endpoint, which older upstream applications do not provide.

Read `/api/system/info` on that miner and check `deviceModel`, `ASICModel`, `asicCount` and `version` locally; do not share the complete response, which also contains network/pool settings. A reported NerdQAxe++ uses the exact application name `esp-miner-NerdQAxe++.bin`. Do not rename another model's binary to satisfy a filename check. If the new WWW was installed first and the old application remains, Beta 1's web-interface uploader may also fail because its authentication check expects the v2 API. Keep the application running and use its original supported OTA mechanism with the exact matching image and any enabled OTP authentication; do not bypass authentication or assume the broken page can restore itself. Provide the reported model and application version for specific recovery steps. A model-loading failure alone does not require a factory flash or NVS erase.

Beta 2 reads update identity through the shared v1 endpoint, separates identity errors from filename mismatches, offers Retry, and retains legacy OTP verification. Beta 1 downloads remain unchanged and do not contain this correction. Other Beta 2 settings pages require its matching application; upload the application first even though the corrected update page can identify older applications.

## Dual MUX setup

Use two SV1 connections to the same MUX Stratum host/port, with distinct worker names. Select **Dual pool**, retain the desired whole-chain job balance and save. Changing pool mode requires a restart; review the saved/runtime state rather than assuming the request acknowledgement proves Dual is running.

**Current central management:** MUX **0.9.75** and 5tratumOS **0.8.31 → Devices** explicitly admit the matched QAxe Beta5 (`5tratumFW-qa-0.1.0-beta.5`, `NerdQAxe++`, `NERDQAXEPLUS2`, BM1370 ×4) and OctAxe Beta3 (`5tratumFW-oct-0.1.0-beta.3`, `NerdOCTAXE-γ`, `NERDOCTAXEGAMMA`, BM1370 ×8) pairs from source `3ffbe84f43e265e61d5bae3eca1cd985def2bb3f`. Admission also requires the expected fresh capability, identity and matching app/web source reports; a similar name, chip count or version prefix is insufficient. This release admission does not qualify another physical PCB revision.

MUX 0.9.75 passed its sampled live acceptance. 5tratumOS 0.8.31 completed the normal system update with settings/state preserved, followed by separate installed-file verification and seven GET-only observations over sixty seconds. The qualified QAxe Beta5 and OctAxe Beta3 miners reported both native streams, running Dual mode with 50% job allocation, and bound pool, power and paired-updater admission. This is sampled API and control-admission evidence; the check did not exercise an authenticated portal UI, perform new miner-control writes or establish independent physical-ASIC assignment.

For admitted configurations, **Miners → Dual MUX mining** reviews the saved/running mode, A/B allocation and any explicitly selected **Connect B to this MUX** proposal before Apply. Mode changes require an explicit restart, followed by actual runtime-state verification. Unrelated operating/network/fan/password/profile/schedule settings remain unchanged. OTP-enabled, active AP or CAN-controlled configurations are refused by the narrow central control.

5tratumOS **Devices** provides pool, schedule and qualified paired-update management for miners already running supported **5tratumFW**. It handles the matching application/web pair; it does not perform the initial conversion from stock Nerd firmware. Use the manual stock web-OTA steps above for that first installation.

**Historical MUX 0.9.73 restriction:** its remote Dual-mode switch admitted QAxe Beta1 only, so QAxe Beta2/3 used the miner's **Pool routing → Dual pool** control. That was a restriction of that older management switch, not of native A/B registration, parent grouping, routing or coin/job injection. It is not the current 0.9.75 admission rule.

A and B are network sessions. Their independent targets and counters do not prove independent physical-chip assignment. The miner's Pool routing page retains direct-pool controls and ten named slots; profile Apply targets A or B in Dual mode.

## Physical screen and recurring logo

Startup shows scrolling status events. While mining, the large 5tratumFW logo slide appears for three seconds every two minutes on the mining overview, after at least thirty seconds without a button press and while telemetry is fresh. Shutdown, thermal, enrollment and animation screens take priority.

To see the recurring slide, open **Miner controls → Display**, disable **Automatic screen off**, save, and leave the physical screen on the mining overview for at least two minutes. With automatic screen off enabled, the display normally sleeps before the slide is due; updates preserve your saved choice.

On the physical screen, **P1 means session A** and **P2 means session B** in the web interface and MUX. QAxe board and regulator temperatures are shared measurements displayed once; they are not individual chip temperatures.

## USB bootloader recovery

Use a known data-capable USB cable. macOS exposes serial devices as `/dev/cu.*`; Windows uses COM ports. A blank screen in bootloader mode is expected, but first verify a serial device appears and identifies the correct ESP32-S3.

1. Release both buttons and disconnect **both main power and USB**. Wait ten seconds.
2. Leave main power disconnected. Hold the button explicitly labelled **BOOT** while reconnecting USB only; release BOOT after two seconds. Do not guess that an unlabelled KEY button is reset.
3. List the serial ports. Run `python -m esptool --chip esp32s3 --port SERIAL_PORT chip-id` with the actual detected port; stop if the identity or device selection is uncertain.
4. Read and privately verify a full backup before any write. The QAxe reference layout is 16MiB, but use the detected layout and existing recovery record, not a guessed offset.
5. Restore only the exact verified full backup for **that same unit**, or use a complete manufacturer recovery package matching its board/partition layout. The OTA pair alone cannot reconstruct an erased device.
6. Release BOOT, disconnect USB and normal power for ten seconds, then reconnect normal power only. Keep USB unplugged while checking the original boot screen and network address.

This project does not publish factory/NVS images or an erase-flash command. A browser flasher is not included in this BETA; do not choose an upstream web-flasher model solely because its name resembles the device.
