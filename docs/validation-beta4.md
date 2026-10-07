# Gamma Beta 4 validation

Version: `5tratumFW-0.1.0-beta.4`. Development BETA for Gamma PCB **601 and 602 only**. Checked on 7 October 2026.

This build places the operating-point confirmation immediately below the manual Review button or the selected tuning slot controls. Review remains read-only; only explicit Apply changes a setting. Manual input/mode changes invalidate the earlier review. Pool Apply and the ten pool/tuning slots retain their previous behavior. No preset operating points or hardware control changes are introduced.

## Build and source

The paired images and corresponding source archive were packaged from commit `655e0d09ed3a25635bb59a01385659b1f28dd08d`. This record was updated after the build and 601 bench checks; it does not change the packaged application or WWW bytes. The manifest's `hardware_tested:false` records the **pre-bench packaging checkpoint**, not the subsequent results below.

| Check | Result |
| --- | --- |
| Pinned full build | Passed with Node **24.14.0** and ESP-IDF **5.5.3**, including production Angular/gzip assets and the ESP32-S3 application/SPIFFS images. |
| Focused operating-profile component checks | **11 passed** in Chrome Headless, including actual DOM placement, one active review, cancellation/input/mode invalidation and exact explicit Apply payloads. |
| Full frontend Karma/Jasmine suite | **121 passed** after regenerating the API client. |
| Production Gamma host regression | All **eight entry points passed**, with simulated hardware/RTOS/storage boundaries. |
| Native production OLED | **27 LVGL views** and the production screen runtime passed with fixtures. This is separate from physical screen readability. |
| GitHub Linux validation | [Run 37645073267 passed](https://github.com/WillItMod/5tratumFW/actions/runs/37645073267) at the exact build commit above, completed **15:40:04 UTC**. Frontend/host checks, web budget, ESP32-S3 build, native OLED rendering and paired-source packaging succeeded. CI performs no miner flashing. |
| Application image | **1,715,472 bytes**, within its 4 MiB OTA slot. SHA-256 `025263c5aa6a5796b0ffc911bfc4e02e7ea7844d14e5bf500aba03abddc35e91`. |
| WWW image | **3,145,728 bytes**. SHA-256 `5e06a5deb65bc503da30d0095af32abe8bbfb15233a7529481e67467c8c86b33`. |
| Compressed web payload | **621,245 bytes**, **19.75%** of WWW capacity; enforced **876,628-byte** project budget passed. Payload excludes SPIFFS metadata. |
| Served web assets on 601 | Version, index and four entry bundles decoded and compared with the packaged build. |

The Linux build also includes the pool-scheduler test-fixture indentation clarification. It adds braces to the existing loop and does not change production behavior.

## Gamma 601 bench results

The application and matching WWW image were installed on a Gamma **601**, followed by the final restart needed to refresh the boot-cached web version. The private deployment record reached `paired-mining-verified`; both reported versions matched Beta 4.

- All **37 exported operating settings** matched before and after OTA. This unit's saved **575 MHz / 1155 mV**, pool configuration and existing profile/schedule configuration were retained. These are observations, not recommended presets or a full NVS/credential backup.
- The final device sample reported **1,181.323 GH/s**, **20 accepted shares** and **zero rejected shares** at 243 seconds uptime. This is a short mining observation, not a sustained soak test.
- The live Miner controls page showed the confirmation immediately beneath **Review operating point**. Its **575.25 MHz** draft was reviewed without applying it; the stored operating point remained 575 MHz / 1155 mV.
- The selected tuning-slot placement, single-review behavior and explicit Apply gating were exercised in the browser component tests. No named tuning-slot Apply or new hardware-control behavior was physically exercised for Beta 4.

The [Beta 3 record](validation-beta3.md) contains the preceding physical pause/resume, pool Apply and weekly scheduler checks. Those are separate evidence; this UI-only Beta 4 change does not claim to repeat that complete control bench sequence.

## Remaining qualification

Gamma **602 remains on stock firmware and unflashed**. Its source/identity/build support does not establish physical qualification. Physical OLED readability, sustained soak, deliberate thermal/fan/UART/storage fault injection and stock recovery remain incomplete.

Low, Stock and Boost presets remain a design discussion: no fixed preset values are implemented or verified. Autotuning, independent ASIC jobs, multi-coin split mining and richer Gamma coin/block injection are not implemented. Existing thermal protection remains active.
