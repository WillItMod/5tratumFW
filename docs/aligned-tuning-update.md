# Aligned tuning-schedule candidates — 28 October 2026

This update brings the same saved tuning-profile schedule to the three existing
firmware families. Each family retains a separate application/web pair and
hardware qualification. No settings or schedules are installed automatically.

| Family | Candidate | Application | Display scope |
| --- | --- | --- | --- |
| Bitaxe Gamma PCB 601/602 | `5tratumFW-0.1.0-beta.10` | `esp-miner.bin` | Existing 128×32 OLED |
| NerdQAxe++ / NERDQAXEPLUS2 | `5tratumFW-qa-0.1.0-beta.8` | `esp-miner-NerdQAxe++.bin` | Standard 320×170 plus explicit experimental 480×320 RGB/BGR selector |
| NerdOCTAXE Gamma / NERDOCTAXEGAMMA | `5tratumFW-oct-0.1.0-beta.5` | `esp-miner-NerdOCTAXE-Gamma.bin` | Existing 320×170 display |

Each application requires the `www.bin` packaged for that same family/version.
The shared web filename does not make the web images interchangeable. These
candidates do not widen support to other PCB revisions, QAxe+, GT800 or 650 DUO.

## Common behaviour

Scheduler can select ten existing tuning slots at up to sixteen weekly time
points. Daily 00:00 → slot 3, 09:00 → slot 1 and 21:00 → slot 2 uses the most
recent enabled point until the next one, including across midnight/week wrap.
The miner needs synchronized network time and an explicit fixed UTC offset;
this schedule does not automatically follow daylight saving time. Reads and
schedule storage do not directly activate tuning. Runtime activation preserves
manual/scheduled pauses and hardware protection and uses the normal saved
clock/voltage path. The separate pool and pause schedules retain their roles.

See the [saved tuning schedule](../firmware/nerdqaxe/docs/tuning-profile-schedule.md)
for the shared API shape and point semantics. Gamma retains its own backend,
security flow and native web controls; it does not use the Nerd image or UI.

## QAxe larger screen

The selector carries forward unpublished Beta7 work and addresses viewport
size, the standard panel's offset and horizontal mirroring together. The
28 October [online firmware reference check](../firmware/nerdqaxe/docs/experimental-480-display.md#online-firmware-reference-check--28-october-2026)
records pinned open-source implementations and a published 480×320 build.
No other fork's flash driver, partitions or tuning values are imported.

Tadets' larger panel is not available locally. Confirm its physical position,
orientation, colours, startup and refresh before treating a profile as qualified.
The existing standard QAxe was also unreachable during the read-only connection
check on 28 October. Builds and simulated tests do not replace those checks.

## Independent chip work remains unavailable

Gamma retains its one-chip primary/fallback behaviour. Nerd A/B streams continue
to share their ASIC chain. Current primary-source implementations still do not
establish a BM1370 command for independent chip-specific work. No capability
flag is enabled and no guessed command is sent. The [chip-targeting investigation](../firmware/nerdqaxe/docs/asic-work-targeting-investigation.md)
records the source evidence, missing proof and bounded capture tool for a future
hardware investigation. This work is outstanding, not implemented by the
tuning schedule or screen selector.

## Release boundary

The candidates require separate matching build/package receipts and explicit
installed-device validation. Public Beta9/Beta6/Beta4 downloads remain unchanged
until a new release is deliberately promoted. Central OS/MUX exact-release
admission is separate from displaying a native mining connection; it must not
be widened solely because a candidate compiled.
