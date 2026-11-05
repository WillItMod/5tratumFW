#include "hashrate_runtime_stubs.h"
#include <assert.h>
#include <math.h>
#include "../../main/tasks/hashrate_monitor_task.c"

int main(void)
{
    measurement_t total[1] = {0}, domains[4] = {0}, errors[1] = {0};
    measurement_t *domain_rows[] = {domains};
    GlobalState state = {.DEVICE_CONFIG.family = {.asic_count = 1, .asic.hash_domains = 4}};
    assert(!hashrate_monitor_display_snapshot(&state, UINT64_C(1000000)).fresh);
    HashrateMonitorModule *monitor = &state.HASHRATE_MONITOR_MODULE;
    monitor->total_measurement = total;
    monitor->domain_measurements = domain_rows;
    monitor->error_measurement = errors;
    assert(pthread_mutex_init(&monitor->lock, NULL) == 0);
    monitor->is_initialized = true;

    /* Baseline is not a computed rate, even if a cached UI value is positive. */
    state.SYSTEM_MODULE.current_hashrate = 1200;
    hashrate_monitor_register_read(&state, REGISTER_TOTAL_COUNT, 0, 100, UINT64_C(1000000));
    assert(total[0].time_us == UINT64_C(1000000) && total[0].rate_sample_us == 0);
    assert(!hashrate_monitor_display_snapshot(&state, UINT64_C(1000000)).fresh);
    hashrate_monitor_register_read(&state, REGISTER_TOTAL_COUNT, 0, 200, UINT64_C(1500000));
    assert(total[0].time_us == UINT64_C(1000000) && total[0].value == 100);
    assert(total[0].rate_sample_us == 0); /* Ignored rapid reply. */

    hashrate_monitor_register_read(&state, REGISTER_TOTAL_COUNT, 0, 400, UINT64_C(2000000));
    float rate = total[0].hashrate;
    assert(rate > 0 && total[0].rate_sample_us == UINT64_C(2000000));
    HashrateDisplaySnapshot snapshot = hashrate_monitor_display_snapshot(&state, UINT64_C(6999999));
    assert(snapshot.fresh && snapshot.rate_gh == rate);
    snapshot = hashrate_monitor_display_snapshot(&state, UINT64_C(7000000));
    assert(!snapshot.fresh && snapshot.rate_gh == rate);
    assert(total[0].hashrate == rate && state.SYSTEM_MODULE.current_hashrate == 1200);

    /* Other register traffic cannot refresh the total-rate sample. */
    hashrate_monitor_register_read(&state, REGISTER_ERROR_COUNT, 0, 5, UINT64_C(7000000));
    hashrate_monitor_register_read(&state, REGISTER_DOMAIN_0_COUNT, 0, 25, UINT64_C(7000000));
    assert(total[0].rate_sample_us == UINT64_C(2000000));
    assert(!hashrate_monitor_display_snapshot(&state, UINT64_C(7000000)).fresh);
    assert(!hashrate_monitor_display_snapshot(&state, UINT64_C(1999999)).fresh);

    /* Fresh zero is real data, and a newer accepted delta restores freshness. */
    hashrate_monitor_register_read(&state, REGISTER_TOTAL_COUNT, 0, 400, UINT64_C(8000000));
    snapshot = hashrate_monitor_display_snapshot(&state, UINT64_C(8000000));
    assert(snapshot.fresh && snapshot.rate_gh == 0);
    hashrate_monitor_register_read(&state, REGISTER_TOTAL_COUNT, 0, 401, UINT64_C(8500000));
    assert(total[0].rate_sample_us == UINT64_C(8000000));

    /* The existing pause/reconnect reset clears display age with counters. */
    hashrate_monitor_reset_measurements(&state);
    assert(total[0].time_us == 0 && total[0].rate_sample_us == 0 && total[0].hashrate == 0);
    assert(!hashrate_monitor_display_snapshot(&state, UINT64_C(9000000)).fresh);
    hashrate_monitor_register_read(&state, REGISTER_TOTAL_COUNT, 0, UINT32_MAX-1, UINT64_C(10000000));
    assert(!hashrate_monitor_display_snapshot(&state, UINT64_C(10000000)).fresh);
    hashrate_monitor_register_read(&state, REGISTER_TOTAL_COUNT, 0, 1, UINT64_C(11000000));
    assert(fabsf(total[0].hashrate - hashCounterToGhs(UINT64_C(1000000), 3)) < 0.001f);
    assert(hashrate_monitor_display_snapshot(&state, UINT64_C(11000000)).fresh);
    assert(!hashrate_monitor_display_snapshot(NULL, UINT64_C(11000000)).fresh);
    state.DEVICE_CONFIG.family.asic_count = 2;
    assert(!hashrate_monitor_display_snapshot(&state, UINT64_C(11000000)).fresh);
    state.DEVICE_CONFIG.family.asic_count = 1;
    state.DEVICE_CONFIG.family.asic.small_core_count = 2040;
    state.POWER_MANAGEMENT_MODULE.frequency_value = 525;
    hashrate_monitor_reset_measurements(&state);
    const register_type_t registers[] = {REGISTER_TOTAL_COUNT,REGISTER_ERROR_COUNT,
        REGISTER_DOMAIN_0_COUNT,REGISTER_DOMAIN_1_COUNT,REGISTER_DOMAIN_2_COUNT,REGISTER_DOMAIN_3_COUNT};
    for (unsigned i = 0; i < 6; ++i)
        hashrate_monitor_register_read(&state,registers[i],0,100,UINT64_C(20000000));
    assert(!hashrate_monitor_autotune_snapshot(&state,UINT64_C(20000000)).valid);
    for (unsigned i = 0; i < 6; ++i)
        hashrate_monitor_register_read(&state,registers[i],0,i == 1 ? 100 : 300,UINT64_C(22000000));
    GammaAutotuneCounterSample tuning=hashrate_monitor_autotune_snapshot(&state,UINT64_C(22000000));
    assert(tuning.valid && tuning.error_percentage == 0 && tuning.useful_ghs > 0 && tuning.sample_id == UINT64_C(22000000));
    tuning=hashrate_monitor_autotune_snapshot(&state,UINT64_C(37000001));
    assert(!tuning.valid && strstr(tuning.readiness_reason,"stale"));
    domains[3].rate_sample_us=0;
    tuning=hashrate_monitor_autotune_snapshot(&state,UINT64_C(22000000));
    assert(!tuning.valid && tuning.blocked_domain==4 && strstr(tuning.readiness_reason,"no counter sample"));
    domains[3].rate_sample_us=UINT64_C(22000000);
    float domain_hashrate=domains[3].hashrate;
    domains[3].hashrate=1; /* Positive but nearly silent, not healthy liveness. */
    tuning=hashrate_monitor_autotune_snapshot(&state,UINT64_C(22000000));
    assert(!tuning.valid && tuning.blocked_domain==4 && strstr(tuning.readiness_reason,"20%"));
    domains[3].hashrate=domain_hashrate;
    errors[0].hashrate=total[0].hashrate*2;
    tuning=hashrate_monitor_autotune_snapshot(&state,UINT64_C(22000000));
    assert(!tuning.valid && !tuning.blocked_domain && strstr(tuning.readiness_reason,"error counter rate"));
    hashrate_monitor_register_read(&state,REGISTER_TOTAL_COUNT,0,400,UINT64_C(38000000));
    assert(total[0].rate_sample_us == 0); /* Long gap cannot revive an old rate. */
    hashrate_monitor_register_read(&state,REGISTER_TOTAL_COUNT,0,600,UINT64_C(40000000));
    assert(total[0].rate_sample_us == UINT64_C(40000000));
    assert(pthread_mutex_destroy(&monitor->lock) == 0);
    puts("PASS: production counter metadata, first/ignored samples, stale positive expiry, fresh zero and reset");
    return 0;
}
