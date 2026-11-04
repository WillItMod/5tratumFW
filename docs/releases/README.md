# Firmware releases

**[Download firmware: choose your miner](../../DOWNLOADS.md)**

| Miner | Current experimental BETA | Release notes | Installation |
| --- | --- | --- | --- |
| **Bitaxe Gamma 601 /602** | [Beta16](https://github.com/WillItMod/5tratumFW/releases/tag/v0.1.0-beta.16) | [Gamma Beta16](v0.1.0-beta.16.md) | [Gamma](../installation.md) |
| **NerdQAxe++** | [Beta12](https://github.com/WillItMod/5tratumFW/releases/tag/qaxe-v0.1.0-beta.12) | [QAxe Beta12](qaxe-v0.1.0-beta.12.md) | [QAxe](../../firmware/nerdqaxe/docs/installation-5tratumfw-qa.md) |
| **NerdOctAxe Gamma** | [Beta10](https://github.com/WillItMod/5tratumFW/releases/tag/octaxe-v0.1.0-beta.10) | [OctAxe Beta10](octaxe-v0.1.0-beta.10.md) | [OctAxe](../../firmware/nerdqaxe/docs/installation-5tratumfw-oct.md) |

These revisions give Tuning a continuous goal → limits and adjacent advanced-clock warning → Save → waiver and Start flow. Save stays visible, explains missing requirements and shows saved versus unsaved values. Saving never requires the run waiver or starts tuning. Live status, J/TH, six charts, share/network difficulty, decisions and recovery remain together.

The controller and protections retain Gamma Beta14 /QAxe Beta11 /OctAxe Beta9 behaviour. The new pairs were installed with settings and served-asset verification; actual browser checks used GET requests and local drafts only. Read the [current validation](../validation-tuning-flow-20261104.md) for exact sources, checks and limits. No physical higher-clock test is claimed. Historical manifests, binaries and their evidence remain unchanged.

Use [5tratumOS 0.8.52 on the existing MAIN/DEV channel](../../DOWNLOADS.md#update-through-5tratumos) for native controls of later qualified Nerd releases; firmware discovery already works with 0.8.50 or later. Hardware, identity and API checks remain required. Gamma602 tuning, physical QAxe rev7 tuning, larger Nerd panels and W5500 remain outside these completed checks.

<details>
<summary>Earlier-release validation and build provenance (Beta11 / QA8 / Oct6)</summary>


| Family | Version and tag | Evidence for that earlier release |
| --- | --- | --- |
| Gamma PCB 601/602 / BM1370×1 | `5tratumFW-0.1.0-beta.11` · `v0.1.0-beta.11` | **EXPERIMENTAL** original W5500 driver, off by default. Software/build checks only; no Beta11 miner installation or physical LAN/Ethernet-only OTA qualification. [Notes](v0.1.0-beta.11.md) · [Validation](../validation-gamma-ethernet.md) |
| NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4 | `5tratumFW-qa-0.1.0-beta.8` · `qaxe-v0.1.0-beta.8` | Standard 320×170 screen checked; test PCB revision unknown. Larger-screen selector remains experimental. [Notes](../../firmware/nerdqaxe/docs/releases/qaxe-v0.1.0-beta.8.md) · [Validation](../validation-aligned-20261028.md) |
| NerdOCTAXE-γ / NERDOCTAXEGAMMA / BM1370×8 | `5tratumFW-oct-0.1.0-beta.6` · `octaxe-v0.1.0-beta.6` | **EXPERIMENTAL** saved RGB/BGR 480×320 choices; Standard 320×170 default. Software/build checks and later owned-PCB 2.2 standard-screen installed-pair check; larger panel unqualified. [Notes](../../firmware/nerdqaxe/docs/releases/octaxe-v0.1.0-beta.6.md) · [Validation](../../firmware/nerdqaxe/docs/validation-oct-beta6.md) |

The published Gamma Beta11 pair was compiled from clean source `e73cbc69b815b3f7e0a3a5af9266f605bb95a33b`; the previous Beta10 pair from `1645747dfc46a83c52e27c567d43be3f0b53a035`; QAxe Beta8 and the previous OctAxe Beta5 from `1a34b24100e1238e331d8145a70f6b08f85f943d`; OctAxe Beta6 from `5567a2d6c1f59808f197062e565fa6525a93f97b`. Their immutable source receipts and binaries are retained; later documentation commits do not relabel them. Local checks passed for all pairs; [Gamma Beta11 validation](../validation-gamma-ethernet.md) records its separate evidence. The six earlier GitHub family runs cover Gamma Beta10, QAxe Beta8 and OctAxe Beta5; Beta6's full pinned local build and all three GitHub publication checks passed; [its validation](../../firmware/nerdqaxe/docs/validation-oct-beta6.md#github-publication-checks) records exact commit `9aa01916` separately from the compiled pair. Manifest hardware-test flags remain unchanged; dated installed-device records do not qualify a later image.

The 480×320 QAxe Beta8 and OctAxe Beta6 profiles remain experimental; actual larger-panel testing is still required. For OctAxe, 480×320 is the accepted working assumption, not an identified controller or vendor flash layout. The default remains Standard 320×170. The earlier short device checks do not qualify another operating point, a controlled cold power-cycle, sustained thermal stability or every protection fault.

Publication does not qualify other PCB revisions; native A/B sessions do not establish independent physical-chip ownership.

</details>

## Earlier releases

The previous physically checked [Gamma Beta10](https://github.com/WillItMod/5tratumFW/releases/tag/v0.1.0-beta.10) remains available, with its separate [601/602 installed-device evidence](../validation-aligned-20261028.md). Clear the Ethernet setting and save under Beta11, then power off and disconnect the module before installing both Beta10 files over Wi-Fi.

[All GitHub releases](https://github.com/WillItMod/5tratumFW/releases) retain their original assets and tags. The previous physically checked standard-screen [OctAxe Beta5](https://github.com/WillItMod/5tratumFW/releases/tag/octaxe-v0.1.0-beta.5) remains available as a fallback. Return to **Standard 320×170**, save and restart under Beta6 before downgrading, then install both matching Beta5 files. See [the installation guide](../../firmware/nerdqaxe/docs/installation-5tratumfw-oct.md#select-the-experimental-larger-screen). Earlier validation records remain specific to their own images.

Earlier Gamma [Beta9](../validation-beta9.md), [Beta8](../validation-beta8.md), [Beta7](../validation-beta7.md), [Beta6](../validation-beta6.md), [Beta5](../validation-beta5.md), [Beta4](../validation-beta4.md), [Beta3](../validation-beta3.md) and [Beta1](../validation.md) records remain historical evidence for their own images. The preceding [QAxe Beta6](../../firmware/nerdqaxe/docs/validation-qa-beta6.md) and [OctAxe Beta4](../../firmware/nerdqaxe/docs/validation-oct-beta4.md) records retain their separate evidence; earlier records remain linked from the model guides.
