# ASIC errors — NerdQAxe++ and NerdOctAxe Gamma

Status: unreleased source change for NerdQAxe++ Beta9 and NerdOctAxe Gamma Beta7.
The published Beta8 and Beta6 image pairs are unchanged. Bitaxe Gamma is a
separate firmware family and its existing error calculation is unchanged.

The shared Nerd dashboard shows **ASIC errors** in the Overview performance
strip and on each ASIC card. The four-chip QAxe and eight-chip OctAxe use the
same calculation and display rules.

Both families already read the BM1370 valid-work and hardware-error counters.
The earlier `/api/v2/dashboard` response included their per-chip rates in
`performance.chips[].hashRate` and `.errorHashRate`, but the dashboard did not
display them. The existing polling cadence and ASIC settings are unchanged.

## Meaning of the percentage

For a chip, the percentage is:

```
100 × errorHashRate / (hashRate + errorHashRate)
```

The device percentage uses the sums of all configured chips' error and total
rates. It is weighted by work, rather than an average of chip percentages.
These are counter-derived rates over the sampling interval, not lifetime
error totals or pool-rejected shares. The valid-plus-error denominator follows
the [upstream Nerd API contract](https://github.com/shufps/ESP-Miner-NerdQAxePlus/blob/develop/docs/API.md).

## Availability

A measured zero-error interval with positive total work displays **0.00%**.
The UI displays **—** when a percentage cannot be measured:

- Startup, an ASIC rail restart, or fewer than two usable counter replies.
- Missing, invalid or more than 15-second-old paired samples.
- A long response gap, until another normal measurement interval completes.
- A paused or shut-down miner, or a failed/stale dashboard request.
- Zero total work, where a percentage has no denominator.

The overall reading requires every configured ASIC's paired counter samples.
One unavailable chip makes the overall percentage unavailable; other chips
can still show their individual readings. Refreshing the page cannot turn
an old hardware measurement into a fresh one.

## Additive v2 API fields

`/api/v2/dashboard` retains the existing numeric `hashRate` and `errorHashRate`
chip fields for compatibility and adds:

| Field | Meaning |
| --- | --- |
| `performance.errorPercentage` | Weighted device error percentage, or `null`. |
| `performance.chips[].errorPercentage` | Chip error percentage, or `null`. |
| `performance.chips[].errorSampleFresh` | Both measured counter samples are current and mining is active. |
| `performance.chips[].sampleAgeSeconds` | Age of the older paired sample, or `null` before a pair is available. |

The dashboard requires the new fields. An older application paired with the
new web interface shows unavailable error readings rather than assuming zero.
Use a matching application and web interface from the same family release.

## Tuning

Errors are useful feedback for assessing stability alongside valid hashrate,
power, temperatures and cooling. This display change does not apply settings
or enable auto-tuning. The proposed continuously adaptive controller is
described separately in [the dynamic tuning design](../../../docs/dynamic-autotuning-design.md).

No miner has been flashed or tuned to validate this source change. See the
[software validation record](validation-asic-errors-20261103.md) for completed
tests, target compilations and remaining physical checks.
