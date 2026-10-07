#pragma once
#include "esp_http_server.h"
esp_err_t GET_five_tratum_profiles(httpd_req_t *req);
esp_err_t POST_five_tratum_profiles(httpd_req_t *req);
esp_err_t POST_five_tratum_profiles_apply(httpd_req_t *req);

esp_err_t GET_five_tratum_pool_schedule(httpd_req_t *req);
esp_err_t POST_five_tratum_pool_schedule(httpd_req_t *req);
