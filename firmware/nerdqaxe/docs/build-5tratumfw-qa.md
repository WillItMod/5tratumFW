# Build the NerdQAxe++ BETA

This build path produces **paired application and WWW OTA files for NerdQAxe++**, selected as `NERDQAXEPLUS2`, with four BM1370 ASICs and an ESP32-S3 controller. The physical PCB revision of the tested development unit is unidentified. A build does not establish compatibility with every revision. It does not support Gamma, NerdQAxe+, NerdOctAxe or the other inherited board definitions.

The current QAxe source/build version is `5tratumFW-qa-0.1.0-beta.5`. Its clean source `3ffbe84f43e265e61d5bae3eca1cd985def2bb3f`, full family CI and offline checks passed. The exact pair passed the scoped supervised update; see [Beta5 validation](validation-qa-beta5.md). The published Beta4 pair retains its own validation; that record does not qualify this new source. Both `version.txt` and `main/http_server/axe-os/src/app/firmware-web-version.ts` must contain the same value. The package verifier checks the ESP application descriptor and the actual compressed web JavaScript; changing a filename does not change either firmware version.

## Requirements

- A clean reviewed QAxe source checkout, including its pinned secp256k1 submodule, or the corresponding source ZIP from the same release.
- Node.js **24.14.0** and its npm on `PATH`.
- Python 3, a C/C++ compiler available as `clang++` or `g++`, CMake and Python's Pillow package for native display PNG fixtures.
- Chrome/Chromium for Angular tests; set `CHROME_BIN` to its executable if needed.
- A running Docker daemon with access to this source directory.

The SDK is pinned to ESP-IDF **5.5.3**:

```text
espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805
```

Run all commands from the QAxe source directory, containing this guide's `tools/` and `main/` directories. In the combined firmware repository, choose the QAxe source directory first; the top-level Gamma build is a different target.

## Build and verify

```sh
node --version
python3 --version
docker info
bash tools/build_5tratumfw_qa.sh
```

The helper checks the QAxe version and reviewed source, installs the locked frontend dependencies, builds/compresses the UI, runs Angular and host regressions, compiles the application and WWW image, renders the production LVGL display with simulated readings, and verifies the paired package. It never discovers, connects to, flashes or resets a miner.

It uses a new empty `build/qa-public/` directory and puts the generated SDK configuration inside that directory. It does not erase or reuse a root `sdkconfig`, run `idf.py set-target`, change the partition layout or apply operating settings. If a build directory already contains files, choose another directory under `build/`:

```sh
QA_BUILD_DIR=build/qa-public-2 bash tools/build_5tratumfw_qa.sh
```

The model is fixed to `BOARD=NERDQAXEPLUS2`. A conflicting `BOARD` is rejected. The public build explicitly disables the diagnostic BM1370 driver, passive capture API and serial capture logs. These investigative features were enabled in earlier local bench candidates; those earlier observations do not qualify the public build's operating behavior.

The corresponding source ZIP includes the exact generated SDK configuration in `BUILD_CONFIG/sdkconfig`, dependency lock and pinned submodule sources. The helper can rebuild an extracted ZIP without Git by verifying its `SOURCE_FILES.json` inventory. A Git build records the clean source commit before compilation. Source files modified after compilation fail package verification.

## Outputs

The new package directory is `release/NerdQAxe++/<version>/`:

| File | Purpose |
| --- | --- |
| `esp-miner-NerdQAxe++.bin` | Application-only OTA image for the qualified QAxe++ profile. |
| `www.bin` | The matching complete WWW/SPIFFS image; use it only with this package's application. |
| `5tratumFW-NerdQAxe++-<version>-source.zip` | Corresponding reviewed source, GPL license, pinned submodule source and generated build configuration. |
| `manifest.json` | Model, version, source/toolchain provenance, image hashes, capabilities and qualification limits. |
| `SHA256SUMS` | SHA-256 checksums for the images, source ZIP, manifest and web report. |
| `web-budget.json` | Every actual web payload file, compressed/total size and successful WWW reconstruction check. |

The application must fit its **4 MiB OTA slot**. WWW must be a complete **3 MiB image**. The verifier recreates SPIFFS from the current payload and exact page/name/metadata configuration, then compares it with `www.bin`. This detects stale web images even when their version strings agree. Existing package directories are never overwritten.

These files contain no NVS partition and are not merged factory flash images. Use the existing application OTA uploader followed by the matching WWW uploader. Keep the source and both images from one package together. The inherited upstream factory/multi-board publishing workflow is not this release path.

Check package hashes before installation:

```sh
cd 'release/NerdQAxe++/5tratumFW-qa-0.1.0-beta.5'
shasum -a 256 -c SHA256SUMS
```

On Linux, `sha256sum -c SHA256SUMS` is equivalent. Follow the model-specific installation and recovery guide, preserve existing device settings and positively identify the board before writing. A compiled board label is not a physical PCB identification.

## Focused checks and validation limits

Run frontend checks independently:

```sh
cd main/http_server/axe-os
npm ci --no-audit --no-fund
npm run build
npm test -- --watch=false --browsers=ChromeHeadless --progress=false
```

From the QAxe source directory, representative host checks include:

```sh
python3 test/host/test_profiles.py
python3 test/host/test_mining_schedule.py
python3 test/host/test_mining_control.py
python3 test/host/test_native_pool_stream.py
python3 test/host/test_mux_status.py
python3 test/host/test_display_data.py
python3 test/host/test_boot_status.py
```

The full helper executes every `test/host/test_*.py` script. These tests use fake hardware/RTOS boundaries and do not contact a miner. The production MUX-to-firmware wire test is optional when MUX source is absent; its explicit skip is not an end-to-end pass. Maintainers with that reviewed source and its Python dependencies can run it explicitly:

```sh
MUX_SOURCE=/path/to/5tratMux_Build python3 test/host/test_mux_production_wire.py
```

After target compilation resolves the pinned LVGL component, render native screen fixtures:

```sh
python3 test/host/render_display_native.py
```

Those output images are simulated, not photographs or hardware telemetry. The exact Beta5 pair's separate short OTA/settings/mining record is in [Beta5 validation](validation-qa-beta5.md); it does not qualify physical display readability, sustained soak or all protection faults. Each future pair needs its own installation and preservation record. Do not copy an earlier candidate's hardware qualification into another build.

Native Dual pool provides two MUX connections using the shared ASIC chain. It does **not** establish independent mining on individual physical ASICs. The manifest keeps `independentWorkAssignment: false`. Saved clocks, voltage, cooling, pool/tuning slots and schedules must be retained through compatible OTA; no presets or automatic tuning run during this build or upgrade.
