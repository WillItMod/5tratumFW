#ifndef FIVETRATUM_GAMMA_AUTOTUNE_RUNTIME_H
#define FIVETRATUM_GAMMA_AUTOTUNE_RUNTIME_H
#include <stdbool.h>
#include <stdint.h>
#include "../firmware/nerdqaxe/main/autotune/presentation.h"
typedef struct GlobalState GlobalState;
typedef struct {
    bool available, supported, telemetryFresh, fault, active, ownsSettings, restoring, currentAvailable;
    fwt_state state;
    fwt_priority priority;
    fwt_point current; /* Confirmed requested settings, not measured PLL frequency. */
    fwt_search_view search;
    char reason[96];
} GammaAutotuneDisplaySnapshot;
/* RAM-only, nonblocking copy. Safe from an LVGL callback: never waits for the
 * tuner mutex, never polls sensors, and never changes observer/flash state.
 * False clears the output; callers must not reuse an older successful copy. */
bool autotune_gamma_try_display_snapshot(GammaAutotuneDisplaySnapshot *out, uint64_t now_us);
void autotune_gamma_init(GlobalState *state);
/* Power-owner only. True supplies a volatile target; no configuration write. */
bool autotune_gamma_select_target(GlobalState *state, uint16_t *voltage_mv, float *frequency_mhz);
bool autotune_gamma_transition_finished(GlobalState *state, bool success);
bool autotune_gamma_transition_allowed(void *state);
bool autotune_gamma_transition_cancelled(void);
void autotune_gamma_manual_override(void);
/* Wire-frame lease: 0=denied, 1=ordinary path, 2=locked adaptive frame. */
int autotune_gamma_wire_begin(bool safety_power_off);
void autotune_gamma_wire_end(int token);
void autotune_gamma_owner_idle(void);
void autotune_gamma_interrupt(const char *reason);
#endif
