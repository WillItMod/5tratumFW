# QAxe mining power and weekly scheduling

The installed `5tratumFW-qa-web-a7` test candidate adds reversible mining power control to the existing standalone four-chip QAxe driver paths. The development test unit is NerdQAxe++ / `NERDQAXEPLUS2` / four BM1370 ASICs. Its physical PCB revision is unidentified; QAxe+, other Nerd models, CAN fleets and OctAxe have no physical qualification from this unit.

The paired a7 application/WWW images were built and installed on this QAxe++ on 7 October 2026. Manual pause/resume, weekly pause/override, paused restart and restoration were checked with live telemetry and fresh accepted shares. See the [exact a7 validation record](validation-a7.md). These checks support a narrowly scoped QAxe++ a7 hub adapter; they do not qualify unidentified revisions, other models or independent ASIC routing.

## Miner controls

Open **Scheduler → Mining power** for Pause, Resume and Return to schedule. Pausing controls the shared ASIC rail, LDO and reset; cooling, the controller, network, web interface and Stratum connections stay available. Paused device power is not zero. Individual ASIC power or operating points cannot be controlled independently.

The interface distinguishes a requested change from the applied hardware state. A successful request acknowledges acceptance or schedule storage; use a fresh status reading to confirm the transition. Initialization, unreadable sensors, a transport failure or an existing protection latch can prevent mining. Resume never clears a hardware fault latch.

Resume uses the existing board initialization with the saved voltage and frequency. It requires four detected ASICs, checked UART writes/baud changes, completed transmission and measured Vout within 30 mV of the saved voltage. No preset or automatic tuning is selected. Old jobs, copied results and hashrate baselines are retired across a power transition.

## Weekly windows

Save up to eight pause windows with start days, local start/end time and a named timezone. Supported zones are UTC, Europe/London, Europe/Berlin, America/New_York, America/Chicago, America/Denver, America/Los_Angeles, Asia/Tokyo, Asia/Shanghai and Australia/Sydney.

Window starts are inclusive and ends exclusive. Overnight windows end the following day. Overlapping windows combine. Scheduling allows mining outside the union, subject to runtime and hardware readiness. Enabled scheduling pauses until the miner's own network clock is valid; it does not depend on the browser or hub remaining online.

Manual Pause/Resume overrides an enabled schedule until its next effective boundary, Return to schedule or restart. A manual override can remain active when a new window is saved; use Return to schedule to apply the weekly policy. Disabling a schedule retains the current requested mining state. Use Resume explicitly when desired. Invalid stored schedules pause safely and remain repairable through the API.

The separate pool-switch scheduler uses up to sixteen weekly time points and a fixed UTC offset. It does not inherit the power scheduler's daylight-saving timezone. Switching a pool does not resume paused mining or change the saved operating point. See [pool and tuning profiles](miner-profiles-and-pool-schedule.md).

## API and persistence

- `GET /api/system/mining/schedule` returns `schemaVersion:1`, identity/hardware/firmware binding, capability, schedule, named-zone limits and requested/applied status.
- `PUT /api/system/mining/schedule` accepts exactly `{enabled,timezone,windows:[{days,start,end}]}` and returns the same snapshot after a successful durable save. Sunday is bit 0; Saturday is bit 6. Times use `HH:MM`.
- `POST /api/system/pause` and `/resume` accept an empty object or body.
- `POST /api/system/mining/schedule/override` accepts `{mode:"schedule"}`.

Existing network/CORS restrictions apply. Writes require the existing OTP policy and reject unsupported boards. Bound identity is peer-reported evidence, not cryptographic firmware attestation. Status is current requested/applied evidence, not proof of accepted shares.

Power policy uses the separate bounded `pf5tfw/power` NVS blob. It commits before publishing a replacement and survives restart. Manual overrides are volatile. Factory self-test is skipped on qualified controlled boards because its old path would otherwise energize ASICs outside the policy controller.

## Checks and remaining work

The source passes production control-state, actual backend/ASIC/board-method, hashrate and weekly persistence/HTTP sanitizer tests. Cases include cold regulator startup, missing workers/timer failures, wrong chip count, invalid Vout, pause during a ramp, UART/baud failures, thermal latch, generation retirement and storage/clock/DST failures. Hardware/I2C/GPIO/sensor/RTOS boundaries remain simulated. The [a7 record](validation-a7.md) separates those checks from target compilation and the observed unit's live rail/fan/pause/resume/restart checks.

This candidate does not add independent per-ASIC job dispatch. Four live ASIC rates and MUX coin/block context are already available in a6, but independent routing remains false until actual isolated concurrent jobs and pipeline retirement are proven.
