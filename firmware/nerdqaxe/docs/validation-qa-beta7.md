# QAxe Beta7 display-selector validation

Date: 15 October 2026. Version: `5tratumFW-qa-0.1.0-beta.7`.
Compiled application, web provenance and source archive:
`223e00ba9678324a20f0c2409ee66b423363463f`.
The intended release tag is `qaxe-v0.1.0-beta.7`; this record documents a local
paired test build, not a published or installed release. Subsequent documentation
corrections do not change that compiled-source receipt. Beta6 assets and their
dated physical validation remain unchanged.

## Change and scope

The QAxe image adds a saved selector for Standard 320×170, 3.5-inch 480×320 RGB
and 3.5-inch 480×320 BGR. Existing settings keep the standard default. Both large
profiles remove the standard panel's single-axis mirror and support explicit
180-degree rotation. Panel inversion honours its saved setting while preserving
the old default appearance.

The active profile is latched at boot. Saving changes the next boot without
reinitializing a live DMA buffer or panel. Only `displayProfile` enters durable
settings. Active geometry, choices and restart state are read-only system
metadata; existing closed MUX capability/status and web provenance schemas are
unchanged. OctAxe exposes only its existing standard screen.

The profile changes the display viewport, draw buffer, pixel clock, colour
order, offset and orientation. Board guards, GPIO wiring, ASIC/regulator drivers,
voltage/frequency, fans, pools, slots, schedules and flash geometry are preserved.
No presets or tuning run during boot or this build.

## Completed software checks

- Pinned Node 24.14.0 frontend production build and 118 Chrome tests passed.
  Twenty additional web-control checks passed.
- The full helper invoked all 26 host-test entrypoints successfully. The optional
  production MUX wire test explicitly skipped because `MUX_SOURCE` was absent;
  it is not an end-to-end MUX pass for this build.
- Actual production settings handlers passed ASan/UBSan harnesses: 46 QAxe and
  50 OctAxe mixed-invalid cases made no settings, pool, flush or reload mutations.
  Valid profile saves, authentication, pending/running factory-update guards,
  durable-only export and reboot readback passed.
- Runtime tests checked profile persistence/latching, requested versus active
  state, stock defaults, both large profiles, rotation and inversion. Ten real
  CMake policy tests checked QAxe-only runtime admission and fixed-profile guards.
- Eleven OTA tests checked ESP32-S3/QA descriptor parsing, the Beta7 compatibility
  floor, malformed/truncated prefixes, read deadlines and rejection before ASIC
  shutdown or flash. Factory OTA is blocked while a large profile is saved or
  active; the settings guard closes the asynchronous worker race.
- Eleven package-identity tests checked actual compiler metadata, including
  stale, missing, conflicting and wrong-board/runtime flags.
- The pinned ESP-IDF 5.5.3 target application and WWW build completed. Native LVGL
  rendered 54 views at each resolution, including all six candidate celebrations.
  Candidate lifecycle tests completed 1,024 replacement events per viewport.
- An independent package audit verified the ESP descriptor, six segment XORs,
  appended image SHA256, every manifest/checksum entry, all 94 web assets and
  byte-identical WWW reconstruction. All 1,271 source-inventory files, tracked
  source, dependency locks and pinned submodule contents matched.

## Paired package

| Asset | Bytes | SHA256 |
| --- | ---: | --- |
| `esp-miner-NerdQAxe++.bin` | 3,581,056 | `9e05da1433514b246a98ee49d995f05946c99772f2a4c4c91853de1b1eec5f7b` |
| `www.bin` | 3,145,728 | `6f8455bcc542e605401c9bb3668255116af644ad48b13d3811432fec8dbb6202` |
| `5tratumFW-NerdQAxe++-5tratumFW-qa-0.1.0-beta.7-source.zip` | 24,549,856 | `8b254bb4247278cf458b5c481e8dd79d70307907bd822056144871188981d769` |

The application has 613,248 bytes of its 4 MiB OTA slot remaining. The complete
WWW image is 3 MiB; its actual compressed payload totals 1,684,378 bytes.
The verified compiler receipt uses standard default profile 0 with QAxe runtime
wide-display support enabled. The manifest declares all three saved choices,
marks both 480×320 profiles experimental and records `hardwareTested: false`.
There is no factory/NVS image.

## Physical and integration limits

No image from this Beta7 pair was installed during these checks. The owned
standard-screen QAxe did not respond to the final HTTP connection checks.
No actual 480×320 panel was available; physical orientation, colours, refresh,
startup and controller compatibility remain unverified. A mirrored photograph
does not identify the controller or a vendor's flash layout. Native rendering,
host tests and compilation cannot establish those physical properties.

Central OS/MUX native controls still require exact Beta7 version/source admission.
The central settings-preservation comparison also needs to normalize the newly
introduced default `displayProfile` when upgrading older firmware. This test
build is not promoted to the general update catalog by these changes.

For panel testing, use the matching QAxe application and WWW pair, then follow
[Screen selection](display-settings.md): save the fitted screen, restart and
refresh the page. Confirm the physical display before treating either larger
profile as qualified. Existing Beta6 hardware observations must not be copied
into this build's record.
