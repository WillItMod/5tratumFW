# Build the NerdOCTAXE-Gamma candidate

This path selects only `BOARD=NERDOCTAXEGAMMA`: **NerdOCTAXE-γ / BM1370 ×8 / ESP32-S3**. It produces a matched application/WWW OTA candidate, not a factory image or physical compatibility qualification. Read [selection](../../../docs/firmware-selection.md) and [qualification](validation-oct-beta2.md) first.

## Source and separate identity

The combined repository keeps the Nerd derivative in `firmware/nerdqaxe/`. Its upstream baseline is [ESP-Miner-NerdQAxePlus v1.1.0-rc1-test1, commit 8b45522a6695c6bbccc3d032370fe08a29856d80](https://github.com/shufps/ESP-Miner-NerdQAxePlus/tree/8b45522a6695c6bbccc3d032370fe08a29856d80). The exact modified source commit and dependency inventory belong in each candidate's manifest and source archive. `SOURCE_ORIGIN.json` records the earlier QAxe import; its QAxe count/model fields are not OctAxe qualification evidence.

OctAxe uses `version-oct.txt` and `main/http_server/axe-os/src/app/firmware-web-version.oct.ts`, both `5tratumFW-oct-0.1.0-beta.3`. This is the next unbuilt pool-routing layout candidate; the earlier Beta2 installation record does not qualify it. Angular's Oct production configuration selects the Oct web version. The existing QAxe `version.txt` and default web-version file remain QAxe Beta5. Do not rename a QAxe binary or globally replace its version to make an Oct image.

## Requirements and build

Use a clean reviewed checkout with recursive submodules, Node **24.14.0**, Python 3, a native C/C++ compiler, CMake, Pillow, Chrome/Chromium and a running Docker daemon. Set `CHROME_BIN` if the browser is not found. ESP-IDF **5.5.3** is pinned to:

```text
espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805
```

From a reviewed combined checkout:

```sh
git submodule update --init --recursive
cd firmware/nerdqaxe
bash tools/build_5tratumfw_oct.sh
```

The helper rejects another `BOARD`, verifies paired Oct versions and clean source, builds/compresses the locked frontend, runs frontend and all host entry points, compiles the target application/WWW images, renders native display fixtures and verifies the package. It never accesses a miner. Diagnostic driver, passive capture and serial capture logs are OFF.

The default target directory is `build/oct-public/`, with generated SDK configuration inside it. Existing contents are never erased or reused. Choose a new empty directory when needed:

```sh
OCT_BUILD_DIR=build/oct-public-2 bash tools/build_5tratumfw_oct.sh
```

The corresponding-source ZIP includes `SOURCE_FILES.json`, pinned dependency sources and the exact configuration in `BUILD_CONFIG/sdkconfig`. The helper can rebuild a verified extracted archive without Git. Source changes after compilation fail packaging.

## Package contract

When the build and verification complete, the output directory is `release/NerdOCTAXE-Gamma/5tratumFW-oct-0.1.0-beta.3/`:

| File | Purpose |
| --- | --- |
| `esp-miner-NerdOCTAXE-Gamma.bin` | OctAxe application-only OTA image |
| `www.bin` | Same build's complete WWW/SPIFFS image |
| `5tratumFW-NerdOCTAXE-Gamma-5tratumFW-oct-0.1.0-beta.3-source.zip` | Corresponding source, GPL notices, dependencies and SDK configuration |
| `manifest.json` | Exact model/count/version/source, image hashes and qualification limits |
| `SHA256SUMS` | Image/source/manifest/web-report checksums |
| `web-budget.json` | Actual compressed payload and WWW reconstruction result |

The packager requires only the Oct board compiler flag, exact model/ASCII agent constants, eight BM1370s, an ESP32-S3 descriptor with the paired version and ESP-IDF 5.5.3, and diagnostic/capture flags OFF. It reconstructs the WWW image from the current compressed assets and verifies hash equality. Existing release output directories are immutable.

The source assumes **16 MiB flash**, a **3 MiB WWW** partition at `0x410000`, and **4 MiB OTA slots** at `0x710000` and `0xb10000`. Those values are package checks; they were independently verified on the owned revision 2.2 unit, without extending that observation to another assembly. The manifest records `physicalLayoutVerified: false`, `hardwareTested: false` and `independentWorkAssignment: false`. No bootloader, partition-table, NVS or merged factory image is distributed. Verify the actual unit before any migration.

Check the package without contacting a miner:

```sh
cd release/NerdOCTAXE-Gamma/5tratumFW-oct-0.1.0-beta.3
shasum -a 256 -c SHA256SUMS
```

On Linux, use `sha256sum -c SHA256SUMS`.

## Focused checks and CI

From `firmware/nerdqaxe/`, source-only checks include:

```sh
python3 test/host/test_octaxe_board.py
python3 test/host/test_mining_driver.py
python3 test/host/test_native_pool_stream.py
python3 test/host/test_capabilities_report.py
python3 test/host/test_status_report.py
python3 test/host/test_nerd_package_identity.py
```

The separate [OctAxe candidate workflow](../../../.github/workflows/octaxe-beta.yml) runs the pinned full Oct helper and retains candidate packages and simulated screen evidence. It has read-only repository permissions and no publication or flashing step. A successful workflow is build evidence only. The optional production-MUX wire test explicitly skips without reviewed MUX source; that skip is not a live MUX pass.

For standalone frontend work, use `npm run build:oct` in `main/http_server/axe-os/`; ordinary `npm run build` deliberately remains the QAxe build. Target compile, mocked GPIO/UART/regulators and rendered fixtures cannot establish electrical behavior, installed flash compatibility, sustained mining or physical-screen readability.
