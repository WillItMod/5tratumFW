# Recovery qualification, 6 November 2026

## Build source and scope

Gamma Beta18 (`v0.1.0-beta.18`), NerdQAxe++ Beta15 (`qaxe-v0.1.0-beta.15`) and NerdOctAxe Gamma Beta13 (`octaxe-v0.1.0-beta.13`) are built from `9303a3faec05a071f6c2a95d4e9c12047f6958d9`. Gamma uses pinned ESP-IDF 6.0.2; the Nerd targets use pinned ESP-IDF 5.5.3. All web builds use Node 24.14.0.

These changes repair restoration and its status reporting. They do not qualify another board revision, higher clock/voltage, larger display or accessory. Only the owned four-chip QAxe is available for new physical checks. Gamma and OctAxe retain software/build qualification for this revision; earlier physical observations remain tied to their original releases.

## Reproduced defects and changed behaviour

The previous production Nerd runtime was exercised under ASan/UBSan with a weak chip, missing counters, Stop during explicit recovery and Stop during automatic error recovery. Advancing 5,400 simulated seconds reproduced retained restoration ownership without any restore write; two cases continued hashing while two remained paused. This reproduced code paths, not the exact cause of a remote user's incident.

Restoration now checks fresh physical temperature, fan, input/output voltage/current, regulator state, identity, immutable backup and the commanded operating point independently of ASIC tuning-quality counters. A physically verified original point can complete with a warning that useful-work/error measurements were missing or unsuitable when verification finished. The tuner remains off; the warning does not declare damaged silicon or a tuned optimum.

Stop is idempotent while its restoration/recovery transaction is active. It does not discard the resume transaction or reset its deadline. A newer explicit control or power-schedule policy retains priority. Gamma's revoked recovery lease remains armed until the normal stop path confirms power off, then the obsolete work fence is cleared before the newer policy can start work. A failed actuation cannot fall through to an unverified saved target.

Each restoration attempt has a 180-second overall bound, with the existing shorter actuation/verification guards retained. Failure is displayed as blocked with its failed gate and elapsed time, rather than an indefinitely active Restoring banner. A blocked hold remains explicit where releasing control would allow an unsafe target. Explicit retry and normal manual handoff remain available.

Moderate ASIC-error rejection waits for the full 30-second physical settling period, then excludes the maximum 15-second counter interval before building the 10-second confirmation streak. Severe-error and physical protection checks retain their earlier response. The 0.5% board and 1.0% worst-chip limits, six correction attempts and two voltage trials are unchanged.

The Tuning page and compact header use controller heartbeat and restoration metadata, expose blocked/completed-with-warning states and distinguish saved settings on a paused miner from a verified running point. Stop/refresh initiation is not denied solely because useful-work counters or heartbeat became stale. New Start and completion claims require fresh matching activity when supported. Diagnostics include the activity snapshot.

## Software and package verification

- All 27 Gamma host-test entrypoints and 37 Nerd host-test entrypoints passed. Gamma's production runtime passed 73 scenarios; the Nerd runtime passed 128 scenarios, including the new restoration cases. C/C++ early-error policy tests passed under AddressSanitizer/UndefinedBehaviorSanitizer.
- The Gamma and Nerd production runtime suites also passed with Linux/GCC, including cached report/activity checks that perform no hardware or storage reads.
- All 330 Gamma and 306 Nerd frontend tests passed. Both production web builds passed. Fifteen production-browser scenarios across the three family routes covered blocked/time-limited restoration, completed-with-warning, paused completion, stale controller activity and malformed metadata.
- All three actual targets compiled and packaged. Gamma's embedded interface and optional matching WWW were verified; each Nerd WWW was reconstructed and bound to all 108 asset records. Manifests, corresponding source archives, certificate bundles and native display fixtures passed. Existing public artifacts remained byte-for-byte unchanged.
- Four retained public 5tratumOS MAIN/DEV archives (0.8.50 and 0.8.52) passed 16 modeled paired-upgrade cases, including both Gamma board profiles. New BETA selection, exclusion from STABLE and source-bound Nerd native-control qualification passed. This was a transport simulation using actual packages, not a live OS installation.

## Owned QAxe checks

The owned QAxe was updated from Beta14 to the exact Beta15 application/WWW pair. All 108 web records, identity, saved settings, profiles, schedules, fitted-fan selection and backup metadata were verified. The owner confirmed the physical screen and both fans normal. Four read-only browser cases verified the actual interface/heartbeat, followed by a live active-run check showing the active label in both page and header.

At the existing 600 MHz /1,150 mV point, the controller successfully started run 1 and completed its full baseline measurement at approximately 4.91 TH/s and 78.7 W. Bounds were fixed at that same point; original thermal/power ceilings were retained. Stop completed and released control, and all original tuning options were restored.

A separate controlled lifecycle paused the miner using its normal power control. Stop while manually paused saved/verified the original settings and released ownership while keeping mining paused. An explicit Restore and resume request was then followed by Stop while the same recovery transaction was still active. Readbacks before and after that Stop confirmed recovery remained active. Recovery completed in 19.01 seconds, original settings were physically verified, normal mining resumed, and the quality-warning field was null. No MCU reboot or changed clock/voltage trial was used. One GET timed out during guarded startup; a later GET-only reconciliation confirmed completion, and no command was replayed.

The original schedule override mode was restored to none. A final read-only audit passed all 15 checks with no differences: original 600 MHz /1,150 mV, both fans required, tuning disabled/unowned, recovery not pending, backup restored and normal mining active. The physical test did not inject a hardware fault, disconnect a fan or deliberately corrupt ASIC measurements; those cases are covered by production-code simulations. No Gamma or OctAxe installation is claimed for this release.
