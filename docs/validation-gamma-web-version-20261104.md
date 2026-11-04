# Gamma active web-version validation, 4 November 2026

Gamma Beta16 (`5tratumFW-0.1.0-beta.16`) was built from `385ae9e2b71b2b83e9a52d0ae369db701abe0209`. The change reports the version of the interface actually selected by the server, instead of the label `Unified` for embedded operation. Its source, image hashes and optional WWW are recorded in the release manifest. Older Gamma Beta15 and Nerd releases are unchanged.

## Software checks

- Fourteen production-function cases passed under AddressSanitizer and UndefinedBehaviorSanitizer, covering embedded versus custom selection, stale unselected WWW, actual mismatches, absent/compressed/malformed metadata, length and CRLF boundaries, and allocation failures.
- All 27 Gamma host test entry points passed.
- All 300 Gamma frontend tests passed, including 14 Overview cases. Unknown or legacy-label metadata produces an unverified warning; a known unequal custom version retains a genuine mismatch warning.
- The pinned Node 24.14.0  / ESP-IDF 6.0.2 target build, native display fixtures and embedded-asset packaging checks passed. [Source CI 41895691115](https://github.com/WillItMod/5tratumFW/actions/runs/41895691115) also passed.
- Production OS 0.8.50 and 0.8.52 MAIN/DEV parsers accepted the actual Gamma Beta16 package for 601/602 in eight paired-update replays. Corrupt images were rejected, saved settings preserved in the modeled transactions, BETA selected16 over15, and STABLE excluded the beta. These are software checks, not physical 602 tests.

## Application-only Gamma 601 check

The owned Gamma 601 was running the verified public Gamma Beta15 application and separate matching web image. Its current operating settings, profiles, schedules, tuner options and original-backup state were captured privately before the update.

Only `esp-miner.bin` was uploaded. No WWW upload was made. The existing firmware migration selected the new embedded interface; the separate older web image was not replaced. After restart:

- Firmware version, active web version and directly served `/version.txt` all reported `5tratumFW-0.1.0-beta.16`.
- All 11 served embedded assets matched the new package bytes.
- The original 580 MHz / 1,155 mV request, fan policy, profiles, schedules and tuner options were retained. The original backup reported restored; fresh mining and fan telemetry were valid.
- A real browser Overview check showed normal hashing at approximately 1.18 TH/s, no mismatch or unverified warning, and no page error. Every browser request was GET; no settings or tuning run changed.

The uploaded image and running application descriptor were bound to the release proof. This does not claim a full application-image hash was read back from the device. No higher-clock tuning or new hardware-compatibility qualification was performed. The physical test applies to this Gamma 601; Gamma 602 uses the same reporting code but was not flashed for this check.

## Use

For a normal manual Gamma update, use this release's `esp-miner.bin`, wait for restart and reload. The matching separate `www.bin` is optional for deliberate custom-WWW use. The existing OS one-click updater continues to install and verify its paired files. No OS revision or new Nerd firmware is required for this Gamma-specific reporting fix. See [downloads](../DOWNLOADS.md) and [Gamma installation](installation.md).
