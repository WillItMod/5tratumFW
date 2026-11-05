# NerdOctAxe Gamma Beta11: readiness diagnostics and bounded error recovery

`5tratumFW-oct-0.1.0-beta.11` is an experimental BETA for NERDOCTAXEGAMMA / BM1370 x8. It does not add board-revision compatibility or qualify a previously untested PCB. The release manifest binds the exact source and matching application/web images. Previous release files remain unchanged.

## Tuning readiness

The Start section shows the exact unmet requirements, including stale or missing sensors, fan feedback, ASIC work/error counters, paused mining and pool connection where available. Passing and failing checks can be expanded, and **Copy tuning diagnostics** provides the current report for support. Saved options, the run waiver and protected hardware limits remain required. Selecting 65 C is permitted when it is inside the board's reported limit; defaults remain ASIC 60 C and VRM 70 C.

QAxe and OctAxe identify the individual ASIC whose work/error counters are unavailable, stale or below the existing minimum-work check. These messages describe readiness evidence; they do not diagnose a damaged ASIC. The diagnostic GET uses cached data and never starts tuning or changes settings.

## Error recovery

An unhealthy sequence can use at most two small voltage correction trials. After that, remaining attempts use the existing bounded clock reduction, preventing voltage retries from consuming the entire correction budget. A measured safe point remains the first recovery choice when it is admissible.

The total remains six correction attempts, resetting only after a full passing measurement. Error thresholds, counter freshness guards, rejected-pair exclusion, one-coordinate actuation and protection checks are unchanged. If recovery is exhausted, the existing guarded original-settings restoration/resume process remains in charge; a real protection fault is never cleared automatically. This is not a promise that every miner can be made stable.

## Update

Install the matching **www.bin** and **esp-miner-NerdOCTAXE-Gamma.bin** from this same family and release. Do not mix Gamma, QAxe or OctAxe files.

In **5tratumOS**, open the miner's firmware controls, select **BETA**, choose **Check for updates**, confirm **NerdOctAxe Gamma Beta11**, then install. The existing dynamic release discovery supports this version without another OS update. Keep the miner powered through installation and verification. Reload the miner's page afterwards.

## Qualification

Software tests cover readiness failures, stale and contradictory diagnostics, save/readback failures, the reported error-correction sequence and automatic guarded recovery. Source/build/package and physical-check evidence are recorded separately in the [dated validation](../validation-reliability-20261105.md). Tests using simulated hardware are not physical verification.

## Rebuilding the source archive on Linux

The published firmware and corresponding source remain bound to the manifest's build commit. A portability fix to the host-test harness adds explicit standard-library headers; it changes no firmware code. The release includes `nerd-host-test-headers.patch` (SHA-256 `9e948414ff00d4a4a818a7f0f1e72e6c8ee125bbe036ae4730543264aea25587`). When rebuilding the extracted Nerd source archive on Linux, apply it from the archive root with `patch -p3 < nerd-host-test-headers.patch` before running the full build helper. Current repository source already includes the fix.
