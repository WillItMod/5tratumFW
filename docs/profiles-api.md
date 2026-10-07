# Miner profiles and pool schedule

These APIs are implemented in the Gamma 601/602 application. They reuse the miner's network access and CORS policy. Requests are bounded to 4095 bytes; names are 1–32 bytes and must contain no control characters. The web interface never retrieves stored passwords. No request in this API restarts the miner automatically.

Every GET response includes `identity.deviceId`, `hardware` and `firmware`:

```json
{
  "identity": {"deviceId": "5tfw:32-lowercase-hex-characters"},
  "hardware": {"boardModel": "Gamma 601", "asicModel": "BM1370", "asicCount": 1},
  "firmware": {"product": "5tratumFW", "version": "5tratumFW-0.1.0-beta.3"}
}
```

The example identifier is a placeholder, not a valid device ID. The actual ID is the first 16 bytes of SHA256 over `5tratum/device/v1`, a terminating zero byte, and the six base Wi-Fi MAC bytes. Raw MAC bytes are not exposed by these endpoints. The ID matches the central hub's legacy Gamma discovery derivation. A controller should recheck the same physical device binding immediately before applying a reviewed change.

## Profiles

`GET /api/5tratum/profiles` is read-only. `schemaVersion` is 1. `tuning` and `pools` each contain exactly ten slots, numbered 0–9. Empty slots are `{ "slot": 0, "configured": false, "name": null }`.

Configured tuning slots add `name`, `frequencyMHz` and `coreVoltageMv`. Configured pool slots add `name` and the connection fields listed below, replacing private `password` with `passwordConfigured: true|false`. GET also returns:

```json
{
  "current": {"frequencyMHz": 550, "coreVoltageMv": 1150},
  "limits": {
    "frequency": {"min": 400, "max": 625, "step": 0.25, "quantization": "nearest-pll"},
    "coreVoltage": {"min": 1000, "max": 1250, "step": 1}
  }
}
```

These are existing model software request bounds, not validated safe presets. Frequency is a requested float; the BM1370 PLL chooses a nearby realizable frequency. Telemetry's actual frequency is separate. A saved current point outside these bounds or off the input increment is preserved unchanged. Changing a value requires the advertised bounds and input increment. No saved value is replaced when opening manual mode. Existing thermal protection remains active.

`POST /api/5tratum/profiles` saves or clears a slot without applying it. The following are separate request forms; unknown or duplicate JSON keys are rejected:

```json
{"type":"tuning","slot":0,"name":"My operating point","frequencyMHz":550.25,"coreVoltageMv":1151}
```

```json
{"type":"pool","slot":0,"name":"My pool","captureCurrent":"primary"}
```

`captureCurrent` is `primary` or `fallback` and captures the currently stored route, including its private password, on the miner. Save edits in Pool routing before capturing them. Alternatively send `type`, `slot`, `name` and explicit pool connection fields. Missing fields retain the existing slot's fields; a new slot requires all fields. Omitted `password` preserves an existing slot's password; `password:""` explicitly clears it.

Gamma connection fields are:

| Field | Accepted value |
| --- | --- |
| `protocol` | `SV1` or `SV2` |
| `host` | 1–253 bytes |
| `port` | integer 1–65535 |
| `user`, private `password` | 0–512 bytes |
| `suggestedDifficulty` | integer 0–65535 |
| `extranonceSubscribe`, `decodeCoinbase` | boolean |
| `tls` | integer 0–2, retaining the existing enum |
| `certificate` | at most 2048 bytes; PEM newlines allowed |
| `channelType` | `standard` or `extended` |
| `authorityPubkey` | 0–52 bytes |

```json
{"type":"pool","slot":0,"clear":true}
```

Clear also supports `type:"tuning"`. A pool slot referenced by any event in the saved schedule cannot be edited or cleared, including while the schedule itself is disabled. Remove those event references first. There is no silent substitution with another slot.

`POST /api/5tratum/profiles/apply` explicitly commits an operating point or one selected route:

```json
{"type":"tuning","slot":0}
```

```json
{"type":"tuning","frequencyMHz":550.25,"coreVoltageMv":1151}
```

```json
{"type":"pool","slot":0,"poolTarget":"primary"}
```

`poolTarget` is `primary` or `fallback`. The other route and unrelated fan, network, display, power and thermal settings are retained. Primary changes request a coordinated Stratum reconnection without rebooting, resuming paused mining, or changing the ASIC operating point. Fallback changes return `restartRequired:true`; the UI leaves restarting to the operator. Tuning changes return `restartRequired:false` and are applied by the existing power task.

Save/Apply success is `{ "ok": true, "restartRequired": false }`. Error bodies are `{ "ok": false, "error": "code" }`:

| HTTP | Code |
| --- | --- |
| 400 | `invalid-profile` |
| 401 | `unauthorized` |
| 409 | `empty-slot` or `profile-in-use` |
| 500 | `allocation-failed` or GET `storage-unavailable` |
| 507 | `storage-unavailable` |

## Pool time-point schedule

`GET /api/5tratum/pool-schedule` returns the binding above, `schemaVersion:1`, `enabled`, `utcOffsetMinutes`, `events`, `clockValid`, `selectedSlot` (number or null), `timezoneMode:"fixed-utc-offset"`, `poolTarget:"primary"`, `maxEvents:16` and `storageValid`. Invalid stored routing schedules are disabled while retaining the current route and the API for repair.

`POST /api/5tratum/pool-schedule` accepts exactly:

```json
{
  "schemaVersion":1,
  "enabled":true,
  "utcOffsetMinutes":0,
  "events":[
    {"enabled":true,"dayMask":127,"timeMinutes":720,"slot":0},
    {"enabled":true,"dayMask":127,"timeMinutes":1080,"slot":1}
  ]
}
```

The response is the current GET shape. A maximum of sixteen events is allowed. `utcOffsetMinutes` is an integer from −720 to 840 in 15-minute steps. This is a fixed UTC offset and does **not** follow daylight-saving changes; it is independent of the power scheduler's named timezone. `dayMask` is 1–127 with Sunday bit 0 through Saturday bit 6. `timeMinutes` is integer 0–1439. `slot` must identify an existing pool profile, 0–9. Every saved event must reference an existing slot; saved event references prevent slot editing/deletion. Conflicting enabled events for the same day/minute are rejected.

The scheduler reconciles the latest enabled point at or before local time, wrapping across the week. A single enabled event therefore selects the same pool for the whole week. Missed points are reconciled to the latest desired slot; clock invalidity retains the current route. Applying a route does not resume mining or bypass the power scheduler. The current primary configuration is compared with the chosen profile so manual pool changes are reconciled on a subsequent tick. Clock and voltage are never scheduled.

Errors use the same JSON error envelope; 400 is invalid schedule, 409 is missing profile, 500 is allocation failure and 507 is storage failure. The old schedule is retained on failed validation or commit.

## Storage and recovery

Slots use separate bounded NVS blobs. Applied operating settings use a checksummed, bounded NVS journal. All candidate strings are allocated before commit, and the in-memory operating snapshot changes only after successful commit. Set/commit failures retain the prior snapshot and attempt rollback without erasing NVS. Startup validates stored hardware identity before replaying the journal and before hardware initialization. A corrupt operating journal stops startup rather than inventing default clocks or pool credentials.

Original individual NVS keys remain intact for recovery and stock-firmware downgrade; once the journal is active, this firmware reads its authoritative operating snapshot. Stock firmware does not understand the new journal and will see those original individual values. No ordinary GET, profile save, or manual-mode toggle changes operating settings.
