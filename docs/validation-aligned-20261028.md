# Aligned update validation — 28 October 2026

These BETA candidates have separate local build and supervised installed-device evidence below. Public Gamma Beta9, QAxe Beta6 and OctAxe Beta4 downloads remain unchanged. The short bench does not qualify other hardware or the experimental larger display.

The corrected Gamma Beta10 was compiled and packaged from clean commit `a1f8dc9bb3756cbd106e0883158a295f6ae9016a`. The initial installed Gamma candidate from `b0451dc657f1d1f76d0e927c04c788295ddd1f87` is preserved privately as superseded; its short mining observations below belong to that initial image. QAxe Beta8 and OctAxe Beta5 were compiled and packaged from clean commit `1a34b24100e1238e331d8145a70f6b08f85f943d`. Nerd production firmware and web source are identical across those source revisions. A host-test header correction at `1a34b241` makes the Nerd regulator fixture portable on Linux; the later Gamma-only journal correction does not change either Nerd application. The earlier Nerd builds are retained privately as superseded candidates. Each manifest records its own exact compiled/source revision, checksums and family. Later documentation commits do not relabel these binaries.

## Source and build evidence

| Check | Result |
| --- | --- |
| Gamma base | ESP-Miner v2.15.4, `d212a0f948cb3ffd90c7ca9572e9b2cf2d865d80`; ESP-IDF 6.0.2, LVGL 9.5.0. |
| QAxe/OctAxe base | shufps v1.1.1-rc1, `bfd07393918ce1b464faf1b7bd1ecd11343197f4`; pinned ESP-IDF 5.5.3. |
| Gamma frontend | Production build and **172/172** browser tests passed with Node 24.14.0. |
| Nerd frontend | Both family production builds and **146/146** browser tests per full helper passed with Node 24.14.0. |
| Gamma host suite | All **13 entry points** passed: boot identity/preservation, display admission, package verification, status, version, hashrate, power runtime, weekly policies/runtime, MUX, profiles, pool schedule and tuning schedule. |
| Settings compatibility | Fixed pre-upgrade journal bytes and saved slots replay without load-time writes; old key IDs retained. All 19 production profile tests passed with an exact verified Beta9 decoder oracle, all four raw-auth baselines, 28↔30 cache/reboot transitions, persisted-before-error rollback, and first queued operating writes. New saved pool authentication round-trips; absent legacy flags preserve the destination. Init/hash errors cannot publish a partial device identity. |
| Power transitions | Production code checks lower-clock success before lowering voltage, and voltage success before upclocking. Failed requests do not become successful applied state. Atomic tuning snapshots prevent mixed pairs. |
| Tuning schedule | Weekly wrap/midnight, invalid/duplicate/missing-slot input, synchronized-time requirement, fixed offsets, runtime retry, manual override and pause/protection boundaries passed in host fixtures. |
| Nerd host suite | Both complete helpers passed all mandatory host entry points; the optional separate MUX-source contract test reported its expected skip. No fake service was a physical miner. |
| Gamma display fixtures | **36** native 128×32 LVGL 9.5.0 views and production screen lifecycle passed with sanitizers; contact sheet inspected. |
| Nerd display fixtures | Shared production renderer checked **54** pages at each 320×170 and 480×320 viewport, six candidate designs and 1,024-event lifecycle cases; driver/button and safety priorities retained. These are simulated fixtures. OctAxe remains 320×170 only. |
| Responsive browser check | Both schedulers inspected at 1440px and 390px, including 00:00/09:00/21:00 named profiles, dirty/save/refresh behaviour and fixed-offset warning. Nerd responsive viewport corrected. All APIs were fake; writes rejected. |
| Certificate bundle | Exact pinned 118 roots, positive control and rejection of twelve incident chains passed for both bases. Every target application contains the expected compiled trust store. |
| Target compilation | All three ESP32-S3 applications and matching WWW images passed. No factory/NVS images were produced for delivery. |
| Package verification | Descriptors, family/version, selected build receipts, source inventories, web budget and image checksums passed. Gamma's ELF-bound embedded table matched every URL, gzip flag and payload byte of the current 11-asset interface; independently recreated SPIFFS matched WWW byte-for-byte. |

The complete local Linux host checks also passed at `1a34b241`: **31/31 Nerd scripts** and **13/13 Gamma entry points**, with Node 24.14.0, Ubuntu 24.04, GCC 13 and Clang 18. Native Linux OLED, both Nerd viewports and 1,024-event candidate lifecycle checks also passed with sanitizers. The initial GitHub Linux failure was a host fixture using `<cmath>` while the production regulator uses `<math.h>`; the test now matches that boundary. No production regulator logic changed in that correction.

All three fresh GitHub PR checks passed for source `1a34b241`: [Gamma](https://github.com/WillItMod/5tratumFW/actions/runs/40711985750), [QAxe](https://github.com/WillItMod/5tratumFW/actions/runs/40711985969), [OctAxe](https://github.com/WillItMod/5tratumFW/actions/runs/40711985808). These are independent Linux builds and package checks; their generated binaries are not asserted byte-identical to the local candidates. Gamma's CI target used the official pinned SDK6 container.

Gamma's local build used clean official SDK 6.0.2 source `7101770dc6db2667b3c477cc31365dd1acd6db4e` and its pinned Linux ARM64 GCC 15.2.0 tools. The cached SDK 5.5.3 container supplied the Linux runtime only. Its actual identity, source/tool archives and compiler are recorded in `build-provenance.json`; this is not presented as an execution of the official SDK6 container. The standard helper and CI pin the official SDK6 image documented in [build instructions](build.md). The original web-budget baseline is retained as a fixed budget, independently of the newer firmware base.

## Matched local images

| Family | Version | App bytes | App SHA-256 |
| --- | --- | ---: | --- |
| Gamma 601/602 | `5tratumFW-0.1.0-beta.10` | 2,798,656 | `9cea42ab5440ea82df4ad3d03342332b0c2ddcca3353d45245efd58f8a38b8ed` |
| QAxe++ | `5tratumFW-qa-0.1.0-beta.8` | 3,579,648 | `e48aa9d509b71a0a5b53509dea214b83f4aa9e13cc0e164f17513f77677d43ec` |
| OctAxe Gamma | `5tratumFW-oct-0.1.0-beta.5` | 3,585,264 | `617efbabb05c1944f5de98df084ec4bcb2bb98ae62c5538213e033747e54f866` |

Every WWW image is 3,145,728 bytes. Gamma's measured web payload is 624,901 bytes; QAxe's is 2,407,646 and OctAxe's is 2,407,650. Each package includes its own matched WWW, source, manifest, budget and checksums. Shared filenames do not make the packages interchangeable.

## Supervised installed-device checks

After the owner brought the four miners online, each application and matching WWW image was installed one device at a time, using fresh hardware/identity, original-settings and package-hash checks. Existing published rollback pairs were privately verified before installation; no factory or NVS image was written. All four rebooted into their new applications with their original device identities, configured points and ten tuning plus ten pool slots preserved. Gamma served all 11 matching web records. Each Nerd interface served 59 exact gzip URLs; their 49 decoded raw duplicates also matched, checking all 108 packaged records.

| Owned hardware | Installed observation |
| --- | --- |
| Gamma PCB 601 | Initial Beta10 pair: 580 MHz / 1155 mV retained; accepted shares progressed with zero rejected in the recorded new session. Owner confirmed the physical screen and fan normal. |
| Gamma PCB 602 | Initial Beta10 pair: 600 MHz / 1150 mV retained; accepted shares progressed with zero rejected in the recorded new session. |
| NerdQAxe++ BM1370×4, PCB revision unidentified | Beta8 standard display pair: 500 MHz / 1130 mV retained; approximately 4 TH/s and accepted shares. Reported applied pause/resume and a temporary same-point tuning schedule passed; original settings, manual override, disabled empty schedule and all slots were restored. Owner confirmed the physical screen and both fans normal. |
| NerdOCTAXE-γ BM1370×8, owned PCB 2.2 | Beta5 standard display pair: 700 MHz / 1180 mV retained; approximately 11.4 TH/s and accepted shares. Reported applied pause/resume and a temporary same-point tuning schedule passed; original settings, manual override, disabled empty schedule and all slots were restored. Owner confirmed the physical screen and both fans normal. |

The Gamma schedule test was held when review identified that the initial 30-entry operating journal would prevent Beta9 startup after a tuning write. The compatibility correction retains the legacy 28-entry format when authentication values match the original raw keys; an actual authentication change can require 30 entries. The journal-corrected Gamma application has passed all 13 Gamma host entry points on both Mac and Linux, including the 19 profile cases, actual SDK6 target compilation, certificate and exact embedded-web/WWW/source packaging checks. The WWW image is byte-identical to the first candidate. A temporary connectivity interruption stopped the first refresh preflight before any update POST. On recovery, the three units not intentionally restarted reported power-on reset classification with startup times around 16:17 UTC; the physical cause was not established. Both journal-corrected Gamma applications were then installed with their existing WWW. Gamma 601 passed reported applied pause/resume, temporary same-point schedule and subsequent normal software-restart/settings-replay checks. The 602 check identified a separate imported mDNS conflict handler automatically persisting a suffixed hostname when both units shared their configured name. Its clock, voltage, pools and slots were unchanged. The final correction keeps a unique runtime discovery alias without changing the configured hostname; it requires a new exact build receipt and bench before final qualification. The initial installed application was 2,798,560 bytes, SHA-256 `93f591fba971869059f0488c840f3f24b6d97d97bfea2b8af6bdadbf8158a4b8`; its archive and bench evidence are retained privately.

## Physical checks still required

Tadets' larger panel is available only on his miner. Its controller cannot be identified from the photograph; verify full-screen position, readable unmirrored text, RGB/BGR colour order, rotation, startup and refresh using the saved screen selector. The online driver comparisons are recorded in the [display reference check](../firmware/nerdqaxe/docs/experimental-480-display.md#online-firmware-reference-check--28-october-2026).

The short same-point bench does not exercise a different frequency/voltage pair, establish electrical or sustained thermal stability, or test every protection fault. Final Gamma checks remain pending the mDNS-preserving application at this point in the record. Imported board/display definitions do not expand the supported families. Gamma retains two active routes and ten saved pool slots through its compatibility layer; upstream eight-active-pool storage is not enabled.

Tuning and pool schedules use a fixed UTC offset and do not automatically change for daylight saving time. The pause schedule retains its configured timezone/DST behaviour. Hardware protection may override a requested operating point.

Independent per-chip BM1370 work remains **unsupported and disabled**. Current primary-source rechecks do not establish an addressed work command or chip-local acceptance mechanism. The bounded passive-capture collector has fifteen host tests but no new physical capture. See the [investigation](../firmware/nerdqaxe/docs/asic-work-targeting-investigation.md). These packages do not claim four independently routed chips.
