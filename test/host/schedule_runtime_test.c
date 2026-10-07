#include "schedule_runtime_stubs.h"
#include "mining_schedule.h"
#include "mining_schedule_api.h"
#include "nvs_config.h"
#include "http_server.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static time_t now;
static GlobalState global;
static esp_err_t read_result = ESP_ERR_NVS_NOT_FOUND, save_result = ESP_OK;
static const char *stored;
static unsigned writes;
static char last_written[2048];
static int fail_allocation = -1;
static void *test_malloc(size_t size) {
    if (fail_allocation == 0) { fail_allocation = -1; return NULL; }
    if (fail_allocation > 0) fail_allocation--;
    return malloc(size);
}
static bool network_allowed = true;
static void (*sync_callback)(struct timeval *);
static httpd_uri_t routes[3];
static unsigned route_count;
static const char *disabled = "{\"enabled\":false,\"timezone\":\"UTC\",\"windows\":[]}";
static const char *weekly = "{\"enabled\":true,\"timezone\":\"UTC\",\"windows\":[{\"days\":2,\"start\":\"20:00\",\"end\":\"21:00\"}]}";

time_t time(time_t *value) { if (value) *value = now; return now; }
static void at(int year, int month, int day, int hour, int minute)
{
    struct tm utc = {.tm_year = year - 1900, .tm_mon = month - 1, .tm_mday = day, .tm_hour = hour, .tm_min = minute};
    now = timegm(&utc);
}
const char *esp_err_to_name(esp_err_t err) { (void)err; return "host error"; }
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (void *)1; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t h, unsigned ticks) { (void)h; (void)ticks; return pdPASS; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t h) { (void)h; return pdPASS; }
esp_err_t nvs_config_read_stored_string(NvsConfigKey key, char **value)
{
    assert(key == NVS_CONFIG_MINING_SCHEDULE);
    *value = read_result == ESP_OK ? strdup(stored) : NULL;
    return read_result;
}
esp_err_t nvs_config_set_string_sync(NvsConfigKey key, const char *value)
{
    assert(key == NVS_CONFIG_MINING_SCHEDULE && value != NULL);
    writes++;
    snprintf(last_written, sizeof(last_written), "%s", value);
    return save_result;
}
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *config)
{
    assert(!config->start);
    sync_callback = config->sync_cb;
    return ESP_OK;
}
esp_err_t esp_netif_sntp_start(void) { return ESP_OK; }
static void sync_time(void)
{
    struct timeval tv = {.tv_sec = now};
    assert(sync_callback);
    sync_callback(&tv);
    mining_schedule_update();
}
esp_err_t is_network_allowed(httpd_req_t *req) { (void)req; return network_allowed ? ESP_OK : ESP_FAIL; }
esp_err_t httpd_resp_send_err(httpd_req_t *req, int code, const char *message)
{
    req->code = code; snprintf(req->reply, sizeof(req->reply), "%s", message); return ESP_OK;
}
esp_err_t httpd_resp_set_hdr(httpd_req_t *req, const char *k, const char *v) { (void)req; (void)k; (void)v; return ESP_OK; }
esp_err_t httpd_resp_set_type(httpd_req_t *req, const char *type) { (void)req; (void)type; return ESP_OK; }
esp_err_t httpd_resp_sendstr(httpd_req_t *req, const char *body)
{
    req->code = 200; snprintf(req->reply, sizeof(req->reply), "%s", body); return ESP_OK;
}
int httpd_req_recv(httpd_req_t *req, char *out, size_t size)
{
    size_t remaining = req->content_len - req->received;
    if (size > remaining) size = remaining;
    if (size > 7) size = 7; // Exercise body reads across many HTTP chunks.
    memcpy(out, req->body + req->received, size); req->received += size; return size;
}
esp_err_t httpd_register_uri_handler(httpd_handle_t server, const httpd_uri_t *route)
{
    (void)server; assert(route_count < 3); routes[route_count++] = *route; return ESP_OK;
}
esp_err_t HTTP_send_json(httpd_req_t *req, const cJSON *json, int *prebuffer)
{
    (void)prebuffer;
    char *text = cJSON_PrintUnformatted(json); assert(text);
    httpd_resp_sendstr(req, text); free(text); return ESP_OK;
}
static httpd_req_t request(httpd_method_t method, const char *uri, const char *body)
{
    httpd_req_t req = {.body = body, .content_len = body ? strlen(body) : 0};
    for (unsigned i = 0; i < route_count; i++) {
        if (routes[i].method == method && strcmp(routes[i].uri, uri) == 0) {
            assert(routes[i].handler(&req) == ESP_OK); return req;
        }
    }
    assert(false); return req;
}
static cJSON *snapshot(void)
{
    cJSON *json = mining_schedule_get_json(); assert(json); return json;
}
static bool boolean(cJSON *object, const char *name) { cJSON *v = cJSON_GetObjectItemCaseSensitive(object, name); assert(cJSON_IsBool(v)); return cJSON_IsTrue(v); }
static void check_status(bool paused, const char *override)
{
    cJSON *json = snapshot(), *status = cJSON_GetObjectItemCaseSensitive(json, "status");
    assert(boolean(status, "requestedPaused") == paused);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(status, "manualOverride")->valuestring, override) == 0);
    cJSON_Delete(json);
}
static void apply(const char *text)
{
    cJSON *json = cJSON_Parse(text); MiningScheduleSettings settings;
    assert(mining_schedule_parse(json, &settings)); cJSON_Delete(json);
    assert(mining_schedule_save(&settings) == ESP_OK);
}

int main(int argc, char **argv)
{
    assert(argc == 2); const char *scenario = argv[1];
    global.SYSTEM_MODULE.mining_runtime_ready = true;
    global.SYSTEM_MODULE.is_connected = true;
    at(2026, 10, 5, 19, 30);
    if (strcmp(scenario, "corrupt") == 0) { read_result = ESP_OK; stored = "broken JSON"; }
    if (strcmp(scenario, "wrong-type") == 0) read_result = ESP_ERR_NVS_TYPE_MISMATCH;
    if (strcmp(scenario, "bad-zone") == 0) { read_result = ESP_OK; stored = "{\"enabled\":true,\"timezone\":\"invalid\",\"windows\":[]}"; }
    if (strcmp(scenario, "trailing") == 0) { read_result = ESP_OK; stored = "{\"enabled\":false,\"timezone\":\"UTC\",\"windows\":[]} bad"; }
    if (strcmp(scenario, "clock") == 0 || strcmp(scenario, "clock-reset") == 0 || strcmp(scenario, "override") == 0 || strcmp(scenario, "jump") == 0) { read_result = ESP_OK; stored = weekly; }
    assert(mining_schedule_init(&global) == ESP_OK);
    mining_schedule_start_time_sync();
    assert(register_mining_schedule_api((void *)1) == ESP_OK && route_count == 3);
    assert(writes == 0);
    if (strcmp(scenario, "corrupt") == 0 || strcmp(scenario, "wrong-type") == 0 || strcmp(scenario, "bad-zone") == 0 || strcmp(scenario, "trailing") == 0) {
        cJSON *json = snapshot(), *status = cJSON_GetObjectItemCaseSensitive(json, "status"), *schedule = cJSON_GetObjectItemCaseSensitive(json, "schedule");
        assert(boolean(status, "requestedPaused") && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(status, "error")));
        assert(!boolean(schedule, "enabled") && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(schedule, "windows")) == 0);
        MiningScheduleSettings canonical; assert(mining_schedule_parse(schedule, &canonical)); cJSON_Delete(json);
        mining_schedule_manual_override(false); assert(global.SYSTEM_MODULE.mining_paused);
        save_result = ESP_FAIL; httpd_req_t failure = request(HTTP_PUT, "/api/system/mining/schedule", disabled); assert(failure.code == 500 && global.SYSTEM_MODULE.mining_paused);
        save_result = ESP_OK; apply(disabled); assert(global.SYSTEM_MODULE.mining_paused); // Disable retains request.
        mining_schedule_manual_override(false); assert(!global.SYSTEM_MODULE.mining_paused);
    } else if (strcmp(scenario, "validation") == 0) {
        const char *invalid[] = {
            "{}", "[]", "{\"enabled\":true,\"timezone\":\"UTC\",\"windows\":[]}",
            "{\"enabled\":0,\"timezone\":\"UTC\",\"windows\":[]}",
            "{\"enabled\":false,\"enabled\":true,\"timezone\":\"UTC\",\"windows\":[]}",
            "{\"enabled\":false,\"timezone\":\"UTC\",\"windows\":[],\"extra\":1}",
            "{\"enabled\":true,\"timezone\":\"Europe/London\",\"windows\":[{\"days\":1.5,\"start\":\"20:00\",\"end\":\"21:00\"}]}",
            "{\"enabled\":true,\"timezone\":\"UTC\",\"windows\":[{\"days\":0,\"start\":\"20:00\",\"end\":\"21:00\"}]}",
            "{\"enabled\":true,\"timezone\":\"UTC\",\"windows\":[{\"days\":128,\"start\":\"20:00\",\"end\":\"21:00\"}]}",
            "{\"enabled\":true,\"timezone\":\"UTC\",\"windows\":[{\"days\":1,\"start\":\"24:00\",\"end\":\"21:00\"}]}",
            "{\"enabled\":true,\"timezone\":\"UTC\",\"windows\":[{\"days\":1,\"start\":\"2:00\",\"end\":\"21:00\"}]}",
            "{\"enabled\":true,\"timezone\":\"UTC\",\"windows\":[{\"days\":1,\"start\":\"20:00\",\"end\":\"20:00\"}]}",
        };
        for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) { httpd_req_t req = request(HTTP_PUT, "/api/system/mining/schedule", invalid[i]); assert(req.code == 400 && writes == 0); }
        network_allowed = false; httpd_req_t denied = request(HTTP_PUT, "/api/system/mining/schedule", weekly); assert(denied.code == 401 && writes == 0); network_allowed = true;
        httpd_req_t good = request(HTTP_PUT, "/api/system/mining/schedule", weekly); assert(good.code == 200 && writes == 1);
        httpd_req_t bad_mode = request(HTTP_POST, "/api/system/mining/schedule/override", "{\"mode\":\"running\"}"); assert(bad_mode.code == 400);
        httpd_req_t clear = request(HTTP_POST, "/api/system/mining/schedule/override", "{\"mode\":\"schedule\"}"); assert(clear.code == 200 && strcmp(clear.reply, "{}") == 0);
    } else if (strcmp(scenario, "serialize-oom") == 0) {
        cJSON *json = cJSON_Parse(weekly); MiningScheduleSettings settings; assert(mining_schedule_parse(json, &settings)); cJSON_Delete(json);
        unsigned failures = 0, successes = 0;
        for (int allocation = 0; allocation < 40; allocation++) {
            unsigned before = writes;
            fail_allocation = allocation; cJSON_Hooks hooks = {.malloc_fn = test_malloc, .free_fn = free}; cJSON_InitHooks(&hooks);
            esp_err_t result = mining_schedule_save(&settings);
            cJSON_InitHooks(NULL); fail_allocation = -1;
            if (result == ESP_OK) {
                assert(writes == before + 1);
                cJSON *document = cJSON_Parse(last_written); MiningScheduleSettings parsed; assert(mining_schedule_parse(document, &parsed)); cJSON_Delete(document);
                assert(parsed.policy.enabled && parsed.policy.window_count == 1 && parsed.policy.windows[0].days == 2 && parsed.policy.windows[0].start_minutes == 1200 && parsed.policy.windows[0].end_minutes == 1260);
                successes++;
            } else { assert(result == ESP_ERR_NO_MEM && writes == before); failures++; }
        }
        assert(failures > 10 && successes > 0);
    } else if (strcmp(scenario, "clock") == 0) {
        check_status(true, "none"); mining_schedule_manual_override(false); check_status(true, "running");
        sync_time(); check_status(false, "running"); assert(mining_schedule_owns_clock());
        apply(disabled); check_status(false, "running");
    } else if (strcmp(scenario, "clock-reset") == 0) {
        sync_time(); check_status(false, "none"); assert(mining_schedule_owns_clock());
        at(1970, 1, 1, 0, 0); mining_schedule_update(); check_status(true, "none"); assert(!mining_schedule_owns_clock());
        at(2026, 10, 5, 19, 30); sync_time(); check_status(false, "none");
        at(1970, 1, 1, 0, 0); sync_time(); check_status(true, "none");
        at(2026, 10, 5, 19, 30); mining_schedule_update(); check_status(true, "none"); assert(!mining_schedule_owns_clock());
        sync_time(); check_status(false, "none");
    } else if (strcmp(scenario, "override") == 0 || strcmp(scenario, "jump") == 0) {
        sync_time(); check_status(false, "none"); mining_schedule_manual_override(true); check_status(true, "paused");
        at(2026, 10, 5, 19, 59); mining_schedule_update(); check_status(true, "paused");
        at(2026, 10, 5, 20, 0); if (strcmp(scenario, "jump") == 0) at(2026, 10, 5, 21, 30);
        mining_schedule_update(); check_status(strcmp(scenario, "jump") != 0, "none");
        if (strcmp(scenario, "override") == 0) { mining_schedule_manual_override(false); check_status(false, "running"); at(2026, 10, 5, 21, 0); mining_schedule_update(); check_status(false, "none"); }
    } else if (strcmp(scenario, "dst-spring") == 0 || strcmp(scenario, "dst-fall") == 0) {
        at(2026, 3, 29, 0, 45); if (strcmp(scenario, "dst-fall") == 0) at(2026, 10, 25, 0, 15);
        sync_time();
        const char *window = strcmp(scenario, "dst-spring") == 0 ? "{\"enabled\":true,\"timezone\":\"Europe/London\",\"windows\":[{\"days\":1,\"start\":\"02:30\",\"end\":\"03:30\"}]}" : "{\"enabled\":true,\"timezone\":\"Europe/London\",\"windows\":[{\"days\":1,\"start\":\"01:30\",\"end\":\"02:00\"}]}";
        apply(window); mining_schedule_manual_override(true);
        if (strcmp(scenario, "dst-spring") == 0) { at(2026, 3, 29, 1, 15); mining_schedule_update(); check_status(true, "paused"); at(2026, 3, 29, 1, 30); }
        else { at(2026, 10, 25, 0, 25); mining_schedule_update(); check_status(true, "paused"); at(2026, 10, 25, 0, 30); }
        mining_schedule_update(); check_status(true, "none");
    } else if (strcmp(scenario, "status") == 0) {
        global.SYSTEM_MODULE.mining_runtime_ready = false;
        cJSON *json = snapshot(), *status = cJSON_GetObjectItemCaseSensitive(json, "status"); assert(boolean(status, "requestedPaused") && !boolean(status, "appliedPaused") && boolean(status, "transitionPending")); cJSON_Delete(json);
        mining_schedule_report_power(true, NULL); json = snapshot(); status = cJSON_GetObjectItemCaseSensitive(json, "status"); assert(boolean(status, "appliedPaused") && !boolean(status, "transitionPending")); cJSON_Delete(json);
        global.SYSTEM_MODULE.hardware_fault = true; strcpy(global.SYSTEM_MODULE.hardware_fault_msg, "regulator failed"); mining_schedule_report_power(false, "regulator failed"); json = snapshot(); status = cJSON_GetObjectItemCaseSensitive(json, "status"); assert(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(status, "error")) && boolean(status, "transitionPending")); cJSON_Delete(json);
    } else {
        assert(strcmp(scenario, "missing") == 0); check_status(false, "none"); assert(writes == 0);
    }
    puts("PASS"); return 0;
}
