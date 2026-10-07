#pragma once

#include "esp_http_server.h"
#include "capabilities_report.h"

esp_err_t GET_five_tratum_capabilities(httpd_req_t *req);
bool readFiveTratumPhysicalDeviceId(char output[FiveTratumCapabilities::DEVICE_ID_SIZE]);
