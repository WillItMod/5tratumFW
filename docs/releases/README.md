# Firmware releases

**[Download firmware — choose your miner](../../DOWNLOADS.md)**

| Miner | Current BETA release | Installation guide |
| --- | --- | --- |
| **Gamma 601 / 602** | [Beta 10](https://github.com/WillItMod/5tratumFW/releases/tag/v0.1.0-beta.10) | [Gamma](../installation.md) |
| **NerdQAxe++** | [Beta 8](https://github.com/WillItMod/5tratumFW/releases/tag/qaxe-v0.1.0-beta.8) | [QAxe](../../firmware/nerdqaxe/docs/installation-5tratumfw-qa.md) |
| **NerdOctAxe Gamma** | [Beta 5](https://github.com/WillItMod/5tratumFW/releases/tag/octaxe-v0.1.0-beta.5) | [OctAxe](../../firmware/nerdqaxe/docs/installation-5tratumfw-oct.md) |

These public BETA prereleases add saved tuning-profile schedules. Gamma Beta10 uses ESP-Miner v2.15.4; QAxe Beta8 and OctAxe Beta5 use shufps v1.1.1-rc1. All four owned miners completed supervised updates and short same-point checks, with normal screens and fans confirmed by the owner. See [aligned changes](../aligned-tuning-update.md), [upstream migration](../upstream-migration-20261028.md) and [dated validation](../validation-aligned-20261028.md).

Use both BIN files from one model's release. [Firmware selection](../firmware-selection.md) records the exact model and PCB coverage.

<details>
<summary>Validation and build provenance</summary>


| Family | Version and tag | Current evidence |
| --- | --- | --- |
| Gamma PCB 601/602 / BM1370×1 | `5tratumFW-0.1.0-beta.10` · `v0.1.0-beta.10` | Separate paired OTA, preserved settings, pause/resume, same-point schedule and normal software-restart checks on 601/602. [Notes](v0.1.0-beta.10.md) · [Validation](../validation-aligned-20261028.md) |
| NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4 | `5tratumFW-qa-0.1.0-beta.8` · `qaxe-v0.1.0-beta.8` | Standard 320×170 screen checked; test PCB revision unknown. Larger-screen selector remains experimental. [Notes](../../firmware/nerdqaxe/docs/releases/qaxe-v0.1.0-beta.8.md) · [Validation](../validation-aligned-20261028.md) |
| NerdOCTAXE-γ / NERDOCTAXEGAMMA / BM1370×8 | `5tratumFW-oct-0.1.0-beta.5` · `octaxe-v0.1.0-beta.5` | Standard 320×170 screen; owned PCB 2.2 unit only. [Notes](../../firmware/nerdqaxe/docs/releases/octaxe-v0.1.0-beta.5.md) · [Validation](../validation-aligned-20261028.md) |

The published Gamma pair was compiled from clean source `1645747dfc46a83c52e27c567d43be3f0b53a035`; both Nerd pairs were compiled from clean source `1a34b24100e1238e331d8145a70f6b08f85f943d`. Their immutable source receipts and binaries are retained; later documentation commits do not relabel them. Local checks and all six final GitHub family runs passed. The manifests retain their pre-bench hardware-test flags; the dated validation records the later installed-device evidence separately.

The 480×320 QAxe profiles are exposed in Beta8 but remain experimental: the actual larger panel is still untested here. OctAxe exposes only 320×170. These short checks do not qualify another operating point, a controlled cold power-cycle, sustained thermal stability or every protection fault.

Publication does not qualify other PCB revisions; native A/B sessions do not establish independent physical-chip ownership.

</details>

## Earlier releases

[All GitHub releases](https://github.com/WillItMod/5tratumFW/releases) retain their original assets and tags. Older entries are labelled **Superseded** and link to the current release for that miner.

Earlier Gamma [Beta9](../validation-beta9.md), [Beta8](../validation-beta8.md), [Beta7](../validation-beta7.md), [Beta6](../validation-beta6.md), [Beta5](../validation-beta5.md), [Beta4](../validation-beta4.md), [Beta3](../validation-beta3.md) and [Beta1](../validation.md) records remain historical evidence for their own images. The preceding [QAxe Beta6](../../firmware/nerdqaxe/docs/validation-qa-beta6.md) and [OctAxe Beta4](../../firmware/nerdqaxe/docs/validation-oct-beta4.md) records retain their separate evidence; earlier records remain linked from the model guides.
