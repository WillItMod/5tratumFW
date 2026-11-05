# Fitted-fan qualification, 5 November 2026

## Source and release scope

NerdQAxe++ **Beta14** (`qaxe-v0.1.0-beta.14`) and NerdOctAxe Gamma **Beta12** (`octaxe-v0.1.0-beta.12`) were built from clean source `012488d6078f914cc68a3dd3594374e8e07aa507` using ESP-IDF 5.5.3 and Node 24.14.0. Gamma remains Beta17 and uses a different fan driver; no new Gamma package is included.

The EMC2302 status registers are read-to-clear and carry per-channel fault bits. The driver captures all three once per acquisition, then evaluates both channels against the same snapshot. Logical Fan1 uses hardware FAN2/M2; logical Fan2 uses FAN1/M1. The change does not write fan speed or output configuration.

Fitted-fan selection is explicit, separately stored and bound to the existing device identity. Missing legacy settings default to both fans; invalid or unreadable stored declarations block Start until explicitly repaired. Selected fan channels must pass controller, configuration, tachometer and RPM checks. A zero-RPM channel is never automatically classified as absent. Cooling configuration cannot change during tuning, recovery or an OTA update. Its saved declaration remains separate from the immutable operating-settings backup.

## Software and package checks

- All 37 Nerd host-test entrypoints passed, including production sensor/controller code with AddressSanitizer and UndefinedBehaviorSanitizer. The updated driver and runtime also passed their Linux/GCC checks.
- Read-to-clear tests cover simultaneous sibling faults, the logical/physical channel mapping, disconnected or stalled fans, I2C errors and invalid tachometer/configuration data. Runtime tests cover both four-chip and eight-chip boards, either single required fan, both required fans, explicit acknowledgement, invalid masks, missing/corrupt/identity-mismatched records, failed persistence, active-session rejection and restoration/recovery.
- All 297 frontend tests passed. Real production-browser checks covered QAxe and OctAxe at desktop and mobile widths, saving cooling separately from run consent, verified readback, invalid/missing metadata and retained tuning options. Stable radio-choice identities prevent repeated Angular control reconstruction.
- Both actual target applications compiled. Each matching 3 MiB WWW image was reconstructed and verified against all 108 served records. Matching manifests, checksums, corresponding source archives, native display fixtures and certificate-bundle checks passed. Previous released artifacts were not changed.
- The retained public 5tratumOS 0.8.50 and 0.8.52 MAIN/DEV code accepted both exact packages in eight modeled paired upgrades, selected the new BETA releases and excluded them from STABLE. Source-bound native-control sidecars passed positive and wrong-source/tag/manifest/count checks. These are software/transport simulations, not live OS installation claims.

## Owned QAxe check

Only the owned four-chip QAxe was available for this release. Its actual PCB revision remains unidentified. The app and all 108 matching web records were verified after installing Beta14. Accessible settings, saved profiles, pool/power/tuning schedules, security state, original clock/voltage and immutable-backup metadata matched the before-update capture.

The unit has **two physical fans**. The test explicitly saved Fan1-only without changing either physical fan output, verified the new declaration, and started a bounded run at the existing **600 MHz / 1,150 mV**. Frequency and voltage search bounds were fixed at that same point; original thermal and power ceilings were retained. No overclocking or undervolting trial was performed.

The controller started run 1 and completed a full baseline measurement of approximately **4.90 TH/s** and **78.8 W**. It recorded the measured-reference event and held the same operating point. Selected fan feedback remained valid. Stop then restored the original operating point, released control and confirmed the original backup restored. Original tuning options were restored separately and both-fan policy was reinstated. A final read-only audit passed all 15 checks with no differences, mining active, no fault and no pending recovery.

Six read-only browser cases against the actual QAxe page also passed at desktop and mobile widths. They issued no state-changing requests and left the configuration, cooling declaration, backup and run ID unchanged.

This does **not** qualify a physically single-fan assembly, Clem's Rev 5.1 unit, a new board revision, OctAxe hardware installation, high-clock tuning, cold power-cycles, sustained thermal stability or larger displays. Single-fan faults are covered by production-code simulations; the owner of the actual single-fan miner must confirm its fitted channel and adequate ASIC/VRM cooling. The owner confirmed that the physical screen and both fans were normal after this installation.

GitHub validation and public asset verification are recorded after publication below. Source, package and bench evidence above is independent of those later checks.
