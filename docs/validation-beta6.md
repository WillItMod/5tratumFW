# Gamma Beta6 validation

Paired candidate: **5tratumFW-0.1.0-beta.6**. Gamma PCB601/602 and BM1370×1 only.

OS device inventory previously used Gamma system information without its MUX advertisement state, so connected Gamma units appeared as “MUX: not advertised”. This version adds the read-only `/api/5tratum/status` endpoint. It reports the same bound physical identity, exact hardware/firmware and the active failover pool. A connected MUX label requires the real current SV1 peer acknowledgment within 90 seconds. Work-generation, selected route and protocol checks suppress results during transitions.

The endpoint reports no per-ASIC rate, coin identity or job metadata; those values are absent rather than inferred from a pool address or Bitcoin-specific telemetry. It does not authorize remote control or independent ASIC assignment. The UI, saved settings, schedules, regulator, clocks, voltage, fans, NVS and partition layout retain their existing behavior.

## Evidence boundaries

The build helper runs the production handler/formatter and locked peer runtime tests, frontend tests, the pinned ESP-IDF5.5.3 target build, native OLED renders and SPIFFS reconstruction. Focused tests exercise exact model guards, current-session resets, 90-second expiry, route/protocol/work-generation changes, memory failure, origin checks and no-store responses. These checks do not establish physical operation.

The final package manifest records the exact source commit and app/WWW/source hashes. Any physical paired OTA, retained settings, accepted shares and OS status observations belong in a separate release validation asset. Beta5 was installed and checked on a601; its historical record does not qualify this candidate.

Use only a matching Gamma app/WWW pair. A QAxe or OctAxe WWW image is incompatible despite sharing the filename.
