#ifndef FIVE_TRATUM_STATUS_H
#define FIVE_TRATUM_STATUS_H

#include "global_state.h"
#include "esp_http_server.h"

esp_err_t five_tratum_status_register(httpd_handle_t server, GlobalState *state);

#endif
