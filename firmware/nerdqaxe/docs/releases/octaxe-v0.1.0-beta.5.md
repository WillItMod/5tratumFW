# OctAxe Gamma Beta5 — RC1 and saved tuning schedules

| Download both files | Purpose |
| --- | --- |
| [esp-miner-NerdOCTAXE-Gamma.bin](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.5/esp-miner-NerdOCTAXE-Gamma.bin) | Miner firmware |
| [www.bin](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.5/www.bin) | Matching web interface |

Verify [SHA256SUMS](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.5/SHA256SUMS) and install the application first, then the matching web image.

`5tratumFW-oct-0.1.0-beta.5` · `octaxe-v0.1.0-beta.5` · compiled/source revision `1a34b24100e1238e331d8145a70f6b08f85f943d`.

**NerdOCTAXE-γ / NERDOCTAXEGAMMA / BM1370×8 / ESP32-S3 / standard 320×170 display only.** Installed-device evidence is limited to the owned **PCB 2.2** assembly. Other revisions remain unqualified; inherited driver definitions do not establish their regulator, wiring, sensor or flash compatibility. This public BETA updates the Nerd base to shufps **v1.1.1-rc1**, retains pinned ESP-IDF 5.5.3 and incorporates the pinned certificate-bundle update.

**Scheduler → Tuning profile schedule** selects whole-miner frequency/voltage from ten saved tuning slots at up to sixteen weekly time points. For example, use slot 3 at 00:00, slot 1 at 09:00 and slot 2 at 21:00. Save the intended operating points first, then configure and enable the schedule. Synchronized network time and an explicit fixed UTC offset are required; tuning and pool schedules do not adjust automatically for daylight saving time. Manual/scheduled pauses and hardware protection retain priority. No default schedule, new operating point or automatic tuning is applied.

**OctAxe retains its existing 320×170 screen.** The experimental QAxe 480×320 RGB/BGR selector is not enabled for this family. Existing regulator selection/sequencing, both fans and thermal protection retain their device-specific behaviour. Native A/B sessions share the eight-chip chain; **independent per-chip work routing remains unsupported and disabled**.

Use **`esp-miner-NerdOCTAXE-Gamma.bin` and this release's matching `www.bin`**, with `manifest.json`, `SHA256SUMS`, `web-budget.json` and `5tratumFW-NerdOCTAXE-Gamma-5tratumFW-oct-0.1.0-beta.5-source.zip`. Verify the model, PCB and existing layout against the installation guide and keep a unit-specific recovery backup. The owned PCB 2.2 unit's 16 MiB flash, 3 MiB WWW partition and two 4 MiB OTA slots were separately verified; that evidence does not establish another assembly's layout. The regulator package marking was not physically read.

Install the application first through the running firmware's supported OTA uploader, wait for restart, then install the matching WWW and refresh. Preserve enabled authentication. These are OTA images, with no factory/NVS or replacement partition images. QAxe, single-chip Gamma and BM1368 OctAxe images are different targets. Existing identity, configured clock/voltage, both fan policies, network/pools, ten tuning slots, ten pool slots and original schedules are retained. [Installation and revision limits](https://github.com/WillItMod/5tratumFW/blob/main/firmware/nerdqaxe/docs/installation-5tratumfw-oct.md).

Full target builds, browser/host checks, package/source checks and all six consolidated GitHub runs passed. The exact pair completed a supervised update on the owned PCB 2.2 unit on 28 October 2026: all 108 packaged web records and saved settings matched; pause/resume and a temporary schedule at the **existing** operating point passed and were cleaned up. Accepted shares progressed at approximately 11.4 TH/s, and the owner confirmed the screen and both fans normal. The manifest's original `hardwareTested: false` receipt is unchanged; the dated validation supplies this later, limited evidence.

The short bench does not qualify other PCB revisions, new operating points, sustained thermal stability, every protection fault or controlled cold-power recovery. Central OS/MUX controls require their own exact-release admission update; publication alone does not enable them. [Exact images, evidence and limitations](https://github.com/WillItMod/5tratumFW/blob/main/docs/validation-aligned-20261028.md).
