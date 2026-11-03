# Adaptive tuning — development candidates

The first candidates are **Bitaxe Gamma 601/602 Beta12**, **NerdQAxe++ Beta9**
and **NerdOctAxe Gamma Beta7**. They are unreleased while validation is in
progress. Use each family's own matched application and web image; no factory
erase or NVS image is part of an update.

The controller runs on the miner. Closing the browser does not stop it. It
adjusts the whole miner's clock and core voltage using current hardware
measurements; it does not independently tune or route individual ASICs.

## Tuning page

Open **Tuning** in the firmware web interface. The Scheduler remains separate.

| Priority | Goal |
| --- | --- |
| Level 1 — Standard Glazed | Favour useful work per reported watt and lower heat. |
| Level 2 — Jam Filled | Balance useful hashrate, efficiency and thermal headroom. |
| Level 3 — Cream Filled, Cherry on Top | Favour sustained useful hashrate within the same protection limits. |

These change the optimisation objective, not a preset clock/voltage pair.
All priorities obey the same hardware and user constraints.

**Continuous tuning On** keeps observing and starts another bounded evaluation
when conditions change or the reassessment interval expires. Stable conditions
need not cause any write. **Off** completes one bounded search and holds its
best measured result. Protection remains active while a result is held.

Changing saved options guides an existing session. Saving options while the
tuner is disabled does not start it. Boot and OTA never automatically restart
a tuning session, even if Continuous tuning was saved as On.

## Temperature and electrical limits

Set **ASIC maximum** and **VRM maximum** independently. The page identifies the
sensor sources and gives guidance for that hardware. A shared board reading
is not an independently measured hottest-chip junction, and an external VRM
sensor differs from a power-stage or regulator die sensor.

The initial temperature values are conservative controller starting options,
not manufacturer-approved universal operating temperatures. They remain below
the installed protection boundaries. The supported Gamma adapter retains its
existing 75°C ASIC and 105°C regulator protection; user tuning ceilings cannot
raise or disable these. Nerd ceilings also depend on its actual configured
protection channels. Missing or disabled required protection prevents Start.

Choose stricter limits for uncertain cooling. Lowering a temperature or power
ceiling can cause a bounded clock reduction or a stop if the requested limit
cannot be met. Increasing voltage is considered for timing errors only when
both thermal and electrical headroom are available. Hot sensors, inadequate
power headroom, weak/missing chips or invalid readings cannot justify a voltage
increase. Firmware cannot infer a PSU, connector or cooling assembly's rating
from the regulator's component rating.

The advanced search bounds initially retain the saved clock and voltage as
upper ceilings. Clock/voltage bounds must include the confirmed current point
when starting or changing an owned session. They do not force a large jump to a
new minimum. Gamma supports fractional MHz requests; the Nerd control interface
uses whole MHz. Both use whole mV requests. Search increments and computed PLL
frequency are not independent measurements of the actual silicon clock.

Reported power may be regulator input telemetry or a labelled output-derived
estimate. It is not measured wall power or a guaranteed PSU input-power limit.
Use the reported source and independently known supply/cooling capability when
choosing a power ceiling. These candidates do not promise a universal safe
overclock or a particular efficiency improvement.

## Enable, waiver and original backup

1. Read the hardware guidance, set compatible bounds and select **Save options**.
2. Read the experimental tuning disclaimer. Its risk acknowledgement is
   initially unticked and must be explicitly accepted for each Start.
3. Select **Start tuning**. The firmware independently checks the waiver,
   supported hardware, valid fresh sensors/counters, cooling, current point and
   a verified original backup before admitting changes.
4. Watch the state and reason, temperatures, errors and baseline/current/best
   requests. A request acknowledgement alone is not confirmation that a change
   completed; the page reads back the reported state.

Before the first enabled session the miner saves its **actual pre-tuning
settings**, not guessed factory defaults. The backup is bound to its physical
identity and hardware family. It includes clock, core voltage and the supported
cooling/PID/protection settings: the Gamma's six relevant settings or the Nerd
family's twenty effective settings. Fault latches are never cleared by a
restore. Pool, network, credentials and slots are not tuning targets and are
not replaced by this backup.

The original backup is retained across runs, disabling and restarts. Starting
again does not replace it with a learned or experimental point. Every Start
requires successful verification of the durable original; a missing, corrupt,
wrong-device or unverified backup denies Start. There is no automatic factory
reset, backup replacement or credential export.

The disclaimer records explicit risk acknowledgement. It does not guarantee
that a particular limitation of responsibility is enforceable in every place.
Existing GPL licensing and third-party notices remain in force.

## Disable and restore at any time

Select **Disable and restore backup**. It remains available after a restart,
when status is stale, when a Start acknowledgement was lost, or when tuning
options need repair. It requests restoration of the original saved configuration
and its live clock/voltage/cooling policy through the protected owner.

If the miner is too hot, paused, faulted or missing required readings, restoration
waits. The backup remains available and the page reports the reason. Restoration
does not clear a fault, restart a stopped ASIC or force old settings onto unsafe
hardware. Use the miner's existing explicit recovery controls when the underlying
condition has been corrected, then retry restoration as reported.

A completed restore is reported only after saved-settings checks and successful
live readback. The browser never automatically repeats the state-changing
request after an uncertain response. Refresh status before choosing another
action. An explicit manual setting or scheduled profile cancels tuning ownership
without overwriting that new request; the original backup remains available for
a later explicit restore.

## Measurements and search behaviour

The controller uses unique complete hardware-counter intervals. Receiving the
same reading in several browser requests does not create more evidence. After
a verified change it allows settling, discards the possible pre-window counter
interval, then gathers a full observation window. Current starting policy uses
30 seconds of settling and a 60-second measurement window after the 15-second
maximum-counter-interval guard, with at least eight distinct accepted samples.
These are engineering starting timings awaiting thermal bench qualification.

Candidate comparison accounts for observed variability and a minimum improvement
margin. Per-chip errors and work checks prevent a good average from hiding a weak
chip. Each search has bounded experiments and recovery attempts. Missing sensors,
counter resets, time regression, stale observations, failed transitions or lost
ownership cannot authorise another upward adjustment.

Candidates remain in RAM. The tuner does not repeatedly save experimental
clocks/voltages to NVS. A restart returns to the durable user configuration with
tuning disabled; the original backup remains available.

## Compatibility and validation

These are experimental opt-in candidates. Existing hardware qualification for a
previous firmware release does not qualify this controller, another PCB revision
or another regulator/cooling assembly. The current implementation and simulator
results must remain distinct from physical calibration and sustained operation.

Gamma's atomic restore can use a new cooling-aware settings journal when needed.
Older images do not understand that extended format. Before any downgrade,
disable tuning, restore and verify the original settings, and review the exact
journal compatibility rather than assuming an older image can read every later
manual cooling change. Keep a unit-specific recovery backup.

See the [design requirements](dynamic-autotuning-design.md) and
[primary-source research](dynamic-autotuning-research.md). A separate validation
record will identify the exact source, software checks, image pairs and any
supervised miner observations before publication.
