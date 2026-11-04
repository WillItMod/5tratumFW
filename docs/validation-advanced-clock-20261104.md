# Advanced clock permission, 4 November 2026

This records software qualification for the pending Gamma Beta14, NerdQAxe++
Beta11 and NerdOctAxe Gamma Beta9 releases. Their final manifests bind the
exact source and images. No higher-clock physical qualification is claimed.
The earlier fast-error candidate at `3e4da565` passed its checks but remained
unpublished; its private packages were preserved when the user requested this
additional feature. No previous public release asset was replaced.

## Permission and command boundaries

- The standard frequency limits remain Gamma 690 MHz and the detected Nerd
  board profile's limit. A separate **1200 MHz experimental project ceiling**
  is available after explicit acknowledgement. It is not a board/silicon rating
  or a recommended operating frequency.
- The shared controller configuration structure is unchanged. Each adapter
  atomically stores its explicit boolean `advancedClockOverride` in the same
  verified options blob as the nine existing fields. Legacy nine-field options
  mean false; no manual clock or older record implies consent. Boot performs no
  options migration write and never starts tuning.
- Saving enabled options requires warning acceptance and warning version 1.
  Every advanced Start also requires fresh advanced acceptance and the existing
  per-run waiver. Unknown, incomplete and incorrectly typed requests do not
  change the saved policy or operating point.
- The browser's warning begins unticked, including for saved enabled options.
  Checking it is local intent only. Explicit Save/readback remains required.
  An idle miner can save disabled permission without lowering its higher manual
  point; Start remains blocked until its admitted range includes that point.
  An owned active point cannot be excluded by new bounds or policy.
- The original backup remains separately identity-bound and unchanged in
  format. Checked original clocks above the standard limit can be restored
  independently of tuning permission. Nerd uses a narrowly authorized restore
  path to prevent its ordinary profile clamp silently changing that original.
- Temperature, voltage, power, fan, fault, sensor freshness, ownership and CAN
  restrictions remain enforced. Manual Nerd frequency limits are unchanged.
  No new automatic reboot or hardware-fault clearing was added.

Numeric frequency range alone is insufficient. Gamma's driver search has
quarter-MHz rounding gaps within the experimental range. Its checked quiet PLL
resolver requires legal nonzero dividers and an error of at most the existing
2 MHz actuator tolerance before transmission. The shared controller's pure
optional preflight skips unsupported probes and backoff points without sending
an invalid frame. Its pointer survives Start and stays outside persisted
configuration. The existing 50 MHz emergency path remains supported. Nerd
retains its actual integer-MHz PLL policy and a separately guarded
power-owner/backend/board command path for tuning and verified restoration.
Gamma's original-backup validation retains its previous 400 MHz lower bound;
emergency command support does not make a 50 MHz backup admissible. The
actual backup-binding regression verifies rejection without a storage write.

## Completed software checks

- Gamma: all **62** actual adapter/API/NVS cases pass with sanitizers, including
  consent, atomic storage failure, legacy records, high-current admission,
  original 750 MHz restoration, disabled policy and reboot.
- Gamma actual driver: all 3,201 quarter-MHz inputs from 400 to 1200 and all 129
  points on the 6.25 MHz grid checked; 48 quarter-MHz rounding holes are denied.
  Invalid/nonfinite/extreme inputs send no frame; 50 MHz remains supported;
  pure preflight does not call the logger.
- All **26** Gamma host entrypoints pass with pinned Node 24.14.0, including
  power, restore, shared controller, decision history, checked commands and
  Ethernet regressions.
- Nerd: all **101** actual runtime cases pass on macOS; ten focused GNU cases
  cover the new policy and fault/update revocation. Actual power-manager,
  backend, board and PLL driver checks pass on macOS and GNU, including
  900/1200 MHz admission, revoked/unacknowledged/faulted/out-of-range denial,
  unchanged voltage and saved settings, and all 1,101 integer inputs from 100
  to 1200 MHz against the real resolver.
- Both web interfaces pass **68** targeted tests each, with eleven new cases
  per application. Full web suites pass **293 Gamma /272 Nerd** cases.
- Shared C11/C++17 sanitizer regressions cover callback retention, skipped
  unsupported probes/backoffs, Start denial and final command preflight,
  alongside fast-error windows, exclusion and unchanged measurement timing.

Target builds, exact source/package checks and any later bounded physical
observations require their own final-source receipts. Host simulation, driver
encoding and numerical bounds do not prove electrical safety, thermal
stability, maximum speed or a guaranteed efficiency improvement. Existing
Gamma 602, larger-screen and W5500 qualification limits remain separate.
