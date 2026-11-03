# Adaptive tuning qualification and memory repair — 3 November 2026

**3 November 2026. Not a release announcement.** The Gamma Beta12, NerdQAxe++ Beta9 and NerdOctAxe Gamma Beta7 development candidates remain unpublished. The first test window stopped after the failures below and returned the updated miners to their previous firmware. A later, explicitly extended window tested the repaired source; its separate results are recorded below. Further presentation work is not part of those installed images.

## Source and qualification boundaries

| Source | Status |
|---|---|
| `9356792c` | Original candidate. Software, build and simulated updater checks passed; physical checks exposed the failures below. |
| `7dd79818` | Local correction for Gamma regulator encoding, required Nerd point fields and manual same-point ownership handoff. Built and checked offline; **never installed on a miner**. It does not fix the later memory issues. |
| `35719b8e` | Memory repair. Fresh target builds, stack analysis and the limited repaired-source physical checks below. |
| `7b205d93` | Gamma-only additive cached regulator diagnostics; installed on the Gamma 601 only. No tuner policy or admission relaxation. |
| Later presentation changes | Additional explanations, graphs and physical tuning pages remain separate from those installed-image observations. |

Passing host tests, browser fixtures, image checks and simulated 5tratumOS updates did not establish safe physical startup, flash access or task-stack headroom. Existing software results remain useful evidence within those limits.

## First-window observations and recovery

- **Bitaxe Gamma 601/602:** the original Beta12 application failed startup voltage restoration because its regulator handling rejected a valid voltage mode with the REL flag set. Tuning had not started. Both were returned to **Gamma Beta10**, with their existing matching web image untouched. Saved settings survived, and normal hashing and fans were observed after recovery.
- **NerdQAxe++:** Beta9 installed, but the actual API omitted the required `baseline`, `current` and `best` point members. This was reproduced with the production ArduinoJson library and corrected in `7dd79818`. A regulator temperature warning near 96°C also made its strict tuning telemetry ineligible; that warning and its protection limits were left intact. No tuning Start was sent on this unit. It was returned to **NerdQAxe++ Beta8**; all 108 packaged web assets, original observable settings, profiles and schedules were verified, with normal hashing and fans.
- **NerdOctAxe Gamma:** Beta7 initially booted and hashed normally; the owner confirmed its screen and both fans. An owner-started session made one observed **5mV downward voltage probe**. The original saved point remained **700MHz /1180mV**. Stop was acknowledged, then the miner lost network access and the owner reported a boot loop. Acknowledgement did not prove restoration completed. The unit was recovered to **NerdOctAxe Gamma Beta6**, with all 108 matching web assets and its original observable settings, profiles and schedules verified. All eight chips resumed hashing, with both fans and normal boot confirmed.

These observations apply only to the owned units and their installed standard screens and cooling. They do not qualify another PCB, larger display, regulator assembly or sustained tuning operation.

## Preserved evidence and attribution limits

Before the OctAxe repair, a complete 16MiB flash image was read and verified through ROM download mode. Offline checks found valid partition-table MD5, valid OTA-selector CRCs, intact application images and the expected selected Beta7 slot. Its saved main configuration and immutable original tuning backup were valid. All 20 backup settings were present and numeric; all 17 independently observable original settings matched the pre-install snapshot.

Nine original defaults remained absent from the main configuration, establishing that the full restore commit had **not completed**. There is no evidence of a committed higher clock/voltage or malformed restored settings. The exact first failing instruction remains unknown because the original panic trace was not captured.

Source and target analysis nevertheless establish two repair requirements:

1. Present tuning records can be read from flash while initialization runs on the power task’s PSRAM stack. The actual SDK configuration requires an internal stack for that cache-disabled flash path. This invalid execution context is a concrete startup risk, consistent with failures after records first exist, but the missing panic trace prevents conclusive attribution of the observed reset.
2. The 4096-byte restore worker has inadequate conservative stack headroom: its acquisition frame alone reserves 2688 bytes, and a real sensor/logging call chain can exceed the worker budget. This is a concrete stack-budget risk, not proof that overflow caused the first reset.

The OctAxe repair restored only its selected application and matching web image. A second full-flash readback verified both payloads and that **every byte outside the written sectors was unchanged**, including NVS, bootloader, partition table and OTA selectors. No erase-all or factory/configuration image was used. Raw backups, identifiers, credentials and decoded private configuration remain private.

## Memory repair and offline regression checks

The repaired source preloads raw tuning records on the internal boot task before
starting either external-memory power task. Hardware binding and thermal-limit
validation remain in RAM after fan initialization. Storage entry points reject an
external stack before calling NVS. Missing, corrupt and unreadable records remain
distinct; storage failure disables tuning without taking over ordinary mining.

The tuner now copies at most eight chip records into a bounded object, leaving
the existing general 64-chip dashboard API unchanged. The source requests 8KiB
for both the boot and internal restore tasks; the HTTP task remains 12KiB.
These sizes were subsequently verified in the fresh target builds below. Restore diagnostics retain its
observed minimum free stack and freeze restoration below a 1KiB margin. A
post-commit diagnostic failure adopts the authorized configuration revision
before retaining ownership, so it cannot fall through to normal actuation.

The production runtime passes **59 scenarios** on macOS and GNU/Linux with
AddressSanitizer, UndefinedBehaviorSanitizer and fail-fast sanitizer settings.
They model every power tick on an external stack and reject any flash call from
that context. Present-record reboot, missing/corrupt/read/allocation failures,
wrong-context actions and low stack before/after commit are covered. The bounded
monitor tests compare four/eight-chip results, reject mismatches and exercise
concurrent publication. All **32 Nerd host entrypoints** pass. These are software
checks, not physical stack measurements or a successful target reboot claim.

## Repaired-source target checks

Fresh QAxe and OctAxe target builds at `35719b8e` request 8192-byte internal boot
and restore stacks, with the existing 12288-byte HTTP stack. Target stack-usage
analysis reduced the acquisition frame from 2688 to 464 bytes. A partial call
chain is not a formal whole-program stack bound; the physical minimum-free
measurements below are separate evidence. A present-record Continuous On boot
variant also passed native/GNU sanitizer checks without flash access from the
external-stack power ticks or automatic tuning Start.

Matched application/web packages passed checksum, source, model and partition
checks, plus simulated production 5tratumOS paired updates. These simulations
did not update an installed OS or constitute physical updater qualification.

## Continued-window observations

The owner explicitly extended the powered test window and accepted bounded
experimental tests. Clock and voltage ceilings remained at or below each
unit's recorded original point. These checks do not establish a maximum TH/s
overclock or a stable undervolt boundary.

- **Additional NerdQAxe++**, originally stock v1.1.0-rc2, received the `35719b8e`
  QAxe Beta9 application and all 108 matching web assets. Its original point was
  600MHz /1150mV. Fixed-point backup/restore, reboot with both tuning records
  present, and inactive restore passed. A completed Efficiency /Continuous Off
  round rejected four measured neighboring trials at 600MHz /1145mV and
  590MHz /1150mV, retaining 600MHz /1150mV. A second Balanced /Continuous On round
  completed one candidate comparison and was deliberately stopped before full
  convergence. Full restoration then passed. A normal reboot retained Continuous
  On and the immutable backup while leaving the tuner inactive, followed by a
  successful inactive restore. The final restore worker reported 5516 bytes
  free of its 8192-byte stack, with no low-stack flag. Original exposed settings
  were retained; the owner confirmed the screen and both fans normal. The
  original QAxe with the regulator temperature warning remained on Beta8 and
  was not tuned.
- **NerdOctAxe Gamma** received the `35719b8e` Beta7 application and all 108
  matching web assets. At its original 700MHz /1180mV point, fixed-point
  Stop/full restore, present-record reboot and inactive restore passed. Restore
  diagnostics observed at least 5408 bytes free of the 8192-byte stack, with no
  low-stack flag. A completed Efficiency /Continuous Off round rejected four
  measured trials at 700MHz /1175mV and 690MHz /1180mV, retaining the original
  point. The owner confirmed its standard screen and both fans normal. A second
  Performance /Continuous On run completed two candidate windows and rejected
  both before being deliberately stopped; it did not complete another full
  round or continuous reassessment cycle. Full restoration passed, followed by
  a normal reboot with Continuous On saved but no automatic Start, and a final
  inactive Stop/full restore. The final worker minimum free stack was 5516
  bytes, with no low-stack flag. Original settings, profiles and schedules were
  retained; the final state was disabled at 700MHz /1180mV with the original
  backup available and restored.
- **Bitaxe Gamma 601/602** received the `35719b8e` Beta12 pair with all 11 web
  assets and saved settings verified. Both mined normally, but strict tuner
  admission reported a fault that the original interface could not explain.
  The 601 subsequently received the `7b205d93` application exposing cached raw
  regulator status. Its six reported status registers were zero with valid
  temperatures, voltage and current; one controlled identical web-image repeat
  did not reproduce the earlier strict fault. The 602 retained the unexplained
  strict fault on `35719b8e`. Both were left at their original settings with
  tuning disabled, no tuner backup and no tuner Start/Stop request. Their screen
  and fan operation was confirmed, but Gamma tuning, backup and restoration
  have **not** been physically qualified.

The short repaired-source checks do not qualify another PCB or regulator,
larger displays, increased clock/voltage, a sustained soak, injected cooling
failure, or every protection/recovery path. Additional UI/display changes need
their own builds and checks. Publication remains on hold until the unresolved
Gamma admission issue and required final qualification are addressed. The last
device write in this window completed at 17:36:17 UTC; final OctAxe readback was
recorded at 17:37:14, before the agreed 17:40 write cutoff. No subsequent UI
change is included in that physical test claim.

## Subsequent read-only presentation work

The web Tuning page now explains the three objectives before selection, labels
the saved goal separately from unsaved choices, explains controller phases and
known backoff reasons, and shows completed current-round means separately from
live readings and the original backup. Tab-local trend graphs are capped at
120 observations/ten minutes and retain gaps for unavailable readings. They
are explicitly not additional hardware-counter samples. Minimum phase duration
and sample count are not a finish estimate. Restore is available near the top
of the page; the existing waiver, authentication and readback paths remain.

Both display families gain a read-only Tuning page through their normal screen
navigation. Bounded RAM snapshots use a nonblocking mutex attempt; they do not
initialize storage, poll hardware, renew an observer lease or control tuning.
Busy, stale or blocked snapshots do not supply apparently current measurements.
The original safety, update, setup, QR and shutdown precedence remains.

Software validation of these additions passed before target packaging:

- Gamma Angular **225/225** and shared Nerd Angular **202/202** tests; production
  web builds for Gamma, QAxe and OctAxe, with unchanged hard build budgets.
- **36** browser cases against the compiled production bundles on loopback,
  including desktop/mobile layouts, goals, stale/failed observations, malformed
  optional progress data, derived-number overflow, graph gaps, retained backup
  controls and explicit waiver/start/restore. Preview data is synthetic.
- Gamma production adapter/API **35** sanitizer cases; Nerd production runtime
  **60** cases on native Clang and isolated pinned GNU, plus all **33** Nerd host
  entrypoints. The shared read-only progress helper runs under C and C++
  AddressSanitizer/UndefinedBehaviorSanitizer checks.
- Actual LVGL9 Gamma **44** rendered views and production `screen.c` carousel
  tests for both 601 and 602. A first full generic extra page exceeded the
  existing fixed display pool; the final three-object page fits without raising
  it. Both runtime fixtures reported allocator peak usage 54,152 bytes, current
  free 7,720 bytes and largest free block 2,840 bytes. These are native allocator
  observations, not physical free-heap measurements.
- Actual LVGL8 Nerd **60-page** suites at both 320×170 and 480×320, retaining the
  real UI initialization, normal screens, QR buffer and warning/candidate
  overlays together. The unchanged custom allocator reported requested-byte
  peaks of 106,561 and 151,139 respectively; the Tuning widgets added 2,508
  requested bytes. The cached snapshot is 192 bytes. These do not measure
  physical allocator overhead or remaining target PSRAM.

The source changes do not modify the controller's search, electrical limits,
storage transactions or actuator policy. Fresh target builds, final package
checks and the remaining supervised Gamma qualification stay separate gates.

## Final software packages — still unpublished

All three final packages were built and independently verified from clean source
**`51afd35cdefc7b8e33c30eb6b06b8ee848a7aaf0`**. The Gamma build uses pinned SDK
6.0.2; QAxe and OctAxe use pinned SDK 5.5.3. Later validation-only documentation
commits do not change their binary/source provenance.

The target compilers caught a portable counter-formatting error that the native
host typedefs did not expose; `5d963f46` corrected both display paths with
`PRIu32`. The full Nerd helper then caught a missing allocator link in its
companion host candidate renderer; the final one-line test-target repair is
`51afd35c`. Both native renderers and final target/package stages passed after
these corrections. No firmware or test-pool/stack limit was raised to pass.

A Gamma package check also caught a stale standard web-output cache, despite a
separately successful new-interface preview build. That pair was rejected and
retained privately. Both the embedded application interface and the separate
WWW image were rebuilt from the final production output. The final verification
parses the actual SPIFFS image, compares each served static asset with the tested
production bundle and verifies the new Tuning page markers. Version strings and
checksums alone are not treated as proof of the new page.

| Candidate | Application SHA256 | Matching WWW SHA256 |
|---|---|---|
| Bitaxe Gamma Beta12 | `e40fb750861ab56ef703fec97a18a75e858f07db7d5ef351510f1e939c51a9fe` | `625fde61fd872e8c3401ffd48fd239dea474468e14f3b41d4d6f2d593c67a0f6` |
| NerdQAxe++ Beta9 | `489b9cb96d9522f4d861342d2618d4c3c7699cbac555c3ddd42d28fbf94cd9f3` | `61cc3f452a481d0658afe7c8fff2e015092d92f9804bd64a2ee604f61144d807` |
| NerdOctAxe Gamma Beta7 | `b7973349c7a845af76f64814dedb18434745fde05b27186f1a28eb7e1a2db447` | `c1bf2260861b90aa992aec8bb94f043ec5719e88dd7aa4365c17d0d3654202f0` |

Each final manifest, checksum list, source archive/inventory, selected target
configuration and actual application descriptor matched independently. Actual
WWW contents matched all nine tested Gamma static assets and all 58 tested
static assets in each Nerd family, excluding their separately checked generated
version/provenance files. Gamma's embedded asset table also matched every WWW
payload. The preserved older Nerd web images fail the new-interface marker
check, providing a negative control.

Read-only target stack analysis recompiles the selected units privately with
stack-usage metadata, verifies unchanged executable code and leaves normal
outputs intact. Gamma's linked display callback is 864 bytes, its snapshot
function 144 bytes and renderer 320 bytes, with the existing 7168-byte LVGL task.
The Nerd linked LVGL timer task is 432 bytes plus its 32-byte wrapper, snapshot
128 bytes, Tuning renderer 192 bytes and update wrapper 64 bytes, with the
existing 6000-byte task. These are function frames, not a complete worst-case
call graph or physical high-water measurement. Existing formatting, library,
RTOS, interrupt and error paths must still be considered.

The unchanged production **5tratumOS 0.8.50** updater passed an offline replay
with the actual final images and source/configuration checks. Five modeled
paired jobs completed with exactly two uploads and retained settings: Gamma
Beta11→Beta12, QAxe Beta8→Beta9, OctAxe Beta6→Beta7, and older QAxe Beta6 /OctAxe
Beta4 migrations exercising optional-API redirects. Corrupted application and
web payloads, wrong-family selection, stable-channel beta selection and repeat
installation were rejected as applicable. The tested updater modules matched
the retained published MAIN/DEV archives, and the network guard recorded zero
events. No OS update is required to discover these releases once published.
This is offline parser/state-machine evidence, not an installed OS or physical
paired OTA test.

The final presentation packages have **not been installed on the miners**.
The earlier source357 physical results remain limited to those exact images.
Gamma 602's unexplained strict admission fault, Gamma tuner backup/restore and
the final supervised interface checks remain outstanding. Publication remains
on hold until those checks pass. The previous write window is closed; the
remaining tests need a newly confirmed powered window. No miners were contacted
for this subsequent offline work.
