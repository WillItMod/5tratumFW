# OctAxe Beta3 validation

Paired version: **`5tratumFW-oct-0.1.0-beta.3`**, BETA tag **`octaxe-v0.1.0-beta.3`**. Exact compiled source: **`3ffbe84f43e265e61d5bae3eca1cd985def2bb3f`**.

NerdOCTAXE-γ / NERDOCTAXEGAMMA / BM1370×8 / ESP32-S3 only. Physical evidence is limited to the owned PCB 2.2 assembly; other revisions remain unqualified.

**The exact pair completed the supervised OTA and short installed-device checks below on 8 October 2026.** Build and fixture evidence remains separate from those physical observations.

## Change and retained behavior

Primary and Secondary connection editors now appear above the ten named pool slots. Save pool settings saves that connection. Save to slot captures its already-saved configuration into the selected named slot; unsaved edits must be saved first. Configured slots have explicit Apply to Primary and Apply to Secondary actions. Rename changes slot metadata; selecting a slot, entering the page or changing its name does not silently apply a pool. Dirty forms and destination-specific requests are covered by tests.

The update retains the existing clocks, voltage, cooling, network, credential-handling, power controls, schedules and hardware protection. It adds no automatic tuning or new preset. Ambiguous or invalid acknowledgments are unconfirmed and are never retried automatically.

Nerd profile responses require `ok: true`, a boolean `restartRequired` and profile/settings readback before the UI confirms a save or Apply. This verifies saved state; it does not establish that a new hardware operating state is already applied.

## Matched package

| Asset | Bytes | SHA256 |
| --- | ---: | --- |
| `esp-miner-NerdOCTAXE-Gamma.bin` | 3,404,592 | `61a7ac96f579359fd377e084a91af8e6826c416207541e20921026f8874ad07d` |
| `www.bin` | 3,145,728 | `7532f78cc9dde8aeb574ee666ba29551dfa6712300a1f15c464e3c3406def418` |
| `5tratumFW-NerdOCTAXE-Gamma-5tratumFW-oct-0.1.0-beta.3-source.zip` | 14,677,666 | `fc98ba04ebd1a6e86daef57393646be98a24663ea2571592fbf885effb7ff469` |

Use the application and WWW from this one family/version, with its manifest, SHA256SUMS and corresponding GPL source. These are OTA partition images, with no factory or NVS image. The common WWW filename does not make the three images interchangeable.

## Build and offline evidence

The pinned full OctAxe build passed 103 frontend tests, 20 browser control checks and all 22 host scripts, including 135 Python cases across 18 suites and the production driver/control harnesses. Fifty-four native display fixtures use simulated readings. The ESP32-S3 descriptor, segment checksum and appended SHA256 were verified, with diagnostic driver, passive capture and capture logging off. WWW recreation matched byte-for-byte. All 94 web records and 1,232 source-archive entries passed their inventory/hash/build-configuration checks. Total web payload: 1,683,234 bytes within the 3 MiB WWW partition.

[Full family CI passed](https://github.com/WillItMod/5tratumFW/actions/runs/37792363720) at exact source `3ffbe84f43e265e61d5bae3eca1cd985def2bb3f`. It builds/tests/packages; it does not flash a device. Node 24.14.0 and pinned ESP-IDF 5.5.3 were used. Manifest/source/asset checksums matched the actual files; downloaded draft release assets matched the six staged package files byte-for-byte.

Production-bundle layout fixtures cover 320, 390, 768 and 1440 pixel widths, connection editors above ten named slots and explicit destination actions. All API traffic in these fixtures was mocked, with no actual miner requests. The final acknowledgment/version changes were checked by focused/full source tests; unchanged-layout screenshots predate those final response checks. Display fixtures do not establish physical-screen readability.

## Supervised installed-device verification — 8 October 2026

[Beta1](validation-oct-beta1.md) and [Beta2](validation-oct-beta2.md) retain their historical scope. The following checks apply to this exact new pair on the same owned unit.

| Unit scope | Matching app/web and served assets | Exported settings, slots and schedules | Fresh rates and accepted shares | Fresh pool/MUX context |
| --- | --- | --- | --- | --- |
| Owned OctAxe Gamma, PCB 2.2 | Exact paired versions; all 94 web assets and source-bound build info matched | All exported settings, 10 pool slots, 10 tuning slots, both schedules, manual override and integrations retained | All eight hardware counters fresh; accepted shares increased by 10 | Both A/B feeds fresh; running Dual pool, 50/50 job allocation |

The supervised update reached the completed paired-version/settings verification phase. App and WWW matched this version, all 94 served compressed/raw asset records matched the built package, and served build information matched source `3ffbe84f43e265e61d5bae3eca1cd985def2bb3f`. Each upload's acknowledgment was followed by bound version/settings/asset inspection; an acknowledgment alone was not treated as success.

Every exported saved setting, both sets of ten named slots, the pool schedule, power schedule with manual override `none`, and integration settings matched the fresh pre-update baseline. The unit retained 700 MHz / 1180 mV and its saved Dual pool 50/50 allocation. Those values describe this unit's retained operating point, not recommended presets.

All eight ASIC counter estimates were fresh, accepted shares increased by 10 during the short same-boot observation, and both A/B streams had fresh MUX coin and forwarded job context. Both currently reported 5TRAT; route coins can change with MUX routing. This is session/context evidence, not proof that a specific physical ASIC belongs to either stream. Individual chip temperatures remained unavailable; genuine shared board/regulator readings were checked separately.

These are short supervised update and preservation observations. No new power-window boundary, thermal fault, sustained soak or physical OLED-readability qualification was performed.

Exported API settings comparisons are not a raw NVS or unexported-password-byte backup. Retain private unit-specific recovery material separately; public validation contains no network addresses, device IDs or credentials.

The immutable package values `hardwareTested: false` and `physicalLayoutVerified: false` are pre-bench packaging facts. Any later physical qualification belongs in this dated validation record; it does not rewrite that historical audit flag.

## Limits

This update does not qualify sustained operation, all protection faults, complete recovery or physical display readability. Preserve the unit's own clock/voltage and protection behavior; values observed on another unit are not presets. Native A/B sessions share one ASIC chain. There is no independent physical-chip work assignment or per-chip coin-routing claim. Central OS/MUX admission and its installed runtime are qualified separately from firmware packaging.
