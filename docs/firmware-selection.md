# Firmware selection and repository structure

For downloads, start with **[Gamma, QAxe or OctAxe](../DOWNLOADS.md)**. This page documents the detailed model and build requirements.

## Choose before downloading

| Model | Source/build | OTA application filename | Matching website | Current support |
| --- | --- | --- | --- | --- |
| Gamma 601/602 | Root; `bash tools/build_5tratumfw.sh` | `esp-miner.bin` | Gamma `www.bin`, same version/package | Public Beta10 BETA pair; separate short 601/602 checks completed. Read [aligned validation](validation-aligned-20261028.md) |
| NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4 | `firmware/nerdqaxe/`; `bash tools/build_5tratumfw_qa.sh` from that directory | `esp-miner-NerdQAxe++.bin` | QAxe `www.bin`, same version/package | Public Beta8 BETA pair; standard 320×170 screen checked, 480×320 selector experimental. Test PCB revision unidentified. Read [aligned validation](validation-aligned-20261028.md) |
| NerdOCTAXE-γ / NERDOCTAXEGAMMA / BM1370×8 | `firmware/nerdqaxe/`; `bash tools/build_5tratumfw_oct.sh` from that directory | `esp-miner-NerdOCTAXE-Gamma.bin` | OctAxe `www.bin`, same version/package | Public Beta6 **EXPERIMENTAL** pair; Standard 320×170 default and explicit saved 480×320 RGB/BGR choices. No physical Beta6 check. Earlier Beta5 evidence is limited to one owned PCB 2.2 unit. Read [OctAxe guide](nerdoctaxe.md) and [Beta6 validation](../firmware/nerdqaxe/docs/validation-oct-beta6.md) |
| Other variants/revisions | None qualified | None | None | No compatibility promise from inherited board definitions |

Check the board label/revision and reported model/ASICs. A common `www.bin` filename does not mean a common web image: download both files from **one model-specific release asset package**. Gamma tags use `v0.1.0-beta.N`; QAxe tags use `qaxe-v0.1.0-beta.N`, with on-device version `5tratumFW-qa-0.1.0-beta.N`; OctAxe uses `octaxe-v0.1.0-beta.N` and `5tratumFW-oct-0.1.0-beta.N`. Versions are intentionally family-specific.

Gamma `5tratumFW-0.1.0-beta.10`, QAxe `5tratumFW-qa-0.1.0-beta.8` and experimental OctAxe `5tratumFW-oct-0.1.0-beta.6` are the current public BETA prereleases. Gamma's pair was compiled from clean source `1645747dfc46a83c52e27c567d43be3f0b53a035`; QAxe Beta8 and the previous OctAxe Beta5 from `1a34b24100e1238e331d8145a70f6b08f85f943d`; OctAxe Beta6 from `5567a2d6c1f59808f197062e565fa6525a93f97b`. Full local builds and package checks passed. The six earlier GitHub family runs apply to Gamma Beta10, QAxe Beta8 and OctAxe Beta5. All three Beta6 publication checks passed at `9aa01916`; see [its validation](../firmware/nerdqaxe/docs/validation-oct-beta6.md#github-publication-checks). Later documentation commits do not change the compiled-source receipts. Gamma uses ESP-Miner v2.15.4 and both Nerd families use shufps v1.1.1-rc1.

The four owned miners completed supervised updates to Gamma Beta10, QAxe Beta8 and OctAxe Beta5, settings and served-asset verification, pause/resume and a temporary schedule using the existing operating point on 28 October 2026. Both Gammas also passed a subsequent normal software restart. The owner confirmed normal screens and fans on those images, including both fans on each Nerd. Use the [dated validation](validation-aligned-20261028.md) for exact scope; these are not controlled cold power-cycle, changed-point or sustained thermal checks. **OctAxe Beta6 has no physical check, including on the standard screen.** QAxe Beta8 and OctAxe Beta6 expose saved 480×320 RGB/BGR choices, but the actual larger panels remain untested. OctAxe's 480×320 geometry is an accepted working assumption; no controller or vendor flash layout is inferred.

The current OctAxe BETA pair is experimental Beta6. The previous physically checked standard-screen [Beta5 pair](https://github.com/WillItMod/5tratumFW/releases/tag/octaxe-v0.1.0-beta.5) remains available, with its installed-pair evidence and prior bench limited to the same owned PCB 2.2 unit. Return to **Standard 320×170**, save and restart under Beta6 before downgrading to both Beta5 files. Publication does not qualify every OctAxe revision, another flash layout or the Beta6 image itself.

The OctAxe BETA's only admitted build identity remains **BOARD `NERDOCTAXEGAMMA`, reported model `NerdOCTAXE-γ`, BM1370×8**. That identity does not identify a PCB revision or verify regulator population, wiring or flash layout. The [Beta5 installed-device validation](validation-aligned-20261028.md), historical [Beta4 validation](../firmware/nerdqaxe/docs/validation-oct-beta4.md) and earlier Beta1/Beta2 bench records apply only to the owned revision 2.2 assembly with its verified 16 MiB layout; no other OctAxe model or assembly is qualified. [Beta6 software validation](../firmware/nerdqaxe/docs/validation-oct-beta6.md) is separate evidence.

The release contains application, WWW, manifest, SHA256SUMS and corresponding source. Read the manifest board/target and image hashes before upload. Both are OTA partition images, not full factory images. Keep the existing settings and a private unit-specific recovery backup; no automatic presets are applied at boot. Gamma's new authentication settings have a documented [Beta9 journal rollback limit](upstream-migration-20261028.md); an old application/WWW pair alone cannot read an extended journal.

## Repository ownership

- Root `main/`, `components/`, `test/`, `tools/build_5tratumfw.sh` and `version.txt`: Gamma baseline, hardware guard and paired OTA build.
- `firmware/nerdqaxe/`: complete separately maintained Nerd upstream derivative, hardware drivers, display/UI, tests and pinned submodule. QAxe uses `version.txt`, `tools/build_5tratumfw_qa.sh` and `tools/package_5tratumfw_qa.py`; the separate OctAxe family uses `version-oct.txt`, `tools/build_5tratumfw_oct.sh` and `tools/package_5tratumfw_oct.py`. Run these helpers from that directory.
- `docs/installation.md`, `compatibility.md`, `validation-beta*.md`: Gamma-only instructions/evidence.
- [QAxe guide](nerdqaxe.md) and `firmware/nerdqaxe/docs/validation-qa-beta*.md`: QAxe instructions and separate validation.
- [OctAxe guide](nerdoctaxe.md), [build guide](../firmware/nerdqaxe/docs/build-5tratumfw-oct.md), [installation limits](../firmware/nerdqaxe/docs/installation-5tratumfw-oct.md) and [Beta6 software validation](../firmware/nerdqaxe/docs/validation-oct-beta6.md): scoped OctAxe instructions and evidence; earlier installed-device records remain specific to their own images.
- Top-level `.github/workflows/gamma-beta.yml`, `qaxe-beta.yml` and `octaxe-beta.yml`: isolated family validation jobs. They build artifacts and never flash a device or automatically publish stable firmware.

Full QAxe source provenance is pinned in its `SOURCE_ORIGIN.json`. Upstream licenses/authorship remain inside both trees. Adding another model requires an explicit compatibility record, model-specific build/partition contract and separate hardware validation.

The current pairs retain separate family validation records; all earlier release records remain historical evidence for their own images. All families produce OTA partition images only, with no factory or NVS image. QAxe and OctAxe native A/B connections share one ASIC chain and do not establish independent physical-chip work or per-chip coin routing. See [next model qualification](next-models.md).
