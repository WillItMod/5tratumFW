# Readiness, recovery and Appearance update, 5 November 2026

The released pairs are Gamma Beta17, NerdQAxe++ Beta13 and NerdOctAxe Gamma Beta11. All three application/WWW/source packages were built from `1e2d1b545a01218ce906936dddba6c82b5af62b7`. Their manifests bind the exact image and source hashes; later documentation commits do not relabel them. Prior Gamma16/QA12/Oct10 downloads are unchanged.

## Changes

The shared controller permits at most two unmeasured voltage correction trials in one unhealthy sequence, preserving six total corrections and reserving remaining attempts for clock reduction. A complete passing measurement resets both allowances. Measured-stable rollback, failed-pair exclusion, counter freshness, one-coordinate actuation and all hardware protections remain.

Both interfaces show the exact Start prerequisites with freshness and per-fan/per-ASIC diagnostics. Gamma can identify a missing or weak work domain. Partial/contradictory diagnostics cannot bypass existing admission; stale data are not shown as ready. Diagnostic JSON owns its strings so a later acquisition cannot change a captured report. The GET path does not command operating changes.

Gamma Appearance now saves the supported scheme/primary-colour pair, confirms readback, exposes Apply/error/repair states, and uses readable light/dark accents across navigation, dashboard, Tuning and the hash chart. Incomplete legacy preferences are repaired only by explicit Apply. Normal loading never writes preferences.

## Software and package checks

- All 321 Gamma and 284 Nerd browser tests passed using Node24.14.0 and headless Chrome. Final readiness tests passed for both interfaces. Production desktop/mobile browser fixtures passed for all three families, including exact blocked requirements, enabling Start after valid readings, 65 C within reported limits, colour persistence, light-mode surfaces and legacy preference repair.
- All 27 Gamma host entry points and all 37 Nerd host scripts passed. Production-controller C/C++ sanitizer tests exercise repeated voltage failures followed by clock reduction in every priority, bounded exhaustion, noisy passing measurements, stable rollback, freshness, fault precedence and counter resets. Runtime tests cover automatic original-settings recovery; new report-lifetime tests use the actual JSON implementation.
- Gamma uses verified ESP-IDF6.0.2 source `7101770dc6db2667b3c477cc31365dd1acd6db4e` and `esp-15.2.0_20251204`, mounted in the cached ARM64 build container. This is not a claim of running the uncached official6.0.2 image. The two Nerd targets use the pinned ESP-IDF5.5.3 image and their distinct board profiles. Same-SDK partial compilation was resumed; the final packages, actual ELF bindings and selected source were independently verified. A local parallel-build file-descriptor limit was resolved by finishing Gamma separately.
- Production web budgets, target builds, native display fixtures, image/version checks, pinned CA checks, corresponding source archives and reconstructed WWW checks passed. Previous published assets were checked unchanged.
- Retained public 5tratumOS0.8.50/0.8.52 MAIN/DEV code passed discovery and 16 simulated paired upgrades with the real packages. These are modeled updater tests, not physical OS installations. Separately bound native-control sidecars preserve dynamic qualification; 470 native control/build files match the earlier reviewed source, with changed tuner code audited separately.

The first GitHub Nerd host runs exposed a Linux-only test-harness portability problem: the harness strips production includes and had relied on transitive declarations of `va_start`/`va_end`. Adding explicit `<cstdarg>` and `<cstdio>` to that harness fixes it. The firmware source already included these headers and target builds were unaffected. The same runtime is rechecked with the corrected harness on macOS, Linux and GitHub. Published binary/source hashes remain bound to the original compiled commit; the separate, hash-identified host-test patch is included for rebuilding the Nerd source archive.

## Owned-miner checks

Each miner received its exact new application and matching interface: Gamma601 by application-only OTA with all11 served assets verified; QAxe/OctAxe by their separate matching app/WWW uploads with all108 assets verified per family. Identity, accessible saved settings, profiles, schedules, tuning options and original backup metadata were captured first. APIs do not provide a raw NVS or password export; these are not raw flash backups. The owner confirmed all three screens and their fans normal.

Actual desktop/mobile pages showed Start enabled after a local waiver, with fresh hardware data, all readiness checks, six graphs, J/TH and difficulty information. Those browser checks made no miner mutations. Separate explicit API tests then started a real controller session on every miner; each acknowledged Start and acquired runID1. These were bounded tests, not claims of maximum speed:

| Miner | Fixed test point | Observed result |
| --- | --- | --- |
| Gamma601 | 580MHz /1155mV | Start confirmed. The existing point failed the full error window (board mean0.423%, peak worst reading1.462%). With both bounds locked, the controller correctly could not choose another point. Automatic guarded recovery restored the original settings and normal mining without a reboot or fault clear; this point is not qualified as a clean tuned result. |
| NerdQAxe++ | 600MHz /1150mV | Start confirmed, complete baseline measured, then holding a valid result at about4.88TH/s. Stop/restoration and original options were verified. |
| NerdOctAxe Gamma | 700MHz /1180mV | Start confirmed, complete baseline measured, then holding a valid result at about11.47TH/s. Stop/restoration and original options were verified. |

Gamma temporarily used full cooling for its test while retaining the saved60 C ASIC ceiling. Its immutable original backup remained intact, and automatic recovery restored the original automatic fan control. No test raised clock, voltage or temperature/power ceilings. Final readback verified tuning inactive, original options and operating points restored, fans working and all three miners hashing.

## Limits

These short checks prove the described installation, Start, measurement/rejection and restoration paths. They do not qualify an unrestricted optimisation search, a high-clock point, every ambient condition, every PCB revision, a larger screen or a W5500 adapter. Error thresholds are project policy, not a damaged-silicon diagnosis. Gamma602, other QAxe/OctAxe revisions and physical LAN remain outside this owned-device evidence.

## Final GitHub and public checks

Validation commit `24ee95ea3d1a7c1ce8edb49fcf3e59e879f24ca6` changes only the host-test includes and documentation relative to the compiled release source. All three GitHub validations passed: [Gamma](https://github.com/WillItMod/5tratumFW/actions/runs/42021120631), [QAxe](https://github.com/WillItMod/5tratumFW/actions/runs/42021120624) and [OctAxe](https://github.com/WillItMod/5tratumFW/actions/runs/42021120587). The corrected actual runtime harness also passed all scenarios locally on macOS and Linux.

All three releases are public prereleases. All 23 public assets were downloaded without authentication and matched their qualified local hashes; all three tags resolve to the exact compiled commit. The real public GitHub catalogue and assets passed 16 model/version checks across retained 5tratumOS0.8.50/0.8.52 MAIN/DEV modules and four native-control qualifications on0.8.52. No OS code update or device reflash is part of that public-discovery check.
