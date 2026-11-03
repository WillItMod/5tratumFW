# BM1370 dynamic tuning research — 3 November 2026

Read-only investigation for Gamma PCB601/602, NerdQAxe++ and NerdOctAxe Gamma. No device was contacted, no settings were changed, and no third-party tuner implementation was copied. ASIC-error runtime additions were not changed during this research.

## Conclusions and confidence

High confidence: adjustable clock and core voltage are useful controls, but error percentage alone is not a sufficient control signal. A successful voltage increase can improve timing margin for an overclocked/undervolted CMOS circuit; lowering clock gives more time per cycle. Neither is a universal BM1370 cure. Higher voltage increases dynamic power approximately with voltage squared, which consumes cooling/regulator/current headroom. PSU sag, fan/cooling problems, communication loss, a silent ASIC/domain, changed workload and failed sensors must not be treated as requests for more voltage.

The cited general CMOS manufacturer references justify the relationship, not a BM1370 voltage/frequency curve. I found no public Bitmain die-level BM1370 datasheet specifying a safe tuning envelope, voltage/frequency curve, thermal junction target or lifetime guarantee. This is a search result limitation, not proof that no such document exists. The Bitaxe hardware authors explicitly describe the ASIC as proprietary. The Bitmain S21 Pro environmental specifications concern the complete miner/inlet air, not a bare BM1370 junction or a Nerd board's VRM sensor.

High confidence: the current code contains useful control sequencing and hardware protections that a tuner must retain. Moderate confidence: a bounded, slow, single-variable-at-a-time empirical controller can be implemented and simulated. Unresolved physical qualification: tolerances, thermal margins, dwell durations, transient safety, voltage/error response and stability must be measured on each admitted board/regulator assembly. Host tests and target compilation cannot establish these.

## Primary sources and short quotations

1. Renesas/IDT AN-124, 3.3 Volt Logic Characteristics and Applications, printed page 29 / PDF page 5, section Delay as a Function of Supply Voltage:
   https://www.renesas.com/en/document/apn/124-33v-logic-characteristics
   Quote: “Propagation delay is inversely proportional to power supply voltage.”
   Application: supports the general timing-margin explanation. This is a different CMOS logic family; do not reuse its numeric curve for BM1370.

2. Texas Instruments HCMOS Design Considerations, printed page 13 / PDF page 19, Power Dissipation:
   https://www.ti.com/lit/an/scla007a/scla007a.pdf#page=19
   It gives the dynamic-power equation P = Cpd × VCC² × frequency. No BM1370 numeric claim follows from that equation. Real board input power also contains regulator losses, fan/controller load and leakage.

3. Bitaxe Gamma hardware authors:
   https://github.com/bitaxeorg/bitaxeGamma/blob/main/readme.md
   Quote: “You must have active cooling on the bitaxe.”
   The ASIC section identifies BM1370 as proprietary; the status section explicitly connects higher Gamma power and overheating with hash frequency/voltage and stock cooling.

4. Bitmain S21 Pro specification:
   https://support.bitmain.com/hc/en-us/articles/31321354157593-S21-Pro-Specification
   Quote: “Inlet air temperature.”
   Application: its operating-temperature and wall-power/efficiency figures describe the complete stock S21 Pro. They cannot justify a universal bare-chip temperature or voltage maximum for these boards.

5. TI TPS53647 datasheet, sections 7.3.12 and 7.6.2.3.36:
   https://www.ti.com/lit/ds/symlink/tps53647.pdf#page=28
   https://www.ti.com/lit/ds/symlink/tps53647.pdf#page=71
   Quotes: “temperature of the hottest power stage.” / “READ_VOUT command returns the actual, measured output voltage.”
   It documents TSEN/power-stage telemetry, measured output voltage, input/output current and power, current/fault protection and voltage slew. Those capabilities do not identify how an actual board has connected its sensors or establish safe PCB/current/connector limits.

6. TI TPS53667 datasheet, sections 7.3.12 and 7.6.2.3.38:
   https://www.ti.com/lit/ds/symlink/tps53667.pdf#page=30
   https://www.ti.com/lit/ds/symlink/tps53667.pdf#page=74
   Same distinction between controller/power-stage thermal telemetry and the PCB's separately located sensor. Six-phase controller capability is not the rating of a complete assembled OctAxe.

7. TI TPS546D24A datasheet, sections 7.6.19, 7.6.24, 7.6.64 and 7.6.66:
   https://www.ti.com/lit/ds/symlink/tps546d24a.pdf#page=66
   https://www.ti.com/lit/ds/symlink/tps546d24a.pdf#page=73
   https://www.ti.com/lit/ds/symlink/tps546d24a.pdf#page=117
   https://www.ti.com/lit/ds/symlink/tps546d24a.pdf#page=119
   Quote: “The READ_VOUT command returns the actual, measured output voltage.”
   Voltage changes have a programmed transition rate; host command completion is not evidence that the requested voltage has settled. Temperature phase-selection behavior must match the actual hardware.

8. Pinned Nerd upstream RC1 monitor:
   https://github.com/shufps/ESP-Miner-NerdQAxePlus/blob/v1.1.1-rc1/main/tasks/hashrate_monitor_task.cpp
   https://github.com/shufps/ESP-Miner-NerdQAxePlus/blob/develop/docs/API.md
   The monitor reads 0x90 and 0x4C with separated bursts every five seconds; its comment identifies 0x90 as valid work and error rate as lost work. API documentation explicitly says total = hashRate + errorHashRate. These are the firmware authors' reverse-engineered interpretation, not a Bitmain datasheet.

9. Pinned Gamma upstream v2.15.4:
   https://github.com/bitaxeorg/ESP-Miner/blob/v2.15.4/components/asic/bm1370.c
   https://github.com/bitaxeorg/ESP-Miner/blob/v2.15.4/main/tasks/hashrate_monitor_task.c
   https://github.com/bitaxeorg/ESP-Miner/blob/v2.15.4/components/asic/frequency_transition_bmXX.c
   https://github.com/bitaxeorg/ESP-Miner/blob/v2.15.4/components/asic/pll.c
   Gamma maps 0x8C to REGISTER_TOTAL_COUNT, the four domain counters at 0x88–0x8B and 0x4C to errors. It reports error/current-total × 100. Do not call its denominator the same valid-work counter as Nerd 0x90 or substitute formulas without qualification.

## Actual local control paths

Reviewed base public source `f827bd81b6b7c1e49c1d29736851df70c47ca76c`. The current ASIC-error additions modify Nerd monitoring only.

### Nerd QAxe++ and OctAxe Gamma

Paths in this subsection are relative to `firmware/nerdqaxe/`.

- `main/tasks/power_management_task.cpp::applyAsicSettings`: when decreasing voltage, finish the requested frequency change first. In the opposite direction, successful voltage write precedes frequency increase. Preserve this ordering.
- `main/tasks/mining_control_state.h::MiningPowerState::reconfigure`: blocks UART admission, waits for in-flight work, drains and clears transport, retires the work generation, reconfigures, validates readiness/protection, drains again, and only then reopens. Failure requests a controlled stop.
- `main/tasks/mining_control.cpp::Backend::retireWork`: clears old jobs and resets the hash/error monitor. An evaluator must discard every pre-change sample and wait for new bounded counter intervals.
- `components/bm1397/asic.cpp::doFrequencyTransition`: ramps in 6.25 MHz increments with 100 ms protection-aware waits. This is a ramp increment, not proof that every requested setpoint must lie on that grid. The PLL solver permits approximately 2 MHz error and tracks computed actual frequency separately from the request. Current persisted target fields are integer MHz.
- `main/tasks/power_management_task.cpp::checkCoreVoltageChanged`: updates applied voltage after the driver's successful write, without measured readback/settle confirmation at this point. The resume path separately checks measured Vout within 0.03 V of saved voltage. This tolerance is existing code behavior, not a universal adaptive-step tolerance.
- `main/boards/drivers/TPS53647.cpp`: VID conversion uses 5 mV steps and truncation; one reserved reset-detection code is deliberately skipped. `set_vout` confirms PMBus write success; it does not prove measured voltage is settled. Several numeric getters ignore the read status and can return zero-initialized data after a failed transaction. A tuner needs explicit read-success plus sample timestamp and register/status provenance.
- `main/boards/nerdoctaxegamma.cpp`: GPIO3 selects TPS53647 versus TPS53667 profiles but is not an authoritative PCB revision. The latter has a legacy +8°C reported external-temperature correction. That offset requires calibration; it is not a manufacturer rule for all OctAxe boards.
- `main/boards/nerdqaxeplus.cpp::getVRTemp/getVRTempInt`: external board VR sensing and PMBus power-stage sensing are separate channels. A maximum should consider every qualified required VR sensor; silently preferring the cooler sensor would hide a hot spot.
- `main/tasks/temperature_sample.h`: a cache timestamp and one valid field make a display snapshot fresh. That display convenience is insufficient for adaptive control requiring individually successful, current ASIC and VRM readings.
- `main/nvs_config.cpp::cfgSetU16`: marks persistent config dirty for asynchronous flushing; existing HTTP/profile paths are persistent writes. Do not use them as a repeated temporary-target actuator.
- Current constructor MHz/mV/current/power ceilings differ by board/regulator. They are driver/profile validation limits, not measured safe autonomous search bounds. Do not silently extend them.

### Gamma PCB601/602

Paths in this subsection are relative to the repository root.

- `main/tasks/power_management_task.c::apply_requested_settings`: same direction-aware ordering: frequency first before voltage reduction; voltage first before frequency increase. A successful solver return is explicitly documented as computed PLL output, not measured readback.
- `components/asic/frequency_transition_bmXX.c`: 6.25 MHz intermediate ramp and 100 ms delays. `components/asic/pll.c` chooses feasible divisors closest to requested frequency; represent requested and computed frequencies separately.
- `components/asic/bm1370.c::BM1370_send_hash_frequency`: returns calculated PLL frequency. Its low-level send helper reports transport failures via logging and has no end-to-end PLL register confirmation. An adaptive controller must not interpret that returned float as measured successful hardware application.
- `main/power/vcore.c`: delegates to TPS546; Gamma has one voltage domain. `main/power/TPS546.c::TPS546_set_vout` writes command, adjusts protection ratios, enables output and reads OPERATION, but does not confirm measured Vout is settled at target.
- `TPS546_get_vout/get_iout/get_vin/get_temperature` return last cached values on I2C failure. A power-loop timestamp does not make those readings current. Implement strict success/timestamp-bearing readings before using headroom to authorize a voltage or clock increase.
- `ulinear16_2_float` and `float_2_ulinear16` read VOUT_MODE without checking status, leaving an uninitialized local mode after a failed read. Even the existing status snapshot calls these conversions. A strict adaptive path must validate the mode read and propagate failure before converting/writing. Resolution is mode-dependent (e.g. exponent -9 gives 1.953125 mV); do not promise 1 mV precision.
- `main/power/power.c`: reported watts are TPS546 output voltage × output current plus a fixed family offset (Gamma 5 W). This is an estimate of device power, not measured wall watts or a complete PSU input-power measurement.
- Current ASIC 75°C / VR 105°C thresholds in `main/tasks/power_management_task.c` are existing firmware protection behavior, not recommended tuning targets. User-facing adaptive maxima must remain below unchanged protections by a separately justified margin. Hardware fault and overheat paths must remain higher priority than adaptive control.
- Existing error% needs fresh second-derived error-counter samples, correct operating generation and domain/chip liveness before being suitable as a controller input. A silent domain can leave error% low while useful hash rate falls; zero error is not proof of a healthy chip.

## Proposed independent controller design (engineering inference)

1. A dedicated Tuning page should separate manual settings, a guarded adaptive enable, live state/reason, baseline/results and explicit Save result. Show separate user-changeable ASIC and VRM maxima with sensor names and the unchanged hard-stop thresholds. Use a board-qualified suggested range only when calibration exists; otherwise explain that no board-specific recommendation has yet been established.
2. Treat limits as constraints: model/regulator profile, user voltage/frequency bounds, power/current budget, ASIC maximum, every required VRM temperature maximum, sensor freshness, fan health, per-chip errors, hash/domain liveness and permitted mining state. Lower user limits immediately narrow the search. Raising them never changes or disables hardware protections.
3. Keep temporary working targets in RAM under the existing single power owner. Manual edits, schedules, pause, thermal faults, network/work loss, OTA and reboot cancel the session or explicitly suspend it. Do not race a manual/profile/scheduler writer, repeatedly write NVS, save a failed point or auto-resume an unfinished experiment after reboot.
4. Observe the existing saved operating point first. Verify real input/rail telemetry, all required ASIC/error counters and sensors, setpoint/transport readback, active fan health and useful work before searching. An unknown value is not headroom and not zero.
5. Use a slow bounded coordinate search: change one variable, verify the actual quantized command and measured settling, discard pre-transition samples, then evaluate new counter deltas at the new operating point. A high error window may justify lower clock; a small voltage increase is a separate trial only with explicit voltage-boost permission and demonstrable thermal/current/power headroom. Keep the better measured stable point, not the numerically highest clock.
6. Hot ASIC, hot VRM, power limit, drooping supply, missing sensors or failed fan means no voltage increase. Reduce load at the existing safe voltage first, then reduce voltage only after confirmed clock reduction. Persistent hardware faults require the existing stop/fault path. If rollback cannot be verified, stop rather than guess.
7. Evaluate independent fresh counter intervals and enough observation time/counts. Re-reading the same five-second reading in a two-second HTTP loop is not extra evidence. Error resolution depends on total counts; one zero interval cannot prove zero long-run error. Do not import binomial confidence claims unless the actual hardware-event distribution has been justified. Use a minimum settled window plus bounded trend/steady-state checks calibrated on real hardware; never claim a universal 30-second stability proof.
8. Use the worst-chip constraint as well as the weighted board metric. A good board average must not mask one failing chip. Track rate/expected-work ratio and Gamma domain liveness separately from error counters and pool rejects. Rejected shares alone conflate network/pool and hardware causes.
9. Compare efficiency using measured useful hash rate and the correctly labelled power source. Rail watts/TH and estimated device watts/TH cannot be presented as wall J/TH. If optimizing maximum output under a budget, state that objective separately from efficiency.
10. Let fan/PID protection act faster than the optimizer. Add hysteresis, a cooldown dwell and candidate/backoff memory so thermal overshoot does not cause rapid clock/voltage hunting. Require repeated steady evidence before stepping upward; respond downward promptly to constraints.

## Required physical qualification before asserting reliable autonomous tuning

- Exact PCB, regulator, flash/layout and sensor-path identity for each admitted Gamma601/602, QAxe++ and OctAxe assembly; do not generalize the owned Oct2.2 or unidentified QA board to other revisions.
- Confirm command quantization, measured Vout/readback tolerance, load settling, transient droop/overshoot and clock application using appropriate independent measurements. Confirm input power/current estimates against calibrated readings and account for PSU, cable/connector and power-stage limits.
- Compare each external/internal temperature channel to an independent reference under load; validate Oct's sensor correction, hot spots and any absent per-chip temperature path. A nearby PCB sensor is not guaranteed junction temperature.
- Record frequency/voltage/error/useful-hash/power/temperature curves on each unit with changes bounded by an approved starting envelope. Repeat at relevant ambient/cooling conditions and sustain the selected result long enough to expose thermal lag and intermittent faults.
- Verify stop/rollback while voltage/frequency changes fail, UART/I2C/temperature data disappears, a chip/domain goes silent, fans stall, the workload stops, power/schedule/manual settings change, an update starts and the controller reboots. Test hardware protections independently of the adaptive session.
- Validate cancellation/expiry after a stalled control loop or crash, no unwanted persistent changes, no reboot into a candidate point, complete rollback reporting and the exact application/WWW update pair.

## Recommendation

The evidence supports building an original bounded controller and its dedicated page. It does not support advertising a universally safe numeric preset or physically validated autonomous tuning now. Separate the implementation/simulator result from hardware-qualified behavior, and keep any unqualified autonomous actuation clearly experimental and off by default until bench results resolve these gaps.
