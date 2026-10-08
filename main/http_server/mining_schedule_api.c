#include "mining_schedule_api.h"
#include "mining_schedule.h"
#include "http_server.h"
#include <string.h>

static int prebuffer_len = 1024;

static bool allowed(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
        return false;
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return true;
}

static cJSON *read_body(httpd_req_t *req)
{
    char body[2048];
    if (req->content_len < 1 || req->content_len >= sizeof(body)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Schedule body must be smaller than 2048 bytes");
        return NULL;
    }
    size_t received = 0;
    while (received < req->content_len) {
        int count = httpd_req_recv(req, body + received, req->content_len - received);
        if (count <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Incomplete request body");
            return NULL;
        }
        received += count;
    }
    body[received] = '\0';
    cJSON *json = strlen(body) == received ? cJSON_ParseWithOpts(body, NULL, true) : NULL;
    if (!json) httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON body");
    return json;
}

static esp_err_t get_schedule(httpd_req_t *req)
{
    if (!allowed(req)) return ESP_OK;
    cJSON *json = mining_schedule_get_json();
    if (!json) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Schedule status unavailable");
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = HTTP_send_json(req, json, &prebuffer_len);
    cJSON_Delete(json);
    return err;
}

static esp_err_t put_schedule(httpd_req_t *req)
{
    if (!allowed(req)) return ESP_OK;
    cJSON *json = read_body(req);
    if (!json) return ESP_OK;
    MiningScheduleSettings settings;
    bool valid = mining_schedule_parse(json, &settings);
    cJSON_Delete(json);
    if (!valid) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid schedule, timezone or pause window");
    esp_err_t err = mining_schedule_save(&settings);
    if (err != ESP_OK) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Schedule could not be committed; current schedule retained");
    return get_schedule(req);
}

static esp_err_t clear_override(httpd_req_t *req)
{
    if (!allowed(req)) return ESP_OK;
    cJSON *json = read_body(req);
    if (!json) return ESP_OK;
    cJSON *mode = cJSON_GetObjectItemCaseSensitive(json, "mode");
    bool valid = cJSON_IsObject(json) && json->child && !json->child->next &&
                 cJSON_IsString(mode) && strcmp(mode->valuestring, "schedule") == 0;
    cJSON_Delete(json);
    if (!valid) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected mode: schedule");
    mining_schedule_clear_override();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{}");
}

esp_err_t register_mining_schedule_api(httpd_handle_t server)
{
    const httpd_uri_t routes[] = {
        {.uri = "/api/system/mining/schedule", .method = HTTP_GET, .handler = get_schedule},
        {.uri = "/api/system/mining/schedule", .method = HTTP_PUT, .handler = put_schedule},
        {.uri = "/api/system/mining/schedule/override", .method = HTTP_POST, .handler = clear_override},
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        esp_err_t err = httpd_register_uri_handler(server, &routes[i]);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}
