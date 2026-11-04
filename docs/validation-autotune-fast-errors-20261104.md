# Fast ASIC-error rejection candidate, 4 November 2026

This is an **unpublished software candidate**, based on documentation commit
`19b4f374` and the released executable source `06d2bae5`. It has not been
installed on a miner. Gamma Beta13, NerdQAxe++ Beta10 and NerdOctAxe Gamma Beta8
downloads and version files remain unchanged. Private compilation checks using
those existing version markers are not replacement release packages.

## Controller changes

All three families use the same original controller in
`firmware/nerdqaxe/main/autotune/core.h`. The full performance comparison still
requires settling and a complete measurement window. An independent error
check can reject an unstable baseline or trial sooner:

| Evidence after the counter guard | Candidate response |
| --- | --- |
| Board errors at least 3%, or any ASIC at least 5% | Reject on the first qualifying fresh interval |
| Board errors above 0.5%, or any ASIC above 1% | Reject after at least three consecutive fresh intervals spanning at least 10 seconds |
| Duplicate, stale or invalid observations | Cannot supply another error interval; invalid data resets the consecutive sequence |
| Gap longer than 15 seconds, or a clean interval | Reset the consecutive sequence |

The independent guard starts at Start or confirmed application of a setting.
It excludes the maximum possible 15-second counter interval after that point,
using the captured hardware timestamp rather than the browser or polling time.
Thus deterministic tests can reject a severe post-change error at 15 seconds,
or persistent ordinary errors at 25 seconds. These are qualifying sample
timings, not physical response-time guarantees. Holding/observing retains its
existing conservative immediate response to a valid over-limit reading.

Error-failed exact clock/voltage pairs are retained in a bounded 16-entry RAM
exclusion list for the current run, including later continuous rounds and
configuration changes. A new Start or boot clears that run-local knowledge.
Candidates, voltage corrections, clock backoffs and rollbacks all veto excluded
points. Exhausting the list stops experiments and requests the existing guarded
original-settings recovery; it does not evict a failure and retry it.

The rollback reference is a separately cached point which passed a complete
clean measurement window, not an unverified trial origin or assumed factory
setting. Returning to it requires current bounds and headroom and changes only
one coordinate through the existing actuator. Otherwise bounded voltage/clock
correction applies. Six consecutive corrections are permitted; a seventh
failed evaluation exhausts error recovery. The durable original backup remains
separate, unchanged and available to the existing guarded recovery owner.
Hardware faults, failed sensors, manual/scheduled handoff and uncertain
actuation retain their existing blocking behaviour. No MCU reboot or automatic
fault clearing was added.

The web interfaces explain early rejection, stable-point rollback and
exhaustion. Early decisions contain their actual instantaneous conditions with
no completed measurement mean or improvement score. The existing wire key
`rejectedWindows` remains compatible, while visible labels now say
**Rejections this run** or **rejections**.

## Software verification

- New production-controller regressions pass as C11 and C++17 with
  AddressSanitizer and UndefinedBehaviorSanitizer on macOS and GNU/Linux.
  The released controller fails the new early severe-error regression in both
  languages, establishing that the tests detect the original delay.
- Cases cover exact thresholds, the worst ASIC behind a low board average,
  distinct IDs and captured timestamps, stale/duplicate data, interrupted
  sequences, counter flushing, temperature/power/fault precedence, safe rollback,
  voltage ceilings, one-coordinate actuation, unchanged full scoring periods,
  exclusion retention/reset and bounded capacity/recovery exhaustion.
- Existing shared controller, decision-history and read-only presentation
  sanitizer checks pass. Completed-window exhaustion still retains genuine
  measurement means; early exhaustion reports instantaneous conditions instead.
- The actual Gamma adapter/API/backup/recovery harness passes all **55** cases.
  The actual Nerd adapter/API/storage harness passes all **93** scenarios,
  including QAxe/OctAxe error exhaustion and true-fault exclusion.
- Both web component suites pass **57/57** targeted Chrome tests, including
  four new tests per application for reason accuracy, evidence, stale status,
  hardware-protection precedence and absence of browser-initiated writes.
- The new root host entrypoint includes the fast-error regression in the normal
  Gamma validation/CI discovery, as well as the Nerd host discovery.

Both production web builds and all three fresh ESP32-S3 target builds pass.
The Gamma compressed web payload is 658,403 bytes across 11 assets, within its
existing budget. No release package or source archive was regenerated.

| Target | Build environment | Selected static stack frames | Configured HTTP stack |
| --- | --- | --- | --- |
| Gamma | Verified ESP-IDF 6.0.2 source `7101770d`, pinned `esp-15.2.0_20251204` toolchain in an existing cached container | `handle_action` 1,376 bytes; `post_tuner` 208; `select_target` 480 | 8,192 bytes |
| QAxe | Helper-pinned ESP-IDF 5.5.3 container, NERDQAXEPLUS2 profile | `command` 1,408 bytes; `acquire` 464; restore worker 144 | 12,288 bytes |
| OctAxe | Helper-pinned ESP-IDF 5.5.3 container, NERDOCTAXEGAMMA profile | `command` 1,408 bytes; `acquire` 464; restore worker 144 | 12,288 bytes |

The Gamma check used the previously verified mounted SDK/toolchain rather than
claiming use of the uncached official 6.0.2 image. Its first configuration
attempt disabled the component manager and could not resolve the cached mDNS
component alias; enabling normal cached dependency resolution corrected the
invocation without a product-source change. The final fresh build and built-CA
check pass with network access disabled. Node 24.14.0 was used for both web
builds; Nerd builds retain all diagnostic/capture flags off.

The controller is 1,072 bytes on the 64-bit host and 1,064 bytes in the Gamma
target ELF, with a compile-time 1,536-byte bound in C and C++. Individual target frames were collected with
`-fstack-usage`; they are not a formal whole-call-chain bound or a physical
stack high-water measurement. Private build receipts verify unchanged source
inventories during compilation, root SDK configuration, version files and
existing published artifacts. They do not establish physical operation.

## Qualification limits

No firmware, settings or tuning request was sent to a miner for this change.
The new rejection thresholds are experimental engineering policy, not
manufacturer-certified safety limits. Host and target checks do not establish
thermal stability, a hashrate gain, electrical safety or compatibility with
every PCB. Unexpected hardware-counter resets remain distinct from managed
actuator resets: this change does not independently identify every unexpected
reset versus unsigned counter rollover. User reports still need the actual
model, installed application/web versions, error percentages and decision
history before attributing their errors to a particular cause.
