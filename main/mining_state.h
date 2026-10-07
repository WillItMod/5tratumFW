#ifndef MINING_STATE_H
#define MINING_STATE_H
#include "global_state.h"

static inline bool mining_state_should_stop(const GlobalState *state)
{
    const SystemModule *system = &state->SYSTEM_MODULE;
    return system->mining_paused || system->hardware_fault || system->pools_unavailable ||
           (!state->SELF_TEST_MODULE.is_active && !system->mining_runtime_ready);
}

static inline bool mining_state_work_allowed(const GlobalState *state)
{
    return state->ASIC_initalized && !mining_state_should_stop(state);
}
#endif
