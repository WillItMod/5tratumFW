# Saved tuning profiles by time of day

Gamma Beta10, QAxe Beta8 and OctAxe Beta5 add **Scheduler → Tuning profile schedule**. This is independent of
the pool-switch schedule and the mining pause/resume windows. It selects the
whole miner's saved frequency and core voltage; it does not tune individual
chips or choose new operating values automatically.

Save the desired operating points as tuning slots under **Miner controls →
Performance** first. Add time points, choose the days and a saved slot for each,
enable the schedule, then save it. For the requested daily policy:

| Time | Saved tuning profile |
| --- | --- |
| 00:00 | Slot 3 |
| 09:00 | Slot 1 |
| 21:00 | Slot 2 |

The most recent enabled time point applies until the next point, including
across midnight and the end of the week. Opening the page or saving a slot does
not activate it. Saving an enabled schedule allows the miner's scheduler to
activate the currently due profile once it is ready. No browser or hub needs
to stay connected. No default schedule or operating point is installed.

Choose an explicit fixed UTC offset. Like the existing pool-slot schedule,
this schedule does not adjust for daylight saving time: UK winter is UTC and
UK summer is UTC+01:00. Its offset is independent of the mining pause scheduler's
named timezone.

The miner checks at twenty-second intervals and waits for synchronized network
time. It validates the saved slot against the current board limits, persists
the accepted clock/voltage request and uses the normal board settings path.
Manual and scheduled pauses, shutdowns, startup readiness and hardware protection
remain in control. A paused miner is never resumed by a tuning time point; the
currently due profile waits until mining is allowed. A failed apply remains
unconfirmed and can be retried by the miner. Accepted profile status does not
measure physical PLL frequency, regulator output or accepted shares.

A successful event runs once per occurrence. A manual operating-point change
after that is retained until another due time point, a schedule replacement or
a restart causes a new evaluation. Disabling the schedule retains the last
accepted operating point. Clearing a tuning slot referenced by any stored event
is rejected until that event is removed, including disabled events.

## API

`GET /api/5tratum/tuning-schedule` returns the stored configuration plus bound
identity/hardware/firmware metadata, `supported`, `clockValid`, `timezoneMode`,
`selectedSlot`, `appliedSlot`, `status`, `error` and `maxEvents`. Status reads
never activate profiles or persist settings.

`POST /api/5tratum/tuning-schedule` accepts only the stored schema:

```json
{"schemaVersion":1,"enabled":true,"utcOffsetMinutes":0,"events":[
  {"enabled":true,"dayMask":127,"timeMinutes":0,"slot":2},
  {"enabled":true,"dayMask":127,"timeMinutes":540,"slot":0},
  {"enabled":true,"dayMask":127,"timeMinutes":1260,"slot":1}
]}
```

API slots are zero-based; the interface labels them 1–10. Up to sixteen time
points are supported. Sunday is bit 0 and Saturday bit 6; 127 selects every day.
Times are minutes after midnight, 0–1439. Offsets range from −720 to +840 minutes
in fifteen-minute increments. Enabled points cannot have overlapping days at
the same time. Every referenced tuning slot must exist. An enabled schedule
requires at least one enabled event.

Writes inherit the existing network, CORS and OTP policy. Configuration is one
bounded `pf5tfw/tune_sched` NVS blob. Validation and reference checks complete
before committing a replacement. The old schedule is retained on a failed
save. Existing pools, profile slots, screen selection and pause schedules are
separate settings.

## Validation boundary

The three public BETA pairs have their own [dated build and installed-device
record](../../../docs/validation-aligned-20261028.md). All four owned miners
reported the selected and applied slot during temporary schedules using their
existing operating point; original slots, schedules and overrides were restored.
That short check does not establish a different operating point, sustained
thermal stability or every protection boundary. Tadets' larger panel remains
physically unqualified; online display references are separate evidence.
