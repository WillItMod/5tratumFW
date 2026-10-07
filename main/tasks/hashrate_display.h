#ifndef HASHRATE_DISPLAY_H_
#define HASHRATE_DISPLAY_H_

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

/* Gamma polls counters each second; accepted deltas can be two seconds apart.
 * This age is display metadata only, never a mining or accounting gate. */
#define HASHRATE_DISPLAY_TTL_US UINT64_C(5000000)

typedef struct {
    float rate_gh;
    uint64_t sample_time_us;
    bool fresh;
} HashrateDisplaySnapshot;

static inline HashrateDisplaySnapshot hashrate_display_sample(float rate_gh,
                                                              uint64_t sample_time_us,
                                                              uint64_t now_us)
{
    return (HashrateDisplaySnapshot) {
        .rate_gh = rate_gh,
        .sample_time_us = sample_time_us,
        .fresh = sample_time_us != 0 && now_us >= sample_time_us &&
                 now_us - sample_time_us < HASHRATE_DISPLAY_TTL_US &&
                 isfinite(rate_gh) && rate_gh >= 0,
    };
}

#endif
