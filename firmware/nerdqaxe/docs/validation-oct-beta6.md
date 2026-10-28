# OctAxe Beta6 experimental release validation — 28 October 2026

Version **`5tratumFW-oct-0.1.0-beta.6`**, compiled and packaged from clean source
`5567a2d6c1f59808f197062e565fa6525a93f97b`. The public experimental BETA
uses this exact locally verified package. Publication is not an installed-device
qualification. Later documentation commits do not relabel its images or
corresponding source.

The user accepted **480×320 (153,600 pixels)** as the working assumption for
an unidentified larger OctAxe panel. This enables an explicit saved display
choice; it does not identify the panel controller, wiring, PCB or flash layout.
The default remains 320×170, with no display change before an explicit save
and restart. [Operation and reference limits](octaxe-480-display.md).

## Completed local checks

- The complete pinned OctAxe build helper passed with Node 24.14.0 and the
  official ESP-IDF 5.5.3 container, retaining the shufps v1.1.1-rc1 base.
- Production frontend build, **159 browser tests**, 20 web-control checks and
  all **29 host entry points** passed. The optional external MUX-source wire
  test reported its expected skip; this is not live MUX evidence.
- Thirteen CMake/header cases and sanitized runtime/actual settings-API
  matrices cover QAxe++, OctAxe Gamma and four unsupported/missing target cases.
  RGB/BGR saves, restart latching, returning to Standard and malformed mixed
  writes were checked without changing unrelated settings.
- Fourteen OTA tests cover saved/active larger profiles, malformed headers,
  deadlines, wrong families and missing/mixed identities. Larger-screen updates
  require QAxe Beta7 onward or OctAxe Beta6 onward within the same family;
  older images are refused before power or flash changes.
- Twelve package tests include adjacent/mixed board definitions in both orders
  and explicit `=0` / `=1` values. The actual package binds the stock default
  and enabled runtime selector to the compiled OctAxe display driver.
- The five changed display/OTA/package entry points also passed on Linux.
  An initial Linux check lacked the SDK-exported CMake path; those two entry
  points were rerun through the standard SDK entrypoint. Initial logs remain.
- Native LVGL 8.3.11 rendered **54 views at each resolution**, including fourteen
  OctAxe views per resolution. Candidate lifecycle checks passed 1,024 replacement
  events at each size with sanitizers. These are simulated fixtures.
- The pinned 118-root certificate bundle, positive control and twelve incident
  chains passed; the target application contains the exact compiled trust store.
- The ESP32-S3 application and matching WWW passed descriptor, source-inventory,
  image/hash and exact reconstructed-SPIFFS checks. Web payload: **2,407,648 bytes**.

Local preflight caught a frontend array-type error, which was corrected before
the successful checks above. The Mac helper also initially selected the system
LibreSSL; its successful run explicitly selected OpenSSL 3. These local failures
were resolved before producing the package. The later public experimental
release retains these exact files. Its GitHub checks are recorded separately
from the local build and do not relabel the compiled source receipt.

## GitHub publication checks

All three push workflows completed successfully at
`9aa01916a24d614eec918a0f2fd8d254ae0947b1`:
[Gamma](https://github.com/WillItMod/5tratumFW/actions/runs/40740167750),
[QAxe](https://github.com/WillItMod/5tratumFW/actions/runs/40740167749) and
[OctAxe](https://github.com/WillItMod/5tratumFW/actions/runs/40740167703).
Run and job conclusions were independently verified. No run failed or was rerun.

The production source at that commit is identical to compiled revision
`5567a2d6`; the intervening commit changes documentation only. The separate CI
builds are not asserted byte-identical to the local package. Publication uses
the immutable local images and source archive listed below. Later publication
edits change documentation only and do not relabel those receipts.

## Exact matched images

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `esp-miner-NerdOCTAXE-Gamma.bin` | 3585312 | `6a74c40c86e8232fe00de95538d44ece4b7257b1fe8dd1bd170ae352d3b7fef6` |
| `www.bin` | 3145728 | `11ffb541424ccaf9331211d3e7b29991a5a8028e57f9d432a09e2adb589e331c` |

The package includes its unchanged manifest, SHA256SUMS, web budget and full
corresponding source with pinned dependency sources, licences and configuration.
The source ZIP checksum is `078e4451756c6048843379e25454750da0c33ca5d8d06ceba2b10bda53844192`.
Its manifest reports `hardwareTested: false` and `physicalLayoutVerified: false`.
No factory, partition-table, NVS or device-backup image is distributed.

## Physical boundary

**No miner was updated or changed during this work.** Neither a standard-screen
Beta6 installation nor the unidentified larger OctAxe panel has been physically
checked. The existing owned PCB 2.2 observations apply to public Beta5 only.
Check viewport, readable unmirrored text, colour order, rotation, startup, refresh
and persistence on an identified compatible panel before qualifying it.

The established software target is NERDOCTAXEGAMMA / NerdOCTAXE-γ / BM1370×8,
with the existing 16 MiB flash, 3 MiB WWW and two 4 MiB OTA-slot contract. This
assumption does not add support for a vendor's different flash/partition layout.
Board guards, clocks, voltage, cooling, pools, slots and schedules are unchanged.
Independent per-chip routing remains disabled. Public Gamma Beta10 and QAxe
Beta8 downloads remain unchanged. Previous OctAxe Beta5 files remain available
with their own standard-screen installed-device evidence; publication of Beta6
does not transfer that physical qualification to the new pair.
