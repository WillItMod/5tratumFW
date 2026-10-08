#ifndef GAMMA_STATUS_SUPPORT_H
#define GAMMA_STATUS_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 0x102
#define HTTPD_401_UNAUTHORIZED 401
#define ESP_MAC_WIFI_STA 0

typedef enum {
    STRATUM_PROTOCOL_UNKNOWN = 0,
    STRATUM_PROTOCOL_V1 = 1,
    STRATUM_PROTOCOL_V2 = 2,
} stratum_protocol_t;
typedef struct {
    struct { struct { const char *name; struct { const char *name; } asic; int asic_count; } family; } DEVICE_CONFIG;
    struct { bool is_using_fallback; } SYSTEM_MODULE;
    stratum_protocol_t stratum_protocol;
} GlobalState;
typedef void *httpd_handle_t;
typedef struct {
    int code;
    unsigned no_store, cors, sends;
    char body[4096];
} httpd_req_t;
typedef enum { HTTP_GET, HTTP_POST } httpd_method_t;
typedef struct {
    const char *uri;
    httpd_method_t method;
    esp_err_t (*handler)(httpd_req_t *);
} httpd_uri_t;
typedef struct { const char *version; } esp_app_desc_t;
const esp_app_desc_t *esp_app_get_description(void);
esp_err_t esp_read_mac(uint8_t *, int);
int mbedtls_sha256(const unsigned char *, size_t, unsigned char *, int);
char *nvs_config_get_string(int);
#define NVS_CONFIG_BOARD_VERSION 0
esp_err_t is_network_allowed(httpd_req_t *);
esp_err_t set_cors_headers(httpd_req_t *);
esp_err_t httpd_resp_send_err(httpd_req_t *, int, const char *);
esp_err_t httpd_resp_set_hdr(httpd_req_t *, const char *, const char *);
esp_err_t httpd_resp_set_type(httpd_req_t *, const char *);
esp_err_t httpd_resp_set_status(httpd_req_t *, const char *);
esp_err_t httpd_resp_sendstr(httpd_req_t *, const char *);
esp_err_t httpd_resp_send_500(httpd_req_t *);
esp_err_t httpd_register_uri_handler(httpd_handle_t, const httpd_uri_t *);
unsigned protocol_coordinator_work_generation(void);
int64_t esp_timer_get_time(void);
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
void test_enter_critical(int *);
void test_exit_critical(int *);
#define taskENTER_CRITICAL(lock) test_enter_critical(lock)
#define taskEXIT_CRITICAL(lock) test_exit_critical(lock)

void set_binding_state(GlobalState *);

#endif
