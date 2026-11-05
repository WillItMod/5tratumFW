# NerdQAxe++ Beta14: explicit fitted-fan checks

`5tratumFW-qa-0.1.0-beta.14` corrects the EMC2302 cooling check. It remains an experimental BETA and does not add board-revision support. Existing releases remain unchanged.

## Fixed

Previously, a fault on either fan-controller channel could make both tuning checks fail even when one fan had a valid RPM. The driver now reads the three read-to-clear status registers once per acquisition and evaluates each channel against that same snapshot. Diagnostics distinguish controller faults, unreadable configuration, unsupported RPM mode, unreadable tachometer and invalid rotation.

The tuner defaults to requiring both fans. A separate **Cooling setup** section lets the owner explicitly select both fans, Fan1 only or Fan2 only, acknowledge the cooling requirements and save. The selection never changes fan speeds or controllers. Every selected fan must still pass its strict checks, and all ASIC/VRM, power, error and mining protections remain. A zero RPM reading is never automatically treated as an absent fan.

## Using a single fitted fan

1. Stop tuning and confirm restoration first if a run owns the settings.
2. Open **Tuning → Cooling setup**. Select only the fan actually fitted, using the same Fan1/Fan2 labels as the miner controls. The fitted cooling must cover both ASICs and the voltage regulator.
3. Read the acknowledgement, then select **Save cooling setup** and wait for confirmation.
4. Save the ordinary tuning options separately. At clocks within the standard limit, the advanced clock override is unnecessary; select **Use standard clock limits** and save if it was enabled accidentally.
5. Accept the run waiver and select **Start tuning** once all required checks pass.

Changing the fitted-fan declaration is allowed only while tuning and recovery are inactive. It is stored separately from tuning limits and the immutable original operating-settings backup. Missing legacy declarations default to both fans; invalid declarations block Start until explicitly repaired. Older firmware still requires both fans, so downgrading cannot silently preserve the single-fan exception.

## Installation and qualification

Use the matched **esp-miner-NerdQAxe++.bin** and **www.bin** from this release, or use the existing 5tratumOS BETA updater. Keep power connected until application, interface and settings verification complete. No factory image or replacement NVS is supplied.

[The dated qualification record](../validation-single-fan-20261105.md) records exact software, package and live-test scope. Single-fan fault cases are tested with production-driver/controller fixtures. The available bench has two physically fitted fans; changing its declaration does not prove a physical one-fan installation. Clem's single-fan unit requires its own confirmation.
