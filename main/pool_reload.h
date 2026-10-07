#ifndef FIVE_POOL_RELOAD_H
#define FIVE_POOL_RELOAD_H
#include "global_state.h"
#include "esp_err.h"
#include <stddef.h>

// Coordinator only, after the primary protocol has acknowledged shutdown.
// The caller holds the profiles lock to snapshot a whole committed pool transaction.
esp_err_t SYSTEM_reload_primary_pool(GlobalState *state, stratum_protocol_t *protocol);
// Copies display/BAP/share identity while protecting old strings from replacement.
void SYSTEM_copy_pool_identity(GlobalState *state, bool fallback, char *host, size_t host_size,
                              char *user, size_t user_size, uint16_t *port);
void SYSTEM_copy_pool_options(GlobalState *state, bool fallback, uint16_t *tls, bool *decode);
// Caller frees this exact saved username, including legacy values over 512 bytes.
char *SYSTEM_duplicate_pool_user(GlobalState *state, bool fallback);
// Connection destruction and ASIC share writes share this bounded I/O lock.
void SYSTEM_stratum_io_lock(void);
void SYSTEM_stratum_io_unlock(void);
#endif
