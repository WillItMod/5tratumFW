# NerdOctAxe Gamma Beta9: experimental error rejection and advanced clock override

`5tratumFW-oct-0.1.0-beta.9` · tag `octaxe-v0.1.0-beta.9` · build source: see this release's `manifest.json`.

For **NERDOCTAXEGAMMA / NerdOCTAXE-γ with eight BM1370 ASICs only**. This new experimental BETA retains NerdOctAxe Gamma Beta8's adaptive-tuning features and adds earlier rejection of unstable trial settings plus an explicit advanced clock override. It is a new version, not a replacement of the previous release's files. The upstream base remains shufps v1.1.1-rc1.

Tuning remains **off until explicitly started**. Every Start requires saved compatible options, an unticked per-run risk acknowledgement and a verified immutable original-settings backup. Boot and OTA never restart tuning automatically, including when Continuous On was saved.

| Download | Purpose |
| --- | --- |
| [esp-miner-NerdOCTAXE-Gamma.bin](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.9/esp-miner-NerdOCTAXE-Gamma.bin) | Model-specific application |
| [www.bin](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.9/www.bin) | Required matching web interface from this release |
| [SHA256SUMS](https://github.com/WillItMod/5tratumFW/releases/download/octaxe-v0.1.0-beta.9/SHA256SUMS) | Verify the package bytes |

The release also includes its manifest and corresponding source. No factory, replacement-NVS or partition-table image is published. Files from different hardware families are not interchangeable.

## Advanced clock override

All three families add **Advanced clock override**, stored as the explicit boolean `advancedClockOverride`. It defaults off. The warning checkbox starts unticked, including after reloading a saved enabled configuration. Read and acknowledge it, choose a compatible maximum, then select **Save options** and wait for configuration readback. Checking the box alone does not write to the miner or start tuning. Starting an advanced run also requires the separate ordinary per-run waiver and an explicit advanced acknowledgement for that Start.

The additional **1,200 MHz experimental project ceiling** is an engineering software boundary, not a hardware rating, safe frequency or promised result. Raising the saved maximum only permits a wider measured search; it does not command an immediate jump to that clock. The page still reports the detected profile's standard maximum. Existing voltage, power, ASIC/VRM temperature, sensor, fan and hardware-fault constraints remain authoritative. This changes the whole miner's clock range, not individual ASIC clocks.

Neither an old report nor an existing higher manual clock implies consent. Reports without valid override metadata leave the advanced controls locked; legacy saved tuning options read with the override disabled. If an idle miner is already running a higher manual point, saving disabled permission does not change that live clock. Start remains blocked until you explicitly acknowledge and save an advanced range that includes the current point, or lower it through the existing manual controls. An active run cannot save bounds that exclude its confirmed current point.

The immutable original backup is independent of this permission and its format is unchanged. A verified original point such as 750 MHz on Gamma or 900 MHz on Nerd remains available to the existing protected restoration path even when the tuning override is disabled; those examples do not establish safe operating ratings or grant tuning consent. Restoration still requires its normal storage, protection and live-readback checks. Later tuning runs never replace that original backup.

The option is saved atomically with the existing tuning configuration. The legacy nine-field record is accepted with `advancedClockOverride: false`; the new canonical record includes a real boolean. This is not a promise that older firmware can read the new tuning-options format. Disable tuning, verify original restoration and review exact settings/journal compatibility before a manual downgrade. No original-backup format migration is introduced.

PLL support is checked before sending a requested setting to the ASIC driver. Unsupported clock points are skipped without emitting invalid PLL frames. The fast-error rejection, stable-point return and guarded recovery below remain in force. **No physical qualification of the advanced override or this new release is claimed.**

## Faster ASIC-error rejection

The tuner can now reject an unstable baseline or trial before the full performance comparison finishes. It uses fresh, distinct hardware-counter intervals captured after the current point was confirmed; repeated page refreshes supply no extra evidence.

| ASIC-error evidence | Response |
| --- | --- |
| At least 3% board errors or at least 5% on any required ASIC | Reject the first qualifying fresh interval, after excluding the possible old 15-second counter interval |
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

On the correct miner select **Firmware update → BETA → Check for updates**, confirm **NerdOctAxe Gamma Beta9**, then **Install firmware → Confirm install**. Keep it powered until **Firmware installation verified**. Open the miner, fully reload, and verify application/web versions, original settings, fans and fresh mining telemetry. Initial conversion from stock firmware uses the [model-specific manual guide](../../firmware/nerdqaxe/docs/installation-5tratumfw-oct.md); the OS updater does not convert stock firmware.

For manual OTA, verify both checksums, upload `esp-miner-NerdOCTAXE-Gamma.bin` first, wait for restart, then upload this same release's `www.bin`. Follow the model guide's refresh/restart checks and verify both versions and original settings. Both files are required; OctAxe does not gain Gamma's embedded-web update format.

For tuning, save the objective and compatible limits, starting with **60°C ASIC /70°C VRM**, or lower where the installed protection requires it. These are project starting targets, not universal certified safe limits. Performance cannot test above its saved clock ceiling. Read and explicitly accept the experimental risk acknowledgement for this run before Start. No waiver is preselected. Later runs do not replace the identity-bound original backup. Keep a separate private recovery backup, and verify completed restoration before another firmware update or downgrade.

## Qualification

The fast-error implementation at source `f848a3825a6abbac1785389abb8043195ddf82d0` has offline controller, runtime/API, UI and target-build checks recorded in [fast-error software validation](../validation-autotune-fast-errors-20261104.md). That record predates the advanced clock override. Those private builds used the previous version markers and are not replacement release packages; they do not qualify the new override. The new version's manifest and checksums identify its exact build/source and images; the earlier physical observations in [3 November validation](../validation-autotune-release-20261103.md) remain limited to their own images.

**No physical fast-error or advanced-override test, or installation of this new release, is claimed.** Software simulation and target compilation do not establish thermal stability, a hashrate/efficiency gain or every electrical fault. Earlier OctAxe observations apply only to the owned PCB 2.2 assembly and verified 16 MiB layout. Standard 320×170 stays the default; the saved 480×320 RGB/BGR larger-panel paths remain physically unqualified. The 480×320 geometry is a working assumption, not an identified vendor/controller match. Other revisions need separate qualification. Existing hardware protection remains active and is not bypassed by tuning. Earlier evidence and release assets remain unchanged.
