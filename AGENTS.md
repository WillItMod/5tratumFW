# 5tratumFW contributor guide

This repository has separate BETA firmware families: the root builds Bitaxe Gamma PCB601/602; `firmware/nerdqaxe/` builds NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4 only. Never interchange their app/WWW pairs. The test QAxe PCB revision is unidentified, and other QAxe/OctAxe revisions remain unqualified. Read `readme.md` and `docs/compatibility.md` before changing board support. The shared upstream drivers are not a compatibility promise for other hardware.

- Keep the exact persisted-identity guard before hardware initialization.
- Preserve NVS keys and device-specific configured clocks, voltage, fan, network and pool settings; do not erase NVS or apply tuning/presets during boot or OTA.
- Retain GPLv3, upstream authorship and third-party notices, including the adapted OLED font.
- Pool routing owns MUX setup. Scheduler remains a separate navigation item. Coinbase decoding defaults off; saved explicit choices persist.
- MUX status is informational and requires a valid current-peer advertisement; it expires at 90 seconds and resets on disconnect/protocol change. It is not payout or per-chip-work verification.
- Never commit credentials, private-device records, backups, logs, generated binaries or caches. Do not add device flashing/deployment to builds or CI.

## Build and checks

Use Node 24.14.0, Python 3, a native C compiler, CMake, Pillow, Chrome/Chromium and Docker. Initialize submodules, then run `bash tools/build_5tratumfw.sh`. See `docs/build.md` for the pinned ESP-IDF 5.5.3 digest and environment setup.

When changing `main/http_server/openapi.yaml`, regenerate the TypeScript API client (`npm run generate:api`) and keep development mocks in `system.service.ts` consistent. Generated clients are ignored; frontend build and test commands regenerate them.

Host tests use production C with fake platform services. Native OLED renders use actual LVGL with simulated readings. Target compilation, host tests and fixture rendering do not prove physical-device operation. Record exact validation in `docs/validation.md` and release notes; never convert the earlier 601 alpha smoke test into a claim about a new BETA build or PCB 602.

Published builds use GitHub prereleases with family-specific versions and matched OTA pairs. Gamma uses `5tratumFW-X.Y.Z-beta.N` and application `esp-miner.bin`; QAxe uses `5tratumFW-qa-X.Y.Z-beta.N` and application `esp-miner-NerdQAxe++.bin`. Package each family's application, its matching `www.bin`, checksums, manifest and corresponding source together. The shared WWW filename does not make the web images interchangeable. Do not publish factory/NVS images from this repository.

## QAxe family

Read `docs/firmware-selection.md` and `docs/nerdqaxe.md`. The QAxe subtree retains GPLv3/upstream provenance and its own pinned build helper; inherited upstream board definitions/workflows are not supported-release promises. Top-level QAxe CI runs only NERDQAXEPLUS2 and never generates factory images or flashes hardware. Preserve both native sessions, shared board telemetry, both fans, security/protocol options, slots and schedules. Native A/B sessions share a chain; independent physical ASIC work ownership remains unverified.
