# Miner profiles and weekly pool switching (Nerd a4)

This API stores ten named tuning slots and ten named pool slots on the miner, in the existing 24 KiB NVS partition. Browsers do not own the profiles. Opening manual mode or saving a slot does not change mining. Apply is an explicit operation. The broadcast ASIC driver cannot apply a pool per ASIC; such requests are rejected.

All endpoints inherit the existing LAN/network restriction and CORS policy. POST requests require `Content-Type: application/json` and the miner's existing OTP authorization: `X-TOTP` or `X-OTP-Session` when OTP is enabled. Never disable that check for hub requests. A hub must verify the peer's complete `identity.deviceId`, firmware product/version and hardware model/count against its discovered inventory immediately before a write. A network address alone is not device identity.

## Pool page in the next source layout

The unbuilt QAxe Beta5 and OctAxe Beta3 source candidates put **Primary** and **Secondary** editors above the ten slots. In Dual pool these are native A/B; in failover Secondary is standby. Save connection edits with **Save pool settings**, then choose a slot/name and **Save to slot** beside that route's save button. Storage reads the saved miner connection, including private credentials; dirty connection drafts must be saved first. Stored rows expose **Apply to Primary**, **Apply to Secondary**, **Rename** and **Clear**. Rename sends only `type`, `slot` and `name`, retaining the existing descriptor and password. Secondary maps to the unchanged `fallback` API value. These actions do not assign work to individual ASICs.

## GET /api/5tratum/profiles

Readonly; no default slots or settings are written. Response:

```json
{
  "schemaVersion": 1,
  "identity": {"deviceId": "5tfw:0102030405060708090a0b0c0d0e0f10"},
  "firmware": {"product": "5tratumFW", "version": "5tratumFW-qa-web-a4"},
  "hardware": {"boardModel": "NerdQAxe++", "asicModel": "BM1370", "asicCount": 4},
  "tuning": [
    {"slot": 0, "configured": true, "name": "Custom", "frequencyMHz": 577, "coreVoltageMv": 1137},
    {"slot": 1, "configured": false, "name": null}
  ],
  "pools": [
    {"slot": 0, "configured": true, "name": "MUX", "host": "mux.example", "port": 7331,
     "user": "miner.worker", "passwordConfigured": true, "protocol": 0, "tls": false,
     "extranonceSubscribe": false, "authorityPubkey": "", "channelType": 0,
     "coinbaseVerifyMode": 0, "coinbaseMaxFee": 3.0, "coinbaseVerifyForce": false},
    {"slot": 1, "configured": false, "name": null}
  ],
  "limits": {"frequency": {"min": 50, "max": 800, "step": 1, "quantization": "nearest-pll"},
             "coreVoltage": {"min": 1005, "max": 1400, "step": 1}},
  "current": {"frequencyMHz": 500, "coreVoltageMv": 1130},
  "independentWorkAssignment": false
}
```

The abbreviated example shows two slots; actual arrays always contain exactly ten slots indexed 0–9. Limits come from the validated board's absolute limits, with conservative software bounds where older board code has no absolute limit. Current values are configured requests; the PLL/regulator quantize physical output. Existing settings outside profile limits are retained until an explicit valid Apply.

Passwords never appear in GET responses. `protocol` is 0 for SV1, 1 for SV2; `channelType` is 0 for extended, 1 for standard. Nerd pool verification fields are additive and differ from Gamma's certificate/decode descriptors; clients must preserve family-specific fields rather than assuming the families share every option.

## POST /api/5tratum/profiles

Bodies are bounded to 8 KiB, names to 48 UTF-8 bytes, serialized individual slots to 1,536 bytes. No NVS partition erase or settings reset is performed.

Save tuning only:

```json
{"type":"tuning","slot":0,"name":"Custom","frequencyMHz":577,"coreVoltageMv":1137}
```

Capture the miner's saved primary or fallback pool, including its real stored password and protocol/verification fields:

```json
{"type":"pool","slot":0,"name":"MUX","captureCurrent":"primary"}
```

Capture reads the saved miner connection, not unsaved browser form edits. Save connection edits first. `captureCurrent` also accepts `fallback`. An API client may edit pool fields explicitly with `type`, `slot`, `name` and the public pool fields above. Omitted fields retain existing slot values; omitted `password` retains its secret. Explicit `password:""` clears it; explicit null is invalid. A new explicit slot requires a valid host/port. Host/user/password are bounded to 128 bytes; authority key to 66 bytes.

Clear a slot:

```json
{"type":"pool","slot":0,"clear":true}
```

Clearing a pool referenced by any stored schedule event fails with `409 profile-in-use`; remove the timepoint first, including from disabled schedules. Slot storage failure returns `507 storage-unavailable`; a failed full-storage replacement retains the old slot.

## POST /api/5tratum/profiles/apply

```json
{"type":"tuning","slot":0}
```

Direct manual Apply uses the same bounded atomic path without consuming a slot:

```json
{"type":"tuning","frequencyMHz":577,"coreVoltageMv":1137}
```

```json
{"type":"pool","slot":0,"poolTarget":"primary"}
```

`poolTarget` also accepts `fallback`. `asicIndex`, `coreIndex` or an ASIC pool target is rejected with `409 unsupported-target`. An empty slot returns `409 empty-slot`. Tuning changes only clock/core voltage. Pool Apply changes only that target connection's complete stored pool descriptor. Other pool settings, balance/mode, keepalive, fan/PID/shutdown limits, version rolling and mining pause state are retained. Configuration is cloned, committed and then published by a nonallocating swap under its existing mutex. NVS failures leave the active config in memory unchanged. Existing board and StratumManager reload paths apply the explicit request; Nerd does not need a full reboot.

Success is `{"ok":true,"restartRequired":false}`. Errors generated by these handlers are `{"ok":false,"error":"invalid-profile"}` (400), `empty-slot`, `unsupported-target`, `profile-in-use` (409), or `storage-unavailable` (503 for read/unavailable, 507 for failed storage). Existing authentication, CORS and malformed JSON errors keep their existing HTTP response behavior. A success response is only sent after persistence succeeds; repeat GET to verify the stored result.

## GET/POST /api/5tratum/pool-schedule

POST exact body:

```json
{"schemaVersion":1,"enabled":true,"utcOffsetMinutes":60,"events":[
  {"enabled":true,"dayMask":127,"timeMinutes":720,"slot":0},
  {"enabled":true,"dayMask":127,"timeMinutes":1080,"slot":1}
]}
```

At most 16 timepoints. `dayMask` bit 0 is Sunday, bit 6 Saturday; 127 means every day. `timeMinutes` ranges 0–1439. Every referenced pool slot must exist, even in a disabled event. Enabled points cannot overlap at the same time/day. Offset is explicit −720…840 minutes in 15 minute increments, displayed as UTC±HH:MM. It is a fixed offset and does not follow DST; the browser's or hub's timezone is never silently inferred. This avoids the older firmware's hardcoded Berlin display timezone being mistaken for the user's schedule timezone.

GET returns the same four stored fields plus the identity/hardware/firmware bindings shown above, `clockValid:boolean`, `selectedSlot:null|0…9`, `timezoneMode:"fixed-utc-offset"`, `poolTarget:"primary"`, and `maxEvents:16`. `selectedSlot` describes the schedule's currently selected profile, not a claim that an upstream pool connection is healthy. It is null when disabled, without an enabled timepoint, or before trustworthy SNTP time. POST only sends the four writable fields; do not echo GET metadata into it.

The normal-stack miner task checks every 20 seconds. The latest preceding enabled timepoint wins, wrapping back into the previous week. One recurring point keeps that pool selected. Before valid SNTP time it retains the current pool. It reloads the shared primary connection when the selected slot changes; it does not reboot, change pool mode/balance, resume paused mining or rewrite ASIC voltage. After an explicit manual pool Apply, the scheduler next changes the pool when the next different scheduled profile is selected. The hub can be offline.

## Verification

`python3 test/host/test_profiles.py` compiles actual schema, slot-storage, HTTP handlers and commit-before-publish helpers under ASan/UBSan. Its fake ESP/NVS boundary checks full-storage failure, readonly empty GET, credentials/redaction, denied authorization, preservation, empty/ASIC targets, references and weekly wraparound without any miner access. Angular edit/home/settings tests check manual 500/1130 preservation, OTP requests, exact profile bodies, explicit offsets, separate Scheduler navigation and per-pool v2 job freshness. Production `npm run build` compresses the UI for the 3 MiB WWW partition; the firmware build verifies partition fit.
