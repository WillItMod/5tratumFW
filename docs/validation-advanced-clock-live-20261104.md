# Advanced clock release validation, 4 November 2026

This record covers the final experimental Gamma Beta14, NerdQAxe++ Beta11 and NerdOctAxe Gamma Beta9 packages. It separates software checks, bounded physical tests and unqualified operating ranges. It does not certify a safe overclock, an efficiency gain or sustained thermal operation.

## Exact release sources

| Family | Version | Compiled source | Successful GitHub validation |
| --- | --- | --- | --- |
| Gamma PCB601/602, BM1370 | `5tratumFW-0.1.0-beta.14` | `684a731d38f626e14d35f66fa1fe1c447a97b275` | [Gamma run 41867661277](https://github.com/WillItMod/5tratumFW/actions/runs/41867661277) |
| NerdQAxe++, NERDQAXEPLUS2, BM1370 x4 | `5tratumFW-qa-0.1.0-beta.11` | `cdf291c6bd6a3240de1c13250745215624c97beb` | [QAxe run 41859581254](https://github.com/WillItMod/5tratumFW/actions/runs/41859581254) |
| NerdOctAxe Gamma, NERDOCTAXEGAMMA, BM1370 x8 | `5tratumFW-oct-0.1.0-beta.9` | `cdf291c6bd6a3240de1c13250745215624c97beb` | [OctAxe run 41859581040](https://github.com/WillItMod/5tratumFW/actions/runs/41859581040) |

Gamma uses ESP-Miner v2.15.4 and ESP-IDF 6.0.2. Both Nerd families use shufps v1.1.1-rc1 and ESP-IDF 5.5.3. Each release manifest, corresponding-source archive and checksums identify its own exact files. Gamma includes the matching web interface in its application; the separate WWW remains available. QAxe and OctAxe still require their matching application/WWW pair. A later documentation commit does not relabel these compiled sources or earlier release assets.

## Software and package checks

All three final-source GitHub validation workflows passed. Local controller, runtime/API, driver, frontend and target checks exercise the fast-error policy, explicit advanced permission, old-options parsing, persistence, PLL preflight, original backup/restore and protection boundaries. Tests distinguish an acknowledged request from a completed operation. The [earlier fast-error record](validation-autotune-fast-errors-20261104.md) remains evidence for its original pre-override source; it is not relabelled as a test of these final packages.

An independent final mixed-source package audit passed on 4 November at 16:39:31 UTC. It checked the actual package sources, application descriptors, hashes and packaged WWW assets, including the Gamma application from 684a731d and Nerd applications from cdf291c6. The retained production 5tratumOS 0.8.50 MAIN and 0.8.50-dev updater code passed six modeled paired-update/settings-preservation jobs, twelve corrupt-image rejection cases and family-isolation checks. The BETA channel admitted the compatible betas and the stable channel excluded them. Nineteen previously published package assets remained unchanged. This replay is software evidence; it is not a physical test through an installed OS browser interface.

The fast guard still excludes the possible old 15-second counter interval. Severe errors of at least 3% board or 5% worst ASIC can reject the first qualifying fresh interval; ordinary errors above 0.5% board or 1% worst ASIC require at least three consecutive fresh intervals spanning 10 seconds. The earliest qualifying software timings are 15/25 seconds, not guaranteed physical reaction times. Successful comparisons retain the minimum 105 seconds after an applied change and eight accepted distinct samples. Up to 16 exact rejected clock/voltage pairs remain excluded for the current run. These are experimental engineering rules, not manufacturer-certified safety limits.

Advanced clock override is explicitly saved, off by default and separate from the per-run waiver. Missing Configure or Start acknowledgement is rejected. The 1,200 MHz project ceiling is a software boundary, not a hardware rating. A higher saved ceiling does not immediately set that clock. Existing temperature, voltage, power, sensor, fan and fault protections remain active. PLL preflight skips unsupported points. The original backup format and protected restoration remain independent of the advanced flag; legacy nine-field options read it as false. Older firmware may not read the new tuning-options format.

## Gamma 601: final 684a731d pair and same-point restoration

The final Gamma application and matching WWW were uploaded and their reported pair verified. All 11 served web assets, six original operating settings, profiles, schedules and tuner options matched the expected records. The application descriptor carried the final build timestamp, 4 November 2026 at 16:34:28. This verification binds the uploaded source-checked files to the reported application and served assets; it does not claim a full application-image hash was measured from the running device.

At the original 580 MHz/1155 mV operating point, an idle Stop/restore request was acknowledged on 4 November 2026 at 16:42:18.891403 UTC. Independent completed verification passed at 16:43:08.810398 UTC, more than 49 seconds later:

- The original backup reported restored and all six original settings were preserved.
- The reported operating point was 580 MHz/1155 mV, with fresh normal mining observations and measured rail voltage within the 30 mV verification tolerance. The clock is the driver-reported command, not an instrument measurement.
- Automatic fan policy was restored, regulator communication flags were zero, and no hardware fault was reported.
- Tuning was off. Request acknowledgement alone was not treated as restoration completion.

This is a bounded, original-point test of the final 684a731d verification path. No tuning trial, above-original clock/voltage operation or full advanced-clock hardware qualification was performed on that final Gamma image. Fan operation is device telemetry evidence; a new owner confirmation of the final Gamma screen/fan is not claimed here.

The final Gamma change restores the saved fan policy and verifies two fresh healthy samples when original clock/voltage are already applied, without resetting useful mining counters or starting another clock/voltage transition. Missing evidence remains unverified. A genuine regulator, voltage, cooling or temperature fault still invokes protection.

### Earlier unpublished CDF Gamma failure

The earlier cdf291c6 Gamma candidate was installed for a bounded 580 MHz/1155 mV to 580 MHz/1150 mV trial. The trial was rejected with about 1.403% worst-ASIC errors and returned to the original requested point. A subsequent Stop was acknowledged, but restoration verification timed out with a regulator communication flag and protective shutdown. A normal restart at the verified original saved settings recovered mining and automatic fan control.

The origin of that communication flag remains unresolved. The final same-point correction and successful bounded verification above do not prove that every possible regulator communication fault has been eliminated. The earlier CDF Gamma image is superseded; its failure is retained here rather than described as a successful completed restore. No hardware fault was masked or cleared by tuning.

## NerdQAxe++: final CDF bounded trial and Stop/restore

The final cdf291c6 QAxe application/WWW, original settings and immutable backup were verified. A 600 MHz/1150 mV to 600 MHz/1145 mV trial was rejected because its measured improvement remained within the noise margin. Stop then completed with all original settings, tuner options, fans and normal mining verified.

Missing advanced Configure and Start acknowledgement returned HTTP 409. An explicitly acknowledged advanced run, with the ordinary per-run waiver, was admitted while its permitted operating range stayed at or below the original point. This checks the consent path, not higher-clock operation. The owner confirmed the QAxe screen and fans looked normal.

The test unit's PCB revision is unidentified. The recognized QAxe rev7 profile is included in code but is physically unqualified by these observations. No claim extends this test to another revision, regulator/cooling assembly or larger display.

## NerdOctAxe Gamma: final CDF bounded rejection and automatic recovery

The final cdf291c6 OctAxe trial changed 700 MHz/1180 mV to 700 MHz/1175 mV. It was rejected by the error-window limit: worst-chip errors were 1.61574%, while board-average errors were 0.1599%. The controller backed off to 690 MHz/1175 mV, then reached the run's permitted clock floor and automatically restored the immutable original 700 MHz/1180 mV point.

Normal mining was verified with tuning off and original tuner options restored. The existing 65°C ASIC preference was retained; the test did not silently replace it with the 60°C starting recommendation. Missing advanced Configure and Start acknowledgement returned HTTP 409; an explicitly acknowledged advanced run was admitted within the original operating range. The owner confirmed the screen and both fans looked normal.

This test applies only to the owned PCB 2.2 assembly and verified 16 MiB layout. It demonstrates that bounded rejection/recovery sequence, not an efficiency gain, high-clock stability or qualification of other OctAxe assemblies.

## Remaining limits

No actual clock or voltage above each miner's pre-test original point was physically qualified. The full 1,200 MHz override range, QAxe rev7 hardware, Gamma 602 tuner admission/backup/restore, larger 480×320 Nerd panels, W5500 LAN and Ethernet-only OTA remain unqualified. Gamma 602's previously reported strict regulator fault was not bypassed. Native or browser fixtures are not physical screen tests.

The default recommended tuning targets remain 60°C ASIC and 70°C VRM, or lower where the installed protection requires it. These are starting guidelines, not universal safe limits; existing saved preferences are retained. No full thermal soak, every-fault test, guaranteed hashrate/efficiency improvement or independent per-chip tuning/routing is claimed. Boot and OTA never restart tuning automatically. Acknowledgements, actual applied settings, completed restoration and safety faults remain distinct states.

For installation and operation see [Downloads](../DOWNLOADS.md), [Adaptive tuning](autotuning.md) and [single-file Gamma updates](single-file-updates.md). Use 5tratumOS 0.8.50 or later on the existing MAIN/DEV channel. The 0.8.51 OS compatibility update adds DualMUX controls for the latest QAxe/OctAxe firmware once available; it does not change the physical qualification recorded here.
