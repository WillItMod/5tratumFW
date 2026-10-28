# NerdQAxe++ BETA

The current public QAxe BETA pair is [`qaxe-v0.1.0-beta.8`](https://github.com/WillItMod/5tratumFW/releases/tag/qaxe-v0.1.0-beta.8). Use its family-specific matched assets, not Gamma images. Board target: **NERDQAXEPLUS2**, model **NerdQAxe++**, ASIC **BM1370×4**. The tested unit's PCB revision is unidentified; support is not extended to QAxe+, OctAxe or other revisions by a shared name.

The `5tratumFW-qa-0.1.0-beta.8` pair is based on shufps v1.1.1-rc1 and was compiled from clean source `1a34b24100e1238e331d8145a70f6b08f85f943d`, with full local checks and final family CI passed. Its standard 320×170 pair completed supervised OTA, all 108 packaged web-record checks, exported-settings checks, manual pause/resume and a temporary tuning schedule at the existing operating point on 28 October 2026. All settings, slots and original policies were restored and accepted shares progressed. The owner confirmed the screen and both fans normal. These are short checks, not changed-point, cold power-cycle or sustained thermal qualification. See [aligned validation](validation-aligned-20261028.md) and [release notes](../firmware/nerdqaxe/docs/releases/qaxe-v0.1.0-beta.8.md).

Beta8 adds a [saved screen selector](../firmware/nerdqaxe/docs/display-settings.md)
and [saved tuning-profile schedule](../firmware/nerdqaxe/docs/tuning-profile-schedule.md).
To enable a larger panel, open **Miner controls → Display → Screen**, choose
**3.5-inch RGB** or **3.5-inch BGR**, select **Save this section**, then
**Restart device** and refresh to confirm the active choice. The selected profile
changes viewport, offset and mirror orientation together; the separate rotation
control handles 180° rotation. The **480×320 profiles remain experimental**:
geometry, gap and mirroring were cross-checked against online firmware sources,
but the actual larger panel still needs its own position, text, colour, startup
and refresh checks. The owned unit's standard screen does not qualify that panel.

There is one QAxe++ application target and filename, not separate 6.0 and 6.1 builds. The existing driver does not distinguish those revisions by power-connector type. The BETA is therefore not described as “6.1 only”, and neither revision has a separately recorded physical qualification. Install the application before the matching web interface when migrating from older upstream firmware; see the [model-detection troubleshooting](../firmware/nerdqaxe/docs/installation-5tratumfw-qa.md#if-the-update-page-cannot-identify-the-model).

- [Installation, matching image pair and USB bootloader recovery](../firmware/nerdqaxe/docs/installation-5tratumfw-qa.md)
- [Current Beta8 installed-pair validation](validation-aligned-20261028.md)
- [Earlier Beta6 installed-pair validation](../firmware/nerdqaxe/docs/validation-qa-beta6.md)
- [Earlier Beta5 installed-pair validation](../firmware/nerdqaxe/docs/validation-qa-beta5.md)
- [Earlier Beta 4 connection-options correction and installed-pair validation](../firmware/nerdqaxe/docs/validation-qa-beta4.md)
- [Earlier Beta 3 validation](../firmware/nerdqaxe/docs/validation-qa-beta3.md)
- [Earlier Beta 2 updater correction](../firmware/nerdqaxe/docs/validation-qa-beta2.md)
- [Earlier Beta 1 installed-device observations](../firmware/nerdqaxe/docs/validation-qa-beta1.md)
- [Legacy updater correction and browser checks](../firmware/nerdqaxe/docs/validation-qa-legacy-update.md)
- [Native dual-MUX connections and limits](../firmware/nerdqaxe/docs/native-mux-pool-streams.md)
- [Ten pool/tuning slots and weekly pool switching](../firmware/nerdqaxe/docs/miner-profiles-and-pool-schedule.md)
- [Power scheduler and reversible ASIC pause](../firmware/nerdqaxe/docs/mining-power.md)
- [Saved tuning profiles by time of day](../firmware/nerdqaxe/docs/tuning-profile-schedule.md)
- [Build/package reproducibly](../firmware/nerdqaxe/docs/build-5tratumfw-qa.md)
- [Original upstream documentation](../firmware/nerdqaxe/docs/upstream-readme.md)

Navigation matches Gamma's six main pages while retaining both fan/PID controllers, thermal shutdown limits, direct SV1/SV2, optional verification, OTP, Alerts, InfluxDB, System and supported CAN options. External Bitcoin data belongs in Integrations; enhanced MUX coin/job context is attached to each native connection rather than assumed to be Bitcoin.

The physical screen has real scrolling startup events and a large 5tratumFW logo slide for three seconds every two minutes after thirty seconds idle, while mining telemetry is fresh. Shutdown, thermal, enrollment and animation screens take priority. Route/job pages show separate native sessions and shared board/regulator temperatures once. Native fixture renders are separate from physical-screen validation.

For the recurring logo, disable **Automatic screen off** under **Miner controls → Display**, save, and leave the physical screen on its mining overview for at least two minutes. Automatic screen off otherwise puts the display to sleep before the slide is due. The physical screen's **P1/P2** labels correspond to **A/B** in the web interface and MUX. See the [screen instructions](../firmware/nerdqaxe/docs/installation-5tratumfw-qa.md#physical-screen-and-recurring-logo).

Native A/B sessions are shown beneath one collapsible parent in compatible MUX/OS versions. Central Dual pool, Power and update controls have separate exact-release/model/source qualification; a connected session alone does not authorize them. Beta8 requires a central release explicitly admitting this version and source; older exact-release policies do not automatically admit it. The miner's own controls remain available under its existing security policy. Streams have separate targets/accounting and coin/job metadata, but use the whole-chain job selector. Independent physical-ASIC work assignment remains unsupported and disabled. Tuning and pool schedules use a fixed UTC offset without automatic daylight-saving changes; the separate pause schedule retains its configured timezone/DST behaviour.
