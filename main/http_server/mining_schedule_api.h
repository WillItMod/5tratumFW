#ifndef MINING_SCHEDULE_API_H
#define MINING_SCHEDULE_API_H
#include "esp_http_server.h"
esp_err_t register_mining_schedule_api(httpd_handle_t server);
#endif
