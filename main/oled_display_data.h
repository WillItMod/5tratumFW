#ifndef FIVE_TRATUM_OLED_DISPLAY_DATA_H
#define FIVE_TRATUM_OLED_DISPLAY_DATA_H

/* Display-only formatting. No inferred coin, route or per-chip measurement. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <inttypes.h>

typedef enum {
    OLED_RATE, OLED_HEALTH, OLED_SHARES, OLED_ROUTE, OLED_OPERATING, OLED_WIFI,
    OLED_JOB, OLED_CANDIDATE, OLED_FAULT, OLED_OVERHEAT, OLED_SELF_TEST,
    OLED_UPDATE, OLED_SETUP, OLED_CONNECTION, OLED_BOOT, OLED_CREDITS
} FiveTratumOledPage;

typedef struct {
    float rate_gh, temperature_c, temperature2_c, vr_temperature_c, power_w;
    float fan_percent, configured_mhz, configured_mv, core_mv, input_mv;
    uint16_t fan_rpm;
    uint64_t accepted, rejected, uptime_seconds;
    int rssi, block_height;
    bool initialized, runtime_ready, requested_paused, applied_paused, pools_unavailable, rate_fresh;
    bool wifi_connected, fallback, sv2, tls, mux_connected, mux_acknowledged, mux_expired, hardware_fault;
    bool mux_fallback;
    const char *best_session, *best_ever, *host, *ip, *ssid, *ap_ssid;
    const char *network_difficulty, *scriptsig, *message, *result, *finished;
    const char *filename, *update_status, *board_name, *board_version;
} FiveTratumOledData;

static inline const char *oled_text(const char *text) { return text && *text ? text : "--"; }
static inline bool oled_positive(float value) { return isfinite(value) && value > 0; }
static inline bool oled_rate_available(const FiveTratumOledData *d)
{
    return d->initialized && d->runtime_ready && !d->requested_paused &&
           !d->applied_paused && !d->pools_unavailable && d->rate_fresh &&
           isfinite(d->rate_gh) && d->rate_gh >= 0;
}
static inline const char *oled_mining_state(const FiveTratumOledData *d)
{
    if (d->requested_paused != d->applied_paused) return d->requested_paused ? "PAUSING" : "RESUMING";
    if (d->applied_paused) return "PAUSED";
    if (!d->runtime_ready || !d->initialized) return "STARTING";
    if (d->pools_unavailable) return "NO POOL";
    if (!d->rate_fresh) return "NO DATA";
    return "MINING";
}
static inline void oled_count(char *out, size_t size, uint64_t value)
{
    const char *units[] = {"", "K", "M", "G", "T", "P", "E"};
    double scaled = (double)value;
    unsigned unit = 0;
    while (scaled >= 10000 && unit < 6) { scaled /= 1000; ++unit; }
    if (!unit) snprintf(out, size, "%" PRIu64, value);
    else snprintf(out, size, "%.1f%s", scaled, units[unit]);
}
static inline void oled_number(char *out, size_t size, float value, const char *unit, unsigned decimals)
{
    if (!oled_positive(value)) snprintf(out, size, "--%s", unit);
    else snprintf(out, size, "%.*f%s", (int)decimals, (double)value, unit);
}
static inline void oled_route(char *out, size_t size, const FiveTratumOledData *d)
{
    bool matching = !d->sv2 && d->mux_connected && d->mux_fallback == d->fallback;
    const char *route = matching && d->mux_acknowledged ? "5tratMUX" :
                        matching && d->mux_expired ? "5tratMUX stale" :
                        d->sv2 ? "SV2" : d->tls ? "SV1/TLS" : "SV1/TCP";
    snprintf(out, size, "P%u %s", d->fallback ? 2 : 1, route);
}
static inline bool oled_safety_active(bool self_test, bool overheat, bool fault, bool asic_status)
{
    return self_test || overheat || fault || asic_status;
}
/* Priority is used by the runtime and tests, including identify/button handling. */
static inline FiveTratumOledPage oled_priority_page(bool self_test, bool overheat, bool fault,
                                                   bool asic_status, bool update)
{
    if (overheat) return OLED_OVERHEAT;
    if (fault) return OLED_FAULT;
    if (self_test) return OLED_SELF_TEST;
    if (asic_status) return OLED_FAULT;
    if (update) return OLED_UPDATE;
    return OLED_RATE;
}
#endif
