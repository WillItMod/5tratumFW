#ifndef SCHEDULE_RUNTIME_STUBS_H
#define SCHEDULE_RUNTIME_STUBS_H
#include "sdk_stubs.h"
#include <sys/time.h>
typedef struct {
    bool start;
    void (*sync_cb)(struct timeval *);
} esp_sntp_config_t;
#define ESP_NETIF_SNTP_DEFAULT_CONFIG(server) ((esp_sntp_config_t){.start = true})
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *);
esp_err_t esp_netif_sntp_start(void);

typedef struct {
    size_t content_len, received;
    const char *body;
    int code;
    char reply[4096];
    char content_type[32], response_type[32];
} httpd_req_t;
typedef enum { HTTP_GET, HTTP_PUT, HTTP_POST } httpd_method_t;
typedef struct {
    const char *uri;
    httpd_method_t method;
    esp_err_t (*handler)(httpd_req_t *);
} httpd_uri_t;
#define HTTPD_400_BAD_REQUEST 400
#define HTTPD_401_UNAUTHORIZED 401
#define HTTPD_500_INTERNAL_SERVER_ERROR 500
esp_err_t httpd_resp_send_err(httpd_req_t *, int, const char *);
esp_err_t httpd_resp_set_hdr(httpd_req_t *, const char *, const char *);
esp_err_t httpd_resp_set_type(httpd_req_t *, const char *);
esp_err_t httpd_resp_sendstr(httpd_req_t *, const char *);
int httpd_req_recv(httpd_req_t *, char *, size_t);
esp_err_t httpd_register_uri_handler(httpd_handle_t, const httpd_uri_t *);
#endif
