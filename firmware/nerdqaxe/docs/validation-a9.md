# QAxe++ a9 diagnostic bench

Checked on 7 October 2026 on the same **NerdQAxe++**, profile
`NERDQAXEPLUS2`, with four BM1370 ASICs. Its physical PCB/regulator revision
remains unknown. This is a local diagnostic candidate, not a qualified release
for other QAxe or OctAxe revisions.

## Exact paired build

Source commit: `66997c5baf0c35c53fc89c4a3d3826edb041162a`.
Version: `5tratumFW-qa-diag-a9`. The ESP-IDF 5.5.3 ESP32-S3 build passed using
the pinned SDK digest
`8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805`.
Both `FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER` and
`FIVETRATUM_BM1370_CAPTURE` are enabled; serial hex logging is disabled.
The separately built compressed web assets carry the same version.

| Image | Bytes | SHA-256 |
| --- | --- | --- |
| `esp-miner-NerdQAxe++.bin` | 3,299,616 of the 4 MiB OTA slot | `bf6ea9bb46df9dd48a66f0590ba239c1e513b5734d9fc976dc657a9a12813ba5` |
| `www.bin` | 3,145,728 | `ad9c68aa142a18d93142a7af04fcf611a5f9af4143ebe44bc4d3a5657a885a51` |

The original 16 MiB recovery backup was rechecked before OTA. Application OTA
wrote `ota_0`; a8 remains in `ota_1`. Each app/WWW upload was acknowledged once
with private durable intent recorded before the write. No NVS erase, factory
flash or partition-layout change was used. Automatic trial rollback remains
disabled. Paired a7 images and the original full backup remain available for
manual restoration; a7 is no longer the other-slot application.

The served index and four entry bundles exactly match the paired a9 build.
All 33 exported settings, except the expected version change, were retained,
including **500 MHz / 1130 mV / version rolling 25011**, fan/PID configuration
and saved pools. Named pool/tuning slots and the pool schedule were unchanged.
The shared power schedule remains disabled with no manual override. This is
exported configuration comparison, not a complete NVS/credential byte comparison.

## Alignment correction and software checks

The paired a8 boot mined normally, but its recorder refused to arm after
allocating PSRAM storage. Target assembly establishes an eight-byte record
alignment; the original allocation did not explicitly request it. The actual
pointer was not inspected, so alignment is the source-supported explanation,
not a direct observation of that pointer. a9 uses explicit aligned PSRAM
allocation with no internal-RAM fallback. The same live arm request then succeeds.

A host fixture reproduces rejection of storage aligned to four but not eight
bytes and checks the corrected allocator arguments. Actual runtime/serial
bodies pass eight groups with ASan/UBSan and a separate thread-sanitizer build,
including delayed producers across rearming, concurrent paging/freezing,
allocation failure, short UART returns and absence of extra UART operations.
Actual driver checks preserve all 117 initialization packets, test canonical
headers and rounded ticket thresholds, and send zero bytes for unsupported
chip targets. The offline analyzer passes nineteen tests. These host fixtures
substitute hardware/RTOS boundaries and remain separate from the live evidence.

## Live bounded capture

Five bursts ran with the miner's existing jobs, pool connections, ticket
threshold, clock, voltage and job timing unchanged. Each recorder stopped at
its time limit. No new ASIC command format or targeting register was sent.

| Capture | Duration | Job writes | Register responses | Nonce responses | RX bytes |
| --- | --- | --- | --- | --- | --- |
| 1 | 1.5 s | 3 | 0 | 0 | 0 |
| 2 | 1.5 s | 3 | 0 | 0 | 0 |
| 3 | 2 s | 4 | 0 | 0 | 0 |
| 4 | 2 s | 4 | 4 | 0 | 44 |
| 5 | 2 s | 5 | 0 | 2 | 22 |

All nineteen job writes returned their full 88 bytes and passed packet/header
checks. Capture 4 additionally recorded the existing counter-read command.
All six 11-byte responses have valid CRCs. Both nonce responses uniquely match
captured primary-pool headers using their returned work ID, nonce and rolled
version. Independent SHA-256d reproduction checks the recorded programmed ASIC
ticket threshold of 2048 and the negotiated version mask. The threshold is
cached driver state rather than a register readback. This establishes normal
mining interoperability; it does not establish physical ASIC attribution.
The separate offline implementation imports no production/analyzer code;
local OpenSSL agrees with its hashlib results. Reversed nonce, rolled-version
and digest byte-order controls fail the ticket threshold for both responses.

All five captures report zero incomplete flags, no captured CRC/TX failures,
and unchanged exported settings. The recorder retains 98,304 bytes of PSRAM.
Capture 5 reported 96,176 bytes free internal RAM, largest internal block
55,284 bytes and minimum internal free RAM 94,920 bytes. These are short-bench
snapshots, not a fragmentation or sustained-load guarantee.

The post-bench sample reports approximately **4.073 TH/s**, **28 accepted /
0 rejected shares**, both fans rotating and no shutdown. Capture is frozen
and inactive. Private raw captures, deployment journals, identities and
configuration exports remain ignored and are not published.

## Remaining requirement

`independentWorkAssignment` remains false. No addressed job format or per-chip
work-acceptance mechanism has been verified. Counter responses have an address;
mining nonce responses do not supply an explicit chip address. Existing
nonce-space attribution is a heuristic and cannot prove independent work.
Two sockets and pool-tagged result contexts support dual-pool reuse, but the
current dispatcher still broadcasts each selected job to all four ASICs.

The missing proof is concurrent different valid headers on selected ASICs,
with the other ASICs retaining their work, followed by verified ID reuse and
pipeline retirement. See the [targeting investigation](asic-work-targeting-investigation.md).
Per-ASIC pool controls, MUX admission and public split-mining release remain
blocked on that mechanism. OS/MUX deployments and public pushes were not made
by this bench. Power transitions were physically checked on a7 and were not
repeated here; thermal/fan/UART fault injection, sustained soak and unidentified
revision compatibility remain unqualified.
