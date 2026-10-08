# OctAxe candidate migration and recovery

This is a qualification procedure for **NerdOCTAXE-γ / NERDOCTAXEGAMMA / eight BM1370 ASICs**. Beta2 is a development candidate installed on one owned PCB revision 2.2 unit after a verified recovery backup and actual layout inspection. Regulator strap/stock profile were reviewed against pinned hardware source; the package marking was not physically read. The dated [validation record](validation-oct-beta2.md) does not authorize compatibility with another assembly.

## Establish the unit and recovery record

Confirm the physical board/revision, installed model/ASIC count, regulator population/strap, temperature paths, cooling and existing flash/partition map before any write. A software model name and `Board::getVersion()` do not establish a PCB revision. Do not change an identity setting or regulator strap to satisfy a software check.

Retain a private full backup for that exact unit and the exact original firmware/recovery package. Record configured frequency, core voltage, version rolling, both fan/PID settings, thermal limits, network and both pool connections. Record slots/schedules when present; an older API lacking a feature does not prove its NVS data is absent. Exported or masked passwords are not a credential backup. Preserve this unit's settings rather than copying values from another miner.

The candidate's **16 MiB / 3 MiB WWW / 4 MiB OTA-slot** source layout was verified on that owned unit and must be checked separately on another unit. Confirm the current device layout can accept the images without replacing its partition table or NVS. Filename checks cannot detect a wrongly selected physical board. Gamma, QAxe, NERDOCTAXEPLUS/BM1368 and other Oct images are different targets.

## Matched pair and routine OTA

Use `esp-miner-NerdOCTAXE-Gamma.bin` and `www.bin` from one verified OctAxe candidate package. Its descriptor/web version must both be `5tratumFW-oct-0.1.0-beta.2`. Read the manifest, qualification notes and checksums. The intended release family is `octaxe-v…`; a QAxe/Gamma pair or generic GitHub “latest” download is not an OctAxe selection.

Once the exact unit/layout and recovery path are established for a supervised bench test:

1. Keep normal power, cooling and network access stable. Retain the original settings record and unit-specific recovery backup.
2. Use the running firmware's supported application updater to upload `esp-miner-NerdOCTAXE-Gamma.bin` first. Retain any enabled OTP/authentication. Wait for completion and restart; do not interrupt a write.
3. Upload the matching Oct `www.bin` through the web-interface updater, then reload the browser. Application-first order avoids loading a new web interface against an older application without its required APIs.
4. Confirm model, eight ASICs and matching app/web versions. Compare every retained operating setting, both fan policies, pools, slots and schedules with the original record before claiming preservation.
5. Verify fresh sensor/counter readings, cooling, accepted shares, restart and the separately documented pause/resume/protection gates. A responding web page alone is not successful hardware qualification.

Once a qualifying Oct release is published, **Updates → Check updates** selects only the exact Oct model/family and complete app/WWW pair. It provides downloads; it does not install or restart the miner. No published candidate means no release is offered. Manual update and OTP rules remain in place.

If the page cannot identify an older application, inspect only the model/version/ASIC fields from `/api/system/info` locally. The full response may contain private network/pool data. Retry the identity read or use the original firmware's supported updater. Do not rename a different board's image or bypass authentication to pass a filename check.

## Network recovery

If the application still responds but web assets fail, use that exact application's documented web updater/recovery page with the matching WWW image and existing access policy. An interrupted WWW update can damage web assets while leaving the separate application intact. Do not erase the device simply to repair the website.

If application and web versions differ, retain the known application and finish the verified same-family pair. A hard reload may repair stale browser assets, but it does not replace a missing or wrong WWW image. Failed upload acknowledgement is not proof of successful installation; check the application/served web versions after the device returns.

## USB bootloader recovery

Follow the manufacturer procedure for the identified physical OctAxe PCB and controller. Confirm the button labels and USB data path first. Do not assume a button labelled KEY is reset or that another model's BOOT wiring applies.

For a board whose documented recovery procedure uses USB-only BOOT entry, release the buttons and disconnect both main power and USB, wait ten seconds, then hold its confirmed BOOT button while reconnecting USB only. A blank display can be normal in bootloader mode; it does not prove a serial connection. Verify the actual serial device and ESP32-S3 identity before continuing. macOS uses `/dev/cu.*`; Windows uses COM ports.

Keep ASIC/main power disconnected during any documented USB-only recovery session. Privately read and verify the current full-flash backup before a write. Restore only an exact verified backup of the same unit or a manufacturer recovery package matching the confirmed PCB and partition layout. After recovery, release BOOT, fully disconnect USB/power, then reconnect normal power as specified by the manufacturer and recheck identity/settings before mining.

The OTA application must never be written at `0x0` as though it were a factory image. The OTA pair cannot reconstruct an erased device; it contains no bootloader, partition table, selector or NVS. Do not guess offsets, force a different identity or erase NVS to obtain a boot. This project provides no Oct factory image, browser flasher or automatic trial rollback guarantee.
