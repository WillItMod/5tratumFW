# QAxe++ a7 validation

Checked on 7 October 2026. The actual test unit reports **NerdQAxe++**, profile `NERDQAXEPLUS2`, **four BM1370 ASICs**. Its physical PCB/regulator revision remains unknown. These observations do not qualify QAxe+, OctAxe, CAN fleets or other revisions.

## Exact build

The application, WWW image and source archive were packaged from commit `70a0e578faca00998b5e33ca65c2af8fe17b1eee`, version `5tratumFW-qa-web-a7`, using ESP-IDF 5.5.3 pinned to digest `8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805`. The ESP32-S3 target build passed. Capture logging was disabled. The frontend production build and all **44 Karma tests** passed.

| Image | Bytes | SHA-256 |
| --- | --- | --- |
| `esp-miner-NerdQAxe++.bin` | 3,286,656 of the 4 MiB OTA slot | `1a4b4296157da5f484354f1355d447cc6d5fa24c96d6ca23e214c57988cf0435` |
| `www.bin` | 3,145,728 | `a0722e943b0c2ac29e0d56e2368257189ad084fc3634d2f5095a739ffe945755` |
| `source.zip` | 13,270,452 | `64fa536506f4c8ad1531da79bce7235898752aed031c2c873bf24ce598c3e8e0` |

The package's `powerControlQualification` describes the pre-install checkpoint. This later record does not change its source archive or images.

Four production sanitizer runners passed: mining-control state/lease retirement, actual ASIC/backend/QAxe board methods, hashrate monitoring, and weekly policy/schema/persistence/HTTP. The backend runner covers eighteen scenarios, including cold regulator startup, wrong chip count, invalid voltage, missing/timer-failed workers, UART/baud faults, cancelled ramps and thermal latches. Hardware, I2C/GPIO, sensor and RTOS boundaries are simulated. These are separate from the live observations below.

## Live paired OTA and preserved configuration

The full 16 MiB original recovery backup was rechecked against its recorded SHA-256 before OTA. Application OTA wrote the inactive `ota_0` slot; a6 remains in `ota_1`. No NVS erase, factory image or partition-layout change was used. Automatic trial rollback is not enabled.

Both application and WWW uploads were acknowledged once, with a private durable intent recorded before each write. The booted a7 app and served index plus four entry bundles matched the corresponding build. All **33 exported settings**, apart from the expected version change, were retained, including **500 MHz / 1130 mV**, `vrFrequency:25011`, fan/PID configuration and saved pool configuration. `vrFrequency` is the ASIC version-rolling frequency setting, not the regulator switching clock. All named pool/tuning slots and the pre-existing pool scheduler were unchanged. This is exported-configuration comparison, not a complete post-update NVS/credential byte comparison.

The paired update check observed approximately **4.014 TH/s** and eight accepted shares. The live MUX route still provided 5TRAT identity and forwarded job context.

## Live power and weekly policy checks

| Check | Observation |
| --- | --- |
| Manual pause | Requested/applied paused agreed; core-voltage telemetry reached **0 mV** and hashrate zero. A settled regulator-power sample fell from **91.125 W** to **0.0625 W**. |
| Cooling and network while paused | HTTP remained available and the rear fan reported **3,024 RPM**. The existing front PID fell to its 15% minimum as the board cooled; its reported RPM became zero. Fan settings were retained, not overridden by the test. |
| Manual resume | Requested/applied running agreed; measured core voltage **1,134 mV** was within 30 mV of the saved 1,130 mV. Both fans subsequently reported rotation. |
| Weekly pause | A temporary Wednesday UTC window was stored durably. Follow schedule paused the shared ASIC rail. Manual Resume ran inside the window; Follow schedule paused it again. |
| Restart inside the window | The enabled window survived restart; the manual override cleared. The rail remained off while network time was unavailable and stayed off once valid time established the active window. |
| Restoration | The original disabled power schedule and all exported settings, named slots and pool schedule were restored or retained. Resume restored mining. The first restored sample reported **4.046 TH/s**, one fresh accepted share, **1,128 mV** core and **992 / 2,981 RPM** fans. After clearing the temporary manual override, another sample reported **4.016 TH/s**, fourteen accepted shares, both fans rotating and the original 500/1130/25011 operating point. |

The power field is regulator-derived telemetry. It excludes some controller/fan loads and is not a wall-meter measurement. Immediately after a transition it can retain the preceding cached sample; those samples are not used to claim steady power savings. None of these observations is a tuning preset or recommended operating point.

## Independent ASIC routing remains unqualified

This candidate still broadcasts jobs to the chain and explicitly reports `independentWorkAssignment:false`. It does not enable per-ASIC pools, clocks, voltage or power. Four counter rates and MUX coin/block metadata do not establish independent concurrent mining. The [targeting investigation](asic-work-targeting-investigation.md) describes the missing job-acceptance mechanism, response checksum and pipeline/drain evidence.

Sustained soak, physical display photographs/readability, deliberately induced thermal/fan/UART/storage faults, stock restoration and unidentified revision compatibility remain incomplete. Simulated fault tests are not physical fault qualification. The source-level controller also supports QAxe+, but that model was not physically tested here.
