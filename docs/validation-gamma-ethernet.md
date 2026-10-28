# Gamma Beta11 Ethernet candidate validation

**Candidate: `5tratumFW-0.1.0-beta.11`, Gamma PCB601/602 BM1370 only.** This is a development candidate; the public Gamma release remains Beta10. No physical W5500 module is available locally, and this candidate has not been installed on a miner.

## Software checks

- The Gamma Angular suite passes **192/192 tests** using Node24.14.0 and Chrome Headless, with the regenerated OpenAPI client. It checks the checkbox, compatible-board/API gating, strict save acknowledgment, isolated Ethernet writes with no saved SSID, saved/startup state, restart confirmation, failed reads and stale response handling.
- Sanitizer-backed tests execute the original W5500 register engine and ESP-NETIF adapter against bounded platform doubles. They cover ring/pointer wrap, packet bounds, unstable counters, missing hardware, finite deadlines, RX ownership, immediate DHCP/ARP transmit reentry and pending-DMA cleanup/quarantine.
- Production NVS/API tests cover both 601 and 602, strict boolean validation, duplicate/case/type rejection, pin/identity conflicts, durable commit-before-cache, injected storage failures, invalid persisted values, reboot readback, stable operating-journal enum IDs and no unrelated settings writes.
- Production network/event tests cover default-off behavior, pin exclusions, missing modules, no SSID, Ethernet preference, Wi-Fi retirement/recovery, stale status, access-point clients, management-task failure, roaming, address changes, pool reconnect requests, custom-interface mDNS and self-test pin reservation.
- The complete Gamma host runner passes **18 entry points: 114 unittest methods plus the plain MUX assertion script**. It also covers identity/startup, display/status, operating profiles/journal, mining power, schedules, pool coordination, Stratum and version pairing. Included Ethernet coverage comprises 18 settings/API native cases, 19 network timelines, four separately compiled pin overrides and two driver executables; those are subsets of the runner, not additional method totals.
- The production web build passes the enforced compressed payload budget: **626,986 bytes**, 11 assets, below the 876,628-byte project budget and within the 3 MiB WWW partition.
- Actual LVGL9.5.0 native rendering and production `screen.c` runtime pass under ASan/UBSan: two executables, 36 framebuffer views, lifecycle/safety/rate/MUX checks and wired-only display assertions. These views use simulated data.

The complete host runner also passes inside Ubuntu24.04 with **GCC13.3.0**, Python3.12.3 and Node24.14.0: all 18 entry points and 114 methods plus MUX assertions, followed by **10/10** additional driver runs. Captured compiler stderr is empty. This closes a GCC indentation warning in a test stub that macOS Clang accepted; a test callback now publishes its completion after the simulated receive operation finishes. Assertions and warning levels remain intact, and production firmware code is unchanged by these test corrections.

The final target uses pinned **ESP-IDF6.0.2**, SDK source `7101770dc6db2667b3c477cc31365dd1acd6db4e`, ESP32-S3 and compiler toolchain `esp-15.2.0_20251204`. A dedicated local ARM64 container runs the mounted SDK/toolchains with network access disabled; its Linux host image is ESP-IDF5.5.3, not the SDK used to compile the firmware. Build receipts identify both separately. The actual SDKCONFIG confirms per-default-interface DNS, four mDNS interfaces and SPI interrupts in IRAM.

## Initial target and paired package

On 28 October 2026, target compilation, pinned CA verification and paired packaging **passed** from clean source `4d3f665777681cdca294cadb37eef2c43cb5ad15`. The build directory began fresh with this exact SDK; the final same-SDK incremental reconfigure/rebuild incorporated the scan/AP lifecycle correction. No source changed during that final run. The manifest records the same commit for the firmware build and corresponding source archive; this later evidence-only documentation update does not relabel those files.

| File | Bytes | SHA256 |
| --- | --- | --- |
| `esp-miner.bin` | 2,836,144 | `4c3d4418499093b6a14e2befd3ecbf0665a39f8ef8eb4dc9340c09c989bddd2b` |
| `www.bin` | 3,145,728 | `6e3277f6cb5c5a0944a4cb48a3b7505c8c89515d52f3065f89e4a9a2615eaddf` |

The application descriptor confirms Beta11 and SDK6.0.2. Packaging compares every embedded web asset with the frozen production bundle and independently regenerates SPIFFS to compare the WWW image byte for byte. A separate verification checks all manifest/checksum entries and all **2,169** source-inventory hashes, including the final network correction. The firmware contains the pinned 55,222-byte trust store; its 118 roots and positive/12 negative incident-chain controls pass. No generated factory or NVS image is distributed. The prior public Beta10 artifacts and root SDKCONFIG remain unchanged.

## Physical qualification remains pending

The tests simulate the receive record as a two-byte big-endian length including its own header, followed by the frame. They do not independently establish the W5500 silicon's RX header/FCS behavior. Link negotiation, DHCP, real packet exchange, cable fallback, sustained mining and Ethernet-only application/WWW OTA require the actual adapter. The [module checklist and instructions](gamma-ethernet.md) record those checks.

Earlier Gamma Beta10 device observations do not qualify this candidate. No CI or build step flashes hardware, and no current public release or one-click update feed is changed by preparing it. The package must retain `hardware_tested: false` until separate dated hardware evidence exists.
