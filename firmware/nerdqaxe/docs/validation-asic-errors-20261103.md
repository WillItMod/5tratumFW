# Nerd ASIC error display validation — 3 November 2026

Status: **historical development checkpoint** for the ASIC-error display later included in NerdQAxe++ Beta10 and NerdOctAxe Gamma Beta8. The following test counts and scope apply only to that checkpoint. The [final release validation](../../../docs/validation-autotune-release-20261103.md) separately identifies the final source, later controller/UI tests and physical limits. Earlier public Beta8/Beta6 assets were not replaced.

## Result

The shared Overview now displays overall and per-chip hardware error
percentages. Four-chip and eight-chip readings use the Nerd counter contract:
error work divided by valid plus error work. Overall results are weighted by
work. A measured zero is distinct from unavailable data.

Paired samples are copied coherently under the monitor mutex. Missing or
invalid samples, startup/reset, pause/shutdown and samples older than 15 seconds
are unavailable. A long reply gap establishes a new baseline before another
measurement is accepted. Browser freshness conservatively includes the whole
HTTP request duration, using monotonic time, as well as time after receipt.

## Completed software checks

- All **29 Nerd host regression entrypoints** pass. The monitor entrypoint
  builds two AddressSanitizer/UndefinedBehaviorSanitizer executables: actual
  production monitor methods and the production dashboard performance block,
  serialized and parsed through the real ArduinoJson library.
- Cases include four/eight chips, weighted totals, fresh zero-work chips,
  measured zero errors, first/second samples, missing chips, stale/future or
  invalid values, wraparound, reset, long gaps, task-creation failure cleanup,
  concurrent publication and pause/shutdown serialization.
- All **169 Angular tests** pass using Node 24.14.0 and Chrome Headless.
  New coverage includes legacy/missing fields, true zero, board and individual
  availability, paused/shutdown/stale data and delayed HTTP responses.
- Both model-specific **production web builds** pass.
- **12 browser fixture cases** load those actual production builds against
  loopback-only synthetic APIs: each family at 1440, 1100 and 390 pixels, plus
  old-application, stale-data and delayed-response cases. Normal layouts have
  no horizontal overflow or browser page errors. Screenshots were visually
  inspected. These are simulated data, not photographs or miner observations.
- Both exact ESP32-S3 application targets compile with pinned **ESP-IDF 5.5.3**,
  image `sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805`,
  in separate build directories with container networking disabled:
  `NERDQAXEPLUS2` and `NERDOCTAXEGAMMA`. The first local compile exposed an
  include-order conflict with the inherited `max` macro; moving the new control
  header before `http_utils.h` corrected it. Both final compilations pass.
- **20 web-control checks** pass. Source and both compiled trust stores contain
  exactly the 118 pinned roots; the positive control passes and all 12 revoked
  incident chains are rejected.
- Diff whitespace checks pass. Independent review found the HTTP-transit
  freshness issue; its correction is included in the final frontend tests,
  production builds and delayed-response browser cases.

The application compilations and frontend bundles validate this development
checkout. They are not a new published, packaged matching application/WWW pair.
Generated images, fixtures, screenshots and local receipts remain outside Git.

## Scope and remaining physical checks

No miner was contacted, flashed, tuned, paused, restarted or configured by these
checks. Counter accuracy on the physical ASICs, register interpretation under
all operating conditions, changed clock/voltage points and sustained thermal
behaviour have not been newly qualified by this display work.

The ASIC-error checkpoint recorded here did not implement a runtime auto-tuner.
The later experimental Beta10/Beta8 releases also implement the controller and Tuning tab; see the [current guide](../../../docs/autotuning.md). These ASIC-error checkpoint results alone do not qualify that controller or the later source.