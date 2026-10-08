# Gamma Beta8 schedule API fix

Candidate paired version: **`5tratumFW-0.1.0-beta.8`**, BETA tag **`v0.1.0-beta.8`**. Supported release identities remain Gamma PCB **601/602**, BM1370×1, ESP32-S3. This source record does not claim Beta8 installation or physical qualification.

## Change

Successful JSON responses from `GET /api/5tratum/pool-schedule` and `GET /api/system/mining/schedule` now explicitly declare `Content-Type: application/json`. Their existing POST/PUT save paths return through the same handlers and receive the corrected type. Previously their valid JSON bodies inherited the HTTP server's `text/html` default, causing strict central OS clients to reject schedule inspection.

The JSON payloads, authentication/network checks, persistence, scheduler decisions and error handling are unchanged. The existing mining override acknowledgment already declares JSON. No consumer is relaxed to accept arbitrary HTML, and no hardware, mining driver, NVS key, partition layout or protection behavior changes. The Gamma application and WWW version markers advance together; QAxe and OctAxe versions and code are unchanged.

## Host regression evidence

The actual production schedule modules and registered HTTP handlers are compiled with fake platform services and the pinned ESP-IDF 5.5.3 cJSON implementation. The response mocks record Content-Type when the response is sent, including the HTTP default when no type was set. Both new GET header checks fail on the pre-fix production handlers and pass after the two header additions.

- Pool scheduler/API suite: **3 tests passed**, including its 12 existing runtime scenarios and the new GET/POST JSON-response scenario. ASan/UBSan is enabled in this harness.
- Mining scheduler/API suite: **11 tests passed**, including GET/PUT JSON/status readback and the existing JSON override acknowledgment. Saved schedule readbacks are compared with subsequent GET bodies; reads do not write storage.
- A separate version check executes the production web-version generator in a temporary tree and checks the same Beta8 version and pinned Node provenance used for the application/WWW pair.

These checks use no miner I/O. Run the full pinned [build and packaging pipeline](build.md) from the committed clean source before accepting the resulting package. The package manifest records its exact build/source commit and hashes; local build receipts and any later installed-device record must state their actual results separately.

## Installation and limits

Use only the matching Gamma Beta8 `esp-miner.bin` and `www.bin` with their manifest, checksums and corresponding source. Follow the existing [installation guide](installation.md); no factory, partition-table or NVS image is produced or required by this fix. Retain each miner's own saved configuration and recovery material.

[Gamma Beta7 validation](validation-beta7.md) records separate short installed checks on both owned Gamma revisions. Those results do not establish operation of this new pair. Beta8 still needs exact paired app/web verification, both schedule Content-Type checks, exported settings/20 slots/both schedules retention and a normal mining observation on each intended unit. Gamma MUX metadata remains informational v1 status; it does not supply rich coin/job context or independent per-chip work.
