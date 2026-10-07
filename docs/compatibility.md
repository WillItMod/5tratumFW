# Gamma compatibility — BETA

**5tratumFW-0.1.0-beta.1 is BETA. This new build has not been installed on a miner.**

The published application and WWW pair supports **Bitaxe Gamma PCB revisions 601 and 602 only**, using the ESP32-S3 target and the upstream Gamma hardware/partition baseline. The same image serves both revisions; it retains each device's existing clock and voltage settings.

| Hardware | Build/identity support | Installed-device evidence |
| --- | --- | --- |
| Bitaxe Gamma PCB 601 | Allowed by the explicit stored-identity guard; Gamma target compile support. | Earlier `5tratumFW-0.1.0-a1` app and WWW were installed, booted and observed mining. This BETA's new OLED and MUX receiver changes have not been installed or device tested. |
| Bitaxe Gamma PCB 602 | Allowed by the explicit stored-identity guard; Gamma target compile support. | The observed device still runs stock ESP-Miner v2.14.2. It has never been flashed with 5tratumFW; there is no installed-device test for this build. |
| Gamma revisions other than 601/602, other Bitaxe boards, GT800, NerdQAxe, NerdOctAxe and other miners | Incompatible with this Gamma image. | No support or installation claim. Do not flash this image. |

The guard validates the existing **NVS board identity**. It does not physically identify the PCB or verify its wiring. Confirm the PCB revision printed on the board and its existing reported identity before following [installation](installation.md). A missing, unreadable or unsupported identity stops normal startup before hardware initialization; the firmware does not infer a supported identity from a similar device name.

Inherited shared source definitions for other hardware do not extend this support contract. No QAxe firmware, device capability prototype or per-chip independent mining implementation is included in this Gamma release. Automatic tuning and clock presets are not implemented or validated.

The earlier Gamma 601 mining smoke test is evidence for the earlier a1 image only. Target compilation, host regression tests and OLED rendering are separate evidence for this BETA; record their actual results in [validation](validation.md). Gamma 602 operation, real schedule/power transitions and the new OLED require physical device validation. The matching MUX server status update remains undeployed, so receiver tests do not establish end-to-end deployed MUX status operation.

Use a matched application and WWW pair from one build. Preserve the original operating point through compatible OTA updates; the firmware retains existing protection behavior, including changes that can follow overheating. See [settings preservation](settings-preservation.md) and [recovery](recovery.md) before attempting recovery from an identity or storage fault.
