# OctAxe Beta 1 candidate validation

Paired candidate version: **`5tratumFW-oct-0.1.0-beta.1`**. Intended tag: **`octaxe-v0.1.0-beta.1`**. Exact software target: **NERDOCTAXEGAMMA / NerdOCTAXE-γ / BM1370 ×8 / ESP32-S3**.

This is a development validation record, not a published-release or physical compatibility claim. The test unit's PCB revision, regulator population and flash/partition layout remain unidentified or unqualified. No OTA/USB installation or physical power/display/MUX result is asserted here. Gamma Beta 5 and QAxe Beta 3 remain unchanged; their physical observations do not qualify OctAxe.

## Source changes and host evidence

Reviewed on 8 October 2026. Tests compile production bodies with mocked external platform/hardware boundaries. Final clean target-build, source-commit and package hashes must be recorded separately before a candidate is used.

| Check | Recorded result and boundary |
| --- | --- |
| Oct board fixture | 11 ASan/UBSan tests passed: actual constructor, selected four/six-phase profile, saved-setting loading, TMP451 channels, failed reads/reinitialization, UART temperature fallback and shutdown behavior; hardware and inherited defaults mocked |
| Mining driver/control | Full sanitizer matrix passed, including 54 Oct profile scenarios: exact model/BM1370/count/CAN admission, eight enumeration responses, partial/extra/zero count rejection, startup failure, pause/resume, generation retirement and protection interruption; UART/GPIO/Buck mocked |
| Native subscriptions | 7 tests passed using actual StratumApi subscribe/send: Oct ASCII alias, one parent identity with A/B indices, partial writes, strict unsafe-model rejection and missing-identity fallback; sockets/identity mocked |
| Capabilities/live status | 13 capability and 12 status tests passed: exact existing UTF-8 γ model, eight counters/descriptors, strict malformed label rejection and informational MUX freshness/context semantics |
| Family package/metadata | 10 tests passed: separate identities, actual Oct model/agent constant assignments, wrong-model/header/count/descriptor/board/version/geometry rejection, diagnostics OFF and OTA-only package contract; image/config fixtures synthetic, SPIFFS recreation mocked |
| Frontend | 36 focused service/settings tests passed; QA and Oct production frontend configurations compiled separately with their own version markers; no target application or device tested by this check |

Four-phase TPS53647 source bounds retain the observed stock v1.0.36 software limits. The existing GPIO3 regulator strap selects TPS53647 or TPS53667 behavior; neither selection proves physical PCB identity. Missing TMP451 muxes retain the base UART temperature-request path. Missing/invalid chip readings remain unavailable, finite hot readings remain visible to protection, and shared board/regulator readings retain their source distinction.

Independent physical-ASIC work assignment remains false. Native A/B streams share the existing chain job selector. A compiled native advertisement or fresh coin/job fixture is not proof that a deployed MUX has admitted this version for remote management.

## Required physical qualification

1. Establish the exact PCB, regulator population/strap, sensor/fan mappings, actual controller flash/partition geometry and a verified unit-specific recovery path.
2. Audit the clean pinned target build: app descriptor, Oct web marker and all served assets, diagnostics OFF, corresponding source, full WWW reconstruction, hashes and manifest assumptions. Record the exact commit and image pair.
3. Supervise application-first paired OTA on that identified unit. Verify model/eight ASICs, both versions and all saved frequency/voltage/version-rolling/fan/network/pool/slot/schedule values. No boot/OTA presets or NVS erase.
4. Observe fresh eight-counter data, both fans, real board/VR/chip sensor behavior and accepted shares on intended routes. Check cold start, reboot, pause/resume and schedule behavior with actual ASIC/regulator power readings; the controller remains powered during pause.
5. Qualify thermal/fan/UART/regulator failure response and sustained operation separately. Host cancellation/fault fixtures cannot replace physical protection testing.
6. Verify the physical branded screen, scrolling startup, recurring large logo and both chip pages. Native framebuffer renders are simulated pixels, not screen photographs.
7. Verify actual MUX A/B parent grouping and fresh per-session coin/block/job injection, including disconnect/expiry and mixed routes. Record OS/MUX versions and their remote-control admission limits without inventing fixed chip owners.

The candidate manifest records `hardwareTested: false`, `physicalLayoutVerified: false` and `independentWorkAssignment: false`. Any later bench result must identify its exact unit/build and be recorded without rewriting historical release packages or extending support to untested revisions.
