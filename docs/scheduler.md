# Scheduler, manual pause, and power saving

Open **Scheduler** for weekly pause windows and manual mining control. The header also provides pause/resume. These controls require the matching 5tratumFW application; installing the web interface alone does not add scheduling or ASIC state confirmation.

Pausing stops ASIC mining, cuts ASIC core power, and holds the ASIC in reset. The ESP32 controller, network access, web interface, and cooling stay available, so paused device power is not zero. Resuming restores the saved voltage and frequency and reinitializes the ASIC. No clock/voltage preset or automatic tuning is applied.

The interface reports measured watts and distinguishes **requested** from **applied** pause state. A successful HTTP request means the request was accepted, not that the hardware transition is already complete. Wait for fresh device status. A transition failure or hardware fault needs attention; pressing Resume does not bypass protection or repair an invalid stored schedule.

## Create a weekly schedule

1. Select a timezone explicitly. Supported choices are UTC, Europe/London, Europe/Berlin, America/New_York, America/Chicago, America/Denver, America/Los_Angeles, Asia/Tokyo, Asia/Shanghai, and Australia/Sydney.
2. Add up to **eight** windows. Select at least one **start day** per window, then choose **Pause from** and **Until** in 24-hour local time.
3. Turn on **Enable schedule** and choose **Save schedule**. An enabled schedule needs at least one window. Start and end cannot be equal.
4. Check the device's reported local time and applied state. Saving persists the schedule on the device and applies it immediately; no restart is required.

Mining pauses inside a window and is allowed outside all windows, subject to hardware and pool readiness. Window starts are inclusive and ends exclusive. Overlapping windows combine into one continuous pause interval.

An overnight window ends on the following day. For example, Monday `23:00` to `07:00` pauses from Monday night through Tuesday morning. Selected weekdays describe when the pause **starts**. Times follow the selected timezone, including its daylight saving rules; a repeated local hour can be included twice and a skipped local hour does not occur.

The schedule is stored in NVS and survives restart. Manual overrides are temporary and clear on restart. After restart, an **enabled** schedule keeps mining paused until the firmware has synchronized its own network clock. It uses `pool.ntp.org`; it does not depend on pool job timestamps or the browser's clock. Check Wi-Fi and network time access if the interface stays at **Not synchronized**.

Scheduling defaults to disabled. With no enabled schedule, normal boot allows mining once the runtime and pool are ready. Disabling a schedule while running keeps the current mining request; it does not automatically resume a miner that is paused. Use Resume when that is your intent.

## Manual control

- **Pause mining** requests ASIC power saving now.
- **Resume mining** requests mining now, subject to a valid enabled schedule clock and protection gates.
- With scheduling enabled, a manual override lasts until the next effective scheduled pause/resume boundary, **Return to schedule**, or a restart. Boundaries hidden by overlapping windows do not expire the override.
- With scheduling disabled, there is no timed boundary. Manual control lasts until another manual request or restart. **Clear manual override** removes the override; it does not force a paused request to resume.

If the weekly union is continuously paused, there may be no future schedule boundary, so use Return to schedule or restart to clear an override. If the clock becomes invalid, an enabled schedule pauses safely without treating missing time as permission to run.

Hardware faults, unavailable pools, and runtime startup can keep mining paused even outside pause windows. The schedule describes when mining is allowed; it does not prove the ASIC is hashing or shares are accepted.
