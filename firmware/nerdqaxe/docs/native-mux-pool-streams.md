# Native dual-pool connections to 5tratMUX

The Nerd firmware already maintains two independent Stratum transports in
Dual mode. Each retains its own extranonce, job context, MUX coin/block
metadata and reconnect generation. Returned ASIC work uses the saved
`job.pool_id` to select the original connection for submission.

Both connections can point to the same MUX. Each needs its own MUX worker
policy; the earlier ordinary same-IP alias resolver merged them. The native
pool-stream extension identifies the physical parent and actual pool index
without claiming that a stream owns a particular ASIC.

## Subscribe agent contract

```text
5tratumFW/<device>/<asic>/<version>/native-pool-v1:<32-lowercase-hex-device-id>:<0-or-1>/work-context-v2
```

The ID is the existing opaque controller ID with its `5tfw:` prefix omitted
from this component. It is a peer report, not cryptographic attestation or
ASIC routing authorization. No raw MAC or credential is included. The actual
pool task passes index 0 or 1. Invalid/unavailable identity falls back to the
previous agent; unsafe components and oversized agents are rejected before
writing JSON. The agent is at most 192 bytes and retains the terminal v2
qualifier for existing job metadata support.

The MUX receiver uses `FIVETRATUM_NATIVE_POOL_STREAMS=1` for the local test
candidate. MUX 0.9.73 releases enable native session recognition in the protected image; an explicit managed environment override of `0` still disables it. Its native pool worker keys and per-worker routing
policies are separate from the ASIC-identity admission mechanism. The latter
remains disabled. Shared board telemetry belongs to the physical parent;
route estimates use accepted work rather than copying the board rate twice.

## Initial bench configuration

Use Dual mode (`poolMode:1`), 50/50 job selection and two SV1 connections to
the same MUX host and its configured Stratum port. Give them distinct worker labels.
Retain the saved ASIC operating point, cooling configuration, named slots
and weekly schedules. Changing the manager mode takes effect on boot.
Sessions A and B can then use separate existing SHA-256 pool targets.
The physical screen labels these sessions P1 and P2 respectively.

This is two independently configured network streams using the native
whole-chain job selector. It does not assign ASIC 1 to session A or ASIC 2 to
session B. Simultaneous independent work on individual ASICs remains a separate
driver/protocol requirement; `independentWorkAssignment` stays false.

## Checks

`python3 test/host/test_native_pool_stream.py` compiles the actual subscribe/send
code under ASan/UBSan. Five tests cover both real API instances, partial writes,
connection errors, legacy fallback, invalid IDs, safe JSON and exact length
boundaries. The identity and socket boundaries are mocked.

The paired `5tratumFW-qa-mux-a10` build was installed on the identified
NerdQAxe++ test unit. Both native connections received fresh accepted shares
on separate 5TRAT and BTC targets. See the [a10 live validation record](validation-a10.md)
for image hashes, retained configuration and the shared-chain limitation.
