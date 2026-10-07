#pragma once
struct esp_app_desc_t {const char *version;};inline const esp_app_desc_t *esp_app_get_description(){static esp_app_desc_t d{"5tratumFW-qa-web-a4"};return &d;}
