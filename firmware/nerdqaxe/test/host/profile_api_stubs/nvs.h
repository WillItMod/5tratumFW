#pragma once
#include "esp_http_server.h"
using nvs_handle_t=int;constexpr int NVS_READONLY=0,NVS_READWRITE=1,ESP_ERR_NVS_NOT_FOUND=2;
int nvs_open(const char *,int,nvs_handle_t *);int nvs_get_blob(nvs_handle_t,const char *,void *,size_t *);int nvs_set_blob(nvs_handle_t,const char *,const void *,size_t);int nvs_commit(nvs_handle_t);void nvs_close(nvs_handle_t);int nvs_erase_key(nvs_handle_t,const char *);
