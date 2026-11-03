#include "handler_work_difficulty.h"
#include "global_state.h"
#include "work_difficulty_report.h"
#include "http_cors.h"
#include "http_utils.h"

esp_err_t GET_five_tratum_work_difficulty(httpd_req_t *req) {
    ConGuard connection(http_server,req);
    if(is_network_allowed(req)!=ESP_OK)return httpd_resp_send_err(req,HTTPD_401_UNAUTHORIZED,"Unauthorized");
    httpd_resp_set_type(req,"application/json");httpd_resp_set_hdr(req,"Cache-Control","no-store");
    if(set_cors_headers(req)!=ESP_OK){httpd_resp_send_500(req);return ESP_FAIL;}
    FiveTratumWork::Snapshot snapshot;
    if(STRATUM_MANAGER)snapshot=STRATUM_MANAGER->getWorkDifficulty();
    JsonDocument document;
    if(!FiveTratumWork::report(document,snapshot))return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Difficulty unavailable");
    return sendJsonResponse(req,document);
}
