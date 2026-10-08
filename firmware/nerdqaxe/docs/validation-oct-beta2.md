# OctAxe Beta 2 candidate validation

Paired version: **`5tratumFW-oct-0.1.0-beta.2`**, candidate tag family **`octaxe-v0.1.0-beta.N`**. Exact software target: **NerdOCTAXE-γ / NERDOCTAXEGAMMA / BM1370×8 / ESP32-S3**. This is a development candidate, not a published or general compatibility claim. The owned unit's PCB revision is 2.2, reported by its owner; other assemblies remain unqualified.

## Change and package

Connection options now exposes Requested Stratum difficulty in normal mode. Previously the disclosure could open empty because its sole field was hidden behind the expert support level. Hardware tuning gates, OTP and section-scoped writes are unchanged. Gamma and QAxe use separate image pairs.

Built from source **`40349836d5d08ec69ec3f50c656274b1dd3214cb`**, with pinned ESP-IDF 5.5.3 and Node 24.14.0. Diagnostics and capture modes were off.

| Package | Bytes | SHA256 |
| --- | ---: | --- |
| `esp-miner-NerdOCTAXE-Gamma.bin` | 3,404,592 | `5352e903a5908ccd9f8fff75d00617c4e1dbfe3e86be2ecea8441643fe4b5b06` |
| `www.bin` | 3,145,728 | `098005bfe3c01c6c7b7a02ae1684ca0d21f3e425d8128f9b4b71169369fdb5f9` |
| Corresponding source archive | 14,666,587 | `2e341f3d4b680b4c67ebcc5cf0cbfafe63dc5888fc4f9fa8522dd1e4103d9d42` |

79 frontend tests, 20 browser control checks, 135 Python host cases, production driver/control harnesses and 54 native display fixtures passed. Both push and pull-request family CI runs passed. Display and fault fixtures use simulated hardware boundaries.

Independent checks verified the ESP32-S3 descriptor, segment checksum and appended SHA256; the WWW image was recreated byte-for-byte. All 94 web asset records (compressed files and raw companions) were checked, with a total payload of 1,681,536 bytes. All 1,230 source archive entries passed safe-path, inventory/hash and build-configuration checks. Manifest metadata and checksums matched the files.

## Supervised update on 8 October 2026

The matching unit's original flash layout, private recovery backup, profile and [Beta1 bench](validation-oct-beta1.md#supervised-revision-22-bench-on-8-october-2026) were established before this update. APP and WWW were each uploaded once and exactly acknowledged. The early post-application Wi-Fi label settled before a GET-only reconciliation; the application upload was not repeated.

Both installed versions and all 94 served compressed/decompressed web assets matched Beta2 and source `40349836d5d08ec69ec3f50c656274b1dd3214cb`. All eight hardware counters were fresh, both fan tachs and shared board/regulator readings were positive, and accepted shares progressed. The existing A MUX stream retained fresh coin and forwarded job/block context.

The owner deliberately changed the saved routing mode from Primary with failover to Dual pool during the update and confirmed that it should remain enabled. Every other exported setting, pool/tuning slot, schedule, manual override and integration report matched the original baseline. The retained operating point was 700 MHz / 1180 mV. The original upload journal and baseline were not altered; an additional GET-only verification recorded the authorized difference.

Saved Dual mode and running mode were checked separately: the application was still running failover at that observation, pending an explicit restart to activate the saved mode. Connection B still used its existing direct-pool endpoint; this check did not convert it to MUX or claim that two MUX streams were active. No pool endpoint, credential, frequency or voltage was changed by the updater.

After reloading the actual miner page, Connection options visibly showed Requested Stratum difficulty with its saved value of 1000, and Routing mode showed Dual pool. The browser check made no configuration writes.

## Limits

These are short observations on one owned revision 2.2 unit. They do not qualify another assembly, sustained operation, enabled schedule boundaries, cold starts or all protection faults. The regulator package marking was not physically read; the scoped strap/stock-profile/hardware-source review is described in the Beta1 record. Individual chip temperatures remain unavailable; real shared board and regulator readings are shown once.

Native A/B connections use the existing shared-chain selector. Independent physical-ASIC work assignment remains unsupported. Central OS/MUX management is qualified separately from firmware installation and advertised sessions. Historical package audit flags remain unchanged; this dated external record adds the scoped bench evidence.
