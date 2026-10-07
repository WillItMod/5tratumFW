#include "pool_reload.h"
#include "nvs_config.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pthread_mutex_t config_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t io_mutex = PTHREAD_MUTEX_INITIALIZER;
void SYSTEM_stratum_io_lock(void) { pthread_mutex_lock(&io_mutex); }
void SYSTEM_stratum_io_unlock(void) { pthread_mutex_unlock(&io_mutex); }
esp_err_t SYSTEM_reload_primary_pool(GlobalState *state, stratum_protocol_t *protocol)
{
    if (!state || !protocol) return ESP_ERR_INVALID_ARG;
    char *kind = nvs_config_get_string(NVS_CONFIG_STRATUM_PROTOCOL);
    char *host = nvs_config_get_string(NVS_CONFIG_STRATUM_URL);
    char *user = nvs_config_get_string(NVS_CONFIG_STRATUM_USER);
    char *password = nvs_config_get_string(NVS_CONFIG_STRATUM_PASS);
    char *certificate = nvs_config_get_string(NVS_CONFIG_STRATUM_CERT);
    esp_err_t error = ESP_OK;
    stratum_protocol_t parsed = STRATUM_PROTOCOL_UNKNOWN;
    uint16_t port = nvs_config_get_u16(NVS_CONFIG_STRATUM_PORT);
    uint16_t tls = nvs_config_get_u16(NVS_CONFIG_STRATUM_TLS);
    uint16_t difficulty = nvs_config_get_u16(NVS_CONFIG_STRATUM_DIFFICULTY);
    bool extranonce = nvs_config_get_bool(NVS_CONFIG_STRATUM_EXTRANONCE_SUBSCRIBE);
    bool decode = nvs_config_get_bool(NVS_CONFIG_STRATUM_DECODE_COINBASE_TX);
    if (!kind || !host || !user || !password || !certificate) error = ESP_ERR_NO_MEM;
    else {
        if (!strcmp(kind, "SV1")) parsed = STRATUM_PROTOCOL_V1;
        else if (!strcmp(kind, "SV2")) parsed = STRATUM_PROTOCOL_V2;
        if (parsed == STRATUM_PROTOCOL_UNKNOWN || !host[0] || strlen(host) > 253 || strlen(user) > 512 ||
            strlen(password) > 512 || strlen(certificate) > 2048 || !port || tls > 2) error = ESP_ERR_INVALID_ARG;
    }
    free(kind);
    if (error != ESP_OK) { free(host); free(user); free(password); free(certificate); return error; }
    pthread_mutex_lock(&config_mutex);
    SystemModule *module = &state->SYSTEM_MODULE;
    char *old_host = module->pool_url, *old_user = module->pool_user;
    char *old_password = module->pool_pass, *old_certificate = module->pool_cert;
    module->pool_url = host; module->pool_user = user;
    module->pool_pass = password; module->pool_cert = certificate;
    module->pool_port = port; module->pool_tls = tls; module->pool_difficulty = difficulty;
    module->pool_extranonce_subscribe = extranonce; module->pool_decode_coinbase_tx = decode;
    // No fallback, pause, voltage, clock, fan or ASIC state is changed by a route reload.
    *protocol = parsed;
    free(old_host); free(old_user); free(old_password); free(old_certificate);
    pthread_mutex_unlock(&config_mutex);
    return ESP_OK;
}
void SYSTEM_copy_pool_identity(GlobalState *state, bool fallback, char *host, size_t host_size,
                              char *user, size_t user_size, uint16_t *port)
{
    pthread_mutex_lock(&config_mutex);
    const SystemModule *module = &state->SYSTEM_MODULE;
    const char *url = fallback ? module->fallback_pool_url : module->pool_url;
    const char *username = fallback ? module->fallback_pool_user : module->pool_user;
    if (host && host_size) snprintf(host, host_size, "%s", url ? url : "");
    if (user && user_size) snprintf(user, user_size, "%s", username ? username : "");
    if (port) *port = fallback ? module->fallback_pool_port : module->pool_port;
    pthread_mutex_unlock(&config_mutex);
}
void SYSTEM_copy_pool_options(GlobalState *state, bool fallback, uint16_t *tls, bool *decode)
{
    pthread_mutex_lock(&config_mutex);
    const SystemModule *module = &state->SYSTEM_MODULE;
    if (tls) *tls = fallback ? module->fallback_pool_tls : module->pool_tls;
    if (decode) *decode = fallback ? module->fallback_pool_decode_coinbase_tx : module->pool_decode_coinbase_tx;
    pthread_mutex_unlock(&config_mutex);
}
char *SYSTEM_duplicate_pool_user(GlobalState *state, bool fallback)
{
    pthread_mutex_lock(&config_mutex);
    const SystemModule *module = &state->SYSTEM_MODULE;
    const char *user = fallback ? module->fallback_pool_user : module->pool_user;
    char *copy = strdup(user ? user : "");
    pthread_mutex_unlock(&config_mutex);
    return copy;
}
