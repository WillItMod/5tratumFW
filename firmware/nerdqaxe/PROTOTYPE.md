# 5tratumFW Nerd development

This branch starts from upstream `v1.1.0-rc1-test1`, commit `8b45522a6695c6bbccc3d032370fe08a29856d80`. The observed test miner is a **NerdQAxe++**, compiled as `NERDQAXEPLUS2`, with four BM1370 ASICs. Its physical PCB revision is unknown. Other Nerd board variants are present in upstream source but are not hardware-qualified by these tests.

This document records the earlier a-series benches. For the public QAxe BETA1, use its [validation record](docs/validation-qa-beta1.md), [installation guide](docs/installation-5tratumfw-qa.md) and [reproducible build guide](docs/build-5tratumfw-qa.md). Historical image names, MUX versions and build commands below are not the current release package.

## Historical a10 test deployment

On 7 October 2026, paired `5tratumFW-qa-mux-a10` application and WWW images were installed through the existing OTA endpoints. The app runs in `ota_1`; the checked a9 application remains in the other OTA slot. The saved operating point remains **500 MHz, 1130 mV and version rolling 25011**. Named slots and schedules were retained; the intended pool configuration change enables native Dual mode with two connections to the test MUX. All five served web entry assets match the paired build. Both streams received fresh accepted shares on separate 5TRAT and BTC targets at approximately 4 TH/s aggregate. This is a local test candidate, not a release certified for unidentified PCB revisions. See the [a10 validation record](docs/validation-a10.md). The prior [a9 bench](docs/validation-a9.md) qualified passive capture and independently matched two nonces to transmitted headers.

The firmware includes a branded web interface, native LVGL display, bounded boot-status scroll and periodic large-logo display. Thermal faults, shutdown, enrollment and identification screens retain priority. Pool settings include ten named slots; tuning includes ten named slots. Saving a slot does not apply it. The pool scheduler has up to sixteen weekly switch points and an explicit fixed UTC offset. See [profile and scheduler API guide](docs/miner-profiles-and-pool-schedule.md).

## Historical a7 shared power control

The a-series candidates retained the separate weekly mining-power scheduler and reversible shared ASIC power control. Manual pause/resume, weekly override/follow, persistence across a paused restart and restoration of the original configuration were physically checked on a7; those transitions were not repeated during the a9 capture or a10 dual-stream benches. See [power controls and validation limits](docs/mining-power.md). Independent ASIC work and per-ASIC pool application remain unavailable. The hub control adapter at that bench was scoped to the exact physically checked a7 QAxe++ family; this does not qualify later builds.

## Physical display telemetry

The native screen uses the established 5tratum emblem, fonts and 320×170 display. Actual per-chip hardware-counter samples provide ASIC rates. A baseline alone is not a rate, measured zero is valid, and samples expire after fifteen seconds. The device total and J/TH require every contributing counter to be fresh. Power can remain visible independently.

QAxe temperature sensors are shared board/regulator measurements; its four zero-valued chip temperature fields are unmeasured sentinels. The a5 display correction shows **ASIC board, VR external and VR internal** once and removes an empty chip-temperature column. It copies a coherent cached snapshot timestamped by the actual power loop, without adding hardware polling. All forty native LVGL views pass the sanitizer, layout and thermal-overlay checks. These fixtures are simulated software checks, not photographs of a physical screen.

The observed test unit reports distinct regulator sensors. Future revision-7 TPS546 qualification must supply explicit sensor provenance: upstream getters can return the same internal reading for both regulator fields. Such hardware must not be labelled as having an external sensor just because both getters return a value. Other Nerd variants remain unqualified.

## MUX negotiation and mining jobs

`GET /api/5tratum/capabilities` and `/api/5tratum/status` provide a stable opaque controller identity, hardware description, operating point, actual counter observations and connection-bound MUX metadata. Identity is peer reported rather than cryptographic firmware attestation. No raw MAC, pool password or Wi-Fi secret belongs in published fixtures.

The firmware subscribes to the versioned MUX extension. Acknowledgement follows real authorization, clears on disconnect and expires after ninety seconds. The v2 job context follows a real forwarded job: job ID, optional coinbase height, actual compact `nBits`, Bitcoin-reference difficulty (`bdiff`) and age. A heartbeat cannot renew an old job. Dual routes retain separate contexts rather than inventing one aggregate height. Unknown coin or height remains unknown. The historical candidate MUX on the test OS host was `0.9.72-fw-preview1`, channel TEST; see the BETA1 validation record for its actual MUX version.

The a6 candidate corrects a coin-ID grammar defect observed on the live 5TRAT route: canonical `5trat` begins with a digit. An actual MUX-sender/firmware-receiver reproduction accepted BTC and BCH but rejected 5TRAT before the change. All nine configured catalog coins now pass both production job rewrite paths. IDs still have bounded lowercase ASCII letters/digits and interior hyphens; job ID/nBits binding, freshness and malformed-frame rejection remain unchanged. After deployment, the live miner and native OS both report 5TRAT and its forwarded job height. Gamma currently accepts the older five-field MUX acknowledgement only; this does not claim Gamma v2 coin/job metadata support.

## Independent ASIC work remains unavailable

Four ASICs are four chips, not four independently controlled internal silicon cores. The present driver broadcasts shared chain jobs. Frequency, voltage, fan control and power are shared; **independent job assignment remains false**, and per-ASIC profile application is rejected. MUX identity routing remains disabled with an empty production trust registry. A capability report cannot enable physical isolation.

The portable job selector and registry have simulated four-job tests, separate immutable routes, generation checks, quarantined partially written IDs and explicit pipeline-drain requirements. They are not connected to live job dispatch. See [hardware investigation and required proof](docs/asic-work-targeting-investigation.md). A working split release still needs isolated concurrent four-chip dispatch, compatible difficulty/version masks and verified UART pipeline retirement.

The [BM1370 driver rewrite](docs/bm1370-driver-rewrite.md) separates the actual
chain-job codec and bounded response parser into a testable driver path. It is
disabled by default; the local a9 bench candidate explicitly enables it. Chip-targeted work remains
unsupported and sends no UART bytes; no job selector has been invented. The
unfinished percentage-allocation fallback is parked outside the active tree.
The active objective remains simultaneous independent mining on the four ASICs.

The [bounded passive capture candidate](docs/bm1370-passive-capture.md) records
actual job packets, canonical headers and raw UART read chunks. The a9 QAxe
bench captured and independently verified real nonce/header pairs. It uses at
most 96 KiB of PSRAM and defaults off. Two live
pool sessions are reusable infrastructure; physical per-ASIC job acceptance
still requires evidence.

## Recovery and upgrades

A complete 16 MiB ROM recovery backup was read at 115200 baud and independently verified against the physical flash MD5 before firmware writes. The partition table, OTA record CRCs, controller identity and ESP32-S3 revision were checked. Private flash/NVS backups contain credentials and remain in ignored `local-devices/` with restrictive permissions; they must never be distributed.

The historical benches used the miner's application OTA endpoint followed by the paired WWW endpoint. Follow the [BETA installation guide](docs/installation-5tratumfw-qa.md) for the current release's upload order. Application OTA chooses the inactive 4 MiB slot. Do not use generated factory/app-flash arguments for this preservation workflow, erase NVS or change partition layouts. Automatic trial rollback is disabled, so the preserved other-slot app and complete recovery backup are recovery tools rather than a promise of unattended rollback.

Earlier USB cable/reset attempts left the screen blank until both power sources were removed. A known data cable and manually confirmed BOOT entry ultimately established native USB-Serial/JTAG. Read-only backup used `--before no-reset` with SYNC and `--after no-reset-stub`; `no-reset-no-sync` is incompatible with the established esptool 5.4.0 stub workflow. Follow the actual controller's documented BOOT procedure, positively identify its serial port, and distinguish main power from USB power.

## Historical reproduction and validation

The tested SDK is the pinned ESP-IDF 5.5.3 image below, targeting ESP32-S3. The SDK resolves LVGL 8.3.11. Use the application's exact version with the matching `FIRMWARE_WEB_VERSION` stamp. Build the compressed web assets first, then use `BUILD_WEB=OFF` to package that prebuilt directory without an uncontrolled dependency install.

```sh
python3 test/host/test_profiles.py
python3 test/host/test_status_report.py
python3 test/host/test_mux_status.py
python3 test/host/test_display_data.py
python3 test/host/test_boot_status.py
python3 test/host/render_display_native.py
cd main/http_server/axe-os
npm ci
npm run build
cd ../../..
docker run --rm -e BOARD=NERDQAXEPLUS2 \
  --mount "type=bind,source=$PWD,target=/project" -w /project \
  espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805 \
  idf.py -B build-display -DBUILD_WEB=OFF \
  -DPROJECT_VER=5tratumFW-qa-web-a7 -DFIVETRATUM_ASIC_CAPTURE_LOGS=OFF build
```

Host tests exercise production serializers, profile persistence and handlers, scheduler selection, strict MUX parsers, sample freshness and native LVGL layout under sanitizers. They substitute hardware/RTOS boundaries and do not contact miners. Firmware compile, served asset checks, settings preservation and real accepted shares are recorded separately in private deployment journals.

Capture logging defaults off. Enabling `FIVETRATUM_ASIC_CAPTURE_LOGS` compiles existing serial packet logging; it adds no addressing mechanism and may affect timing. Earlier a1/a2 capture/display manifests and previews are historical build records, not the public BETA's validation. Generated build artifacts and logs remain ignored.
