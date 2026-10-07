#ifndef OPERATING_PROFILES_H
#define OPERATING_PROFILES_H
#include "global_state.h"
#include "esp_http_server.h"
#include "cJSON.h"
#define FIVE_PROFILE_SLOTS 10

esp_err_t operating_profiles_register(httpd_handle_t server, GlobalState *state);
void operating_profiles_init(GlobalState *state);
bool operating_profile_pool_exists(unsigned slot);
bool operating_profile_pool_name(unsigned slot, char *name, size_t size);
bool operating_profile_pool_hash(unsigned slot, uint32_t *hash);
bool operating_profile_current_pool_hash(bool fallback, uint32_t *hash);
esp_err_t operating_profile_apply_pool(unsigned slot, bool fallback);
esp_err_t operating_profile_apply_pool_guarded(unsigned slot, bool fallback, bool (*allow)(void *), void *context);
esp_err_t operating_profiles_with_pool_slots(uint16_t slot_mask, esp_err_t (*commit)(void *), void *context);
// Scheduler supplies this guard; a referenced slot cannot be changed/deleted.
bool operating_profile_pool_referenced(unsigned slot);
cJSON *operating_profiles_binding(void);
#endif
