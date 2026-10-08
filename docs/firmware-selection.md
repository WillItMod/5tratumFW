# Firmware selection and repository structure

## Choose before downloading

| Model | Source/build | OTA application filename | Matching website | Current support |
| --- | --- | --- | --- | --- |
| Gamma 601/602 | Root; `bash tools/build_5tratumfw.sh` | `esp-miner.bin` | Gamma `www.bin`, same version/package | Qualified Beta7 BETA pair. Separate short checks on both models. Read [Beta7 validation](validation-beta7.md) |
| NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4 | `firmware/nerdqaxe/`; `bash tools/build_5tratumfw_qa.sh` from that directory | `esp-miner-NerdQAxe++.bin` | QAxe `www.bin`, same version/package | Qualified Beta5 BETA pair. Test PCB revision unidentified. Read [Beta5 validation](../firmware/nerdqaxe/docs/validation-qa-beta5.md) |
| NerdOCTAXE-γ / NERDOCTAXEGAMMA / BM1370×8 | `firmware/nerdqaxe/`; `bash tools/build_5tratumfw_oct.sh` from that directory | `esp-miner-NerdOCTAXE-Gamma.bin` | OctAxe `www.bin`, same version/package | Qualified Beta3 BETA pair; limited to one owned PCB 2.2 unit. Actual flash geometry was verified; regulator package marking remains unobserved. No other revision is qualified. Read [OctAxe guide](nerdoctaxe.md) and [Beta3 validation](../firmware/nerdqaxe/docs/validation-oct-beta3.md) |
| Other variants/revisions | None qualified | None | None | No compatibility promise from inherited board definitions |

Check the board label/revision and reported model/ASICs. A common `www.bin` filename does not mean a common web image: download both files from **one model-specific release asset package**. Gamma tags use `v0.1.0-beta.N`; QAxe tags use `qaxe-v0.1.0-beta.N`, with on-device version `5tratumFW-qa-0.1.0-beta.N`. Versions are intentionally family-specific.

The current qualified BETA pairs are Gamma **`5tratumFW-0.1.0-beta.7`**, QAxe **`5tratumFW-qa-0.1.0-beta.5`** and OctAxe **`5tratumFW-oct-0.1.0-beta.3`**, with family tags `v0.1.0-beta.7`, `qaxe-v0.1.0-beta.5` and `octaxe-v0.1.0-beta.3`. Each has passed its full family CI and offline checks at source `3ffbe84f43e265e61d5bae3eca1cd985def2bb3f`. All three have passed scoped installed-pair checks on 8 October 2026, including separate Gamma 601/602 checks. The table records current evidence for each family. Read the new [Gamma](validation-beta7.md), [QAxe](../firmware/nerdqaxe/docs/validation-qa-beta5.md) and [OctAxe](../firmware/nerdqaxe/docs/validation-oct-beta3.md) validation records before selecting a download.

The first qualified OctAxe BETA pair is Beta3. Its new installed-pair evidence and prior bench are limited to the same owned PCB 2.2 unit. Publication does not qualify every OctAxe revision; no new pair is physically qualified by the build alone.

The OctAxe BETA's only admitted build identity remains **BOARD `NERDOCTAXEGAMMA`, reported model `NerdOCTAXE-γ`, BM1370×8**. That identity does not identify a PCB revision or verify regulator population, wiring or flash layout. The dated [Beta3 validation](../firmware/nerdqaxe/docs/validation-oct-beta3.md) and earlier Beta1/Beta2 bench records apply only to the owned revision 2.2 assembly; no other OctAxe model or assembly is qualified.

The release contains application, WWW, manifest, SHA256SUMS and corresponding source. Read the manifest board/target and image hashes before upload. Both are OTA partition images, not full factory images. Keep the existing settings and a private unit-specific recovery backup; no automatic presets are applied at boot.

## Repository ownership

- Root `main/`, `components/`, `test/`, `tools/build_5tratumfw.sh` and `version.txt`: Gamma baseline, hardware guard and paired OTA build.
- `firmware/nerdqaxe/`: complete separately maintained Nerd upstream derivative, hardware drivers, display/UI, tests and pinned submodule. QAxe uses `version.txt`, `tools/build_5tratumfw_qa.sh` and `tools/package_5tratumfw_qa.py`; the separate OctAxe family uses `version-oct.txt`, `tools/build_5tratumfw_oct.sh` and `tools/package_5tratumfw_oct.py`. Run these helpers from that directory.
- `docs/installation.md`, `compatibility.md`, `validation-beta*.md`: Gamma-only instructions/evidence.
- [QAxe guide](nerdqaxe.md) and `firmware/nerdqaxe/docs/validation-qa-beta*.md`: QAxe instructions and separate validation.
- [OctAxe guide](nerdoctaxe.md), [build guide](../firmware/nerdqaxe/docs/build-5tratumfw-oct.md), [installation limits](../firmware/nerdqaxe/docs/installation-5tratumfw-oct.md) and [Beta3 validation](../firmware/nerdqaxe/docs/validation-oct-beta3.md): scoped OctAxe instructions and evidence; earlier Beta1/Beta2 records remain historical.
- Top-level `.github/workflows/gamma-beta.yml`, `qaxe-beta.yml` and `octaxe-beta.yml`: isolated family validation jobs. They build artifacts and never flash a device or automatically publish stable firmware.

Full QAxe source provenance is pinned in its `SOURCE_ORIGIN.json`. Upstream licenses/authorship remain inside both trees. Adding another model requires an explicit compatibility record, model-specific build/partition contract and separate hardware validation.

Gamma Beta7, QAxe Beta5 and OctAxe Beta3 are separate qualified BETA pairs. Earlier Gamma Beta6 and QAxe Beta4 releases retain their own historical records. All three families produce OTA partition images only, with no factory or NVS image. QAxe and OctAxe native A/B connections share one ASIC chain; separate sessions and multiple ASICs do not establish independent physical-chip job ownership or per-chip coin routing. See [next model qualification](next-models.md) for the OctAxe, GT800 and Gamma Duo650 sequence.
