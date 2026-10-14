# Firmware releases

**[Download firmware — choose your miner](../../DOWNLOADS.md)**

| Miner | Current BETA release | Installation guide |
| --- | --- | --- |
| **Gamma 601 / 602** | [Beta 9](https://github.com/WillItMod/5tratumFW/releases/tag/v0.1.0-beta.9) | [Gamma](../installation.md) |
| **NerdQAxe++** | [Beta 6](https://github.com/WillItMod/5tratumFW/releases/tag/qaxe-v0.1.0-beta.6) | [QAxe](../../firmware/nerdqaxe/docs/installation-5tratumfw-qa.md) |
| **NerdOctAxe Gamma** | [Beta 4](https://github.com/WillItMod/5tratumFW/releases/tag/octaxe-v0.1.0-beta.4) | [OctAxe](../../firmware/nerdqaxe/docs/installation-5tratumfw-oct.md) |

Use both BIN files from one model's release. [Firmware selection](../firmware-selection.md) records the exact model and PCB coverage.

<details>
<summary>Validation and build provenance</summary>


| Family | Version and tag | Current evidence |
| --- | --- | --- |
| Gamma PCB 601/602 / BM1370×1 | `5tratumFW-0.1.0-beta.9` · `v0.1.0-beta.9` | Animated candidate celebrations; full build/offline checks, exact-source CI, separate 601/602 paired OTA and short mining/power checks. [Notes](v0.1.0-beta.9.md) · [Validation](../validation-beta9.md) |
| NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4 | `5tratumFW-qa-0.1.0-beta.6` · `qaxe-v0.1.0-beta.6` | Qualified published BETA pair; test PCB revision unknown. [Validation](../../firmware/nerdqaxe/docs/validation-qa-beta6.md) |
| NerdOCTAXE-γ / NERDOCTAXEGAMMA / BM1370×8 | `5tratumFW-oct-0.1.0-beta.4` · `octaxe-v0.1.0-beta.4` | Qualified published BETA pair; owned PCB 2.2 unit only. [Validation](../../firmware/nerdqaxe/docs/validation-oct-beta4.md) |

All three standard pairs compile from clean source `689b2c8fcd01c4b507be102bb1caed22f4552738`. Later documentation records their separate 14 October 2026 installed-device checks. The 480×320 QAxe profiles remain experimental, without public assets or updater admission.

Publication does not qualify other PCB revisions; native A/B sessions do not establish independent physical-chip ownership.

</details>

## Earlier releases

[All GitHub releases](https://github.com/WillItMod/5tratumFW/releases) retain their original assets and tags. Older entries are labelled **Superseded** and link to the current release for that miner.

Earlier Gamma [Beta8](../validation-beta8.md), [Beta7](../validation-beta7.md), [Beta6](../validation-beta6.md), [Beta5](../validation-beta5.md), [Beta4](../validation-beta4.md), [Beta3](../validation-beta3.md) and [Beta1](../validation.md) records remain historical evidence for their own images. The preceding [QAxe Beta5](../../firmware/nerdqaxe/docs/validation-qa-beta5.md) and [OctAxe Beta3](../../firmware/nerdqaxe/docs/validation-oct-beta3.md) records retain their separate evidence; earlier records remain linked from the model guides.
