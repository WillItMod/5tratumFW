# QAxe++ native dual-MUX bench

On 7 October 2026 the identified NerdQAxe++ with four BM1370 ASICs was
updated through its existing OTA endpoints to paired
`5tratumFW-qa-mux-a10` application and web images. The application runs in
`ota_1`; the previously checked a9 application remains in `ota_0`. The PCB
revision remains unidentified. This validates this test unit, not every QAxe
or OctAxe revision.

## Build and retained configuration

Firmware build commit: `1800a1913c65c652bf536f97b8f8bc6bc03fb60f`.
The pinned ESP-IDF 5.5.3 build completed for `NERDQAXEPLUS2`. The native
subscribe sender's five ASan/UBSan host tests passed. Paired image hashes:

| Image | Bytes | SHA256 |
| --- | ---: | --- |
| `esp-miner-NerdQAxe++.bin` | 3,300,288 | `deee1791e3fc9be011895486aaa5f93bf44b6099d51d3af3e19c287a2ae9f2e5` |
| `www.bin` | 3,145,728 | `0c4807ec8cd28e3369e2a95f92a4760965a2778975de1d9c735eda56826f8002` |

The served index and four web entry bundles exactly matched this build.
The saved operating point stayed at **500 MHz, 1130 mV and version rolling
25011**. Public saved configuration was checked before and after boot;
named pool/tuning slots and both schedules were retained. Password fields
were omitted from configuration writes; their bytes were not exported or
independently compared.

The intended configuration delta was native Dual mode, a 50/50 job balance
and a second SV1 connection to the existing local MUX. A private journal
recorded the original fields and each write intent before its request. The
verified full ROM backup predates these updates and remains private.

## Actual routing observation

Both native pool connections authenticated through the test MUX on
`the local test MUX on its configured Stratum port`. They have distinct stable pool-stream identities under
the same reported physical controller identity. The MUX assigned stream A
to the existing 5TRAT target and stream B to the existing BTC target.

After applying those separate allocations, fresh accepted shares were
observed on both targets: 72 on A and 26 on B since its target change, with
zero reported rejects at that observation. The device reported approximately
4 TH/s, both fans running and no shutdown. These are live pool acceptance
observations, not host simulations or direct measurements of individual
chip ownership.

The initial test MUX version was `0.9.72-fw-preview2`, built from a frozen
private source snapshot, with native pool identity enabled and ASIC identity
admission disabled. Its prior container and stopped-state backup were
retained. Public images and repositories were not pushed by this bench.

The final private MUX candidate, `0.9.72-fw-preview5`, also qualified both
native connections with `warm-template-mux` routing. Its tested interface
uses `NerdQAxe(A)` and `NerdQAxe(B)` below one collapsible physical miner and
provides a **Dual MUX mining** switch in **Miners**. The live read, review and
already-enabled apply paths passed, including settings equality before and
after and no requested miner restart. New accepted shares arrived on both
5TRAT and BTC after the MUX restart. The current operating point remained
500 MHz and 1130 mV.

This candidate's final backend checks passed 47 tests covering controls,
actual seamless sockets, job provenance and Docker context. The frontend
passed 133 Node tests and production-renderer desktop/mobile checks. The
exact test image is
`sha256:c2283d4a74f406679d633c4907e9bb32d8c57858f7b12e6fecbc6d8b878af6c0`.

## Scope

The native job selector alternates work across the shared ASIC chain. Two
network streams, separate job/extranonce contexts, independent MUX policies
and accepted shares on two coins are working. This does **not** demonstrate
one independently assigned stream per physical ASIC.
`independentWorkAssignment` remains false. No guessed addressed-job packet,
clock, voltage or forwarding-gate command was sent during this bench.

Private deployment, configuration and acceptance records are stored under
the ignored device and MUX cache directories. They include identifiers and
credentials or their configuration context and must not be published.
