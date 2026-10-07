# Gamma Beta 3 validation

Version: `5tratumFW-0.1.0-beta.3`. Development BETA, Gamma PCB identities **601 and 602 only**. Checked on 7 October 2026.

The paired application and WWW image were installed on a Gamma **601**. Gamma **602 remains on stock firmware**; target support and host identity checks do not establish physical operation there.

## Build and source

Both installed images and their corresponding source archive were packaged from commit `dce0882d9e923a2ed54d4f7a8e616ef61463832c`. This validation record was added after the build and bench checks; it does not change the packaged application or WWW bytes. The package manifest's `hardware_tested:false` records the pre-bench packaging checkpoint. Use the results below for subsequent 601 evidence.

| Check | Result |
| --- | --- |
| Production Angular build and regenerated API client | Passed with pinned Node 24.14.0. |
| Frontend Karma/Jasmine | **117 passed** in Chrome Headless 154, including exact saved-pool Apply and empty-slot blocking. |
| Production Gamma host tests | All **eight entry points passed**: boot/settings, fresh hashrate, reversible power, weekly policy/API, MUX receiver, operating-profile transactions and pool switching. Hardware/RTOS/storage boundaries are simulated. |
| Native production OLED | **27 LVGL 9 views** and actual screen runtime passed under ASan/UBSan with fixtures. |
| ESP32-S3 application and SPIFFS build | Passed in pinned ESP-IDF 5.5.3 container. |
| Application image | **1,715,472 bytes**, within its 4 MiB OTA slot. SHA-256 `c73bf3e0180509509d4509f27cea6e20ab74a17b448c3b3084039b87984ccaac`. |
| WWW image | **3,145,728 bytes**. SHA-256 `a72bb26fb73bf83084bfc3676dde91691e34526609ec808aa4ca3dd9ddb3fcc3`. |
| Compressed web payload | **621,000 bytes**, 19.74% of WWW capacity; enforced 876,628-byte project budget passed. |
| Served web assets on 601 | Version, index and four entry bundles decoded and compared byte-for-byte with the packaged build. |

## Gamma 601 bench results

The application booted in `ota_0`. After the matching WWW upload and one final restart, both the app and reported web version were Beta 3 and the mismatch warning cleared. The web version is read at startup; a successful WWW upload alone does not refresh that cached field.

- All **37 exported operating settings** matched before and after OTA, including this unit's saved **550 MHz / 1150 mV**, fan, pool and network configuration. These are observations, not recommended presets or a full NVS/credential comparison.
- Both profile types returned ten slots. Pool slots **5tratMUX** and **Fallback** were retained; eight further slots remained empty and ready for other pools. The primary slot was recaptured from the active route with suggested difficulty 0 and Coinbase decoding off before the final Apply checks.
- Applying the saved primary pool succeeded without rebooting, resuming paused ASICs or changing the exported operating settings. The existing primary route was used for this check; a different pool/payout destination was not tested.
- Manual pause reached confirmed applied state. The miner reported **23 mV core voltage**, **4.99 W device power** and **4,503 RPM fan speed** while HTTP remained reachable. These are device telemetry readings, not independent electrical instrument measurements.
- Resume restored the saved operating point and fresh positive hashrate. No tuning preset was applied.
- An enabled current weekly window paused mining after Return to schedule cleared the preceding manual override. A saved pool time point selected slot 0 while ASICs stayed paused.
- Power schedule, pool schedule and slots survived a restart. Manual Resume overrode the active pause window; Return to schedule paused it again.
- The original disabled power and pool schedules were restored. Final readings showed **1.09 TH/s**, a fresh accepted share, zero reported ASIC errors, fan operation and the unchanged 550 MHz / 1150 mV configuration.
- The real miner's Pool routing page showed all ten named/empty slots and direct Apply controls. Browser verification was read-only; API checks above exercised the actual writes.

## Remaining qualification

Gamma 602 operation, physical OLED readability, sustained soak, deliberate thermal/fan/UART/storage fault injection and stock recovery remain unqualified. Compiled host failure scenarios cannot substitute for those physical checks.

The test MUX candidate advertises legacy status to the Gamma 601. Rich coin/block metadata is implemented separately in the QAxe candidate; this Gamma image does not implement that v2 protocol. Independent per-ASIC jobs, multi-coin split mining and automated tuning are not implemented in this image. Existing thermal protections remain active.
