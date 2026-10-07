#pragma once
#include <string>
using esp_err_t=int;using httpd_handle_t=int;
constexpr int ESP_OK=0,ESP_FAIL=-1,HTTPD_401_UNAUTHORIZED=401,HTTPD_RESP_USE_STRLEN=-1;
struct httpd_req_t {size_t content_len=0;std::string body,status="200 OK",response;void *user_ctx=nullptr;bool authorized=true,otp=true;};
inline void httpd_resp_set_status(httpd_req_t *r,const char *s){r->status=s;}
inline void httpd_resp_set_type(httpd_req_t *,const char *){}
inline void httpd_resp_set_hdr(httpd_req_t *,const char *,const char *){}
inline esp_err_t httpd_resp_send_err(httpd_req_t *r,int status,const char *s){r->status=std::to_string(status);r->response=s;return ESP_FAIL;}
inline esp_err_t httpd_resp_send_500(httpd_req_t *r){r->status="500";return ESP_FAIL;}
