#include "five_tratum_status.h"
#include "five_tratum_status_report.h"
#include "http_server.h"
#include "mux_peer_status.h"
#include "operating_profiles.h"
#include "protocol_coordinator.h"
#include "esp_timer.h"

#include <string.h>

static GlobalState *state;

static esp_err_t unavailable(httpd_req_t *req)
{
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"status-unavailable\"}");
}

static esp_err_t get_status(httpd_req_t *req)
{
    if (is_network_allowed(req) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    if (set_cors_headers(req) != ESP_OK ||
        httpd_resp_set_type(req, "application/json") != ESP_OK ||
        httpd_resp_set_hdr(req, "Cache-Control", "no-store") != ESP_OK)
        return httpd_resp_send_500(req);
    if (!state) return unavailable(req);

    const unsigned generation = protocol_coordinator_work_generation();
    const bool fallback = state->SYSTEM_MODULE.is_using_fallback;
    const stratum_protocol_t protocol = state->stratum_protocol;
    FiveTratumStatusReport snapshot = {
        .peer = mux_peer_status_snapshot(),
        .using_fallback = fallback,
        .protocol_v1 = protocol == STRATUM_PROTOCOL_V1,
    };
    // Do not carry a prior session's acknowledgement across a pool/protocol
    // transition. The peer itself is copied with its TTL clock under its lock.
    snapshot.coherent = generation == protocol_coordinator_work_generation() &&
                        fallback == state->SYSTEM_MODULE.is_using_fallback &&
                        protocol == state->stratum_protocol;
    const int64_t now_us = esp_timer_get_time();
    if (now_us < 0) return unavailable(req);
    snapshot.uptime_seconds = (uint64_t)now_us / 1000000ULL;
    cJSON *binding = operating_profiles_binding();
    cJSON *report = five_tratum_status_report(binding, &snapshot);
    cJSON_Delete(binding);
    if (!report) return unavailable(req);
    char *body = cJSON_PrintUnformatted(report);
    cJSON_Delete(report);
    if (!body) return unavailable(req);
    if (strlen(body) > FIVE_TRATUM_STATUS_MAX_BYTES) {
        cJSON_free(body);
        return unavailable(req);
    }
    const esp_err_t result = httpd_resp_sendstr(req, body);
    cJSON_free(body);
    return result;
}

esp_err_t five_tratum_status_register(httpd_handle_t server, GlobalState *global)
{
    if (!global) return ESP_ERR_INVALID_ARG;
    state = global;
    const httpd_uri_t route = {
        .uri = "/api/5tratum/status",
        .method = HTTP_GET,
        .handler = get_status,
    };
    return httpd_register_uri_handler(server, &route);
}
