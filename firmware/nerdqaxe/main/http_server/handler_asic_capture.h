#pragma once
#include "esp_http_server.h"
esp_err_t GET_asic_capture(httpd_req_t *req);
esp_err_t POST_asic_capture(httpd_req_t *req);
