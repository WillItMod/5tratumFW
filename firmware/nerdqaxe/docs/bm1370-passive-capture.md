# QAxe BM1370 software capture

This diagnostic candidate records the jobs and raw UART responses used by our
four-chip QAxe++. It is the next bench step in the
[driver investigation](bm1370-driver-rewrite.md). It does not implement a
physical ASIC selector, change a job format, or enable independent work
assignment. The percentage allocation draft remains parked.

## Build and storage

`FIVETRATUM_BM1370_CAPTURE` defaults `OFF`. Enabling it requires
`FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER=ON`. The candidate version is
`5tratumFW-qa-diag-a9`; its application and WWW must be packaged together.
Normal initialization, operating settings, fan control, saved slots and
schedulers retain their existing paths.

Arming allocates at most 96 KiB explicitly in PSRAM with the record's required
alignment and no internal-RAM fallback. It retains that storage for later captures. Each of the 384 records
is exactly 256 bytes; a capture stops after at most two seconds or when full.
Storage exhaustion is explicitly incomplete, rather than silently overwriting
earlier observations. Capture is RAM-only, starts disabled at boot and writes
no NVS setting. Allocation failure leaves it inactive.

Producer calls use a short memory-copy critical section. No allocation,
logging, UART poll, UART completion wait or network operation occurs while
that lock is held. A session ticket prevents a delayed producer from entering
a newly armed capture. Frozen exports copy at most eight records under the
lock, then serialize and send outside it. Capture IDs are scoped to one boot.

## What is observed

The normal job task supplies its actual mining-operation generation. A single
composite TX record contains the requested 88-byte packet, canonical 80-byte
header template, original version mask, logical counter, pool index and actual
shared ASIC ticket threshold. It copies those values before the job registry
takes ownership. The UART's signed returned byte count is recorded with that
same packet, without associating a later global pool setting or recycled job
ID. Pool credentials, coinbase strings and wallet strings are not recorded.

Ordinary UART writes and raw bytes returned by each UART read are recorded
once. The decoder's retained tail is not recorded again. Raw RX generation
remains unknown: the software read time cannot identify the generation in
which a chip produced a queued response. Flushes, baud changes, errors and
software retirement/reset are distinct markers. Generation retirement does
not claim a physical drain.

Timestamps describe serialized software observations, not electrical edges.
A full UART write return establishes transport acceptance, not completion on
the wire or ASIC acceptance. The UART event queue remains disabled, so this
recorder cannot attest absence of FIFO or driver-buffer overruns. Captures
beginning while mining also have unknown pre-arm work. Nonce-to-chip slicing
remains a configuration-dependent attribution heuristic.

## Candidate-only API

`/api/5tratum/asic-capture` exists only in the capture build and is gated to the
standalone NerdQAxe++ / BM1370 / four-chip profile. GET and POST reuse network
and OTP/session validation. Arming additionally requires a running operation
lease and four actually enumerated ASICs.

- POST `{"action":"arm","durationMs":2000}` starts a capture.
- POST `{"action":"freeze","captureId":1}` freezes that matching capture.
- GET with no query returns status, identity and heap telemetry.
- GET `?captureId=1&offset=0` exports up to eight frozen records. Follow
  `nextOffset` until it equals `status.recordCount`.

Another active capture cannot be replaced. Live-record export, mismatched
IDs, unknown actions, oversized requests and out-of-range durations/cursors
are rejected. The endpoint has no clock, voltage, pool, command or register
input. Starting or exporting a capture does not alter mining settings.

The offline analyzer accepts a complete merged export or a list of its
immutable pages:

```sh
python3 tools/bm1370_capture_analyze.py capture-pages.json > report.json
python3 test/host/test_bm1370_capture.py
python3 test/host/test_bm1370_capture_runtime.py
python3 test/host/test_bm1370_capture_analyze.py
```

The analysis checks framing/checksums and independently hashes returned
nonces against captured headers. It retains all compatible captured contexts
across job-ID reuse and reports ambiguity instead of assigning the most recent
job automatically. Incomplete capture, unknown pre-arm work and physical
identity limits remain explicit. An accepted share or two live pool sessions
does not qualify simultaneous per-ASIC mining.

## Bench checkpoint

The ESP-IDF 5.5.3 a8 target build for `NERDQAXEPLUS2` passed with both diagnostic
flags enabled. That application occupies 3,299,616 bytes of its 4 MiB partition;
WWW occupies 3 MiB. The codec and existing actual-driver/power checks pass;
the pure recorder passes eight sanitizer groups covering all 384 capacities,
73,920 bounded writes, immutable paging, short returns and clock boundaries.
Actual runtime/serial tests pass with real host pthreads under ASan/UBSan and
a separate thread-sanitizer build. They exercise delayed producers across
rearming, concurrent snapshots/freezes, explicit PSRAM allocation failure,
no per-event allocation, immutable paging and actual serial-hook ordering.
Capture-mode actual-driver tests preserve all 117 initialization packets and
check canonical headers, actual rounded ticket masks, 64-bit generation,
short writes and zero writes for unsupported chip targets. The offline
analyzer passes nineteen tests including a published valid nonce independently
checked with OpenSSL, CRC mutations, ID reuse and ambiguous contexts. UART,
allocator, timer, regulator and GPIO boundaries remain host fixtures. Live
bench observations are recorded separately.

The live paired a8 boot submitted shares at approximately 4 TH/s, with all
exported settings, profiles and schedules retained. Its first capture requests
were rejected while capture remained disabled. Storage was allocated, but the
recorder refused to arm. Target assembly confirms `alignof(Record)==8`; ordinary
PSRAM allocation does not explicitly request that alignment. This is the
source-supported explanation for the observed rejection, rather than an
observation of the actual pointer. The a9 correction uses
`heap_caps_aligned_alloc(alignof(Record), ...)`. A host allocation fixture that
is four-byte-aligned but not eight-byte-aligned reproduces the rejection;
explicitly aligned allocation passes the complete runtime sanitizer checks.
No ASIC setting or command was changed to work around the capture failure.

The paired a9 build then armed successfully on the QAxe. Five short captures
recorded nineteen complete job writes, four CRC-valid register responses and
two CRC-valid mining nonce responses. Both nonces uniquely match captured
headers at the recorded ticket threshold, independently reproduced with
SHA-256d. All captures stopped at their time limit with recorder flags clear;
that does not attest complete electrical capture, chip identity or isolation.
See [a9 validation](validation-a9.md) for exact images, configuration retention
and the remaining qualification limits.

Keep the verified paired a7 images and original 16 MiB recovery backup for
manual restoration. Automatic trial rollback is disabled and WWW has one
partition. A successful compile alone does not qualify this candidate for
unidentified board revisions or another ASIC family.
