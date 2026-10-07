# 5tratMUX peer advertisement, version 1

This additive Stratum V1 extension allows the Gamma OLED to identify an advertised MUX peer on its **current** connection. It changes no device configuration, NVS settings, routing policy, or ASIC power gate. Stratum V2 has no equivalent extension in this implementation.

The compatible server opts in only downstream sessions whose `mining.subscribe` agent starts with `5tratumFW/`. Ordinary miners and other firmware receive no added message. The miner sends no new pool RPC probe.

After upstream authorization succeeds, the server sends this notification:

```json
{"id":null,"method":"mining.5tratum.status","params":[{"protocolVersion":1,"product":"5tratMUX","workScope":"chain-broadcast","independentWorkAssignment":false,"ttlSeconds":90}]}
```

The compatible server repeats it every 30 seconds until the downstream session closes. Heartbeats after initial authorization describe the local proxy connection and may continue during upstream recovery. Pool readiness and accepted-share counters must therefore remain separate from peer identification.

## Receiver contract

The accepted envelope contains one `id: null`, the exact reserved `method`, and a `params` array containing exactly one object. An optional `jsonrpc: "2.0"` is accepted. The status object has exactly the five fields shown above, with the shown values and JSON types. Unknown fields, duplicates, non-null IDs, embedded-NUL strings, and messages over 512 serialized bytes are rejected. Invalid reserved advertisements revoke the prior badge.

The reserved method is consumed before ordinary Stratum result/share handling, including an invalid advertisement with a non-null ID. Extension messages must never increment accepted/rejected share counters or be interpreted as share responses.

The firmware records the active route and connection generation. Connection start, disconnect, reconnect, coordinator shutdown, and a switch away from V1 clear the advertisement. A callback for an earlier generation cannot refresh a later connection. Fallback sessions require their own advertisement; a primary MUX advertisement cannot label a different fallback pool as MUX.

Freshness uses monotonic time. A valid advertisement is fresh strictly before **90 seconds** and expired at 90 seconds. The display evaluates age when taking a snapshot, so a blocked receive loop cannot freeze the badge as permanently fresh. Saved host/port/worker settings never create the badge.

## Meaning and validation limits

The advertisement identifies the connected peer's claim. It is not cryptographic authentication, proof of accepted shares, upstream pool health, payout verification, hardware isolation, or permission for independent ASIC routing. Version 1 describes `chain-broadcast` work and explicitly sets `independentWorkAssignment` to `false`; per-chip independent mining is not implemented or validated by this Gamma image.

The matching server change was **not deployed during repository preparation**, and no newly generated image was flashed as part of repository setup. Host tests and a successful target build do not establish a real deployed miner/server handshake. Consult the release validation record for the exact tested firmware and server versions before claiming that behavior on hardware.
