# Recover application or web updates

Begin with the least invasive repair. Keep the miner's power stable and preserve its settings record. A routine repair should not erase flash or overwrite NVS. These steps apply only to a physical Gamma PCB `601` or `602` with the matching stored identity and compatible partition layout.

## The web interface is blank or incomplete

1. Try a full browser reload to discard cached assets. Verify you are opening the miner's configured network address.
2. If the application still responds, open the built-in **5tratumFW BETA Recovery** page at `/recovery`. This page is embedded in the application and does not depend on the WWW filesystem. An older installed application can still display its earlier recovery title.
3. Upload the exact `www.bin` from the same 5tratumFW release as the installed app. Wait for the HTTP response; the embedded page advises allowing 60 seconds and avoiding restart before a response. Do not disconnect power during the write.
4. After success, reload the miner's normal page. If needed, restart only after the upload response, then confirm versions and operating settings.

WWW updates erase and rewrite the web partition, so an interrupted write can leave web assets unavailable. The application remains a separate image. If the embedded recovery page is accessible, it can repair that partition without a full factory flash.

For an application that responds to HTTP but has unusable web assets, the raw upload endpoints are `/api/system/OTAWWW` for `www.bin` and `/api/system/OTA` for `esp-miner.bin`. They take the raw binary body with `Content-Type: application/octet-stream`; do not send a multipart form. Prefer the provided update/recovery controls unless you understand this distinction. Uploads require configured station-mode network access and are refused in AP/AP+station mode.

## An upload fails

Read the reported error and confirm the target file. A file-too-large response means the image does not fit the device's WWW partition. Do not truncate or compress the partition image to make it fit. Application validation errors mean activation did not succeed; do not treat an HTTP error as a completed firmware update.

Recheck the release checksum and partition compatibility, then retry the matching file over a stable local connection. Wait for a clear result before restarting or starting another upload. If the device is reachable after an application restart, read its reported version to determine which image is actually running.

## The application does not boot or network access is unavailable

USB/serial recovery may be necessary. First inspect the serial boot log and identify the current partition map, active application slot, stored board identity, and reason for failure. Keep any private flash/NVS backup outside this repository; it can contain Wi-Fi and pool credentials.

Use the board vendor's exact recovery process for the physical PCB revision and your existing partition layout. An application OTA BIN is not a merged factory image and must not be flashed at address `0x0`. Writing only a factory-slot application may also leave an OTA selector pointing at another slot. Do not guess offsets or erase OTA/NVS data to force it to boot.

For reference, this repository's standard `partitions.csv` uses NVS at `0x9000` (24 KiB), factory app at `0x10000`, WWW at `0x410000` (3 MiB), OTA app slots at `0x710000` and `0xb10000`, and OTA selection data at `0xf10000`. These offsets describe the source layout; they do not establish the map of a previously modified device.

The paired release package intentionally contains no factory image. An upstream/vendor factory recovery image for the **exact** revision may restore a known baseline, but it can overwrite the stored operating point, network/pool settings, and board configuration. Treat factory recovery as a separate restoration procedure, then verify all settings before mining or reinstalling 5tratumFW.

## Storage or identity errors

The firmware stops on NVS initialization failures or an unreadable/unsupported stored board identity. It intentionally preserves flash instead of erasing it. Recover the correct settings/identity only after checking the physical Gamma revision. Do not set `601`/`602` on a different board to get past the guard.

An invalid stored mining schedule requests a pause until a valid schedule is saved. If the app and web interface are available, repair it in **Scheduler**. An enabled schedule waiting for network time is also a legitimate pause; check the reported clock before treating it as a boot failure.

After recovery, compare configured frequency, core voltage, fan policy, both routes, network settings, and schedule with your private record. Check fresh applied ASIC state and then pool-side worker activity. A completed flash alone does not establish mining or protection behavior.
