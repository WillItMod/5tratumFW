# Dynamic auto-tuning proposal

Status: **original design and qualification requirements, 3 November 2026**.
The subsequent experimental Gamma Beta13, QAxe Beta10 and OctAxe Beta8 releases implement a controller and Tuning page. See the [current guide](autotuning.md) and [exact-source validation](validation-autotune-release-20261103.md). This original design remains a requirements document, not a physical validation record or a claim about earlier published images.

See the [primary-source research and confidence record](dynamic-autotuning-research.md)
for the evidence behind the control strategy and the remaining measurement and
hardware-calibration gaps.

The intended targets are Bitaxe Gamma PCB601/602 / BM1370×1, NerdQAxe++ /
NERDQAXEPLUS2 / BM1370×4 and NerdOctAxe Gamma / NERDOCTAXEGAMMA / BM1370×8.
Each actual PCB, regulator and cooling assembly needs its own qualification.
Implement separate Gamma and Nerd control/telemetry adapters, with
model-specific qualified limits, beneath the same goals and continual-tuning
controls. A shared UI does not make their hardware-control paths interchangeable.

## Requested behaviour

The tuner measures the miner, evaluates its operating requirements and chooses
clock/voltage changes within qualified constraints. It responds to changing
cooling, thermal headroom, errors and electrical load. It does not choose from
three fixed MHz/mV profiles.

Provide a standalone **Tuning** navigation item for the tuner, with status,
priority, continual-tuning controls and separate user-adjustable **ASIC maximum
temperature** and **VRM maximum temperature** fields. Keep the time-based
Scheduler separate. Opening the Tuning page does not enable or start tuning.

One feedback engine has three selectable priorities:

| Priority | Display name | Objective |
| --- | --- | --- |
| L1 | Standard Glazed | Give greater weight to measured efficiency while retaining useful, stable hashrate. |
| L2 | Jam Filled | Balance measured efficiency, stable hashrate and thermal headroom. |
| L3 | Cream Filled, with a cherry on top | Give greater weight to sustained valid hashrate within the same qualified electrical, thermal and error constraints. |

These are objective weights and tradeoffs. They do not provide fixed frequency,
voltage, fan settings or relaxed protection limits. The supported adjustment
envelope is determined separately for the identified hardware, and the user can
choose stricter power and temperature ceilings. The weights and any minimum
useful-hashrate condition need calibration; this proposal assigns no untested
numeric weights or limits.

The interface also has **Continual tuning: On / Off**, alongside an explicit
**Start tuning** action and a separate **Stop now** action:

- **On:** after the initial bounded tuning run, keep monitoring conditions and
  perform further bounded adjustments when evidence justifies them. Allow
  stable periods without writes; continuous evaluation does not require
  continuous clock or voltage changes.
- **Off:** complete one bounded tuning run, select its best stable measured
  result and hold that result. Turning this option off during a run requests
  completion of that run; it does not abruptly restore the starting point.
  Once a result is held, do not start another tuning run automatically.
- **Stop now, a manual setting change or a higher-priority owner:** cancel the
  run promptly and make a safe transition or handoff. This is distinct from
  turning continual tuning off.

Turning continual tuning on while holding a result resumes observation and
re-baselines before making a new adjustment. A change of priority likewise
requires a new evaluation; it must not immediately apply a corresponding preset.
Every mode retains existing cooling and hardware protection.

For an initial implementation, auto-tuning starts disabled. The browser can
close after starting a run: the control loop belongs on the miner, rather than
depending on an OS host, browser polling or cloud connectivity.

## Available measurements and control boundaries

The Nerd runtime already provides the following foundations:

- [HashrateMonitor](../firmware/nerdqaxe/main/tasks/hashrate_monitor_task.cpp)
  reads each ASIC's nonce and hardware-error registers. Its current polling
  cadence is five seconds, with separate reply bursts. A polling interval is
  not a sufficient tuning dwell time or a proof that a reading is valid.
- [ChipHashrateSample](../firmware/nerdqaxe/main/tasks/chip_hashrate_sample.h)
  distinguishes a measured zero from an unavailable sample and carries capture
  time. Error telemetry must provide equivalent availability, freshness and
  reset/generation handling before a tuner consumes it. Use a consistent
  snapshot of all required chips; do not treat missing chips as zero errors.
- The [v2 dashboard](../firmware/nerdqaxe/main/http_server/v2/handler_v2_dashboard.cpp)
  exposes per-chip valid/error hashrate, aggregate rates, frequency, power,
  voltage/current and thermal/fan information. Its presentation values are not
  themselves a safe, atomic controller input contract.
- [Power management](../firmware/nerdqaxe/main/tasks/power_management_task.cpp)
  reads regulator telemetry, runs fan control and handles faults and thermal
  shutdown. It already orders a frequency reduction before lowering VCORE,
  and a voltage increase before an upward clock transition. Preserve its
  protection polling and transition safeguards.
- [Mining control](../firmware/nerdqaxe/main/tasks/mining_control.cpp) provides
  the qualified control owner, work-generation retirement, guarded transitions,
  pause requests and latched-fault behaviour. Auto-tuning must integrate with
  this path rather than writing directly to UART, GPIO or the regulator.

Use valid/error counter deltas from comparable observation windows. Report a
hardware-error fraction from actual counter-derived valid and error work, with
explicit availability, rather than inferring errors from pool rejection counts.
Counter resets, rollover, missing replies and work-generation changes invalidate
affected windows until a new baseline is established.

Pool accepted-share progress can corroborate useful mining over a sufficiently
long observation. Pool outages, difficulty changes and random share arrival make
short-window accepted-share counts unsuitable as the sole objective. Pool
rejections need their own cause classification and do not stand in for ASIC
errors. Temporary lack of jobs must suspend scoring, not trigger an overclock
to compensate for zero hashrate.

Power provenance differs by regulator and firmware family:

- The Nerd TPS53647/TPS53667 path reads regulator input-power and input-current
  telemetry. These measurements exclude anything outside the measured regulator
  path and are not wall power.
- The Nerd [rev7 TPS546 adapter](../firmware/nerdqaxe/main/boards/drivers/rev7/TPS546.cpp)
  currently returns output voltage × output current as `get_pin()`, and derives
  `get_iin()` from that output power divided by input voltage. These values omit
  regulator loss and are not direct input-power/current measurements.
- The Gamma [power path](../main/power/power.c) uses regulator output power plus
  a configured family offset; its TPS546 current is output current. It is also
  an estimate rather than a measured PSU input or wall reading.

Carry source, location, units, successful capture time and measured/estimated
status in the tuner telemetry contract. Efficiency must be labelled accordingly
and use power and valid hashrate from compatible measurement windows. Do not
apply an input-current ceiling to an output-current reading or use an estimate
as a guaranteed input-power limit. Calibrate offsets, resolution and uncertainty
before scoring small changes. Missing or stale power prevents an efficiency
conclusion. A required input-power/current guard needs a qualified measurement
or conservatively validated bound; without one, deny candidates that depend on
unproven electrical headroom.

The [Gamma thermal path](../main/thermal/thermal.c) and
[device configurations](../main/device_config.h) use the EMC2101 external diode
channel for the supported 601/602 ASIC, and TPS546 internal die temperature for
the regulator. The [Gamma power task](../main/tasks/power_management_task.c)
owns its separate pause, recovery and setting transitions. The Nerd counter
polling interval and protection state machine must not be transplanted into it.

The [ASIC frequency path](../firmware/nerdqaxe/components/bm1397/asic.cpp)
currently broadcasts PLL commands to the chain. The exposed voltage request is
board-wide. The first design therefore adjusts the whole miner, using the
weakest chip's reliable telemetry as a constraint. It does not claim independent
per-chip clocks, voltage or pool ownership.

Gamma's BM1370 path reads register `0x8C` as a total-work counter and `0x4C` for
errors, whereas the Nerd path treats `0x90` as valid work and `0x4C` as errors.
The current Gamma error fraction is errors / total; the Nerd fraction is
errors / (valid + errors). Do not add errors to Gamma's denominator or treat a
common dashboard label as proof of identical windows, units or reset behaviour.
Qualify each counter's semantics and conversion to useful work before sharing
the performance objective between adapters.

## Hardware qualification and limits

Bind an eligible controller instance to the exact stored device identity,
supported model/ASIC pair, expected ASIC count, observed regulator path and
qualified hardware profile. A responding regulator, a software model string or
the OctAxe regulator strap alone does not identify the physical PCB.

The [QAxe board](../firmware/nerdqaxe/main/boards/nerdqaxeplus2.cpp) includes both
older regulator and rev7 TPS546 paths. The
[OctAxe board](../firmware/nerdqaxe/main/boards/nerdoctaxegamma.cpp) distinguishes
TPS53647 and TPS53667 configurations. Their existing manual absolute limits are
outer rejection bounds, not an approved auto-tuning envelope or recommended
operating point.

Each qualified profile needs evidence for clock/voltage combinations, permitted
transition sizes, regulator/input power and current ceilings, sensor behaviour,
voltage readback tolerance, fan requirements and thermal headroom. The controller
uses the intersection of those limits, existing protections and any stricter
user constraints. A change of hardware identification invalidates learned data.
Unknown or unqualified hardware remains ineligible for automatic adjustment.

### Independent user temperature ceilings and guidance

Store and validate the two user temperature ceilings separately from objective
weights and from existing manual fan/PID settings. They apply in all three
priorities and in both continual-tuning modes. A lower ceiling is an instruction
to reduce the operating point before that boundary; raising it only enlarges
the search space inside already qualified limits. It cannot disable protection,
raise hardware fault registers, change sensor calibration, force a fan reduction
or make unqualified hardware eligible.

For each required sensor source, the effective limit is the stricter of its
user ceiling, qualified tuning ceiling and existing protection boundary, with a
qualified margin for sensor uncertainty, thermal lag and control delay. The
margin is part of the hardware profile and cannot be zeroed by the user. Where
an existing shutdown limit is lower, it remains authoritative. Reject invalid,
nonfinite or out-of-range limits; zero does not mean disable protection.

Explain what each number controls next to its field:

| Hardware/sensor path | ASIC maximum input | VRM maximum input and additional guards |
| --- | --- | --- |
| Gamma 601/602 | EMC2101 external diode reading, with the existing board-specific calibration. | TPS546 internal die reading; preserve regulator status and hardware faults. |
| NerdQAxe++ older regulator | Existing shared ASIC/board reading, which includes a firmware offset; do not label it a measured hottest-chip junction. | External VRM-area TMP1075 where present, plus separately qualified TPS53647 power-stage telemetry/status guards. |
| NerdQAxe++ rev7 TPS546 | The actual discovered ASIC/board temperature path and its existing calibration. | TPS546 internal die reading; qualify the exact accepted converter/device ID and voltage-domain arrangement. |
| NerdOctAxe Gamma | Hottest valid per-chip TMP451 path when fitted and qualified; otherwise the real shared board/ASIC path. | External VRM-area reading and separately qualified power-stage telemetry/status guards; retain the existing TPS53667 correction where that path applies. |

The UI must show the active sensor location, measured value, capture age and
whether ASIC temperature is shared/inferred. Do not give an identical suggested
range to a VRM board sensor and an internal die sensor merely because both use
°C. If multiple regulator locations are required, retain their own qualified
limits; one cooler sensor must not mask another hot or missing sensor.

Provide profile-specific guidance with source and confidence: the validated
suggested range, the existing protection threshold on that measurement scale,
and why a requested value is rejected. Start guidance from the unit's observed
stable baseline and demonstrated cooling, below its existing protection line.
Use a more conservative limit when cooling or supply capability is uncertain.
Before calibration exists, display **Guidance not yet qualified for this hardware**
and keep automatic adjustment unavailable; do not substitute component absolute
maximum ratings as a recommendation.

The checked Gamma source currently acts on ASIC readings above 75°C or TPS546
readings above 105°C. Those are existing firmware protection thresholds, not
recommended tuning targets or manufacturer-approved ASIC limits. The Nerd
shutdown inputs are the existing independently stored channel limits, so report
the unit's actual values rather than assuming defaults. No change to these
protections is part of this proposal.

TI's TPS53647/TPS53667 TSEN path can represent the hottest connected smart power
stage and saturates at 127.9°C; its default overtemperature response is a separate
hardware mechanism. TPS546 temperature is an internal die measurement. These
facts do not establish a safe miner-level VRM target. A manufacturer-qualified
BM1370 junction-temperature/voltage operating envelope has not been established
by this research, and must not be inferred from a regulator or sensor rating.

Per-chip temperatures are unavailable on some assemblies. Use their genuine
shared board and regulator temperatures without inventing per-chip readings.
Where qualified per-chip sensors are present, include the hottest valid reading
and its freshness in the constraints.

There is no general ambient-temperature sensor in this contract. Estimate a
change in cooling effectiveness from measured temperature trends, fan response,
power and hashrate under comparable load. Label this as an inference. Do not
display a calculated ambient temperature as a measurement or diagnose a specific
cooling fault from this inference alone. An optional real ambient sensor would
require a separate sensor contract and qualification.

### Measurement uncertainty and remaining research gaps

The TMP1075 and TMP451 data sheets describe 0.0625°C resolution; the EMC2101
external channel has 0.125°C resolution. Those step sizes are not accuracy
guarantees for a miner's junction temperature. Existing board offsets, diode
ideality settings, remote-diode correction, sensor placement and thermal lag
remain part of the profile's uncertainty budget. In particular, the Nerd TMP451
path applies an existing software scale/offset; a raw sensor data-sheet accuracy
claim cannot be applied to the final reading without checking that calibration.

TPS546D24A's internal temperature specification is ±3°C, with 0.25°C sensor
resolution. The current Gamma getter converts to an integer, so the tuner must
describe the actual delivered precision. Voltage/current telemetry also has
specified measurement uncertainty; displaying many digits does not establish
equivalent accuracy. Evaluate improvements against that uncertainty instead of
rewarding insignificant numerical changes.

Some existing getters return a previous good value or zero after communication
failure, and do not carry the successful acquisition age. The Gamma EMC2101 path
also logs open/short diode codes before converting them to a number. A safe
tuner adapter must distinguish successful acquisition, age, saturation and
explicit sensor fault codes from plausible numeric data. Repeatedly reading a
cache cannot refresh the underlying sensor. Reject invalid observations and
apply the qualified conservative stop policy without using them to authorize
more clock or voltage.

Current input-current display ranges, converter output-current capability and
programmed overcurrent fault limits have different meanings. Eligibility must
also account for the actual PSU, connectors, cable losses, PCB power stage and
cooling; a converter's component rating alone does not qualify the assembled
miner. A firmware probe or a schematic from another revision cannot fill these
gaps. Component-level electrical/sensor facts are high-confidence references;
safe auto-tuning envelopes, suggested temperature ranges and control timings
remain unqualified until measured on each intended assembly.

## Feedback loop

Use an explicit state machine: **Disabled → Qualifying → Baseline → Evaluating →
Applying → Settling → Measuring → Holding / Observing**, with **Suspended** and
**Faulted** exits. All hardware transitions pass through the existing protected
owner. Every state exposes its reason, active request, last confirmed result,
measurement age, confidence and whether another tuning run is allowed.

1. **Qualify and baseline.** Verify identity, hardware, initialized workers,
   normal protection status, suitable jobs and complete fresh telemetry. Record
   the user's durable settings and actual starting state separately. Measure a
   stable baseline before describing it as known stable.
2. **Evaluate.** Compare the current point against the selected objective and
   all constraints. Invalidate data collected during faults, pause/resume,
   transport problems, job starvation or a changed generation. Track both
   aggregate useful work and the worst reliable chip error behaviour.
3. **Choose a bounded candidate.** Use the local measured response to select a
   nearby permissible adjustment. Distinguish a clock experiment from a voltage
   experiment; do not change unrelated fan, pool, version-rolling, display or
   schedule settings. Never expand limits because the requested objective is L3.
   If errors rise at a given clock, a voltage increase is only a candidate when
   the qualified electrical envelope, both thermal ceilings, input supply and
   regulator headroom permit it. Otherwise reduce the clock or reject the point.
   An error increase never grants permission to exceed a temperature limit.
   More voltage can increase power and heat; use measured response rather than
   assuming it always improves efficiency or repairs every error source.
4. **Apply and settle.** Follow the existing voltage/clock ordering, guarded
   ramps and cancellation checks. Verify the completed transition and relevant
   readback. Retire pre-transition work/counter windows. Continue polling
   protection during settling; never score a transition as a steady state.
5. **Measure.** Collect a complete qualified window. Allow uncertainty for
   counter quantization, thermal lag and sensor noise. Extend observation or
   decline the comparison if the evidence is insufficient.
6. **Accept or reject.** Retain a candidate only when it meets stability and
   safety constraints and improves the objective by more than the uncertainty
   and hysteresis margin. Reject a candidate showing excessive error growth,
   lost chip responses, insufficient useful work or a constraint violation.
7. **Hold or continue.** With continual tuning Off, finish the bounded run and
   hold the best stable measured result. With On, monitor and re-evaluate after
   a qualified condition change or bounded reassessment interval. Warming
   conditions can justify backing off; improved headroom can permit a new
   constrained experiment. A stable optimal point can remain unchanged.

Limits on adjustment rate, total experiments, elapsed run time and unsuccessful
attempts prevent endless searching. Minimum dwell, stabilization criteria,
measurement length, hysteresis, error tolerances, thermal slope constraints and
cooldown need calibration for each hardware profile. No universal numeric
threshold or timing in this proposal is considered validated.

Adaptive decisions must use monotonic time. SNTP/DST changes do not accelerate
experiments or invalidate elapsed-time accounting. Bound memory and history;
retain enough decision evidence to explain a move without logging credentials.

## Temporary settings, ownership and persistence

The existing [profile Apply handler](../firmware/nerdqaxe/main/http_server/handler_profiles.cpp)
commits a configuration patch before loading it into the board. It is suitable
for explicit saved requests; repeatedly calling it for a tuning sweep would
persist experiments and make an unproven point a boot setting.

Implement a separate volatile operating-point request beneath configuration
policy and above the protected hardware owner. It must report **stored user
settings**, **active tuning request**, **last hardware-confirmed request** and
**measured readback** distinctly. Configuration refreshes must not silently
reapply a stale saved value over a live experiment. No candidate or every-cycle
score is written to NVS.

Holding the best result is also volatile. With continual tuning Off, that point
is retained until a deliberate control handoff, restart or interruption; thermal
protection remains active. An optional explicit **Save current result** may
persist an already qualified result, but saving is not required for adaptive
operation and does not turn the three priorities into presets. Explain this
restart behaviour in the UI.

Use a bounded tuning ownership lease with generation/revocation checks, not a
long-held hardware mutex. A missing/failed controller cannot leave ordinary
control permanently blocked. The owner renews its lease while healthy; hardware
and protection calls remain short and cancellable.

Ownership precedence is hardware protection/shutdown, manual or scheduled pause,
explicit manual control/OTA, scheduled tuning-profile ownership, then automatic
tuning. Pool changes do not by themselves grant tuning ownership, but job
continuity must be re-established before scoring resumes.

- A manual operating-point change revokes tuning before the normal write path
  applies it. Do not restore an old baseline over the user's new request.
- A due [tuning schedule](../firmware/nerdqaxe/main/http_server/tuning_schedule_runtime.cpp)
  revokes the run and takes ownership through the existing scheduler. Cancelled
  auto-tuning must not fight it. Automatic resumption needs an explicit policy;
  the initial design stays suspended until the user restarts tuning.
- A manual/scheduled pause stops experiments and leaves the power controller in
  charge. The tuner cannot resume mining. After an allowed resume it re-baselines
  before scoring or adjusting; turning continual tuning Off still prevents a
  new automatic run.
- OTA/restart revokes tuning and prevents further transient writes. Boot uses
  the durable user configuration rather than the last experimental point. The
  initial implementation requires an explicit new Start after an interrupted
  run and does not automatically replay volatile learned candidates.
- Changing enabled state, goals or limits uses the existing authenticated,
  bounded settings policy. Reads and browser reconnections never start a run.

## Faults, stopping and restoration

Freshness failure, a missing chip, invalid measurements, a fan fault, an input
supply/regulator fault or a protection trip ends candidate acceptance. Stop
further optimization immediately. Use the qualified fault policy to suspend
adjustments, back off or request an ASIC pause. Cooling and normal protection
remain active independently of the tuner.

On Stop now or a recoverable rejected candidate, restore a previously verified
point only while the tuner still owns the request and the protected control path
confirms that restoration is safe. If a new manual/scheduled owner has taken
over, hand off without overwriting it. If the baseline is no longer safe under
the present conditions, do not restore it merely because it was safe earlier.

A latched thermal/electrical fault, failed guarded transition or required manual
restart remains latched. Never clear the fault, restore voltage, restart the
chain or resume mining automatically to finish a tuning run. Report the failure
and leave recovery to the existing explicit control policy. Repeated failures
cannot repeatedly power-cycle a miner or restart the same search indefinitely.

## Acceptance gates

Before any physical experiment, automated host tests must exercise production
controller and ownership code with deterministic fake time and hardware edges:

- All priorities use the same protection limits and produce decisions from
  measured conditions, not a priority-to-MHz/mV table.
- Continual On can reduce or increase a candidate within bounds when the
  simulated environment changes; a stable scenario causes no unnecessary writes.
  Continual Off completes one bounded run and holds its verified result. Switching
  it Off mid-run finishes that run, whereas Stop now revokes promptly.
- Startup/uninitialized/stale/partial samples, genuine zero errors, rollover,
  resets, nonfinite values, time discontinuities, power transitions and varying
  job availability cannot yield a falsely stable score.
- No candidate escapes the qualified hardware/user envelope. Invalid identity,
  wrong ASIC count, unsupported regulator profile and missing required sensors
  deny automatic writes.
- Simulated heating, cooling, thermal lag, fan failure, excessive errors, supply
  droop, UART failures and delayed responses exercise rejection, safe stopping
  and bounded retries. Faults remain latched where required.
- Ownership tests interleave manual changes, due schedules, pause/resume, OTA,
  lease expiry and controller failure. A revoked generation cannot write again,
  restore over a new owner or clear a protection fault.
- Only explicit permitted persistence changes durable settings. Interrupted
  experiments boot at the original durable configuration. NVS failure leaves
  existing durable settings intact.
- Authentication, bounded input/output, status semantics and UI state survive
  disconnect/reconnect without a duplicate Start or write retry. Existing
  settings, fans, pools, profiles and schedules pass preservation checks.
- Independent ASIC/VRM ceilings are validated against the correct source and
  immutable profile limits. Raising either ceiling cannot weaken protection,
  bypass a missing sensor, change hardware fault registers or admit a candidate
  with inadequate voltage/current/power headroom. All three priorities and both
  continual modes obey these constraints.

These tests are necessary but do not qualify electrical or thermal behaviour.
Before enabling adjustment on real hardware, calibrate each intended
PCB/regulator/cooling profile using supervised, bounded tests beginning at that
unit's existing stable operating point. Determine and record measurement noise,
sensor delay, valid-counter/error response, PLL quantization, voltage readback,
safe transition policy and settling/measurement criteria. Use the actual power
supply and cooling assembly; one unit does not qualify another revision.

Physical acceptance must then demonstrate reproducible, sustained improvements
against a comparable baseline; correct response to controlled cooling/load
changes; stable continual operation without oscillation; correct one-run hold;
safe stop/manual/schedule/OTA handoffs; and all intended fault/restart behaviour.
Define numeric limits and pass/fail criteria from calibration before those
acceptance runs. Preserve raw measurements and exact source/image identities.
Do not describe an untested safeguard or simulator outcome as a hardware pass.

## Primary references

- [Nerd upstream RC1 ASIC frequency and counter path](https://github.com/shufps/ESP-Miner-NerdQAxePlus/blob/v1.1.1-rc1/components/bm1397/asic.cpp)
- [Nerd upstream RC1 QAxe board](https://github.com/shufps/ESP-Miner-NerdQAxePlus/blob/v1.1.1-rc1/main/boards/nerdqaxeplus2.cpp)
- [Nerd upstream RC1 OctAxe regulator and sensor paths](https://github.com/shufps/ESP-Miner-NerdQAxePlus/blob/v1.1.1-rc1/main/boards/nerdoctaxegamma.cpp)
- [Nerd upstream RC1 power-management path](https://github.com/shufps/ESP-Miner-NerdQAxePlus/blob/v1.1.1-rc1/main/tasks/power_management_task.cpp)
- [Separate Gamma upstream 2.15.4 power-management path](https://github.com/bitaxeorg/ESP-Miner/blob/v2.15.4/main/tasks/power_management_task.c)
- [Gamma upstream 2.15.4 device and sensor configurations](https://github.com/bitaxeorg/ESP-Miner/blob/v2.15.4/main/device_config.h)
- [Gamma upstream 2.15.4 output-derived power estimate](https://github.com/bitaxeorg/ESP-Miner/blob/v2.15.4/main/power/power.c)
- [Official QAxe hardware repository and revision scope](https://github.com/shufps/qaxe)
- [Official OctAxe hardware repository](https://github.com/Patsch91/NerdOCTAXE-Gamma)
- [TI TPS53647 data sheet, especially TSEN and overtemperature protection](https://www.ti.com/lit/ds/symlink/tps53647.pdf)
- [TI TPS53667 data sheet, especially TSEN and overtemperature protection](https://www.ti.com/lit/ds/symlink/tps53667.pdf)
- [TI TPS546D24A data sheet, temperature and electrical measurement specifications](https://www.ti.com/lit/ds/symlink/tps546d24a.pdf)
- [TI TMP1075 data sheet, resolution and measurement accuracy](https://www.ti.com/lit/ds/symlink/tmp1075.pdf)
- [TI TMP451 data sheet, remote-diode calibration and measurement accuracy](https://www.ti.com/lit/ds/symlink/tmp451.pdf)
- [Microchip EMC2101 data sheet, external-diode channel and fault handling](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/EMC2101-Data-Sheet-DS20006703.pdf)

These are reference interfaces and hardware constraints. The proposed feedback
engine is original work; this document does not import an external tuner or
change existing third-party licensing and attribution.
