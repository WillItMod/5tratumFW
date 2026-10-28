# Upstream migration — 28 October 2026

The aligned candidates use these exact upstream source revisions:

| 5tratumFW family | Base release | Base commit | Target SDK |
| --- | --- | --- | --- |
| Gamma 601/602 Beta10 | Bitaxe ESP-Miner v2.15.4 | `d212a0f948cb3ffd90c7ca9572e9b2cf2d865d80` | ESP-IDF 6.0.2 |
| QAxe++ Beta8 | shufps ESP-Miner-NerdQAxePlus v1.1.1-rc1 | `bfd07393918ce1b464faf1b7bd1ecd11343197f4` | Pinned ESP-IDF 5.5.3 |
| OctAxe Gamma Beta5 | shufps ESP-Miner-NerdQAxePlus v1.1.1-rc1 | `bfd07393918ce1b464faf1b7bd1ecd11343197f4` | Pinned ESP-IDF 5.5.3 |

Upstream source changes are merged into the firmware, rather than changing only
a version label. Nerd retains the existing supported SDK, which is newer than
RC1's upstream 5.3.6 build. Gamma requires the newer 6.0.2 SDK and LVGL 9.5.0.
Builds and hardware observations remain separate evidence for each candidate.

5tratumFW keeps its own web interface and release identities. Gamma's custom
Angular/PrimeNG interface remains an independent frontend rather than being
replaced by the upstream Tailwind interface. The embedded Gamma web assets and
the retained WWW image come from that same family build. Nerd retains the
5tratumFW home, routing, controls, scheduler, screen selector and family updater.
New upstream protocol/security fields are preserved through those controls.

The Gamma compatibility layer deliberately retains two active routes and their
existing persisted keys and journal identifiers. It does not automatically
convert an installed miner to upstream's eight-active-pool storage model, drop
saved connections or replay journals under renumbered identifiers. Ten named
saved pool slots remain separate from the two active routes. Unsupported active
pool indices are rejected rather than silently discarding configuration.

Gamma retains the 28-entry operating journal understood by Beta9 when the two
authentication values match their original individual NVS keys. Both same-point
and changed-point tuning writes are checked with the exact older decoder. An
actual authentication change that differs from those keys uses the extended
30-entry journal; Beta9 cannot read that format. Reverting both authentication
values to their original baseline while on the new firmware restores the legacy
format. A matching old application/web pair alone is not recovery for an extended
journal; retain a unit-specific backup and use compatible firmware. Queued
operating writes use the same atomic path from their first update.

Gamma's imported discovery code now retains the configured hostname during
name conflicts. A unique mDNS alias is kept at runtime and reported separately
as `mdnsHostname`/`fullHostname`; it does not become a saved NVS setting. The
normal HTTP settings action still saves an explicitly requested base hostname.
Production-function regression tests cover startup, repeated boots, explicit
updates, conflicts, errors and IPv6/network fallbacks.

Both bases include the pinned October 2026 certificate-bundle change. The
offline test checks its checksum, 118 roots, a positive control and rejection
of the twelve published incident chains. Target builds also check that the
compiled certificate bundle is the expected one inside the application image.

The source still admits only the established Gamma 601/602, NerdQAxe++ and
NERDOCTAXEGAMMA families through their separate build and boot guards. Imported
upstream definitions, themes or drivers do not qualify additional hardware.
Neither upstream base supplies proven independent chip-specific BM1370 work;
that capability remains disabled.
