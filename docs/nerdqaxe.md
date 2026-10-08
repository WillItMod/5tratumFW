# NerdQAxe++ BETA

Use the QAxe-specific `qaxe-v0.1.0-beta.4` prerelease pair, not Gamma assets. Board target: **NERDQAXEPLUS2**, model **NerdQAxe++**, ASIC **BM1370×4**. The tested unit's PCB revision is unidentified; support is not extended to QAxe+, OctAxe or other revisions by a shared name.

There is one QAxe++ application target and filename, not separate 6.0 and 6.1 builds. The existing driver does not distinguish those revisions by power-connector type. The BETA is therefore not described as “6.1 only”, and neither revision has a separately recorded physical qualification. Install the application before the matching web interface when migrating from older upstream firmware; see the [model-detection troubleshooting](../firmware/nerdqaxe/docs/installation-5tratumfw-qa.md#if-the-update-page-cannot-identify-the-model).

- [Installation, matching image pair and USB bootloader recovery](../firmware/nerdqaxe/docs/installation-5tratumfw-qa.md)
- [Beta 4 connection-options correction and installed-pair validation](../firmware/nerdqaxe/docs/validation-qa-beta4.md)
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

MUX 0.9.73 recognizes A/B sessions beneath one collapsible parent, including Beta 2/3. Its Miners → Dual MUX mining management switch admits Beta 1 only; Beta 2/3 must use the miner's own Pool routing → Dual pool control until MUX adds this version to its qualified control list. Existing native A/B routing and coin/job injection do not use that management-switch allowlist. Streams have separate targets/accounting and coin/job metadata, but still use the existing whole-chain job selector. Independent physical-ASIC work assignment remains unverified.
