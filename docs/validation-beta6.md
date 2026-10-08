# Gamma Beta6 validation

Published pair: **5tratumFW-0.1.0-beta.6**. Gamma PCB 601/602 and BM1370×1 only. Exact compiled source: `8de17c936b6669a40c548f7d647c189fd5899124`.

OS device inventory previously used Gamma system information without its MUX advertisement state, so connected Gamma units appeared as “MUX: not advertised”. This version adds the read-only `/api/5tratum/status` endpoint. It reports the same bound physical identity, exact hardware/firmware and the active failover pool. A connected MUX label requires the real current SV1 peer acknowledgment within 90 seconds. Work-generation, selected route and protocol checks suppress results during transitions.

The endpoint reports no per-ASIC rate, coin identity or job metadata; those values are absent rather than inferred from a pool address or Bitcoin-specific telemetry. It does not authorize remote control or independent ASIC assignment. The UI, saved settings, schedules, regulator, clocks, voltage, fans, NVS and partition layout retain their existing behavior.

## Evidence boundaries

The build helper runs the production handler/formatter and locked peer runtime tests, frontend tests, the pinned ESP-IDF5.5.3 target build, native OLED renders and SPIFFS reconstruction. Focused tests exercise exact model guards, current-session resets, 90-second expiry, route/protocol/work-generation changes, memory failure, origin checks and no-store responses. These checks do not establish physical operation.

The final package manifest records the exact source commit and app/WWW/source hashes. The immutable [Beta6 release validation asset](https://github.com/WillItMod/5tratumFW/releases/download/v0.1.0-beta.6/validation-beta6.md) records the final build and physical observations for this exact pair; [release downloads](https://github.com/WillItMod/5tratumFW/releases/tag/v0.1.0-beta.6) contain the matching images, manifest and checksums. Earlier Beta5 observations remain historical.

On 8 October 2026, one 601 and one 602 received the exact Beta6 application and WWW images. Both retained all 37 exported settings, 10 named pool slots, 10 tuning slots and both schedules, with unchanged saved operating points. All 11 served web assets match their built payloads. Short same-boot observations show accepted-share progress on each miner, and an independent installed OS discovery check confirms fresh MUX acknowledgments on both.

These checks do not establish physical OLED readability, sustained stability, wall-power savings or complete fault recovery. They do not repeat a complete power-control or protection-fault bench, and exported-settings comparison is not a raw NVS or credential backup. The OctAxe source/build candidate remains separately unqualified for physical installation.

Use only a matching Gamma app/WWW pair. A QAxe or OctAxe WWW image is incompatible despite sharing the filename.
