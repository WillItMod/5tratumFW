# QAxe Beta 3 validation

Paired version: **5tratumFW-qa-0.1.0-beta.3**. Target remains NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4. The test unit's physical PCB revision is unidentified.

This update makes the complete approved 5tratumFW artwork larger in the web header, without a separate typed name. Updates adds a QAxe-only **Check updates** catalog and matching application/web download links. The browser makes the public catalog request without miner authentication headers. It excludes other firmware families and incomplete image pairs; checks do not install or restart a miner. Beta 2's v1 update identity and explicit legacy OTP fallback remain in place.

The approved web logo bytes are unchanged: SHA256 `8bfb9a51b658a73e3a71ec2e87c1e7ff8a669831a902841b4052ed231c4d35a7`. Hardware initialization, mining drivers, protections, NVS, partition geometry, A/B work selection, schedules, profiles, screen artwork and coin-feed contracts are unchanged. This update changes web layout/catalog and the paired version.

## Qualification

The release's exact source commit and app/web/source hashes are recorded in `manifest.json` and `SHA256SUMS`. The pinned helper runs Angular/browser unit tests, production helper/host regressions, ESP-IDF 5.5.3 target compilation, 40 native LVGL fixtures and exact corresponding-source/WWW reconstruction. These checks use simulated boundaries and do not establish physical operation.

The release's separate validation asset records the final checks and any new installed-device observations. Earlier [Beta 1 physical observations](validation-qa-beta1.md) and [Beta 2 updater checks](validation-qa-beta2.md) remain historical records for their own images.

MUX 0.9.73's remote Dual-mode management switch admits Beta 1 only. Use the miner's Pool routing → Dual pool control for later versions. Native A/B registration/routing and coin/job injection do not use that remote-switch allowlist. A/B still share the ASIC chain; independent physical-chip mining remains unverified. No other model or physical PCB revision is newly qualified by this web change.

Follow [application-first paired installation](installation-5tratumfw-qa.md), preserve operating settings and keep a private unit-specific recovery backup. No factory/NVS image or automatic trial rollback is supplied.
