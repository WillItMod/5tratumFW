# Gamma compatibility — BETA

**Current source: 5tratumFW-0.1.0-beta.6, a development BETA candidate. Read [Beta6 validation](validation-beta6.md) for this version. Earlier installed-device observations apply to their recorded image pairs.**

The published application and WWW pair supports **Bitaxe Gamma PCB revisions 601 and 602 only**, using the ESP32-S3 target and the upstream Gamma hardware/partition baseline. The same image serves both revisions; it retains each device's existing clock and voltage settings.

| Hardware | Build/identity support | Installed-device evidence |
| --- | --- | --- |
| Bitaxe Gamma PCB 601 | Allowed by the explicit stored-identity guard; Gamma target compile support. | Beta5 paired app/WWW installed; 37 exported settings and profile/schedule configuration retained, mining and accepted shares observed. See the Beta5 release validation asset. These results do not qualify the Beta6 candidate. |
| Bitaxe Gamma PCB 602 | Allowed by the explicit stored-identity guard; Gamma target compile support. | The owner-updated device reports the Beta5 application. Beta6 paired OTA, settings retention and live status reporting require separate validation. |
| Gamma revisions other than 601/602, other Bitaxe boards, GT800, NerdQAxe, NerdOctAxe and other miners | Incompatible with this Gamma image. | No support or installation claim. Do not flash this image. |

The guard validates the existing **NVS board identity**. It does not physically identify the PCB or verify its wiring. Confirm the PCB revision printed on the board and its existing reported identity before following [installation](installation.md). A missing, unreadable or unsupported identity stops normal startup before hardware initialization; the firmware does not infer a supported identity from a similar device name.

Inherited shared source definitions for other hardware do not extend this support contract. No QAxe firmware, device capability prototype or per-chip independent mining implementation is included in this Gamma release. Automatic tuning and clock presets are not implemented or validated.

The earlier Gamma 601 a1, Beta 2 and Beta 3 observations apply to those earlier images only. The [Beta 4 installed-device record](validation-beta4.md) establishes paired OTA, settings preservation, a short mining observation and the live UI change on 601; it does not repeat the preceding full control bench sequence. Target compilation, host regression tests and OLED rendering are separate evidence. Physical OLED readability, sustained soak and fault recovery remain unqualified for this candidate. The test MUX candidate advertises status to the 601; this Gamma receiver handles the legacy acknowledgment, not the QAxe's richer coin/block metadata.

Use a matched application and WWW pair from one build. Preserve the original operating point through compatible OTA updates; the firmware retains existing protection behavior, including changes that can follow overheating. See [settings preservation](settings-preservation.md) and [recovery](recovery.md) before attempting recovery from an identity or storage fault.
