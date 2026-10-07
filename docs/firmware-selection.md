# Firmware selection and repository structure

## Choose before downloading

| Model | Source/build | OTA application filename | Matching website | Current support |
| --- | --- | --- | --- | --- |
| Gamma601/602 | Root; `bash tools/build_5tratumfw.sh` | `esp-miner.bin` | Gamma `www.bin`, same version/package | Stored-identity guard allows601/602. Beta4 installed on601;602 unflashed |
| NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4 | `firmware/nerdqaxe/`; `bash tools/build_5tratumfw_qa.sh` from that directory | `esp-miner-NerdQAxe++.bin` | QAxe `www.bin`, same version/package | Separate BETA, unidentified test PCB revision. Read that release's validation |
| Other variants/revisions | None qualified | None | None | No compatibility promise from inherited board definitions |

Check the board label/revision and reported model/ASICs. A common `www.bin` filename does not mean a common web image: download both files from **one model-specific release asset package**. Gamma tags use `v0.1.0-beta.N`; QAxe tags use `qaxe-v0.1.0-beta.N`, with on-device version `5tratumFW-qa-0.1.0-beta.N`. Versions are intentionally family-specific.

The release contains application, WWW, manifest, SHA256SUMS and corresponding source. Read the manifest board/target and image hashes before upload. Both are OTA partition images, not full factory images. Keep the existing settings and a private unit-specific recovery backup; no automatic presets are applied at boot.

## Repository ownership

- Root `main/`, `components/`, `test/`, `tools/build_5tratumfw.sh` and `version.txt`: Gamma baseline, hardware guard and paired OTA build.
- `firmware/nerdqaxe/`: complete separately maintained QAxe upstream derivative, hardware drivers, display/UI, tests, pinned submodule and model-specific build/package helpers.
- `docs/installation.md`, `compatibility.md`, `validation-beta*.md`: Gamma-only instructions/evidence.
- `docs/nerdqaxe.md` and `firmware/nerdqaxe/docs/`: QAxe instructions and separate validation.
- Top-level `.github/workflows/gamma-beta.yml` and `qaxe-beta.yml`: isolated family validation jobs. They build artifacts and never flash a device or automatically publish stable firmware.

Full QAxe source provenance is pinned in its `SOURCE_ORIGIN.json`. Upstream licenses/authorship remain inside both trees. Adding another model requires an explicit compatibility record, model-specific build/partition contract and separate hardware validation.
