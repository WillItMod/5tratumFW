# Adaptive tuning qualification and memory repair — 3 November 2026

**3 November 2026. Not a release announcement.** The Gamma Beta12, NerdQAxe++ Beta9 and NerdOctAxe Gamma Beta7 development candidates are not physically qualified for publication. Live testing was stopped after failures, and every updated miner was returned to its previous firmware. A separate additional QAxe++ was left unchanged.

## Source and qualification boundaries

| Source | Status |
|---|---|
| `9356792c` | Original candidate. Software, build and simulated updater checks passed; physical checks exposed the failures below. |
| `7dd79818` | Local correction for Gamma regulator encoding, required Nerd point fields and manual same-point ownership handoff. Built and checked offline; **never installed on a miner**. It does not fix the later memory issues. |
| Memory repair in this checkout | Source and host regressions pass independent review. Fresh target stack analysis, matched packages and separate physical qualification remain required. |

Passing host tests, browser fixtures, image checks and simulated 5tratumOS updates did not establish safe physical startup, flash access or task-stack headroom. Existing software results remain useful evidence within those limits.

## Physical observations and final state

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
These sizes require final target verification. Restore diagnostics retain its
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

## Required next qualification

Move flash loading off PSRAM task stacks, keep the normal power loop free of flash calls, remove the oversized counter snapshot from sensor-call stacks and establish target stack margin. Add regressions for present/missing/corrupt records and task context, then inspect the final compiled call chains. Only after those checks pass should supervised backup, Stop/restore, reboot with records present and inactive restore be tested again. Dynamic search follows those gates.

No repaired candidate is described as physically qualified. Publication remains on hold pending repair and qualification.
