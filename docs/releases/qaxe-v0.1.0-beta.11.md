# NerdQAxe++ Beta11: experimental faster ASIC-error rejection

`5tratumFW-qa-0.1.0-beta.11` · tag `qaxe-v0.1.0-beta.11` · build source: see this release's `manifest.json`.

For **NERDQAXEPLUS2 / NerdQAxe++ with four BM1370 ASICs only**. This new experimental BETA retains NerdQAxe++ Beta10's adaptive-tuning features and adds earlier rejection of unstable trial settings. It is a new version, not a replacement of the previous release's files. The upstream base remains shufps v1.1.1-rc1.

Tuning remains **off until explicitly started**. Every Start requires saved compatible options, an unticked per-run risk acknowledgement and a verified immutable original-settings backup. Boot and OTA never restart tuning automatically, including when Continuous On was saved.

| Download | Purpose |
| --- | --- |
| [esp-miner-NerdQAxe++.bin](https://github.com/WillItMod/5tratumFW/releases/download/qaxe-v0.1.0-beta.11/esp-miner-NerdQAxe%2B%2B.bin) | Model-specific application |
| [www.bin](https://github.com/WillItMod/5tratumFW/releases/download/qaxe-v0.1.0-beta.11/www.bin) | Required matching web interface from this release |
| [SHA256SUMS](https://github.com/WillItMod/5tratumFW/releases/download/qaxe-v0.1.0-beta.11/SHA256SUMS) | Verify the package bytes |

The release also includes its manifest and corresponding source. No factory, replacement-NVS or partition-table image is published. Files from different hardware families are not interchangeable.

## What changed

The tuner can now reject an unstable baseline or trial before the full performance comparison finishes. It uses fresh, distinct hardware-counter intervals captured after the current point was confirmed; repeated page refreshes supply no extra evidence.

| ASIC-error evidence | Response |
| --- | --- |
| At least3% board errors or at least 5% on any required ASIC | Reject the first qualifying fresh interval, after excluding the possible old 15-second counter interval |
| Above 0.5% board errors or above 1% on any required ASIC | Reject after at least three consecutive fresh intervals spanning at least 10 seconds, after the same guard |
| Clean or invalid interval, or a gap longer than 15 seconds | Reset the consecutive-error sequence; stale or duplicate readings cannot advance it |

The earliest qualifying software timings are **15 seconds for severe errors** and **25 seconds for persistent ordinary errors**, counted from Start or confirmed application of a setting. They are not guaranteed physical response times. A held or monitored result retains its immediate response to a fresh error-limit breach.

Successful performance comparisons still require the full observation. After an applied change, the usual 30-second settling, 15-second counter exclusion and 60-second measurement remain, a minimum of **105 seconds** with at least eight accepted distinct samples. Early rejection reports the actual decision readings, not a completed mean or a claimed improvement.

The controller first tries to return to a point that passed a complete clean measurement window, within current bounds and temperature/power headroom, changing only clock or voltage through the existing actuator. This measured stable point is separate from the immutable pre-tuning backup. If no permitted return is available, the bounded correction policy can consider **+5mV only with thermal and electrical headroom and within the saved voltage ceiling**, otherwise it reduces clock.

Up to **16 exact error-rejected clock/voltage pairs** are excluded for the current run, including later Continuous rounds and option changes. A new Start or boot clears that RAM history. Candidates, corrections, backoffs and rollbacks cannot retry an excluded pair. Six consecutive corrections are permitted; a seventh failed evaluation, full exclusion list or exhausted permitted fallback stops experiments and requests the existing guarded original-settings recovery. A verified stable point may instead be held when no new nearby candidate is permitted.

That recovery can pause, cool, restore and independently verify the immutable original settings, then resume normal mining with tuning off. A real hardware fault, unsafe temperatures, missing telemetry, invalid backup, failed verification or newer manual/scheduled ownership blocks or cancels it. It never clears a hardware fault, reboots the MCU or retries indefinitely. The UI explains early rejection, stable-point return and exhaustion; **Rejections this run** includes early and held-point error rejections.

The existing three measured priorities, Live dashboard, Adjust this tune, gap-aware graphs, current-work pool/network difficulty and read-only physical Tuning page remain. Independent per-chip tuning/routing is not implemented.

## Update and operate

Keep your existing 5tratumOS channel: **0.8.50 on MAIN** or **0.8.50-dev on DEV**, then reload the portal. Both OS channels discover these same firmware releases through the miner's **BETA** channel. A new OS release is not needed for every compatible firmware beta.

Before updating a miner already running tuning, select **Disable and restore backup** and wait for original saved/live settings to be confirmed restored and tuning inactive. An accepted request alone is not completed restoration. If it is already paused and ordinary restoration is waiting, use the separate **Restore original settings and resume mining** action and wait for verification. Do not clear a fault or erase settings to force an update.

On the correct miner select **Firmware update → BETA → Check for updates**, confirm **NerdQAxe++ Beta11**, then **Install firmware → Confirm install**. Keep it powered until **Firmware installation verified**. Open the miner, fully reload, and verify application/web versions, original settings, fans and fresh mining telemetry. Initial conversion from stock firmware uses the [model-specific manual guide](../../firmware/nerdqaxe/docs/installation-5tratumfw-qa.md); the OS updater does not convert stock firmware.

For manual OTA, verify both checksums, upload `esp-miner-NerdQAxe++.bin` first, wait for restart, then upload this same release's `www.bin`. Follow the model guide's refresh/restart checks and verify both versions and original settings. Both files are required; QAxe does not gain Gamma's embedded-web update format.

For tuning, save the objective and compatible limits, starting with **60°C ASIC /70°C VRM**, or lower where the installed protection requires it. These are project starting targets, not universal certified safe limits. Performance cannot test above its saved clock ceiling. Read and explicitly accept the experimental risk acknowledgement for this run before Start. No waiver is preselected. Later runs do not replace the identity-bound original backup. Keep a separate private recovery backup, and verify completed restoration before another firmware update or downgrade.

## Qualification

The fast-error implementation at source `f848a3825a6abbac1785389abb8043195ddf82d0` has offline controller, runtime/API, UI and target-build checks recorded in [fast-error software validation](../validation-autotune-fast-errors-20261104.md). Those private builds used the previous version markers and are not replacement release packages. The new version's manifest and checksums identify its exact build/source and images; the earlier physical observations in [3 November validation](../validation-autotune-release-20261103.md) remain limited to their own images.

**No physical fast-error test or installation of this new release is claimed.** Software simulation and target compilation do not establish thermal stability, a hashrate/efficiency gain or every electrical fault. The test QAxe PCB revision remains unidentified. Standard 320×170 stays the default; the saved 480×320 RGB/BGR larger-panel paths remain physically unqualified. These files are not for QAxe+, OctAxe or other board variants. Existing hardware protection remains active and is not bypassed by tuning. Earlier evidence and release assets remain unchanged.
