#include "tuning_schedule_api.h"
#include "tuning_schedule.h"
#include "http_server.h"
#include "nvs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static int prebuffer=2048;
static esp_err_t error_response(httpd_req_t *req,const char *status,const char *code)
{
    httpd_resp_set_status(req,status);httpd_resp_set_type(req,"application/json");
    char body[96];snprintf(body,sizeof(body),"{\"ok\":false,\"error\":\"%s\"}",code);
    return httpd_resp_sendstr(req,body);
}
static bool allowed(httpd_req_t *req)
{
    if(is_network_allowed(req)!=ESP_OK){error_response(req,"401 Unauthorized","unauthorized");return false;}
    if(set_cors_headers(req)!=ESP_OK){error_response(req,"500 Internal Server Error","allocation-failed");return false;}
    httpd_resp_set_hdr(req,"Cache-Control","no-store");return true;
}
static esp_err_t get_schedule(httpd_req_t *req)
{
    if(!allowed(req))return ESP_OK;
    cJSON *json=tuning_schedule_get_json();
    if(!json)return error_response(req,"500 Internal Server Error","storage-unavailable");
    httpd_resp_set_type(req,"application/json");
    esp_err_t error=HTTP_send_json(req,json,&prebuffer);cJSON_Delete(json);return error;
}
static esp_err_t post_schedule(httpd_req_t *req)
{
    if(!allowed(req))return ESP_OK;
    if(!req->content_len || req->content_len>=2048)return error_response(req,"400 Bad Request","invalid-profile");
    char *body=malloc(req->content_len+1);
    if(!body)return error_response(req,"500 Internal Server Error","allocation-failed");
    size_t received=0;
    while(received<req->content_len){int count=httpd_req_recv(req,body+received,req->content_len-received);if(count<=0)break;received+=count;}
    body[received]=0;
    cJSON *json=received==req->content_len && strlen(body)==received && !strstr(body,"\\u0000")?cJSON_ParseWithOpts(body,NULL,true):NULL;
    TuningSchedule schedule;
    bool valid=tuning_schedule_parse(json,&schedule);free(body);cJSON_Delete(json);
    if(!valid)return error_response(req,"400 Bad Request","invalid-profile");
    esp_err_t error=tuning_schedule_save(&schedule);
    if(error==ESP_ERR_INVALID_ARG || error==ESP_ERR_NVS_NOT_FOUND)return error_response(req,"409 Conflict","empty-slot");
    if(error==ESP_ERR_NO_MEM)return error_response(req,"500 Internal Server Error","allocation-failed");
    if(error!=ESP_OK)return error_response(req,"507 Insufficient Storage","storage-unavailable");
    // Saving is separate from the timed runtime activation, just like QAxe.
    httpd_resp_set_type(req,"application/json");return httpd_resp_sendstr(req,"{\"ok\":true,\"restartRequired\":false}");
}
esp_err_t register_tuning_schedule_api(httpd_handle_t server)
{
    const httpd_uri_t routes[]={
        {.uri="/api/5tratum/tuning-schedule",.method=HTTP_GET,.handler=get_schedule},
        {.uri="/api/5tratum/tuning-schedule",.method=HTTP_POST,.handler=post_schedule}
    };
    for(unsigned i=0;i<sizeof(routes)/sizeof(routes[0]);i++){esp_err_t error=httpd_register_uri_handler(server,&routes[i]);if(error!=ESP_OK)return error;}
    return ESP_OK;
}
