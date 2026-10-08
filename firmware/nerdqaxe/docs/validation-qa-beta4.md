# QAxe Beta 4 validation

Paired version: **`5tratumFW-qa-0.1.0-beta.4`**, published prerelease tag **`qaxe-v0.1.0-beta.4`**. Exact software target: **NerdQAxe++ / NERDQAXEPLUS2 / BM1370×4 / ESP32-S3**. The owned test unit's PCB revision remains unidentified; this record does not qualify another board, QAxe+, OctAxe or Gamma.

## Change and source

The Pool routing page's Connection options disclosure always rendered, but its only field, Requested Stratum difficulty, was incorrectly hidden at the default normal support level. Beta4 exposes this connection setting in normal mode. Frequency, voltage and other hardware tuning gates remain unchanged. Form validation, OTP, saved values and section-scoped requests are retained.

Built from source commit **`40349836d5d08ec69ec3f50c656274b1dd3214cb`**, using the pinned ESP-IDF 5.5.3 image and Node 24.14.0. The app and web marker both identify Beta4. This family has its own app and WWW pair; the shared filename `www.bin` does not make it interchangeable with another family.

| Package | Bytes | SHA256 |
| --- | ---: | --- |
| `esp-miner-NerdQAxe++.bin` | 3,398,816 | `7f4be84f794d8b83aaac87943436b302eb56473322b3cde13e93d3c844eeffb4` |
| `www.bin` | 3,145,728 | `8d9fd0b630c136ce254c6e8d649cf381e0315b4a9a14f017a6de4da730e9ab81` |
| Corresponding source archive | 14,666,587 | `933830c29926b240d243dce2773308a16ef2cf7cf8d5c7bd75214611fdc286ca` |

## Build and offline checks

79 frontend tests passed, including production-template tests for QAxe and OctAxe at normal support level, the saved difficulty value, exact difficulty-only OTP write, unchanged/invalid requests and retained hardware tuning gates. Also passed: 20 browser control checks, 135 Python host cases, production C/driver harnesses and 54 native display fixtures. Display fixtures use simulated readings and are not physical screen validation.

The isolated clean QA build used only NERDQAXEPLUS2, with capture and diagnostic modes off. The ESP32-S3 app descriptor, segment checksum and appended SHA256 were verified. WWW was independently recreated byte-for-byte; all 94 web asset records (compressed files and raw companions), their provenance and the 1,681,538-byte total payload budget were checked. All 1,230 source archive entries passed safe-path, exact inventory/hash and build-configuration checks. Manifest metadata and SHA256SUMS matched the actual files.

## Physical verification

The owned QAxe was updated from its already qualified Beta3 installation on 8 October 2026. APP was uploaded first and exactly acknowledged; its new boot was reconciled using GET-only inspection after the early Wi-Fi status label settled. The application upload was not repeated. WWW was then uploaded once and exactly acknowledged.

The installed app and web both reported Beta4, and all 94 served compressed/decompressed assets matched the package. All exported saved settings, 10 pool slots, 10 tuning slots, pool schedule, power schedule/manual override and integration reports matched the fresh pre-update baseline. Saved 500 MHz / 1130 mV and the native dual-pool 50/50 allocation were retained. Unexported password bytes were not independently measured; working sessions and exported credential-presence flags were checked.

All four individual hardware counters were fresh, both tachs and the shared board/regulator temperature readings were positive, and accepted shares increased by 13 from the new boot's post-application baseline. Both native MUX sessions retained fresh per-session coin and forwarded job/block context. Coins may rotate; this does not establish one physical ASIC per session.

After reloading the actual miner page, the Connection options panel visibly showed Requested Stratum difficulty with its saved value of 1000 in normal mode. Routing mode visibly remained Dual pool. No setting was changed through the browser during this check.

These are short operating observations. Sustained operation, all hardware faults and physical LCD artwork/readability are not qualified by this update. Native A/B sessions use the shared ASIC chain; independent physical-chip mining remains unsupported.
