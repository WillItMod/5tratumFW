# Adaptive tuning: experimental BETA

The current experimental releases are **Gamma Beta18**, **NerdQAxe++ Beta15** and **NerdOctAxe Gamma Beta13**, built from `e5f422c68a75e5696d8497f8c913d6a81078c72f`. They separate physical original-settings restoration from tuning-quality admission and show bounded recovery progress. See [current recovery validation](validation-recovery-20261106.md); the previous [reliability](validation-reliability-20261105.md) and [fitted-fan](validation-single-fan-20261105.md) records keep their original scope. Gamma includes its matching interface in the application. QAxe/OctAxe retain matched app/WWW pairs. [Downloads and OS instructions](../DOWNLOADS.md).

The controller runs on the miner. Closing the browser does not stop it. It
adjusts the whole miner's clock and core voltage using current hardware
measurements; it does not independently tune or route individual ASICs.

## Tuning page

Open **Tuning** in the firmware web interface. The Scheduler remains separate.
An active or held session opens on **Live dashboard**; use **Adjust this tune**
to change the goal or limits. An idle miner opens on **Tuning options**. The
activity banner and restore action stay visible when switching views.

**Save before the run waiver.** Choose the objective and bounds, acknowledge the advanced warning if the draft exceeds the reported standard limit, then select **Save options** and wait for readback. The ordinary per-run waiver is not required to save options. Once saved, accept that waiver and select **Start tuning**; an advanced Start also requires its separate acknowledgement. Saving options does not start the controller. A local draft is not a saved miner change.

**Read the Start checklist.** It identifies the exact missing sensor, fan, ASIC counter or mining condition. Expand all checks or use **Copy tuning diagnostics** for support. Fresh status is required; a waiver cannot bypass protection or missing measurements. A temperature above the recommended default can be saved when it remains inside the board's reported limit.

**Check the fitted fans on QAxe and OctAxe.** Tuning options offers **Both fans** (default), **Fan 1 only** and **Fan 2 only**. Select the actual fitted arrangement, explicitly acknowledge that it cools both the ASICs and VRM, then **Save cooling setup** and wait for confirmation. This is separate from Save options and the run waiver. It does not start tuning, change fan speed or overwrite the pre-tuning backup. While a tune owns the miner, disable it and finish restoration before changing the fan selection. An unselected channel is omitted from tuning admission; every selected channel still needs valid running-fan feedback. All other protections remain active. Do not deselect a failed fitted fan to bypass a fault.

| Priority | Goal |
| --- | --- |
| Level 1: Standard Glazed | Favour useful work per reported watt and lower heat. |
| Level 2: Jam Filled | Balance useful hashrate, efficiency and thermal headroom. |
| Level 3: Cream Filled, Cherry on Top | Favour sustained useful hashrate within the same protection limits. |

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

## Restoration status and mining quality

**Disable and restore backup** stops tuning and restores the original settings. It does not cancel an existing recovery or turn an explicit manual pause into a resume. A paused miner can finish saving the original settings with a clear notice that live voltage has not been verified. Use the separate **Restore original settings and resume mining** action when you want that paused miner to resume, after reviewing the reported reason.

The page shows the actual restoration phase, exact reason, elapsed time and the 180-second overall attempt limit. A failed attempt is labelled **Restoration blocked**, including **Time limit reached** when applicable. It retains the failed safety check instead of saying Restoring indefinitely. Refresh, review the reason and retry only when that requirement is resolved. A stale controller heartbeat blocks a new Start and completion claims; Stop and diagnostic refresh remain available.

A safe physical restore requires fresh temperatures, input/output voltage, regulator status, selected fan feedback, the correct device identity and original backup, plus clock/voltage confirmation. Missing or high-error ASIC work measurements do not by themselves prevent restoring an otherwise verified original operating point. They produce a visible **quality warning** and tuning stays off. This confirms the physical restoration, not that the ASIC is healthy or that its settings are suitable for another tune.

Ordinary mining is not paused by default. An explicitly configured pause schedule, a manual pause, hardware protection or a rejected tuning point can stop work. Keep the exact reason and diagnostics before restarting, because a reboot clears volatile control state. These distinctions do not override protection or justify forcing an unsafe miner to run.

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
cannot be met. Error recovery permits at most two unmeasured voltage increases before reserving remaining attempts for clock reduction. Six total corrections remain allowed until a complete clean measurement resets the allowance. Increasing voltage is considered for timing errors only when
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

The minimum clock also limits protective backoff. A narrow range such as
690–800MHz can prevent a miner from meeting a newly lowered temperature target.
If it reaches the minimum and still needs to reduce load, mining pauses. The
stopped point is recorded as rejected under those constraints; this does not
mean the ASIC or that operating point is permanently defective. Review the
minimum as well as the maximum when changing temperature or power limits.

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

### Advanced clock override

Gamma Beta16, NerdQAxe++ Beta12 and NerdOctAxe Gamma Beta10 include an explicit
advanced clock override as an experimental release feature. The current installed pairs have been checked separately; see [flow validation](validation-tuning-flow-20261104.md). Higher-clock physical operation remains unqualified. The page shows the
standard maximum reported by the detected profile: 690MHz for Gamma, 800MHz for
the standard Nerd profile, or 1,000MHz for the recognized QAxe rev7 profile.
The additional **1,200MHz experimental project ceiling** is a software boundary,
not a hardware rating, safe operating frequency or guarantee of stability or
achievable hashrate.

The advanced warning checkbox starts unticked. Existing higher manual settings
or an original backup do not imply permission to enable it. A saved enabled
mode is identified separately; reopening the page does not accept the warning
for you. Read and explicitly acknowledge the warning, choose your maximum clock,
and select **Save options**. Checking the box alone neither writes settings nor
starts tuning. Save is confirmed by reading the miner's configuration back.
The ordinary per-run disclaimer is still required before **Start tuning**, and
the page must have your explicit advanced acknowledgement for an advanced run.

Unsupported frequencies can cause instability, overheating, hardware damage,
interrupted mining or financial loss. The existing tuning disclaimer and its
limitation of responsibility still apply. The override changes only the
permitted clock search range: voltage, reported power, ASIC and VRM temperature,
sensor, fan and fault constraints remain authoritative. The chosen maximum is a
search ceiling, not an immediate clock command or a preset result.

When disabling the override, first review the maximum against the standard
profile limit. If an active run is above that limit, its bounds still have to
include the confirmed current point; lower that point through the existing
controls or explicitly disable and restore before saving incompatible bounds.
For an idle miner with a higher manual clock, saving the disabled permission
and standard-range options does not change that live clock. A later Start is
blocked until the current point is included with explicit advanced permission
or lowered manually. The **Set override to disabled** control changes local
intent and still needs an explicit Save. Original higher-clock backups remain
available for protected restoration independently of tuning permission. An old or mismatched report
without valid override metadata does not unlock advanced clocks; use a matching
application and web interface. A legacy nine-field tuning-options record reads
with the override disabled. New options save the explicit boolean atomically
with the rest of the configuration; older firmware may not read that new format.
The original backup format is unchanged. Disable tuning, verify restoration and
review downgrade compatibility before returning to older firmware.

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

For a miner which is already paused, **Restore original settings and resume
mining** is a separate explicit recovery action. It keeps auto-tuning off. The
firmware first validates the powered-off state, cooling and protection, restores
and verifies the durable original configuration, then requests normal mining
startup. Recovery is not complete until live settings, fans and mining state
are verified. It never clears a hardware fault or bypasses normal startup
protection. A manual power action or an update cancels a pending recovery.

When an ordinary measured trial is rejected, the controller returns to its better measured point when headroom permits. If protective backoff exhausts the permitted clock range, or bounded error-correction attempts are exhausted, the failed point is retained in the decision history. A clock-floor rejection records the initiating readings; error exhaustion records the available decision evidence, distinguishing completed window means from instantaneous readings without inventing an improvement score. These two soft tuning failures can make one guarded automatic recovery attempt: pause and cool, validate protection and sensors, restore and independently verify the immutable pre-tune settings, then resume and verify normal mining with tuning off. Cooling admission requires the applicable temperature headroom; Gamma holds mining work until fresh powered ASIC/regulator/fan readings pass. Nerd applies the extra 5°C margin before startup and continues to enforce user ceilings during powered startup. A real hardware fault, invalid backup, missing telemetry, failed verification or newer manual/scheduled control blocks or cancels recovery. There is no automatic fault clearing, MCU reboot or repeated retry. The original point can differ from tighter tuning targets selected later; improve cooling or review the permitted range before another Start.

On Gamma 601/602, the ASIC temperature diode is unavailable while ASIC power is off. Recovery may power and initialize the ASIC with mining work held back until fresh powered sensor checks pass, then verify normal mining counters.

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
the reported rejection count accumulates across the current run. Browser trend
observations start when the page is opened, contain connection gaps and are not additional controller
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

Completed evaluation windows above 0.5% board errors or 1% worst-chip errors
trigger bounded error recovery. The independent early check below can reject
an unstable baseline or trial sooner. While holding or observing a
result, a fresh error reading above either threshold instead triggers an
immediate clock backoff; it does not wait for another full window. During
full-window error recovery, an additional 5mV is considered only with
both thermal and power headroom and within the user's voltage ceiling.
Otherwise the controller reduces clock. Thermal/power protection backoffs can
be larger than a normal search step (approximately 5%, rounded to the clock
step); they are load reduction, not an optimisation probe. These are
experimental engineering policies, not manufacturer-certified error or safety
limits. Searches are local and bounded, not a guaranteed global optimum.

### Earlier rejection of unstable trials

Gamma Beta16, NerdQAxe++ Beta12 and NerdOctAxe Gamma Beta10 add a separate fast
error check while measuring a baseline or testing a trial. The full settling
and performance-measurement periods remain unchanged: after a confirmed
application, 30 seconds of settling plus the 15-second counter exclusion and
60-second observation give a minimum 105 seconds, with at least eight accepted
distinct samples. Early rejection cannot establish a performance gain or replace
a completed comparison window. See the [offline fast-error validation record](validation-autotune-fast-errors-20261104.md)
for the implementation checks and physical limits. That historical record uses
the previous version markers for private compilation; the new releases use
unique versions and do not replace those older public files. Current installed-pair and form-flow checks are separate from the earlier controller experiments; no higher-clock operation is claimed.

Only fresh, distinct hardware-counter intervals belonging to the current
point’s evaluation count. The possible old 15-second counter interval is
excluded before this check can judge a trial. Repeated browser reads and stale
values are not additional evidence. Managed changes reset counters and restart
the guard; a monotonic-clock regression stops the controller. The earliest qualifying timings are 15 seconds for severe errors or 25 seconds
for persistent ordinary errors from Start or confirmed application. These are
software evidence timings, not physical response guarantees. After
that guard, a severe reading of at least 3% board errors or 5% on any required
ASIC rejects the point without waiting for its full measurement window. Lower
errors above the normal 0.5% board or 1% worst-ASIC limit require at least
three consecutive fresh, distinct accepted samples spanning at least 10
seconds before early rejection. A clean interval, invalid reading or gap
longer than 15 seconds resets that sequence. While holding or observing an
existing result, the current immediate clock backoff on one fresh error-limit
breach remains in force; that response does not wait for the three-sample
trial check.

The controller first attempts to return to a measured stable point within the
current bounds, with sufficient thermal and electrical headroom, and through a
single clock or voltage change. That measured point is separate from the
immutable original-settings backup. If it cannot safely return, the existing
bounded correction and guarded recovery rules apply; an error reading cannot
authorize an unsafe voltage increase.

Error-rejected clock/voltage pairs are excluded from further experiments
during the current run, including later rounds of continuous tuning. Up to 16
exact error-rejected pairs are retained in RAM, not a permanent hardware
blacklist. Starting a new run or restarting clears it; it does not prove a
previously rejected setting is safe under new conditions. If another rejection
cannot be recorded because the exclusion list is full, the controller stops
proposing experiments and requests the existing guarded original-settings
recovery rather than forgetting a failure and repeating it. If all
nearby candidates were excluded, a verified stable point can be held; if no
permitted bounded fallback is available, error recovery is exhausted. Decision
readings at early rejection are reported separately from completed measurement
means and improvement scores. The visible **Rejections this run** count
includes completed-window rejections and early or held-point error rejections.

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

See the [design requirements](dynamic-autotuning-design.md), [primary-source research](dynamic-autotuning-research.md) and [fast-error software validation](validation-autotune-fast-errors-20261104.md) for the current policy. The [previous release validation](validation-autotune-release-20261103.md) retains its exact image/source and separately scoped miner observations.

The [3 November memory-repair history](validation-autotune-memory-20261103.md) records earlier failures, preserved-settings recovery and limited source `35719b8e` tests. The [3 November release validation](validation-autotune-release-20261103.md) records that previous release source and its separate software/physical evidence; Gamma 602, larger Nerd panels and W5500 hardware remain unqualified.
