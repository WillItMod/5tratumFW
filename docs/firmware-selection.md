# Firmware selection and repository structure

## Choose before downloading

| Model | Source/build | OTA application filename | Matching website | Current support |
| --- | --- | --- | --- | --- |
| Gamma601/602 | Root; `bash tools/build_5tratumfw.sh` | `esp-miner.bin` | Gamma `www.bin`, same version/package | Published Beta5 family; stored-identity guard allows601/602. Beta4 installed on601;602 unflashed. Read [Beta5 validation](validation-beta5.md) |
| NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4 | `firmware/nerdqaxe/`; `bash tools/build_5tratumfw_qa.sh` from that directory | `esp-miner-NerdQAxe++.bin` | QAxe `www.bin`, same version/package | Separate BETA, unidentified test PCB revision. Read that release's validation |
| NerdOCTAXE-γ / NERDOCTAXEGAMMA / BM1370×8 | `firmware/nerdqaxe/`; `bash tools/build_5tratumfw_oct.sh` from that directory | Candidate `esp-miner-NerdOCTAXE-Gamma.bin` | OctAxe `www.bin`, same version/package | **Source-only candidate, not physically qualified.** Exact PCB, regulator and flash geometry remain unverified. Read [OctAxe guide](nerdoctaxe.md) |
| Other variants/revisions | None qualified | None | None | No compatibility promise from inherited board definitions |

Check the board label/revision and reported model/ASICs. A common `www.bin` filename does not mean a common web image: download both files from **one model-specific release asset package**. Gamma tags use `v0.1.0-beta.N`; QAxe tags use `qaxe-v0.1.0-beta.N`, with on-device version `5tratumFW-qa-0.1.0-beta.N`. Versions are intentionally family-specific.

The OctAxe source candidate has its own version **`5tratumFW-oct-0.1.0-beta.1`** and tag contract **`octaxe-v0.1.0-beta.1`**. These identify the candidate; they do not assert that a release is published or ready for installation. Its only admitted build identity is **BOARD `NERDOCTAXEGAMMA`, reported model `NerdOCTAXE-γ`, BM1370×8**. That identity does not identify a PCB revision or verify regulator population, wiring or flash layout. No other OctAxe model or revision is qualified.

The release contains application, WWW, manifest, SHA256SUMS and corresponding source. Read the manifest board/target and image hashes before upload. Both are OTA partition images, not full factory images. Keep the existing settings and a private unit-specific recovery backup; no automatic presets are applied at boot.

## Repository ownership

- Root `main/`, `components/`, `test/`, `tools/build_5tratumfw.sh` and `version.txt`: Gamma baseline, hardware guard and paired OTA build.
- `firmware/nerdqaxe/`: complete separately maintained Nerd upstream derivative, hardware drivers, display/UI, tests and pinned submodule. QAxe uses `version.txt`, `tools/build_5tratumfw_qa.sh` and `tools/package_5tratumfw_qa.py`; the separate OctAxe candidate uses `version-oct.txt`, `tools/build_5tratumfw_oct.sh` and `tools/package_5tratumfw_oct.py`. Run these helpers from that directory.
- `docs/installation.md`, `compatibility.md`, `validation-beta*.md`: Gamma-only instructions/evidence.
- [QAxe guide](nerdqaxe.md) and `firmware/nerdqaxe/docs/validation-qa-beta*.md`: QAxe instructions and separate validation.
- [OctAxe guide](nerdoctaxe.md), [build guide](../firmware/nerdqaxe/docs/build-5tratumfw-oct.md), [installation limits](../firmware/nerdqaxe/docs/installation-5tratumfw-oct.md) and [Beta1 validation](../firmware/nerdqaxe/docs/validation-oct-beta1.md): OctAxe candidate instructions and evidence.
- Top-level `.github/workflows/gamma-beta.yml`, `qaxe-beta.yml` and `octaxe-beta.yml`: isolated family validation jobs. They build artifacts and never flash a device or automatically publish stable firmware.

Full QAxe source provenance is pinned in its `SOURCE_ORIGIN.json`. Upstream licenses/authorship remain inside both trees. Adding another model requires an explicit compatibility record, model-specific build/partition contract and separate hardware validation.

Gamma Beta5 and QAxe Beta3 retain their existing family versions and image pairs. The OctAxe candidate produces OTA partition images only, with no factory or NVS image. QAxe and OctAxe native A/B connections share one ASIC chain; separate sessions and multiple ASICs do not establish independent physical-chip job ownership or per-chip coin routing. See [next model qualification](next-models.md) for the OctAxe, GT800 and Gamma Duo650 sequence.
