# Adaptive tuning — development candidates

The first candidates are **Bitaxe Gamma 601/602 Beta13**, **NerdQAxe++ Beta10**
and **NerdOctAxe Gamma Beta8**. They are unreleased while validation is in
progress. Use each family's own matched application and web image; no factory
erase or NVS image is part of an update.

The controller runs on the miner. Closing the browser does not stop it. It
adjusts the whole miner's clock and core voltage using current hardware
measurements; it does not independently tune or route individual ASICs.

## Tuning page

Open **Tuning** in the firmware web interface. The Scheduler remains separate.
An active or held session opens on **Live dashboard**; use **Adjust this tune**
to change the goal or limits. An idle miner opens on **Tuning options**. The
activity banner and restore action stay visible when switching views.

| Priority | Goal |
| --- | --- |
| Level 1 — Standard Glazed | Favour useful work per reported watt and lower heat. |
| Level 2 — Jam Filled | Balance useful hashrate, efficiency and thermal headroom. |
| Level 3 — Cream Filled, Cherry on Top | Favour sustained useful hashrate within the same protection limits. |

These change the optimisation objective, not a preset clock/voltage pair.
All priorities obey the same hardware and user constraints.

**Efficiency** compares useful work per reported watt. It can deliberately
choose less TH/s when the efficiency improves enough. **Balanced** scores both
normalised useful hashrate (55%) and normalised efficiency (45%). **Performance**
scores sustained useful hashrate; power, temperatures and errors still constrain
the search. It can use more power for a measured speed gain. These are goals,
not successively higher fixed voltages or guaranteed speed levels.

A Performance run cannot test a higher clock than its saved maximum. The
initial maximum matches the miner's existing clock, so raising that maximum is
an explicit experimental choice. A downward-only qualification run cannot prove
the miner's maximum TH/s. Nor does a lower-voltage result at one clock prove
that voltage will remain stable at a higher clock.

**Continuous tuning On** keeps observing and starts another bounded evaluation
when conditions change or the reassessment interval expires. Stable conditions
need not cause any write. **Off** completes one bounded search and holds its
best measured result. Protection remains active while a result is held.

Changing saved options guides an existing session. Use **Save changes to this
tune** while it is running; the page confirms the changed saved options after
reading them back. There is no second Start or waiver to edit an admitted
session. While an adjustment or restore is pending, the page explains why
another change must wait. Saving options while the tuner is disabled does not
start it. Boot and OTA never automatically restart
a tuning session, even if Continuous tuning was saved as On.

## Temperature and electrical limits

Set **ASIC maximum** and **VRM maximum** independently. The page identifies the
sensor sources and gives guidance for that hardware. A shared board reading
is not an independently measured hottest-chip junction, and an external VRM
sensor differs from a power-stage or regulator die sensor.

The recommended default tuning targets are **60°C ASIC** and **70°C VRM**
(or lower if the installed protection requires it). The page warns above 60°C
ASIC, with a stronger warning above 65°C; it warns above 70°C VRM, with a
stronger warning at 80°C or higher. These are project starting recommendations,
not manufacturer-approved universal operating temperatures or permission to
run continuously at a protection cutoff. Saved user choices survive upgrades;
the new defaults do not silently replace an existing configuration.

User-configurable higher values remain bounded by the installed protection.
Emergency protection thresholds are not recommended operating targets. The
supported Gamma adapter retains its existing 75°C ASIC and 105°C regulator protection; user tuning ceilings cannot
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

Nerd limits come from the detected board profile. The standard supported
QAxe/OctAxe profile is capped at 800MHz; the recognized QAxe rev7 regulator
profile exposes 1,000MHz. The normal settings editor and Tuning both use the
reported board limit. This metadata is a firmware boundary, not a guarantee
that a particular miner can operate reliably at that frequency.

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

The normal web header displays **TUNING ACTIVE** and the saved objective while
a session is running, including when the Tuning page is closed. Monitoring,
holding, restoration, blocked and stale/unavailable states have separate labels.
An unavailable observation must not silently turn an active session into Off.

The Tuning page explains the current state in plain language alongside the
actual controller reason. The visual loop represents repeated tests, not a
linear progress bar for the entire tune. A settling or observation duration is
a **minimum**, not a promise of completion: the miner still needs enough fresh,
distinct hardware intervals. Invalid readings and busy snapshots appear as
unavailable rather than as zero or a healthy result.

Live values and completed measurement means have different purposes. Any
reference/best comparison is for the **current search round**. A later round
can remeasure its reference as conditions change; that reference is not the
immutable original-settings backup. Experiment counts reset for each round;
the reported rejected-window count accumulates across the current run. Browser trend observations start when the
page is opened, contain connection gaps and are not additional controller
samples. Reopening the page does not reconstruct a complete tuning history.

The live overview includes useful **TH/s**, **J/TH**, ASIC and VRM temperatures,
reported watts, and board/worst-ASIC error percentages. J/TH is reported watts
divided by useful TH/s; it is not a wall-meter measurement. Six recent trend
charts cover temperatures separately, power, useful hashrate, energy per useful
hash and ASIC errors. Invalid or missing readings produce gaps, not zeroes.

**Recent controller decisions** comes from actual miner records: starting a run,
measuring a reference, requesting/applying/failing a change, accepting a better
measured point, rejecting a trial or encountering excessive errors. Completed
window means and the actual comparison threshold accompany evaluated results.
A requested change is not labelled applied until confirmed. The miner retains
eight recent events in RAM; an open tab can retain up to 100 observed events for
the same run, and identifies missed entries. This is not a permanent audit log.

Both Overview and Tuning show **pool share difficulty** and **network/job target
difficulty** separately for each pool. Values come from the miner's current
valid dispatched work, normalized to Bitcoin difficulty-1 units. They are not
best-share difficulty or an external explorer estimate. Disconnects and
invalidated work show unavailable data; an older still-valid job keeps its
original age instead of acquiring a fabricated new timestamp.

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

Normal neighboring probes change one setting at a time: 6.25MHz for Gamma,
10MHz for QAxe/OctAxe, or 5mV. The measured result must beat the current best by
more than the observed noise margin, with a minimum 0.2% score margin. A rejected
probe returns to the better measured point when there is sufficient headroom.
No improvement is a valid result; the starting settings can remain the best.

Board errors above 0.5% or worst-chip errors above 1% trigger the bounded error
recovery policy after a full evaluation window. While holding or observing a
result, a fresh error reading above either threshold instead triggers an
immediate clock backoff; it does not wait for another full window. During
full-window error recovery, an additional 5mV is considered only with
both thermal and power headroom and within the user's voltage ceiling.
Otherwise the controller reduces clock. Thermal/power protection backoffs can
be larger than a normal search step (approximately 5%, rounded to the clock
step); they are load reduction, not an optimisation probe. These are
experimental engineering policies, not manufacturer-certified error or safety
limits. Searches are local and bounded, not a guaranteed global optimum.

## Physical tuning display

The additional Tuning page on the miner is read-only and uses the normal screen
navigation. It reports the current goal, state and available measurements, including useful
TH/s and J/TH. Limited displays alternate concise measurements with the current
point and last real decision. On Gamma, ACTIVE/MONITOR/HOLD/OFF/RESTORE/BLOCKED or
STALE remains visible in the heading. Selecting it cannot start tuning,
change limits or accept a waiver. Setup, update, shutdown and fault screens
retain priority. The full explanations and controls remain in the web Tuning
tab. Native rendered fixtures are separate from physical panel qualification.

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

The [3 November validation and memory-repair record](validation-autotune-memory-20261103.md)
describes the earlier physical failures, preserved-settings recoveries and the
qualification still required before these candidates can be published.
