# Build and validate 5tratumFW BETA

The matched Gamma Beta7, QAxe Beta5 and OctAxe Beta3 pairs built from clean commit `3ffbe84f43e265e61d5bae3eca1cd985def2bb3f` and passed their separate family CI and offline package checks. Build evidence is separate from the completed short installed-pair checks for Gamma 601/602, QAxe Beta5 and the owned PCB 2.2 OctAxe Beta3. See [Gamma Beta7 validation](validation-beta7.md), [QAxe Beta5 validation](../firmware/nerdqaxe/docs/validation-qa-beta5.md) and [OctAxe Beta3 validation](../firmware/nerdqaxe/docs/validation-oct-beta3.md).

The current source version is **`5tratumFW-0.1.0-beta.8`** for the Gamma schedule API response-type fix. Successful pool and power schedule JSON responses now declare `application/json`, including their save/readback responses. Beta8 retains the Beta7 layout and existing scheduler/storage behavior. Its build and installed-device evidence must be recorded separately in [Beta8 validation](validation-beta8.md); the completed Beta7 checks are historical evidence for that earlier pair. The application and web interface use the same value from `version.txt`. Keep release versions unique and within the ESP application descriptor's 31-byte limit. Published GitHub releases for this development build must be marked **prerelease/BETA**.

These instructions produce paired OTA images for **Bitaxe Gamma PCB revisions 601 and 602 only**. A successful compile does not establish compatibility with another board or prove operation on a physical miner. See [compatibility](compatibility.md) and [installation](installation.md) for the hardware and upgrade requirements.

## Prerequisites

- Git, including recursive submodule support.
- Node.js **24.14.0** and the npm shipped with it; the build checks the exact Node version.
- Python 3 and a native C compiler available as `cc`, or selected through `CC`.
- CMake available on `PATH` and Python's Pillow package for native OLED rendering. Install Pillow in the Python environment used for the build, for example with `python3 -m pip install Pillow`.
- Docker with the daemon running and access to the repository directory.
- Chrome or Chromium for Karma frontend tests. Set `CHROME_BIN` to the browser executable when Karma cannot find it automatically.

The pinned ESP-IDF image is:

```text
espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805
```

That digest identifies a multiarchitecture OCI index for Linux AMD64 and ARM64. Docker selects the matching image. The target firmware remains ESP32-S3 on both build hosts. The image provides ESP-IDF **5.5.3**; no local ESP-IDF installation is needed for this build path.

## Full build from a clean checkout

```sh
git clone --recurse-submodules https://github.com/WillItMod/5tratumFW.git
cd 5tratumFW
node --version
docker info
bash tools/build_5tratumfw.sh
```

For an existing checkout, initialize the pinned dependency first:

```sh
git submodule update --init --recursive
```

Run the build from a clean source checkout rather than reusing an older `build/` or `dist/` tree. The build uses the npm lockfile, regenerates the OpenAPI client, builds and compresses the web interface, runs frontend and Gamma host tests, checks the web size budget, compiles the target application and WWW partition, renders the native OLED layout, and verifies the paired package. It does not connect to a miner.

## Outputs

When the build and verification complete, the package is written under `artifacts/5tratumFW-0.1.0-beta.8/`:

| File | Purpose |
| --- | --- |
| `esp-miner.bin` | Application-only OTA image; use the firmware/app BIN uploader. |
| `www.bin` | Complete WWW/SPIFFS image; use the web/WWW BIN uploader. |
| `5tratumFW-0.1.0-beta.8-source.zip` | Corresponding source, license notices, pinned submodule source and generated build configuration. |
| `manifest.json` | Product version, upstream baseline, supported board identities, toolchain, image sizes and SHA-256 hashes. |
| `SHA256SUMS` | Checksums for the images, source archive and web budget report. |
| `web-budget.json` | Measurement of the compressed files actually placed in the WWW partition. |

These are OTA files, not factory or merged flash images. They do not contain an NVS partition or device settings. See [installation](installation.md) for update order, preservation limits and recovery guidance.

The package verifier checks application and web versions, the ESP-IDF application descriptor, ESP32-S3 configuration, the web budget, application image size and WWW partition size. It regenerates the WWW image from the current compressed bundle and compares its hash with `www.bin`, detecting a stale WWW image even when the version string is unchanged.

The current partition table reserves **3 MiB** for WWW. The compressed payload must be below **1 MiB**, below 75% of that partition, and within **128 KiB** of the measured upstream web payload. The report excludes SPIFFS metadata; ESP-IDF additionally checks that the generated filesystem fits its partition.

## Focused checks

Build and test the frontend independently:

```sh
cd main/http_server/axe-os
npm ci --no-audit --no-fund
npm run build
npm run test:ci
```

Run the host regression suite from the repository root:

```sh
python3 tools/test_gamma_host.py
```

The runner executes the Gamma boot tests first, then every sorted `test/host/test_*.py` script as a separate Python process. It deliberately runs script entry points directly: the MUX receiver regression script does not define unittest classes and would be skipped by unittest discovery.

The host tests compile production C code with fake drivers, storage, time or HTTP where appropriate. The scheduler runtime tests obtain the exact cJSON source from the pinned ESP-IDF image. These checks exercise preservation, identity validation, weekly schedule behavior, power-control state transitions and MUX receiver handling without miner I/O. They cannot prove electrical behavior, ASIC performance, OTA installation or thermal safety on physical hardware.

The separate native OLED renderer uses the managed LVGL source downloaded by the ESP-IDF target build. Run it **after a successful target build**:

```sh
python3 test/host/render_oled_native.py
```

It compiles the production layout against LVGL 9.3 with ASan/UBSan and uses simulated readings to produce **128 × 32** pixel views. The report and PNG fixtures are written to `.cache/oled-native/output/`, including `oled-native-report.json` and `oled-simulated-contact-sheet.png`. Inspect those fixtures at the physical display dimensions and record the result in the release validation notes. The fixtures contain no physical hardware measurements. `test/host/generate_oled_assets.py` regenerates source assets; it is not a layout test and is not run automatically by the host regression runner.

GitHub's **Gamma BETA validation** workflow repeats frontend tests, host tests, the web budget, target compilation, native OLED rendering and paired package verification on Linux. It saves the verified package and OLED fixtures as separate workflow artifacts. It generates no factory/NVS images and has no stable release publishing step. Any GitHub prerelease should attach the verified paired images, source archive, manifest and checksums from one build, and state which tests actually ran.

## Validation and BETA limits

Record the exact version, commit, test results and build host for each release. Keep compiled support separate from installed-device testing. The earlier `5tratumFW-0.1.0-a1` app/WWW pair was smoke tested on a Gamma 601; that is historical evidence for that earlier build. [Beta7 validation](validation-beta7.md) records this exact pair's separate supervised updates on 601 and 602. Those short observations do not qualify physical OLED readability, a new schedule/power boundary, sustained soak or all protection faults. The package manifest's `hardware_tested: false` is the immutable pre-bench packaging result; the dated validation records add later scoped installation evidence.

The matching MUX status server changes also need deployment and live end-to-end validation. This firmware's receiver can be compiled and host tested without claiming that a connected peer advertises the protocol. No hub deployment or device flashing occurs in CI or these build commands.

## Common build problems

An unexpected Node version fails early. Select Node 24.14.0 before installing npm dependencies. A missing browser causes frontend tests to fail; install Chrome/Chromium or set `CHROME_BIN`. A stopped Docker daemon or a repository outside Docker's shared directories prevents container compilation. Missing submodule source is fixed with `git submodule update --init --recursive`.

Do not package device backups, credentials, logs, local miner inventories, caches or unrelated untracked files. Generate the package from the reviewed repository source. Preserve [third-party notices](third-party-notices.md) and the root GPL license with the corresponding source.
