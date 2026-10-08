# OctAxe Beta 1 candidate validation

Paired candidate version: **`5tratumFW-oct-0.1.0-beta.1`**. Intended tag: **`octaxe-v0.1.0-beta.1`**. Exact software target: **NERDOCTAXEGAMMA / NerdOCTAXE-γ / BM1370 ×8 / ESP32-S3**.

This is a development validation record, not a published-release or general physical compatibility claim. The build checks below were followed by the supervised, unit-specific bench checks recorded here on 8 October 2026. The owner's reported PCB is revision 2.2. Other assemblies and revisions are unqualified. Gamma and QAxe have separate releases and validation records.

## Source changes and host evidence

Reviewed on 8 October 2026. Tests compile production bodies with mocked external platform/hardware boundaries. The paired images used for the supervised bench were built from source commit `8de17c936b6669a40c548f7d647c189fd5899124`; their package and installed-byte checks are recorded below.

| Check | Recorded result and boundary |
| --- | --- |
| Oct board fixture | 11 ASan/UBSan tests passed: actual constructor, selected four/six-phase profile, saved-setting loading, TMP451 channels, failed reads/reinitialization, UART temperature fallback and shutdown behavior; hardware and inherited defaults mocked |
| Mining driver/control | Full sanitizer matrix passed, including 54 Oct profile scenarios: exact model/BM1370/count/CAN admission, eight enumeration responses, partial/extra/zero count rejection, startup failure, pause/resume, generation retirement and protection interruption; UART/GPIO/Buck mocked |
| Native subscriptions | 7 tests passed using actual StratumApi subscribe/send: Oct ASCII alias, one parent identity with A/B indices, partial writes, strict unsafe-model rejection and missing-identity fallback; sockets/identity mocked |
| Capabilities/live status | 13 capability and 12 status tests passed: exact existing UTF-8 γ model, eight counters/descriptors, strict malformed label rejection and informational MUX freshness/context semantics |
| Family package/metadata | 10 tests passed: separate identities, actual Oct model/agent constant assignments, wrong-model/header/count/descriptor/board/version/geometry rejection, diagnostics OFF and OTA-only package contract; image/config fixtures synthetic, SPIFFS recreation mocked |
| Frontend | 36 focused service/settings tests passed; QA and Oct production frontend configurations compiled separately with their own version markers; no target application or device tested by this check |

Four-phase TPS53647 source bounds retain the observed stock v1.0.36 software limits. The existing GPIO3 regulator strap selects TPS53647 or TPS53667 behavior; neither selection proves physical PCB identity. Missing TMP451 muxes retain the base UART temperature-request path. Missing/invalid chip readings remain unavailable, finite hot readings remain visible to protection, and shared board/regulator readings retain their source distinction.

Independent physical-ASIC work assignment remains false. Native A/B streams share the existing chain job selector. A compiled native advertisement or fresh coin/job fixture is not proof that a deployed MUX has admitted this version for remote management.

## Supervised revision 2.2 bench on 8 October 2026

This evidence applies to one owned NerdOCTAXE-γ / BM1370×8 unit, with PCB revision 2.2 reported by its owner. A private read-only full-flash backup was verified against the controller, and the original firmware, NVS page checksums and active image were checked before installation. No bootloader, partition-table or NVS image was written.

The actual controller reported 16 MiB flash. Its partition table matched the candidate's 3 MiB WWW and two 4 MiB OTA slots. The original active app and WWW matched official v1.0.36. The revision 2.2 schematic/BOM at upstream hardware commit `1243a493dfb21da26d7d2752780daa6f2918d40a`, the four-phase GPIO strap selection and the original settings were reviewed together. The regulator package marking was not physically read; this is a scoped profile match, not independent attestation of every revision 2.2 assembly.

| Check | Result and scope |
| --- | --- |
| Exact paired application | `esp-miner-NerdOCTAXE-Gamma.bin`, 3,404,592 bytes, SHA256 `add3604443b9cd2901840fe2a5cd9837e7a18c9d8ab48af7f2cbf26965148563` |
| Exact paired WWW | `www.bin`, 3,145,728 bytes, SHA256 `71bc8e6c4b99df32e8d43484000e0cc96dbe1916f575b9ca94ae4959c6cd0586` |
| Paired OTA | Application first, exact upload acknowledgement, post-boot settings check, then WWW. Installed app/web version Beta1; all 94 compressed and decompressed served web assets matched the package |
| Settings retention | Saved 700 MHz, 1180 mV, version rolling frequency, thermal limits, primary/fallback pools, network, integrations, slots and schedules retained; no tuning preset applied |
| Second fan migration | All seven second-fan NVS settings were proven absent before migration. Follow mode and the inherited thermal limit were checked against source defaults; both fan tachs were positive |
| Running observation | 90 seconds in one boot; accepted shares increased from 61 to 75, all eight hardware counters remained fresh, aggregate approximately 11.3 TH/s, shared board temperature 61.3–61.4°C and regulator temperature 53.9–55.8°C |
| MUX data | Native A stream connected to the configured MUX with fresh 5TRAT coin identity and forwarded job/block context. No second B stream was enabled by this bench |
| Manual pause/resume | Regulator input telemetry fell from 181.75 W to 0.0625 W with the ASIC output at 0 mV. Controller remained reachable and both fans kept running; resume restored the saved voltage, all eight fresh counters and accepted-share progress |
| Power schedule storage | An explicitly disabled weekly window was saved and read back, then the exact original disabled schedule and manual override were restored; mining remained running |
| Physical screen | Owner confirmed the 5tratumFW screen and normal fan sound. Recurring large-logo timing and both four-chip pages were not independently confirmed |

Regulator input telemetry is not a whole-device or wall-power measurement. The controller and fans continue to draw power during a mining pause. Individual chip temperatures remained unavailable on this assembly; shared board and regulator temperatures were real readings and were shown once, rather than repeated as invented chip temperatures.

## Remaining physical qualification

Enabled weekly boundary behavior, power-cycle persistence of the new schedules, cold starts, thermal/fan/UART/regulator fault responses and sustained operation need their own physical checks. Native framebuffer renders and mocked fault tests do not qualify those behaviors. The recurring large logo, scrolling startup and both chip pages require a separate physical-screen observation.

Native A/B streams retain the shared-chain job selector. Independent physical-ASIC work assignment remains false. This bench does not qualify OS/MUX remote configuration allowlists or a second native stream, and does not extend support to other OctAxe revisions or ASIC families.

The historical Beta1 package records `hardwareTested: false`, `physicalLayoutVerified: false` and `independentWorkAssignment: false`, describing the build-time package audit. Its files and manifest are retained unchanged; this later bench record adds only the scoped evidence above. Beta1 has not been published as a supported download.
