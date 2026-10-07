#ifndef PROFILES_RUNTIME_STUBS_H
#define PROFILES_RUNTIME_STUBS_H
#include "sdk_stubs.h"
#include <pthread.h>
typedef struct { size_t content_len, received; const char *body; int code; char reply[32768]; } httpd_req_t;
typedef enum { HTTP_GET, HTTP_POST } httpd_method_t;
typedef struct { const char *uri; httpd_method_t method; esp_err_t (*handler)(httpd_req_t *); } httpd_uri_t;
#define HTTPD_401_UNAUTHORIZED 401
#define HTTPD_500_INTERNAL_SERVER_ERROR 500
esp_err_t httpd_resp_send_err(httpd_req_t *, int, const char *);
esp_err_t httpd_resp_set_hdr(httpd_req_t *, const char *, const char *);
esp_err_t httpd_resp_set_type(httpd_req_t *, const char *);
esp_err_t httpd_resp_set_status(httpd_req_t *, const char *);
esp_err_t httpd_resp_sendstr(httpd_req_t *, const char *);
void httpd_resp_send_500(httpd_req_t *);
int httpd_req_recv(httpd_req_t *, char *, size_t);
esp_err_t httpd_register_uri_handler(httpd_handle_t, const httpd_uri_t *);
typedef struct { char version[32]; } esp_app_desc_t;
const esp_app_desc_t *esp_app_get_description(void);
#define ESP_MAC_WIFI_STA 0
esp_err_t esp_read_mac(uint8_t *, int);
int mbedtls_sha256(const unsigned char *, size_t, unsigned char *, int);
void protocol_coordinator_request_primary_reload(GlobalState *);
#endif
