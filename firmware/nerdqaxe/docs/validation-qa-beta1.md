# QAxe BETA1 validation

This record separates source/host/browser checks from installed-device observations. This BETA targets NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4; the test unit's physical PCB revision is unidentified.

## Source and simulated UI checks — 7 October 2026

The six main pages retain QAxe-specific options and separate pool/network/update ownership. Twenty production helper checks verify that section saves cannot carry hidden clocks, network, pool or fan changes; masked credentials are omitted, empty replacement Wi-Fi password remains explicit, and positional pool/fan index placeholders are retained.

All 47 ChromeHeadless browser unit tests pass, including section ownership, legacy route redirects, positional credential preservation, OTP handling and rejection of unconfigured pool schedule slots.

Twenty-eight actual Angular-bundle Chromium fixture checks verify six-link navigation, mixed 5TRAT/BTC identity, nearby operating-point review/application, both fan panels, relocated external data settings, ten named pool slots, narrow pool PATCH, successful-save dirty state, independent power/pool schedule endpoints, old settings-link aliases and no horizontal overflow at390px across all six pages. Fixtures are simulated; they do not write to a miner.

The full production-source host suite ran109 tests (108 passed, one optional dependency skip). Six production MUX-sender → firmware parser/status/LCD-format tests include Dual, cross-session isolation, job mismatch, expiry and reconnect. The native LVGL carousel/layout is covered by40 fixture pages; these are rendered host fixtures, not a new physical-screen photo.

## Coin-feed observation on preceding a10

Read-only checks on the running QAxe++ verified two healthy native sessions under one reported controller identity. A carried 5TRAT candidate23763/nBits1921275e; B carried BTC candidate970381/nBits17021ef0. Both work contexts matched the current forwarded job and the MUX target; bdiff matched the live nBits calculation and accepted shares were observed on each route. A mixed Dual aggregate coin/job is intentionally absent. The BETA UI fixes the misleading aggregate `Coin unknown` label by listing the fresh route coins.

## Preliminary private pair before the logo correction — 7 October 2026

This first pair was kept private and was superseded before publication to use the approved full 5tratumFW artwork. Its hashes below are historical qualification evidence, not download instructions; the final release manifest and matching checksums identify the installable pair.

The preliminary immutable pair was built from canonical repository commit `d41058c80a1ea353a33847f1a5eb46236010675f`, with source root `firmware/nerdqaxe/`, Node24.14.0 and ESP-IDF5.5.3 pinned at `espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805`. The isolated NERDQAXEPLUS2 target build, 47 browser unit tests, production helper/host regressions, 40 native LVGL fixture pages and package reconstruction checks passed. Diagnostic ASIC targeting, passive capture and serial hexadecimal logs are disabled. Automatic trial rollback is disabled.

| OTA image | Bytes | SHA256 |
| --- | ---: | --- |
| `esp-miner-NerdQAxe++.bin` | 3,287,648 | `a0ffe12852c17f64511b87eda9af9307768f78781e66ae726f41bfe0da962115` |
| QAxe `www.bin` | 3,145,728 | `7cfa029f5c157dafcf0ed18d9d71f2e48e9e07cad5e2aad75f684f5315f2369e` |

The compressed website payload is 1,534,390 bytes inside its 3MiB partition. This image is separate from Gamma's website image despite sharing the `www.bin` filename. Application and WWW were uploaded through the running miner's normal OTA endpoints without changing NVS or the partition table. Both uploads were acknowledged and the restarted application reports `5tratumFW-qa-0.1.0-beta.1`.

Read-only post-boot checks bound the system, settings, capability, profile and schedule endpoints to the same physical controller and exact firmware version. All 92 decoded served web assets, the root index and build provenance match the paired build; the local ESP32-S3 application descriptor and package hashes match the build receipt. Runtime version corroborates the uploaded application; these APIs do not provide application flash-byte readback.

The saved 500MHz / 1130mV operating point, version rolling, both connections, fan/network/display settings, ten pool slots, ten tuning slots and both weekly schedules were retained. Masked password bytes cannot be compared through GET APIs; configured flags were retained and actual accepted shares corroborate functioning pool credentials. The new build operated near 4TH/s, with board temperature near55°C and no shutdown during this short check. Both sessions produced fresh accepted shares with no rejected shares observed; one bounded observation recorded increases of4/2 on A/B.

The frozen MUX0.9.73 control implementation at `513a976d3fb7017422170af53181e699927bcab0` was exercised read-only against the actual new BETA. It admitted the exact device/version/configuration, reported saved and runtime Dual enabled, no restart pending and one active connection per stream. No control Apply or restart was issued during that check. Actual routing/coin-feed observations use the existing `0.9.72-fw-preview5` test MUX; MAIN0.9.73 package acceptance is a separate MUX release record.

## BETA coin-feed qualification

The preliminary served BETA interface and actual native sessions passed fresh route/job and nBits-derived difficulty checks. After natural expiry of an active fleet attack, A carried5TRAT candidate23778/nBits191b33f7 and B carriedBTC candidate970386/nBits17021ef0. Both contexts matched the bracketed live MUX target, current forwarded job and compact-target difficulty. The mixed Dual aggregate coin/job was null. Accepted counters progressed monotonically by21/5 over a612second combined observation with no reboot or rejected shares; one90second mixed window was quiet on B, while a separate fresh window proved4/2 acceptance. No route lease, allocation change or miner control write was needed.

The temporary80/20 B portfolio belonged to an active shared-fleet attack; natural event expiry restored its manual100%BTC policy. Same-coin intervals may expose the common aggregate coin. The active plan and fleet events determine the effective target, so a prior B coin snapshot is not a permanent target assertion.

## Final branded pair

The complete approved 5tratumFW logo replaces the generic OS emblem and separate lettering on startup, the recurring large-logo slide and the website header. Telemetry headers use the approved wordmark. The original artwork SHA256 is `90b7f3f9fee2d758f974d07cbfc6ba386614f387fc4c0855fb12957479f15158`. The encoded LCD artwork consumes 123,120 bytes of flash with no runtime decoder.

The final paired package was built from clean canonical commit `62f10df4ead69e0032c56c8a78995ab349fb36b2`, using the same pinned ESP-IDF 5.5.3 and Node 24.14.0 toolchain. Its target build, 47 browser unit tests, 20 production helper checks, host regressions, 40 native LVGL pages and strict image/source reconstruction checks passed again. All diagnostic/capture flags remain disabled.

| Final OTA image | Bytes | SHA256 |
| --- | ---: | --- |
| `esp-miner-NerdQAxe++.bin` | 3,398,400 | `5a96f174666e6fbd6d2b70521e0736c8fb083743d80988e71b7ee71e58f2962b` |
| QAxe `www.bin` | 3,145,728 | `e65e09703a2be1358cef43d6cfc64af446b24e95dc12501f3cf9c4f705d4f866` |

The website payload is 1,678,840 bytes within its 3 MiB partition; the application fits its 4 MiB slot with 19% free. The corresponding source archive SHA256 is `49f83b44fa55ff80ea97ae3f651cf2ddf35cecc04ed298a4f75a9b524aa92a2e`.

Both final images were accepted by normal paired OTA, followed by a normal reboot. Read-only checks verified all 94 decoded served assets, the root index and exact build provenance against this final pair. System/capability/profile/schedule endpoints report the same controller and BETA version. The saved 500 MHz / 1130 mV point, all settings, ten pool slots, ten tuning slots and both schedules were retained. After sessions settled, both native streams produced fresh accepted shares (1/1 in a short observation) and fresh mixed 5TRAT/BTC coin/job/nBits information matched the test MUX. The device operated near 4 TH/s with board temperature near 53°C. Application flash bytes are not exposed by these APIs; runtime version, upload acknowledgement and local descriptor/package checks are separate evidence.

An independent Chromium run added 36 checks of the final actual Angular bundle: the complete logo displays at 104×64 inside its 72px header, without horizontal overflow at 1440, 390 or 320px. Navigation, theme and keyboard focus remain functional. Eight artwork/provenance checks verified original bytes, the exact resized image, decoded compressed assets and source/package identity. These browser checks use local fixtures and do not claim a physical LCD photograph.

## Signed MUX 0.9.73 live acceptance

The exact protected AMD64 MAIN candidate built from MUX source `513a976d3fb7017422170af53181e699927bcab0` was installed on the test server after independent checks of its trusted Ed25519 signature, both architecture image graphs/native binaries, asset checksums, source provenance and successful CI. Its archive SHA256 is `7e6b567ac77b95630190c12eb63c6a0b1d1fc0671bb5817e07bb5628a06eb15f`; its installed image/config digest is `sha256:9d5d995fbed171fbad4acc7e70bc57af683bae77c06a9069cb1203f216757806`. Existing environment, mounts, security configuration, licensing and saved state were preserved, with a consistent backup and the preceding container retained for rollback. An initial deployment stopped at its backup-helper step and restored the old container; the corrected helper completed before the new candidate started.

After startup/session caches settled, read-only checks against the actual MAIN app and final branded firmware passed. Both native A/B identities had one active connection, used `warm-template-mux`, and retained their 100% 5TRAT / 100% BTC allocations. A reported fresh candidate height 23780 / nBits `192aa6c6`; B reported 970392 / `17021ef0`. Both matched the effective target, current forwarded job and compact-target difficulty; the mixed aggregate coin/job remained unselected. Accepted shares increased by 3/1 on A/B. All 94 served assets, saved settings, 20 slots and both schedules were checked again and retained.

The actual native-control GET endpoint admitted the exact BETA for both stream selections, with saved/runtime Dual enabled, connected counts `[1,1]` and no restart pending. No mode Apply, forced route change or miner restart was issued during these acceptance checks. The combined sanitized receipt SHA256 is `31b62ae24cb375111c6eec57528779f3f4edfd21950bb727c7cd2b82efdc2cd1`.

Twelve Chromium checks of the actual live MAIN interface verified one collapsible NerdQAxe parent, `NerdQAxe(A)` / `NerdQAxe(B)` labels, SEAMLESS delivery, the available checked Dual control under Miners, and 5TRAT/BTC tickers. No configuration action or write was issued. Shared board telemetry is cached separately and expires after 15 seconds; one snapshot briefly showed unavailable readings, while three subsequent GET samples had fresh real readings. This can occur at a probe/refresh boundary and is separate from the online mining connections; continuous remote telemetry availability was not established by this short check.

Only NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4 has this package. Other QAxe/OctAxe revisions are unqualified. Gamma uses its separate validation records and image pair. Power pause/resume transitions were checked on the earlier a7 bench and were not repeated during this BETA's paired-update check. Native display fixtures verify layout/formatting, not a new physical-screen photograph. The installed image includes scrolling startup status and the periodic large-logo slide; the preserved Automatic screen off setting controls whether the panel remains awake long enough to show it. No physical per-ASIC work ownership or prolonged soak is claimed.
