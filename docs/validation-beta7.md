# Gamma Beta7 validation

Paired version: **`5tratumFW-0.1.0-beta.7`**, BETA tag **`v0.1.0-beta.7`**. Exact compiled source: **`3ffbe84f43e265e61d5bae3eca1cd985def2bb3f`**.

Gamma PCB 601/602 / BM1370×1 / ESP32-S3 only. Each revision needs its own installed-device record.

**The exact pair completed separate supervised OTA and short installed-device checks on Gamma 601 and 602 on 8 October 2026.** Build and fixture evidence remains separate from those physical observations.

## Change and retained behavior

Primary and Secondary connection editors now appear above the ten named pool slots. Save pool settings saves that connection. Save to slot captures its already-saved configuration into the selected named slot; unsaved edits must be saved first. Configured slots have explicit Apply to Primary and Apply to Secondary actions. Rename changes slot metadata; selecting a slot, entering the page or changing its name does not silently apply a pool. Dirty forms and destination-specific requests are covered by tests.

The update retains the existing clocks, voltage, cooling, network, credential-handling, power controls, schedules and hardware protection. It adds no automatic tuning or new preset. Ambiguous or invalid acknowledgments are unconfirmed and are never retried automatically.

The existing Gamma profile API response/error contract is retained. Destination-specific Apply and unchanged settings are checked in mocked production UI tests; acknowledgment does not independently prove physical pool or hardware state.

## Matched package

| Asset | Bytes | SHA256 |
| --- | ---: | --- |
| `esp-miner.bin` | 1,717,760 | `2d53083782742cd241211ebc4b4ed07f901fb99fd883f210cf9f73b3bd06d8c9` |
| `www.bin` | 3,145,728 | `e88b600a210c513c7bd6e819987f5762016edb8510eda911a393858fb0dbf7a9` |
| `5tratumFW-0.1.0-beta.7-source.zip` | 18,210,653 | `ae427dd88971021a2704847a0a0ea5a9e7ff0c03005c226565373c5c793fdb67` |

Use the application and WWW from this one family/version, with its manifest, SHA256SUMS and corresponding GPL source. These are OTA partition images, with no factory or NVS image. The common WWW filename does not make the three images interchangeable.

## Build and offline evidence

The pinned full Gamma build passed 140 frontend tests, nine host entry points and 27 native OLED fixture views, with the production-C/ASan/UBSan harnesses completed. The app descriptor identifies ESP32-S3 and ESP-IDF v5.5.3; the segment checksum and appended SHA256 were verified. WWW recreation matched byte-for-byte. The 621,837-byte web payload passed the enforced 876,628-byte budget within its 3 MiB partition. Source archive checks covered 1,989 safe members and 1,988 inventory hashes.

[Full family CI passed](https://github.com/WillItMod/5tratumFW/actions/runs/37792363778) at exact source `3ffbe84f43e265e61d5bae3eca1cd985def2bb3f`. It builds/tests/packages; it does not flash a device. Node 24.14.0 and pinned ESP-IDF 5.5.3 were used. Manifest/source/asset checksums matched the actual files; downloaded draft release assets matched the six staged package files byte-for-byte.

Production-bundle layout fixtures cover 320, 390, 768 and 1440 pixel widths, connection editors above ten named slots and explicit destination actions. All API traffic in these fixtures was mocked, with no actual miner requests. The final acknowledgment/version changes were checked by focused/full source tests; unchanged-layout screenshots predate those final response checks. Display fixtures do not establish physical-screen readability.

## Supervised installed-device verification — 8 October 2026

[Beta6](validation-beta6.md) remains the historical short bench for the earlier 601 and 602 pairs. The following observations apply to this exact Beta7 pair, separately on each owned unit.

| Unit scope | Matching app/web and served assets | Exported settings, slots and schedules | Retained operating point | Short mining/sensor observation |
| --- | --- | --- | --- | --- |
| Gamma PCB 601 | Exact Beta7 app/web versions; all 11 served web records matched | All exported settings, 10 pool slots, 10 tuning slots and both schedules retained; manual override none | 580 MHz / 1155 mV | Accepted shares increased by one; ASIC board 59.875°C, VR 48°C |
| Gamma PCB 602 | Separate exact Beta7 app/web verification; all 11 served web records matched | All exported settings, 10 pool slots, 10 tuning slots and both schedules retained; manual override none | 525 MHz / 1150 mV | Accepted shares increased by one; ASIC board 59.875°C, VR 55°C |

Each supervised update reached the completed paired-version/settings verification phase. App and WWW reported this version, all 11 served asset records matched the built package, and the identity-bound update helper checked versions and exported settings after each upload. An upload acknowledgment alone was not treated as success. Package provenance is tied to clean source `3ffbe84f43e265e61d5bae3eca1cd985def2bb3f`, the manifest, source inventory and recreated WWW; Gamma's served build information does not independently advertise or attest that source commit.

The complete exported settings, both sets of ten named slots, power schedule with manual override `none`, and pool schedule matched each unit's fresh pre-update baseline. The retained clocks and voltages above are observations of those units, not recommended presets. Both primary and fallback pool configurations were retained; this Gamma family does not expose Nerd native Dual A/B sessions.

The accepted-share increases and reported sensor readings are short same-boot observations. They do not establish an upstream payout, a new power-window boundary, thermal-fault behavior, sustained soak or physical OLED readability.

Exported API settings comparisons are not a raw NVS or unexported-password-byte backup. Retain private unit-specific recovery material separately; public validation contains no network addresses, device IDs or credentials.

The immutable package `hardware_tested: false` is a pre-bench packaging fact. Any later physical qualification belongs in this dated validation record; it does not rewrite that historical audit flag.

## Limits

This update does not qualify sustained operation, all protection faults, complete recovery or physical display readability. Preserve the unit's own clock/voltage and protection behavior; values observed on another unit are not presets. Gamma uses Primary/fallback pool routing, not Nerd native Dual A/B sessions. There is no independent physical-chip work assignment or per-chip coin-routing claim. Central OS/MUX admission and its installed runtime are qualified separately from firmware packaging.
