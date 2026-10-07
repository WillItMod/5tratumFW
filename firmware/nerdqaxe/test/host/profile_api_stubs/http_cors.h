#pragma once
#include "esp_http_server.h"
inline int is_network_allowed(httpd_req_t *r){return r->authorized?ESP_OK:ESP_FAIL;}
inline int set_cors_headers(httpd_req_t *){return ESP_OK;}
