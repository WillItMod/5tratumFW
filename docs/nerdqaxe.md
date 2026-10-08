# NerdQAxe++ BETA

The current qualified QAxe BETA pair is `qaxe-v0.1.0-beta.5`. Use its family-specific matched assets, not Gamma images. Board target: **NERDQAXEPLUS2**, model **NerdQAxe++**, ASIC **BM1370×4**. The tested unit's PCB revision is unidentified; support is not extended to QAxe+, OctAxe or other revisions by a shared name.

The new `5tratumFW-qa-0.1.0-beta.5` matched pair is built from source `3ffbe84f43e265e61d5bae3eca1cd985def2bb3f`, with family CI and offline checks passed. Its exact pair completed a supervised OTA, 94-asset and exported-settings check on 8 October 2026. It places Primary/A and Secondary/B editors above the ten named pool slots, with separate connection save, named slot storage and explicit Apply destinations. Strict response checks require a valid acknowledgment and profile/settings readback; an unconfirmed result is not retried automatically. Model scope is unchanged, and [Beta5 validation](../firmware/nerdqaxe/docs/validation-qa-beta5.md) keeps Beta4's earlier observations separate.

There is one QAxe++ application target and filename, not separate 6.0 and 6.1 builds. The existing driver does not distinguish those revisions by power-connector type. The BETA is therefore not described as “6.1 only”, and neither revision has a separately recorded physical qualification. Install the application before the matching web interface when migrating from older upstream firmware; see the [model-detection troubleshooting](../firmware/nerdqaxe/docs/installation-5tratumfw-qa.md#if-the-update-page-cannot-identify-the-model).

- [Installation, matching image pair and USB bootloader recovery](../firmware/nerdqaxe/docs/installation-5tratumfw-qa.md)
- [Current Beta5 installed-pair validation](../firmware/nerdqaxe/docs/validation-qa-beta5.md)
- [Earlier Beta 4 connection-options correction and installed-pair validation](../firmware/nerdqaxe/docs/validation-qa-beta4.md)
- [Earlier Beta 3 validation](../firmware/nerdqaxe/docs/validation-qa-beta3.md)
- [Earlier Beta 2 updater correction](../firmware/nerdqaxe/docs/validation-qa-beta2.md)
- [Earlier Beta 1 installed-device observations](../firmware/nerdqaxe/docs/validation-qa-beta1.md)
- [Legacy updater correction and browser checks](../firmware/nerdqaxe/docs/validation-qa-legacy-update.md)
- [Native dual-MUX connections and limits](../firmware/nerdqaxe/docs/native-mux-pool-streams.md)
- [Ten pool/tuning slots and weekly pool switching](../firmware/nerdqaxe/docs/miner-profiles-and-pool-schedule.md)
- [Power scheduler and reversible ASIC pause](../firmware/nerdqaxe/docs/mining-power.md)
- [Build/package reproducibly](../firmware/nerdqaxe/docs/build-5tratumfw-qa.md)
- [Original upstream documentation](../firmware/nerdqaxe/docs/upstream-readme.md)

Navigation matches Gamma's six main pages while retaining both fan/PID controllers, thermal shutdown limits, direct SV1/SV2, optional verification, OTP, Alerts, InfluxDB, System and supported CAN options. External Bitcoin data belongs in Integrations; enhanced MUX coin/job context is attached to each native connection rather than assumed to be Bitcoin.

The physical screen has real scrolling startup events and a large 5tratumFW logo slide for three seconds every two minutes after thirty seconds idle, while mining telemetry is fresh. Shutdown, thermal, enrollment and animation screens take priority. Route/job pages show separate native sessions and shared board/regulator temperatures once. Native fixture renders are separate from physical-screen validation.

For the recurring logo, disable **Automatic screen off** under **Miner controls → Display**, save, and leave the physical screen on its mining overview for at least two minutes. Automatic screen off otherwise puts the display to sleep before the slide is due. The physical screen's **P1/P2** labels correspond to **A/B** in the web interface and MUX. See the [screen instructions](../firmware/nerdqaxe/docs/installation-5tratumfw-qa.md#physical-screen-and-recurring-logo).

Native A/B sessions are shown beneath one collapsible parent in compatible MUX/OS versions. Central Dual pool, Power and update controls have separate exact-release/model/source qualification; a connected session alone does not authorize them. Firmware Beta5 requires a central release that explicitly admits this pair. The miner's own Pool routing controls remain available under its existing security policy. Streams have separate targets/accounting and coin/job metadata, but use the whole-chain job selector. Independent physical-ASIC work assignment remains unsupported.
