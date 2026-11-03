#include "work_difficulty_api.h"
#include "http_server.h"
#include "mining_state.h"
#include "esp_timer.h"
#include <math.h>
static GlobalState *state;

cJSON *work_difficulty_report(const WorkDifficultySnapshot *snapshot)
{
    if(!snapshot)return NULL;
    cJSON *root=cJSON_CreateObject();if(!root)return NULL;
    if(!cJSON_AddNumberToObject(root,"schemaVersion",1)||!cJSON_AddStringToObject(root,"normalization","bitcoin-diff1"))goto failed;
    cJSON *array=cJSON_AddArrayToObject(root,"pools");if(!array)goto failed;
    for(unsigned i=0;i<2;i++) {
        const WorkDifficultyPool *p=&snapshot->pools[i];
        const bool available=p->available&&p->selected&&p->connected&&p->received_at_us&&snapshot->now_us>=p->received_at_us&&isfinite(p->share_difficulty)&&p->share_difficulty>0&&isfinite(p->network_difficulty)&&p->network_difficulty>0;
        cJSON *row=cJSON_CreateObject();if(!row)goto failed;
        if(!cJSON_AddItemToArray(array,row)){cJSON_Delete(row);goto failed;}
        if(!cJSON_AddNumberToObject(row,"index",i)||!cJSON_AddBoolToObject(row,"selected",p->selected)||!cJSON_AddBoolToObject(row,"connected",p->connected)||!cJSON_AddBoolToObject(row,"available",available)||!cJSON_AddStringToObject(row,"source","active-stratum-job"))goto failed;
        if(available) {
            if(!cJSON_AddNumberToObject(row,"shareDifficulty",p->share_difficulty)||!cJSON_AddNumberToObject(row,"networkDifficulty",p->network_difficulty)||!cJSON_AddNumberToObject(row,"jobAgeSeconds",(double)(snapshot->now_us-p->received_at_us)/1000000.0))goto failed;
        } else if(!cJSON_AddNullToObject(row,"shareDifficulty")||!cJSON_AddNullToObject(row,"networkDifficulty")||!cJSON_AddNullToObject(row,"jobAgeSeconds"))goto failed;
    }
    return root;
failed:cJSON_Delete(root);return NULL;
}
static esp_err_t get_work_difficulty(httpd_req_t *req)
{
    if(is_network_allowed(req)!=ESP_OK)return httpd_resp_send_err(req,HTTPD_401_UNAUTHORIZED,"Unauthorized");
    if(set_cors_headers(req)!=ESP_OK||httpd_resp_set_type(req,"application/json")!=ESP_OK||httpd_resp_set_hdr(req,"Cache-Control","no-store")!=ESP_OK)return httpd_resp_send_500(req);
    if(!state)return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Difficulty unavailable");
    const int64_t now=esp_timer_get_time();WorkDifficultySnapshot snapshot;
    work_difficulty_snapshot(&snapshot,now>0?(uint64_t)now:0,mining_state_work_allowed(state));
    if(!mining_state_work_allowed(state))for(unsigned i=0;i<2;i++)snapshot.pools[i].available=false;
    cJSON *report=work_difficulty_report(&snapshot);if(!report)return httpd_resp_send_500(req);
    char *text=cJSON_PrintUnformatted(report);cJSON_Delete(report);if(!text)return httpd_resp_send_500(req);
    const esp_err_t result=httpd_resp_sendstr(req,text);cJSON_free(text);return result;
}
esp_err_t work_difficulty_register(httpd_handle_t server,GlobalState *global)
{
    if (!global) return ESP_ERR_INVALID_ARG;
    state = global;
    const httpd_uri_t route={.uri="/api/5tratum/work-difficulty",.method=HTTP_GET,.handler=get_work_difficulty};
    return httpd_register_uri_handler(server,&route);
}
