# OctAxe BETA installation and recovery — owned PCB 2.2 scope

**NerdOctAxe Gamma Beta 6 is a public EXPERIMENTAL release with a saved 480×320 RGB/BGR selector and Standard 320×170 default. Its standard screen and both fans were confirmed normal after a verified paired update using production updater modules in a local QA driver on the owned PCB 2.2 unit. The larger panel remains physically unqualified. QAxe files are not OctAxe firmware.**

This guide applies only to **NerdOCTAXE-γ / NERDOCTAXEGAMMA / eight BM1370 ASICs**. The current experimental Beta6 pair was compiled from clean source `5567a2d6c1f59808f197062e565fa6525a93f97b`; read [its original software validation](validation-oct-beta6.md). On **28 October 2026**, the production 5tratumOS updater modules, invoked by a local QA driver, completed **Beta 5 → Beta 6** on the owned PCB 2.2 unit, verifying both application/web versions and saved settings. The installed 5tratumOS system was not upgraded for this test; browser interactions were checked separately with fictional miners. The owner confirmed the standard screen and both fans looked normal. This later check does not qualify the larger panel or other assemblies.

The previous standard-screen [Beta5 pair](https://github.com/WillItMod/5tratumFW/releases/tag/octaxe-v0.1.0-beta.5) remains available. Beta5 passed separate supervised settings/mining, pause/resume and same-point profile-schedule checks; [that dated evidence](../../../docs/validation-aligned-20261028.md) remains scoped to Beta5. Bench evidence is limited to one owned PCB 2.2 unit after a verified private recovery backup and actual layout inspection. Regulator strap/stock profile were reviewed against pinned hardware source; the package marking was not physically read. No other assembly or revision is qualified by this guide.

## Establish the unit and recovery record

Confirm the physical board/revision, installed model/ASIC count, regulator population/strap, temperature paths, cooling and existing flash/partition map before any write. A software model name and `Board::getVersion()` do not establish a PCB revision. Do not change an identity setting or regulator strap to satisfy a software check.

Retain a private full backup for that exact unit and the exact original firmware/recovery package. Record configured frequency, core voltage, version rolling, both fan/PID settings, thermal limits, network and both pool connections. Record slots/schedules when present; an older API lacking a feature does not prove its NVS data is absent. Exported or masked passwords are not a credential backup. Preserve this unit's settings rather than copying values from another miner.

The target's **16 MiB / 3 MiB WWW / 4 MiB OTA-slot** source layout was verified on that owned unit and must be checked separately on another unit. Confirm the current device layout can accept the images without replacing its partition table or NVS. Filename checks cannot detect a wrongly selected physical board. Gamma, QAxe, NERDOCTAXEPLUS/BM1368 and other Oct images are different targets.

## Regulator selection and later PCB revisions

This image is not hardcoded to PCB 2.2. The [inherited OctAxe driver](../main/boards/nerdoctaxegamma.cpp) reads the existing GPIO3 strap: LOW or an unconnected pin selects the four-phase TPS53647 path; HIGH selects the six-phase TPS53667 path. The latter path is inherited for the later 3.0+ designs. Sensor probing also retains the older temperature-request path when the newer TMP451 muxes are absent. Neither a strap result nor a responding sensor proves a PCB revision.

A PCB 3.1 unit using the same **NERDOCTAXEGAMMA / NerdOCTAXE-γ / BM1370 ×8** design is expected to use this software target, but has **not been physically qualified here**. Before installation, confirm its actual regulator population and GPIO3 strap, controller/ASIC/regulator/fan/sensor pinout and the same 16 MiB flash with two 4 MiB OTA slots and a 3 MiB WWW partition. Preserve that unit's configured operating point and establish its own recovery record. A matching software target is not an all-revision compatibility promise; the installed-pair evidence remains scoped to the owned PCB 2.2 unit.

## Matched pair and routine OTA

Use `esp-miner-NerdOCTAXE-Gamma.bin` and `www.bin` from the same [OctAxe Beta6 release](https://github.com/WillItMod/5tratumFW/releases/tag/octaxe-v0.1.0-beta.6). Both descriptor/web versions must be `5tratumFW-oct-0.1.0-beta.6`. Read the manifest, experimental limits and checksums. The installed-pair check above covers the owned standard-screen unit only. The intended release family is `octaxe-v…`; a QAxe/Gamma pair or generic GitHub “latest” download is not an OctAxe selection.

Once the exact unit/layout and recovery path are established:

1. Keep normal power, cooling and network access stable. Retain the original settings record and unit-specific recovery backup.
2. Use the running firmware's supported application updater to upload `esp-miner-NerdOCTAXE-Gamma.bin` first. Retain any enabled OTP/authentication. Wait for completion and restart; do not interrupt a write.
3. Upload the matching Oct `www.bin` through the web-interface updater, then reload the browser. Application-first order avoids loading a new web interface against an older application without its required APIs.
4. Confirm model, eight ASICs and matching app/web versions. Compare every retained operating setting, both fan policies, pools, slots and schedules with the original record before claiming preservation.
5. Verify fresh sensor/counter readings, cooling, accepted shares, restart and the separately documented pause/resume/protection gates. A responding web page alone is not successful hardware qualification.

**Updates → Check updates** selects published releases for the exact Oct model/family and complete app/WWW pair. It provides downloads; it does not install or restart the miner. No release is offered when there is no compatible published pair. Manual update and OTP rules remain in place.

If the page cannot identify an older application, inspect only the model/version/ASIC fields from `/api/system/info` locally. The full response may contain private network/pool data. Retry the identity read or use the original firmware's supported updater. Do not rename a different board's image or bypass authentication to pass a filename check.

## Update from 5tratumOS

For an existing compatible **NerdOctAxe Gamma** running 5tratumFW:

1. Update 5tratumOS to **0.8.48** through **Settings → Updates**, then reload the portal.
2. Open **Fleet → Devices** in Orbit, or **5tratumFW devices** in Classic.
3. On the correct miner, select **Firmware update**, choose **BETA**, then **Check for updates**.
4. Confirm that **Firmware version** names **NerdOctAxe Gamma Beta 6**. This is the newest compatible default; Standard 320×170 remains the firmware's default screen.
5. Select **Install firmware → Confirm install** and wait for **Firmware installation verified**. The OS installs the application and its matching WWW, handles restarts and checks the versions and retained settings. Keep the miner powered throughout.
6. Use **Open miner** to inspect the result or select a screen using the steps below.

The same model-labelled updater offers **Bitaxe Gamma 601/602 Beta 10** and **NerdQAxe++ Beta 8** as their latest defaults. **NerdQAxe++ Beta 6** is also selectable when upgrading an older NerdQAxe++ installation; it is not a downgrade option for a miner already running Beta 6 or newer. Future compatible releases are discovered without another OS update for each firmware version. The BETA channel also includes compatible stable releases.

On older **5tratumOS 0.8.47**, select **Open miner** from Devices and use the miner's manual **Updates** page instead: install the matched `esp-miner-NerdOCTAXE-Gamma.bin` first, wait for restart, then install the same release's `www.bin` and refresh. Confirm both report `5tratumFW-oct-0.1.0-beta.6`. Keep the unit/layout requirements above. Neither route converts stock Nerd firmware; follow the separate initial-installation instructions below for a stock unit.

## Select the experimental larger screen

The reported larger panel is treated as **480×320 (153,600 pixels)** by accepted working assumption. Its controller, wiring and any vendor-specific flash layout remain unidentified. The application defaults to **Standard 320×170** and does not detect a larger panel automatically.

1. Complete the one-click update above and wait for **Firmware installation verified**, or finish the manual Beta6 application/WWW installation in that order. Reload the miner's browser page.
2. Open **Miner controls → Display → Screen**.
3. Select **3.5-inch 480×320 RGB** or **3.5-inch 480×320 BGR** for the larger panel. Keep **Standard 320×170** for the standard screen.
4. Select **Save this section**, wait for the saved/next-boot notice, then select **Restart device**. Selecting a choice alone does not change the active screen.
5. Reload and confirm the active profile. Check full-screen position, readable unmirrored text, colours, startup and refresh. If the red/blue colours are exchanged, try the other RGB/BGR choice and repeat save/restart. **Rotate miner screen** remains the separate 180° control.

The larger profiles change resolution, offset and mirror orientation together. The standard-screen check above does not qualify a larger panel; record its actual position, reflection, colours and restart behaviour before treating it as qualified. The screen selector does not change the board identity, clock, voltage, cooling, pools, slots or schedules.

To return to the previous physically checked standard-screen **Beta5**, first select **Standard 320×170** on Beta6, select **Save this section**, then **Restart device** and confirm Standard is active. Only then install the matching Beta5 application followed by its Beta5 `www.bin`. A saved or active larger profile blocks older OctAxe images that lack the selector; installing a QAxe image does not provide a downgrade path.

## Central management and initial installation

From **5tratumOS 0.8.48**, the paired firmware updater uses fresh hardware/identity evidence and the published package contract independently of the older Dual pool control list. A new compatible firmware version does not need another OS update merely to become discoverable and installable. Model, partition layout, image integrity and API checks remain required; new hardware or a breaking contract can require an adapter.

The separate central **Dual pool** and **Power** controls retain their own release/source qualification. Enabling one-click firmware installation does not extend those control permissions. Native A/B sessions and the miner's own controls remain available under their security policy. A similar model name, count or prefix is insufficient.

Earlier central qualification (8 October 2026): MUX 0.9.75 passed its sampled live acceptance. 5tratumOS 0.8.31 completed the normal system update with settings/state preserved, followed by separate installed-file verification and seven GET-only observations over sixty seconds. The qualified QAxe Beta5 and OctAxe Beta3 miners reported both native streams, running Dual mode with 50% job allocation, and bound pool, power and paired-updater admission. This is sampled API and control-admission evidence; the check did not exercise an authenticated portal UI, perform new miner-control writes or establish independent physical-ASIC assignment.

5tratumOS Devices can manage the pool/power schedules and verified application/web update pair on an already-qualified **5tratumFW** miner. It does not convert stock Nerd firmware. For an identified and qualified stock unit with a working web interface, use its manual application uploader first and then the matching Oct `www.bin`.

Stock **v1.1.0.1** uses the same hidden controls described in the [three-step manual-upload instructions](installation-5tratumfw-qa.md#can-i-install-from-the-original-nerd-web-interface): single-click the **Danger Zone** warning triangle beside **Settings → Mining Settings**, then scroll below **Release & Update** to **Legacy Update** on the same page. Use **Manual Firmware Upload → Browse / Flash** with the Oct application `esp-miner-NerdOCTAXE-Gamma.bin`, wait for the application restart, then install the matching Oct `www.bin` through the web-image updater. The displayed upstream `esp-miner-factory-…` GitHub choice is a different format, not the file expected by this manual application control. This source-verified UI path does not extend the owned PCB 2.2 qualification or claim a physically tested v1.1.0.1 migration.

The upstream USB browser flasher also selects a merged factory image and cannot install this OTA pair. See the [QAxe installation FAQ](installation-5tratumfw-qa.md#installation-faq) for the upstream format/API references; QAxe filenames and hardware qualification do not apply to OctAxe.

## Network recovery

If the application still responds but web assets fail, use that exact application's documented web updater/recovery page with the matching WWW image and existing access policy. An interrupted WWW update can damage web assets while leaving the separate application intact. Do not erase the device simply to repair the website.

If application and web versions differ, retain the known application and finish the verified same-family pair. A hard reload may repair stale browser assets, but it does not replace a missing or wrong WWW image. Failed upload acknowledgement is not proof of successful installation; check the application/served web versions after the device returns.

## USB bootloader recovery

Follow the manufacturer procedure for the identified physical OctAxe PCB and controller. Confirm the button labels and USB data path first. Do not assume a button labelled KEY is reset or that another model's BOOT wiring applies.

For a board whose documented recovery procedure uses USB-only BOOT entry, release the buttons and disconnect both main power and USB, wait ten seconds, then hold its confirmed BOOT button while reconnecting USB only. A blank display can be normal in bootloader mode; it does not prove a serial connection. Verify the actual serial device and ESP32-S3 identity before continuing. macOS uses `/dev/cu.*`; Windows uses COM ports.

Keep ASIC/main power disconnected during any documented USB-only recovery session. Privately read and verify the current full-flash backup before a write. Restore only an exact verified backup of the same unit or a manufacturer recovery package matching the confirmed PCB and partition layout. After recovery, release BOOT, fully disconnect USB/power, then reconnect normal power as specified by the manufacturer and recheck identity/settings before mining.

The OTA application must never be written at `0x0` as though it were a factory image. The OTA pair cannot reconstruct an erased device; it contains no bootloader, partition table, selector or NVS. Do not guess offsets, force a different identity or erase NVS to obtain a boot. This project provides no Oct factory image, browser flasher or automatic trial rollback guarantee.
