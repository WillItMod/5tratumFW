# Gamma compatibility — BETA

**Qualified Gamma Beta8 BETA pair: `5tratumFW-0.1.0-beta.8`.** Separate short 601/602 installed-pair checks are recorded in [Beta8 validation](validation-beta8.md). Earlier [Beta7](validation-beta7.md) and [Beta6](validation-beta6.md) records remain evidence for their own pairs.

The Beta8 pair retains support for **Bitaxe Gamma PCB revisions 601 and 602 only**, ESP32-S3 and the existing Gamma hardware/partition baseline. The same image serves both revisions and retains each unit's own configured clock and voltage.

| Hardware | Build/identity support | Installed-device evidence |
| --- | --- | --- |
| Bitaxe Gamma PCB 601 | Allowed by the exact stored-identity guard; Gamma target compile support. | Beta8 paired update verified on 8 October 2026; all 11 served records, saved settings, 20 slots and both schedules retained; both successful schedule GETs declare JSON, with one new accepted share. |
| Bitaxe Gamma PCB 602 | Allowed by the exact stored-identity guard; Gamma target compile support. | Separate Beta8 pair verified on 8 October 2026; all 11 served records, saved settings, 20 slots and both schedules retained; both successful schedule GETs declare JSON, with one new accepted share. |
| Gamma revisions other than 601/602, other Bitaxe boards, GT800, NerdQAxe, NerdOctAxe and other miners | Incompatible with this Gamma image. | No support or installation claim. Do not flash this image. |

The guard validates the existing **NVS board identity**. It does not physically identify the PCB or verify its wiring. Confirm the PCB revision printed on the board and its existing reported identity before following [installation](installation.md). A missing, unreadable or unsupported identity stops normal startup before hardware initialization; the firmware does not infer a supported identity from a similar device name.

Inherited shared source definitions for other hardware do not extend this support contract. No QAxe firmware, device capability prototype or per-chip independent mining implementation is included in this Gamma release. Automatic tuning and clock presets are not implemented or validated.

The earlier Gamma 601 a1, Beta 2, Beta 3 and [Beta 4 observations](validation-beta4.md) apply to those earlier image pairs. [Beta7](validation-beta7.md) adds its own paired OTA and short mining observations on both models; [Beta6](validation-beta6.md) remains an earlier separate record. Beta7 did not repeat a complete power-control or protection-fault bench. Target compilation, host regression tests and OLED rendering are separate evidence. Physical OLED readability, sustained soak and fault recovery remain unqualified. The test MUX advertises fresh status to both units; this Gamma receiver handles the legacy acknowledgment, not the QAxe's richer coin/block metadata.

Use a matched application and WWW pair from one build. Preserve the original operating point through compatible OTA updates; the firmware retains existing protection behavior, including changes that can follow overheating. See [settings preservation](settings-preservation.md) and [recovery](recovery.md) before attempting recovery from an identity or storage fault.
