#ifndef FIVETRATUM_GAMMA_AUTOTUNE_RUNTIME_H
#define FIVETRATUM_GAMMA_AUTOTUNE_RUNTIME_H
#include <stdbool.h>
#include <stdint.h>
typedef struct GlobalState GlobalState;
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
