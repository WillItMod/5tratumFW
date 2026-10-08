# OctAxe BETA installation and recovery — owned PCB 2.2 scope

This guide applies only to **NerdOCTAXE-γ / NERDOCTAXEGAMMA / eight BM1370 ASICs**. Beta3 is the first qualified BETA pair; its exact pair passed the supervised update on the owned PCB 2.2 unit, as recorded in [the Beta3 record](validation-oct-beta3.md). Prior Beta1/Beta2 bench evidence is limited to one owned PCB 2.2 unit after a verified private recovery backup and actual layout inspection. Regulator strap/stock profile were reviewed against pinned hardware source; the package marking was not physically read. No other assembly or revision is qualified by this guide.

## Establish the unit and recovery record

Confirm the physical board/revision, installed model/ASIC count, regulator population/strap, temperature paths, cooling and existing flash/partition map before any write. A software model name and `Board::getVersion()` do not establish a PCB revision. Do not change an identity setting or regulator strap to satisfy a software check.

Retain a private full backup for that exact unit and the exact original firmware/recovery package. Record configured frequency, core voltage, version rolling, both fan/PID settings, thermal limits, network and both pool connections. Record slots/schedules when present; an older API lacking a feature does not prove its NVS data is absent. Exported or masked passwords are not a credential backup. Preserve this unit's settings rather than copying values from another miner.

The target's **16 MiB / 3 MiB WWW / 4 MiB OTA-slot** source layout was verified on that owned unit and must be checked separately on another unit. Confirm the current device layout can accept the images without replacing its partition table or NVS. Filename checks cannot detect a wrongly selected physical board. Gamma, QAxe, NERDOCTAXEPLUS/BM1368 and other Oct images are different targets.

## Regulator selection and later PCB revisions

This image is not hardcoded to PCB 2.2. The [inherited OctAxe driver](../main/boards/nerdoctaxegamma.cpp) reads the existing GPIO3 strap: LOW or an unconnected pin selects the four-phase TPS53647 path; HIGH selects the six-phase TPS53667 path. The latter path is inherited for the later 3.0+ designs. Sensor probing also retains the older temperature-request path when the newer TMP451 muxes are absent. Neither a strap result nor a responding sensor proves a PCB revision.

A PCB 3.1 unit using the same **NERDOCTAXEGAMMA / NerdOCTAXE-γ / BM1370 ×8** design is expected to use this software target, but has **not been physically qualified here**. Before installation, confirm its actual regulator population and GPIO3 strap, controller/ASIC/regulator/fan/sensor pinout and the same 16 MiB flash with two 4 MiB OTA slots and a 3 MiB WWW partition. Preserve that unit's configured operating point and establish its own recovery record. A matching software target is not an all-revision compatibility promise; the installed-pair evidence remains scoped to the owned PCB 2.2 unit.

## Matched pair and routine OTA

Use `esp-miner-NerdOCTAXE-Gamma.bin` and `www.bin` from one verified OctAxe BETA package after its dated installed-pair record is completed. For Beta3, both descriptor/web versions must be `5tratumFW-oct-0.1.0-beta.3`. Read the manifest, qualification notes and checksums. The intended release family is `octaxe-v…`; a QAxe/Gamma pair or generic GitHub “latest” download is not an OctAxe selection.

Once the exact unit/layout and recovery path are established for a supervised bench test:

1. Keep normal power, cooling and network access stable. Retain the original settings record and unit-specific recovery backup.
2. Use the running firmware's supported application updater to upload `esp-miner-NerdOCTAXE-Gamma.bin` first. Retain any enabled OTP/authentication. Wait for completion and restart; do not interrupt a write.
3. Upload the matching Oct `www.bin` through the web-interface updater, then reload the browser. Application-first order avoids loading a new web interface against an older application without its required APIs.
4. Confirm model, eight ASICs and matching app/web versions. Compare every retained operating setting, both fan policies, pools, slots and schedules with the original record before claiming preservation.
5. Verify fresh sensor/counter readings, cooling, accepted shares, restart and the separately documented pause/resume/protection gates. A responding web page alone is not successful hardware qualification.

**Updates → Check updates** selects published releases for the exact Oct model/family and complete app/WWW pair. It provides downloads; it does not install or restart the miner. No release is offered when there is no compatible published pair. Manual update and OTP rules remain in place.

If the page cannot identify an older application, inspect only the model/version/ASIC fields from `/api/system/info` locally. The full response may contain private network/pool data. Retry the identity read or use the original firmware's supported updater. Do not rename a different board's image or bypass authentication to pass a filename check.

## Central management and initial installation

MUX **0.9.75** and 5tratumOS **0.8.31 → Devices** explicitly admit the matched OctAxe Beta3 (`5tratumFW-oct-0.1.0-beta.3`, `NerdOCTAXE-γ`, `NERDOCTAXEGAMMA`, BM1370 ×8) and QAxe Beta5 pairs from source `3ffbe84f43e265e61d5bae3eca1cd985def2bb3f`, with the expected fresh capability, identity and matching app/web source reports. Release admission does not qualify a different physical PCB. Central controls keep saved and running pool mode distinct and require an explicit restart when changing mode. A/B sessions still share the ASIC chain; they do not assign individual chips.

5tratumOS Devices can manage the pool/power schedules and verified application/web update pair on an already-qualified **5tratumFW** miner. It does not convert stock Nerd firmware. For an identified and qualified stock unit with a working web interface, use its manual application uploader first and then the matching Oct `www.bin`; newer upstream firmware places those controls in Danger Zone. The upstream USB browser flasher selects a different merged factory-image format and cannot install this OTA pair. See the [QAxe installation FAQ](installation-5tratumfw-qa.md#installation-faq) for the upstream format/API references; QAxe filenames and hardware qualification do not apply to OctAxe.

## Network recovery

If the application still responds but web assets fail, use that exact application's documented web updater/recovery page with the matching WWW image and existing access policy. An interrupted WWW update can damage web assets while leaving the separate application intact. Do not erase the device simply to repair the website.

If application and web versions differ, retain the known application and finish the verified same-family pair. A hard reload may repair stale browser assets, but it does not replace a missing or wrong WWW image. Failed upload acknowledgement is not proof of successful installation; check the application/served web versions after the device returns.

## USB bootloader recovery

Follow the manufacturer procedure for the identified physical OctAxe PCB and controller. Confirm the button labels and USB data path first. Do not assume a button labelled KEY is reset or that another model's BOOT wiring applies.

For a board whose documented recovery procedure uses USB-only BOOT entry, release the buttons and disconnect both main power and USB, wait ten seconds, then hold its confirmed BOOT button while reconnecting USB only. A blank display can be normal in bootloader mode; it does not prove a serial connection. Verify the actual serial device and ESP32-S3 identity before continuing. macOS uses `/dev/cu.*`; Windows uses COM ports.

Keep ASIC/main power disconnected during any documented USB-only recovery session. Privately read and verify the current full-flash backup before a write. Restore only an exact verified backup of the same unit or a manufacturer recovery package matching the confirmed PCB and partition layout. After recovery, release BOOT, fully disconnect USB/power, then reconnect normal power as specified by the manufacturer and recheck identity/settings before mining.

The OTA application must never be written at `0x0` as though it were a factory image. The OTA pair cannot reconstruct an erased device; it contains no bootloader, partition table, selector or NVS. Do not guess offsets, force a different identity or erase NVS to obtain a boot. This project provides no Oct factory image, browser flasher or automatic trial rollback guarantee.
