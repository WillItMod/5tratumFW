# Gamma Beta8 schedule JSON-header validation

Paired version: **`5tratumFW-0.1.0-beta.8`**, BETA tag **`v0.1.0-beta.8`**. Exact compiled and corresponding-source commit: **`eff5e633f372bb46a86803580c453645600f5fcd`**.

Gamma PCB **601/602**, BM1370×1, ESP32-S3 only. The exact pair completed separate supervised installed-device checks on Gamma 601 and 602 on 8 October 2026. **Installed strict OS31 inspection completed on both models; later owner tuning changes were recorded separately.** Earlier [Beta7](validation-beta7.md) and all previous validation records remain unchanged.

## Change and retained behavior

Successful `GET /api/5tratum/pool-schedule` and `GET /api/system/mining/schedule` responses now explicitly declare `Content-Type: application/json`. Existing POST/PUT save readbacks use the same handlers and receive the corrected type. Beta7 returned valid JSON with the server's HTML default, so strict central OS clients correctly refused schedule inspection.

Only these two successful response headers and the matched Gamma version markers change at runtime. JSON schemas, authentication/network checks, persistence, scheduler decisions, manual overrides, error handling, hardware drivers, NVS keys, partition layout and protection behavior are retained. No consumer is relaxed to accept HTML. QAxe and OctAxe source/version/package pairs remain unchanged.

## Matched package

| Asset | Bytes | SHA256 |
| --- | ---: | --- |
| `esp-miner.bin` | 1,717,792 | `03ad827132215b8755c693d9082f5a720522d5b42e8efa5ccfffc8354d1a19a2` |
| `www.bin` | 3,145,728 | `a7518eccce43c111b830bd236e03f6d66c6c0ccd450a3c079d72e959b5d451e4` |
| `5tratumFW-0.1.0-beta.8-source.zip` | 18,230,516 | `2f4b6991a8c498f0ad3fb44dd336ed953cbc7b0c8f645749a648e80de1a0e7c4` |

Use this Gamma app/WWW pair with its manifest, checksums and GPL source. These are OTA partition images, without factory, partition-table or NVS images. A shared WWW filename is not cross-family compatibility.

The seven-asset Gamma BETA package contains `esp-miner.bin`, `www.bin`, `5tratumFW-0.1.0-beta.8-source.zip`, `manifest.json`, `SHA256SUMS`, `web-budget.json` and `validation-gamma-0.1.0-beta.8.md`. All six existing package files remain byte-identical; the seventh file is this dated validation record.

## Build and offline evidence

The full pinned build passed **140 frontend tests, ten host entry points and 27 native OLED fixture views**. The focused production HTTP suites passed **3 pool-schedule tests** and **11 mining-schedule tests**, including the JSON-header checks. Both new header regressions fail against the pre-fix handlers and pass after the fix. The actual web-version generator check passed. These host/mock tests use no miner I/O.

The app descriptor identifies this version, ESP32-S3 and ESP-IDF v5.5.3; the segment checksum and appended SHA256 passed. The compiled partition-map checksum passed and the source layout is unchanged, without a new physical partition-map readback claim. WWW recreation matched byte-for-byte. The 621,837-byte payload passes the enforced 876,628-byte budget in its 3 MiB partition. Source verification covered 1,997 safe archive members and 1,996 inventory hashes, matching clean source and the pinned submodule. Node 24.14.0 and the pinned ESP-IDF environment were used.

Exact-source Gamma CI passed in [run 37810069620](https://github.com/WillItMod/5tratumFW/actions/runs/37810069620) and [run 37810214181](https://github.com/WillItMod/5tratumFW/actions/runs/37810214181), both at `eff5e633f372bb46a86803580c453645600f5fcd`. CI builds/tests/packages and does not flash. Public asset comparison is recorded separately from this installed-device validation. Local build, CI and the scoped installed checks are separate evidence; none establishes sustained hardware safety.

## Supervised installed-device verification — 8 October 2026

The following observations apply to this exact Beta8 pair, separately on each owned unit. Each one-shot supervised update reached `paired-version-settings-verified`; upload ACK alone was not treated as success.

| Unit scope | Matching app/web and served assets | Exported settings, slots and schedules | Retained operating point | Short mining/sensor observation | Both schedule GET Content-Types |
| --- | --- | --- | --- | --- | --- |
| Gamma PCB 601 | Exact Beta8 app/web; all 11 served web records matched | Saved exported settings, 10 pool slots, 10 tuning slots and both schedules retained; manual override none | 580 MHz / 1155 mV | Accepted shares increased by 1; ASIC board 59.75°C, VR 47°C | `application/json` for pool and power schedule GETs |
| Gamma PCB 602 | Exact Beta8 app/web; all 11 served web records matched | Saved exported settings, 10 pool slots, 10 tuning slots and both schedules retained; manual override none | 525 MHz / 1150 mV | Accepted shares increased by 1; ASIC board 60.375°C, VR 55°C | `application/json` for pool and power schedule GETs |

The strict response checks observed HTTP 200 and the corrected JSON type on both `/api/5tratum/pool-schedule` and `/api/system/mining/schedule` for each unit. Both primary and fallback configurations were retained. The retained operating points are those units' observations, not recommended presets. Each accepted-share increase and sensor reading is a short same-boot observation, without a payout, new power-window boundary, fault test or soak claim.

Installed OS **v0.8.31**, source `c15cb275c0c191cf354a3581dfb1b68833130854`, completed actual strict GET-only control inspection on both models, including the same-response JSON headers for both schedules. Gamma 601 completed its original unchanged-state confirmation. After the historical paired and OS update-time retention checks, the owner changed Gamma 602 to 600 MHz, enabled overclocking and saved tuning slot 1. The owner confirmed those changes; a separate current-state inspection matched them and retained every other saved field. Its original 525 MHz paired report and journal were left unchanged, without an unchanged-state confirmation claim for the later configuration.

A separate deployed-OS 60-second GET-only observation passed seven samples for all four owned miners with a maximum inventory age of 5.06 seconds, fresh board telemetry and admitted pool/power/updater controls. The first observation retained two unavailable status reads separately. The normal updater completed once; 193 permanent package files matched, the successful MUX handoff consumed its marker, and nginx routes were preserved. No authenticated portal UI interaction or miner setting write is claimed by these read-only inspections.

Source provenance is tied to this clean compiled commit, manifest/source inventory and recreated WWW; Gamma's served build information does not independently advertise or attest the source commit. Exported API settings are not raw NVS or an unexported-password-byte backup. Keep private recovery material separately. Public evidence omits network addresses, device IDs, worker names and credentials.

The immutable manifest's `hardware_tested: false` is a pre-bench fact. This dated later qualification does not rewrite that flag or the package bytes.

## Limits

Gamma uses Primary/fallback, not Nerd native Dual A/B. MUX acknowledgment remains informational v1 status; rich per-session coin/job metadata and independent physical-chip mining are not added. This fix adds no automatic tuning, preset or ASIC-setting change. These short checks do not establish new power-window boundaries, thermal faults, sustained soak, all recovery paths, physical OLED readability or upstream payout. Central OS/MUX admission is qualified separately.
