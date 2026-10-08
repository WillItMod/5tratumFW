# QAxe Beta 2 validation — 8 October 2026

Paired version: **5tratumFW-qa-0.1.0-beta.2**. Release tag: **qaxe-v0.1.0-beta.2**. Target: NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4 / ESP32-S3. The existing test unit's physical PCB revision is unidentified.

## Correction

Updates now reads the reported model/version from the shared v1 `/api/system/info` endpoint, waits for identity before allowing application upload, separates lookup errors from filename errors and offers Retry. The exact application filename remains `esp-miner-NerdQAxe++.bin`. Legacy OTP identification falls back to an explicit v1 OTP flag only when the v2 identify endpoint returns 404 or 501. Network/authentication failures and missing/malformed OTP flags remain failures.

Only the QAxe frontend, paired version and documentation change relative to the preceding main branch. Hardware initialization, mining drivers, protections, partition layout, NVS keys, schedules, profiles, native A/B sessions, display artwork and coin-feed contracts are unchanged. Gamma keeps its separate Beta 4 release.

## Build, browser and package qualification

The legacy updater correction passed 55 Angular/Karma unit tests and 13 compiled-production Chromium fixture checks against a simulated QAxe++ running `v1.0.37.3-LTS` with v2 endpoints absent. They cover delayed/failed identity, Retry, exact filename rejection, explicit legacy OTP states and mobile layout. No OTA or other device writes were made by those checks. See the [correction record](validation-qa-legacy-update.md) for the preceding Linux CI evidence.

The Beta 2 package is produced by the pinned model-specific helper with Node 24.14.0 and ESP-IDF 5.5.3 at `espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805`. Release checks cover the target build, all browser unit tests, production helper/host regressions, 40 native LVGL fixture pages, app descriptor/version/size, actual compressed web version/provenance, exact SPIFFS reconstruction and corresponding-source inventory. Diagnostic targeting, passive capture and capture logs are disabled. The production MUX-to-firmware wire check is an explicit optional skip when its separate dependencies/source are absent.

The release's `manifest.json` records the exact compiled source commit, image/source hashes and toolchain. `web-budget.json` records the actual compressed payload. `SHA256SUMS` covers the downloaded images, corresponding source, manifest, web report and this validation asset. These OTA images contain no factory/NVS image. Both image versions must match.

## Physical qualification and MUX boundary

Beta 2 has no new physical OTA, mining, settings-retention, thermal or screen observation. Beta 1's installed-device observations remain in [their original record](validation-qa-beta1.md); they are not Beta 2 results. There is one QAxe++ compile profile, with no separate PCB 6.0/6.1 image. Neither revision has a separately recorded physical qualification, and other QAxe/OctAxe models remain outside this release's support claim.

MUX 0.9.73 admits Beta 1 only through its Miners → Dual MUX mining management switch. Beta 2 uses the miner's own Pool routing → Dual pool control until a MUX update qualifies this version. Native A/B registration, grouping, routing and current-job coin/block injection do not use that management-switch version allowlist. This compatibility statement is source review, not a new live Beta 2 coin-feed observation. A/B still share the chain job selector; independent physical-ASIC work ownership is not implemented or verified.

Install the application first and then the matching QAxe web image, following the [installation guide](installation-5tratumfw-qa.md). Keep current settings and a private unit-specific recovery backup. Do not rename another model's binary or erase NVS to resolve a loading/filename error.
