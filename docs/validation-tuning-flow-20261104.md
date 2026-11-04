# Tuning Save and Start flow validation, 4 November 2026

This record covers the new public experimental firmware pairs and the clarified options-saving flow. Saving tuning options is separate from accepting the per-run waiver and starting the controller. The physical browser checks below changed only local form drafts; they did not save a 900 MHz clock, start a tune or increase a miner's operating point.

## Exact versions and source

All three packages were built from `747a8fccfa0ef38064753159093f795418ea6b64`:

| Family | Version | Successful GitHub validation |
| --- | --- | --- |
| Bitaxe Gamma PCB601/602 | `5tratumFW-0.1.0-beta.15` | [Gamma run 41872756715](https://github.com/WillItMod/5tratumFW/actions/runs/41872756715) |
| NerdQAxe++ / NERDQAXEPLUS2 | `5tratumFW-qa-0.1.0-beta.12` | [QAxe run 41872756009](https://github.com/WillItMod/5tratumFW/actions/runs/41872756009) |
| NerdOctAxe Gamma / NERDOCTAXEGAMMA | `5tratumFW-oct-0.1.0-beta.10` | [OctAxe run 41872756465](https://github.com/WillItMod/5tratumFW/actions/runs/41872756465) |

Gamma retains ESP-Miner v2.15.4 and its embedded matching web interface. QAxe and OctAxe retain shufps v1.1.1-rc1 and require their matching application/WWW files. Each package's manifest, checksums and corresponding-source archive identify its exact files. Historical release assets remain unchanged.

The mining/tuning controller behaviour is unchanged from the preceding releases: Gamma Beta14 source `684a731d38f626e14d35f66fa1fe1c447a97b275`, and QAxe Beta11/OctAxe Beta9 source `cdf291c6bd6a3240de1c13250745215624c97beb`. Their separately bounded controller tests remain in [the prior live validation](validation-advanced-clock-live-20261104.md); the new UI revision does not relabel those experiments.

## What the form now explains

1. Choose the objective and permitted clock, voltage, power and temperature bounds.
2. For a draft above the reported standard clock ceiling, read and explicitly tick the **Advanced clock override** warning. This checkbox starts unticked; existing settings never imply consent.
3. Select **Save options** and wait for the saved configuration readback. The ordinary per-run waiver is not required to save options.
4. When ready to run, read and accept that separate waiver, then select **Start tuning**. An advanced Start also requires its own advanced acknowledgement.

A local form edit is not a saved miner setting. Saving options does not start tuning, and accepting a waiver does not itself apply a clock. The 1,200 MHz experimental project ceiling remains a software boundary, not a safe operating rating or promised result. Existing voltage, temperature, power, fan, sensor and fault protections remain authoritative.

## Software and installed-pair checks

All three GitHub workflows above passed for the exact new source. The full UI suites passed 296 Gamma cases and 275 Nerd cases, with 71 targeted flow cases and four mocked browser write-flow checks. Mocked writes are software tests; they do not show that a 900 MHz setting was written to hardware.

The new application/WWW pairs were installed and verified on the owned Gamma 601, NerdQAxe++ and NerdOctAxe Gamma. The readback checked the reported application/web pair, original operating settings, profiles, schedules and tuner preferences, plus all 11 served assets on Gamma and 108 on each Nerd. This was a preservation/readback check, not a new higher-clock or tuning experiment. Gamma 602 was not included in these installed-pair checks.

The user was actively editing the QAxe maximum to 750 MHz. That updated user preference was captured as the current pre-update baseline and preserved; it was not overwritten with earlier test bounds or a factory default. Preserving the saved 750 MHz ceiling does not establish that the miner was run at 750 MHz, nor does it acknowledge the advanced-clock warning on the user's behalf.

## Actual browser checks used GET requests only

The real firmware Tuning pages were opened against the three installed miners. A local draft maximum of 900 MHz was entered in each page. Save was blocked before the local advanced warning was acknowledged. After that checkbox was explicitly ticked, Save became enabled while the ordinary run waiver remained false. **Save was not clicked and Start was not invoked.**

The receipts cover Gamma and QAxe at 1440px width and OctAxe at 1440px and 390px width. Each view showed the six graphs, J/TH and current-work difficulty information without page errors. In every case:

- Observed requests were GET only; no write request was made.
- The saved configuration was identical before and after the browser check.
- The tuning run identifier stayed at zero; no tuning run started.
- The 900 MHz value remained an unsaved local draft.

This establishes the displayed Save-before-waiver flow and responsive view on the actual served pages. It is not physical proof of a 900 MHz save, advanced overclock, successful tune or efficiency improvement.

## OS instructions and remaining limits

Use the existing MAIN or DEV channel. Firmware discovery and matched installation require 5tratumOS 0.8.50 or later. The recommended OS release is 0.8.52 on MAIN or 0.8.52-dev on DEV when offered; that compatibility update uses verified public release qualifications and live device capabilities for DualMUX controls instead of a fixed latest-firmware version list. This record does not claim that 0.8.52 was already deployed during the firmware browser checks. See [current installation instructions](../DOWNLOADS.md#update-through-5tratumos).

No above-original operating clock/voltage or full advanced-clock hardware range was qualified. Gamma 602 tuner admission/backup/restore, QAxe rev7 hardware, larger 480×320 Nerd screens, W5500 LAN and Ethernet-only OTA remain outside this proof. OctAxe physical observations remain limited to the owned PCB 2.2 assembly and its verified 16 MiB layout. A reported model or compiled profile is not physical identification of another board.

The recommended starting targets remain 60°C ASIC /70°C VRM, or lower where installed protection requires it. Existing saved preferences are retained. The immutable original backup remains independent of the advanced flag, and neither boot nor OTA starts tuning automatically. No full thermal soak, every-fault coverage, guaranteed efficiency/hashrate gain or independent per-chip tuning/routing is claimed.
