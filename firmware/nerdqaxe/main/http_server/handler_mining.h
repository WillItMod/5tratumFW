#pragma once
#include "esp_http_server.h"
esp_err_t GET_mining_schedule(httpd_req_t *req);
esp_err_t PUT_mining_schedule(httpd_req_t *req);
esp_err_t POST_mining_pause(httpd_req_t *req);
esp_err_t POST_mining_resume(httpd_req_t *req);
esp_err_t POST_mining_schedule_override(httpd_req_t *req);
