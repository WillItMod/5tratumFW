# Aligned update validation — 28 October 2026

These are local, uninstalled BETA candidates. Public Gamma Beta9, QAxe Beta6 and OctAxe Beta4 downloads retain their existing evidence. This record does not qualify their successors for physical operation.

Gamma Beta10 was compiled and packaged from clean commit `b0451dc657f1d1f76d0e927c04c788295ddd1f87`. QAxe Beta8 and OctAxe Beta5 were compiled and packaged from clean commit `9874c765bd0b41136df28dba8e7220d56ed9853f`. The entire Nerd subtree is identical between those commits; subsequent root Gamma changes do not alter either Nerd package. Each manifest records its own exact compiled/source revision, checksums and family. Later documentation commits do not relabel these binaries.

## Source and build evidence

| Check | Result |
| --- | --- |
| Gamma base | ESP-Miner v2.15.4, `d212a0f948cb3ffd90c7ca9572e9b2cf2d865d80`; ESP-IDF 6.0.2, LVGL 9.5.0. |
| QAxe/OctAxe base | shufps v1.1.1-rc1, `bfd07393918ce1b464faf1b7bd1ecd11343197f4`; pinned ESP-IDF 5.5.3. |
| Gamma frontend | Production build and **172/172** browser tests passed with Node 24.14.0. |
| Nerd frontend | Both family production builds and **146/146** browser tests per full helper passed with Node 24.14.0. |
| Gamma host suite | All **13 entry points** passed: boot identity/preservation, display admission, package verification, status, version, hashrate, power runtime, weekly policies/runtime, MUX, profiles, pool schedule and tuning schedule. |
| Settings compatibility | Fixed pre-upgrade journal bytes and saved slots replay without load-time writes; old key IDs retained. New saved pool authentication round-trips; absent legacy flags preserve the destination. Init/hash errors cannot publish a partial device identity. |
| Power transitions | Production code checks lower-clock success before lowering voltage, and voltage success before upclocking. Failed requests do not become successful applied state. Atomic tuning snapshots prevent mixed pairs. |
| Tuning schedule | Weekly wrap/midnight, invalid/duplicate/missing-slot input, synchronized-time requirement, fixed offsets, runtime retry, manual override and pause/protection boundaries passed in host fixtures. |
| Nerd host suite | Both complete helpers passed all mandatory host entry points; the optional separate MUX-source contract test reported its expected skip. No fake service was a physical miner. |
| Gamma display fixtures | **36** native 128×32 LVGL 9.5.0 views and production screen lifecycle passed with sanitizers; contact sheet inspected. |
| Nerd display fixtures | Shared production renderer checked **54** pages at each 320×170 and 480×320 viewport, six candidate designs and 1,024-event lifecycle cases; driver/button and safety priorities retained. These are simulated fixtures. OctAxe remains 320×170 only. |
| Responsive browser check | Both schedulers inspected at 1440px and 390px, including 00:00/09:00/21:00 named profiles, dirty/save/refresh behaviour and fixed-offset warning. Nerd responsive viewport corrected. All APIs were fake; writes rejected. |
| Certificate bundle | Exact pinned 118 roots, positive control and rejection of twelve incident chains passed for both bases. Every target application contains the expected compiled trust store. |
| Target compilation | All three ESP32-S3 applications and matching WWW images passed. No factory/NVS images were produced for delivery. |
| Package verification | Descriptors, family/version, selected build receipts, source inventories, web budget and image checksums passed. Gamma's ELF-bound embedded table matched every URL, gzip flag and payload byte of the current 11-asset interface; independently recreated SPIFFS matched WWW byte-for-byte. |

Gamma's local build used clean official SDK 6.0.2 source `7101770dc6db2667b3c477cc31365dd1acd6db4e` and its pinned Linux ARM64 GCC 15.2.0 tools. The cached SDK 5.5.3 container supplied the Linux runtime only. Its actual identity, source/tool archives and compiler are recorded in `build-provenance.json`; this is not presented as an execution of the official SDK6 container. The standard helper and CI pin the official SDK6 image documented in [build instructions](build.md). The original web-budget baseline is retained as a fixed budget, independently of the newer firmware base.

## Matched local images

| Family | Version | App bytes | App SHA-256 |
| --- | --- | ---: | --- |
| Gamma 601/602 | `5tratumFW-0.1.0-beta.10` | 2,798,560 | `93f591fba971869059f0488c840f3f24b6d97d97bfea2b8af6bdadbf8158a4b8` |
| QAxe++ | `5tratumFW-qa-0.1.0-beta.8` | 3,579,648 | `d4736e51f92b3fd4e094ce7113874c6f1f94983bb9099547d6923ef4528935e1` |
| OctAxe Gamma | `5tratumFW-oct-0.1.0-beta.5` | 3,585,264 | `f16039af630c941256dc992a20a01d71f0a50e8f4c5b6e3445cdf9adff01eae9` |

Every WWW image is 3,145,728 bytes. Gamma's measured web payload is 624,901 bytes; QAxe's is 2,407,646 and OctAxe's is 2,407,650. Each package includes its own matched WWW, source, manifest, budget and checksums. Shared filenames do not make the packages interchangeable.

## Physical checks still required

No candidate was installed or flashed, and no device settings were changed. The owned standard QAxe did not respond to the read-only connection check. Tadets' larger panel is available only on his miner. Its controller cannot be identified from the photograph; verify full-screen position, readable unmirrored text, RGB/BGR colour order, rotation, startup and refresh using the saved screen selector. The online driver comparisons are recorded in the [display reference check](../firmware/nerdqaxe/docs/experimental-480-display.md#online-firmware-reference-check--28-october-2026).

Each new family/base also needs its own compatible paired OTA, preserved settings, cold start, accepted shares, real pause/resume, scheduled profile transitions and protection checks. Imported board/display definitions do not expand the supported families. Gamma retains two active routes and ten saved pool slots through its compatibility layer; upstream eight-active-pool storage is not enabled.

Schedules use a fixed UTC offset and do not automatically change for daylight saving time. Hardware protection may override a requested operating point.

Independent per-chip BM1370 work remains **unsupported and disabled**. Current primary-source rechecks do not establish an addressed work command or chip-local acceptance mechanism. The bounded passive-capture collector has fifteen host tests but no new physical capture. See the [investigation](../firmware/nerdqaxe/docs/asic-work-targeting-investigation.md). These packages do not claim four independently routed chips.
