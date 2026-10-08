#include "gamma_status_support.h"
#include "five_tratum_status.h"
#include "five_tratum_status_report.h"
#include "mux_peer_status.h"
#include "operating_profiles.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GlobalState global = {
    .DEVICE_CONFIG.family = {.name = "Gamma", .asic.name = "BM1370", .asic_count = 1},
    .stratum_protocol = STRATUM_PROTOCOL_V1,
};
static httpd_uri_t route;
static unsigned registrations, snapshot_clocks, binding_reads, generation = 7;
static int64_t now_us = 1000000;
static const char *board_version = "601";
static esp_app_desc_t app = {.version = "5tratumFW-0.1.0-beta.7"};
static bool authorized = true, cors_allowed = true, mac_available = true, digest_available = true;
static bool saved_board_available = true;
static int send_result = ESP_OK, registration_result = ESP_OK;
static pthread_mutex_t peer_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned depth;
static void (*before_peer_lock)(void);
static long allocation_index, fail_allocation = -1, live_allocations;

static void *json_malloc(size_t size)
{
    if (allocation_index++ == fail_allocation) return NULL;
    void *result = malloc(size);
    if (result) ++live_allocations;
    return result;
}
static void json_free(void *pointer)
{
    if (pointer) { assert(live_allocations > 0); --live_allocations; }
    free(pointer);
}

const esp_app_desc_t *esp_app_get_description(void) { return &app; }
esp_err_t esp_read_mac(uint8_t *output, int source)
{
    ++binding_reads;
    assert(source == ESP_MAC_WIFI_STA);
    if (!mac_available) return ESP_FAIL;
    const uint8_t fixture[6] = {1, 2, 3, 4, 5, 6};
    memcpy(output, fixture, sizeof(fixture));
    return ESP_OK;
}
int mbedtls_sha256(const unsigned char *bytes, size_t size, unsigned char *digest, int mode)
{
    // Crypto is a platform boundary. Verify the unchanged binding helper's exact
    // namespaced NUL-delimited input; never use or output a physical MAC.
    const char prefix[] = "5tratum/device/v1";
    assert(mode == 0 && size == sizeof(prefix) + 6);
    assert(!memcmp(bytes, prefix, sizeof(prefix)));
    assert(bytes[sizeof(prefix)] == 1 && bytes[size - 1] == 6);
    if (!digest_available) return -1;
    for (unsigned i = 0; i < 32; ++i) digest[i] = (unsigned char)(i + 1);
    return 0;
}
char *nvs_config_get_string(int key)
{
    assert(key == NVS_CONFIG_BOARD_VERSION);
    return saved_board_available ? strdup(board_version) : NULL;
}
esp_err_t is_network_allowed(httpd_req_t *req) { (void)req; return authorized ? ESP_OK : ESP_FAIL; }
esp_err_t set_cors_headers(httpd_req_t *req) { ++req->cors; return cors_allowed ? ESP_OK : ESP_FAIL; }
esp_err_t httpd_resp_set_hdr(httpd_req_t *req, const char *name, const char *value)
{
    assert(!strcmp(name, "Cache-Control") && !strcmp(value, "no-store"));
    ++req->no_store;
    return ESP_OK;
}
esp_err_t httpd_resp_set_type(httpd_req_t *req, const char *value)
{
    (void)req;
    assert(!strcmp(value, "application/json"));
    return ESP_OK;
}
esp_err_t httpd_resp_set_status(httpd_req_t *req, const char *status) { req->code = atoi(status); return ESP_OK; }
esp_err_t httpd_resp_sendstr(httpd_req_t *req, const char *body)
{
    assert(body && strlen(body) < sizeof(req->body));
    ++req->sends;
    snprintf(req->body, sizeof(req->body), "%s", body);
    return send_result;
}
esp_err_t httpd_resp_send_err(httpd_req_t *req, int code, const char *body)
{
    req->code = code;
    return httpd_resp_sendstr(req, body);
}
esp_err_t httpd_resp_send_500(httpd_req_t *req) { req->code = 500; return ESP_FAIL; }
esp_err_t httpd_register_uri_handler(httpd_handle_t server, const httpd_uri_t *value)
{
    (void)server;
    ++registrations;
    assert(!strcmp(value->uri, "/api/5tratum/status") && value->method == HTTP_GET);
    route = *value;
    return registration_result;
}
unsigned protocol_coordinator_work_generation(void) { return generation; }
int64_t esp_timer_get_time(void) { if (depth) ++snapshot_clocks; return now_us; }
void test_enter_critical(int *lock)
{
    (void)lock;
    if (before_peer_lock) { void (*hook)(void) = before_peer_lock; before_peer_lock = NULL; hook(); }
    assert(!pthread_mutex_lock(&peer_lock));
    assert(depth++ == 0);
}
void test_exit_critical(int *lock)
{
    (void)lock;
    assert(depth-- == 1);
    assert(!pthread_mutex_unlock(&peer_lock));
}
static cJSON *get(httpd_req_t *request)
{
    *request = (httpd_req_t){.code = 200};
    const int result = route.handler(request);
    assert(result == send_result);
    assert(request->sends == 1 && request->no_store == 1 && request->cors == 1);
    return cJSON_Parse(request->body);
}
static cJSON *item(const cJSON *root, const char *key) { return cJSON_GetObjectItemCaseSensitive(root, key); }
static cJSON *pool(const cJSON *root, int index) { return cJSON_GetArrayItem(item(item(root, "mux"), "pools"), index); }
static void assert_unavailable_metadata(const cJSON *root)
{
    assert(cJSON_IsNull(item(root, "coin")) && cJSON_IsNull(item(root, "workContext")));
    assert(cJSON_GetArraySize(item(root, "asics")) == 0);
    assert(cJSON_IsFalse(item(item(root, "work"), "independentAssignment")));
    assert(cJSON_IsFalse(item(item(root, "mux"), "independentWorkAssignment")));
    for (int i = 0; i < 2; ++i) {
        cJSON *value = pool(root, i);
        assert(cJSON_IsNull(item(value, "serverId")) && cJSON_IsNull(item(value, "coin")) && cJSON_IsNull(item(value, "workContext")));
        assert(item(value, "ttlSeconds")->valuedouble == 90);
    }
}
static void assert_not_advertised(const cJSON *root)
{
    for (int i = 0; i < 2; ++i) assert(cJSON_IsFalse(item(pool(root, i), "connected")));
    assert_unavailable_metadata(root);
}
static void begin(bool fallback)
{
    global.SYSTEM_MODULE.is_using_fallback = fallback;
    global.stratum_protocol = STRATUM_PROTOCOL_V1;
    const uint64_t peer_generation = mux_peer_status_begin(fallback);
    mux_peer_status_receive(peer_generation, true, now_us);
}
static void transition_to_v2(void)
{
    ++generation;
    global.stratum_protocol = STRATUM_PROTOCOL_V2;
}
static void transition_to_fallback(void)
{
    ++generation;
    global.SYSTEM_MODULE.is_using_fallback = true;
}
static void replace_same_route(void)
{
    ++generation;
    const uint64_t next = mux_peer_status_begin(false);
    mux_peer_status_receive(next, true, now_us);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    const cJSON_Hooks hooks = {.malloc_fn = json_malloc, .free_fn = json_free};
    cJSON_InitHooks((cJSON_Hooks *)&hooks);
    set_binding_state(&global);
    assert(five_tratum_status_register(NULL, NULL) == ESP_ERR_INVALID_ARG && registrations == 0);
    assert(five_tratum_status_register(NULL, &global) == ESP_OK && registrations == 1);
    const char *scenario = argv[1];
    httpd_req_t request;
    if (!strcmp(scenario, "primary") || !strcmp(scenario, "fallback") || !strcmp(scenario, "602")) {
        const bool fallback = !strcmp(scenario, "fallback");
        if (!strcmp(scenario, "602")) board_version = "602";
        begin(fallback);
        now_us += 17000000;
        cJSON *root = get(&request);
        assert(root && request.code == 200);
        assert(item(root, "schemaVersion")->valuedouble == 1);
        assert(item(item(root, "work"), "activePool")->valuedouble == (fallback ? 1 : 0));
        assert(item(root, "observedUptimeSeconds")->valuedouble == 18);
        assert(cJSON_IsTrue(item(pool(root, fallback ? 1 : 0), "connected")));
        assert(cJSON_IsTrue(item(pool(root, fallback ? 1 : 0), "transportConnected")));
        assert(item(pool(root, fallback ? 1 : 0), "statusAgeSeconds")->valuedouble == 17);
        assert(cJSON_IsFalse(item(pool(root, fallback ? 0 : 1), "transportConnected")));
        cJSON *binding = operating_profiles_binding();
        assert(cJSON_Compare(item(root, "identity"), item(binding, "identity"), true));
        assert(cJSON_Compare(item(root, "hardware"), item(binding, "hardware"), true));
        assert(cJSON_Compare(item(root, "firmware"), item(binding, "firmware"), true));
        assert_unavailable_metadata(root);
        assert(!strstr(request.body, "password") && !strstr(request.body, "pool_url") && !strstr(request.body, "worker"));
        cJSON_Delete(binding);
        cJSON_Delete(root);
    } else if (!strcmp(scenario, "lifetime")) {
        const uint64_t old = mux_peer_status_begin(false);
        cJSON *root = get(&request);
        assert_not_advertised(root);
        assert(cJSON_IsTrue(item(pool(root, 0), "transportConnected")));
        cJSON_Delete(root);
        mux_peer_status_receive(old, true, now_us);
        now_us += 89999999;
        root = get(&request);
        assert(cJSON_IsTrue(item(pool(root, 0), "connected")));
        cJSON_Delete(root);
        ++now_us;
        root = get(&request);
        assert_not_advertised(root);
        assert(cJSON_IsTrue(item(pool(root, 0), "expired")));
        assert(item(pool(root, 0), "statusAgeSeconds")->valuedouble == 90);
        cJSON_Delete(root);
        mux_peer_status_disconnect();
        root = get(&request);
        assert_not_advertised(root);
        assert(cJSON_IsFalse(item(pool(root, 0), "transportConnected")));
        assert(cJSON_IsNull(item(pool(root, 0), "statusAgeSeconds")));
        cJSON_Delete(root);
        const uint64_t next = mux_peer_status_begin(true);
        global.SYSTEM_MODULE.is_using_fallback = true;
        mux_peer_status_receive(old, true, now_us);
        root = get(&request);
        assert_not_advertised(root);
        cJSON_Delete(root);
        mux_peer_status_receive(next, true, now_us);
        mux_peer_status_receive(old, false, now_us);
        root = get(&request);
        assert(cJSON_IsTrue(item(pool(root, 1), "connected")));
        cJSON_Delete(root);
        mux_peer_status_receive(next, false, now_us);
        root = get(&request);
        assert_not_advertised(root);
        cJSON_Delete(root);
    } else if (!strcmp(scenario, "transition")) {
        void (*changes[])(void) = {transition_to_v2, transition_to_fallback, replace_same_route};
        for (unsigned i = 0; i < sizeof(changes) / sizeof(changes[0]); ++i) {
            begin(false);
            before_peer_lock = changes[i];
            cJSON *root = get(&request);
            assert(root && request.code == 200);
            assert_not_advertised(root);
            assert(cJSON_IsFalse(item(pool(root, 0), "transportConnected")));
            assert(cJSON_IsFalse(item(pool(root, 1), "transportConnected")));
            cJSON_Delete(root);
        }
        begin(false);
        global.SYSTEM_MODULE.is_using_fallback = true;
        cJSON *root = get(&request);
        assert_not_advertised(root);
        assert(item(item(root, "work"), "activePool")->valuedouble == 1);
        cJSON_Delete(root);
        begin(false);
        global.stratum_protocol = STRATUM_PROTOCOL_V2;
        root = get(&request);
        assert_not_advertised(root);
        cJSON_Delete(root);
    } else if (!strcmp(scenario, "future")) {
        begin(false);
        --now_us;
        cJSON *root = get(&request);
        assert_not_advertised(root);
        assert(cJSON_IsTrue(item(pool(root, 0), "expired")));
        cJSON_Delete(root);
    } else if (!strcmp(scenario, "auth")) {
        begin(false);
        const unsigned initial_reads = binding_reads, initial_clocks = snapshot_clocks;
        authorized = false;
        request = (httpd_req_t){.code = 200};
        assert(route.handler(&request) == ESP_OK && request.code == 401);
        assert(!request.cors && !request.no_store);
        authorized = true;
        cors_allowed = false;
        request = (httpd_req_t){.code = 200};
        assert(route.handler(&request) == ESP_FAIL && request.code == 500);
        assert(binding_reads == initial_reads && snapshot_clocks == initial_clocks);
    } else if (!strcmp(scenario, "identity")) {
        begin(false);
        for (int i = 0; i < 8; ++i) {
            mac_available = i != 0;
            digest_available = i != 1;
            saved_board_available = i != 2;
            board_version = i == 3 ? "603" : "601";
            global.DEVICE_CONFIG.family.asic_count = i == 4 ? 2 : 1;
            global.DEVICE_CONFIG.family.asic.name = i == 5 ? "BM1366" : "BM1370";
            app.version = i == 6 ? "stock-2.0.0" : i == 7 ? "5tratumFW-fixture\n" : "5tratumFW-0.1.0-beta.7";
            cJSON *root = get(&request);
            assert(request.code == 503 && cJSON_IsFalse(item(root, "ok")));
            cJSON_Delete(root);
            assert(live_allocations == 0);
        }
    } else if (!strcmp(scenario, "allocation")) {
        begin(false);
        unsigned failures = 0, successes = 0;
        for (long index = 0; index < 240; ++index) {
            fail_allocation = index;
            allocation_index = 0;
            request = (httpd_req_t){.code = 200};
            assert(route.handler(&request) == ESP_OK);
            fail_allocation = -1;
            assert(request.sends == 1 && request.no_store == 1);
            if (request.code == 503) ++failures;
            else { assert(request.code == 200); ++successes; }
            assert(live_allocations == 0);
        }
        assert(failures > 100 && successes > 0);
    } else if (!strcmp(scenario, "malformed-binding")) {
        cJSON *binding = operating_profiles_binding();
        assert(binding);
        FiveTratumStatusReport snapshot = {0};
        cJSON *root = five_tratum_status_report(binding, &snapshot);
        assert(root); cJSON_Delete(root);
        cJSON_AddStringToObject(binding, "password", "fixture-private");
        assert(!five_tratum_status_report(binding, &snapshot));
        cJSON_DeleteItemFromObjectCaseSensitive(binding, "password");
        cJSON_AddStringToObject(item(binding, "identity"), "deviceId", "5tfw:0102030405060708090a0b0c0d0e0f10");
        assert(!five_tratum_status_report(binding, &snapshot));
        cJSON_Delete(binding);
        binding = operating_profiles_binding();
        cJSON_SetValuestring(item(item(binding, "identity"), "deviceId"), "5tfw:00000000000000000000000000000000");
        assert(!five_tratum_status_report(binding, &snapshot));
        cJSON_Delete(binding);
        assert(!five_tratum_status_report(NULL, &snapshot) && !five_tratum_status_report(NULL, NULL));
    } else if (!strcmp(scenario, "send-failure")) {
        begin(false);
        send_result = ESP_FAIL;
        cJSON *root = get(&request);
        assert(root && request.code == 200);
        cJSON_Delete(root);
    } else if (!strcmp(scenario, "registration")) {
        registration_result = ESP_FAIL;
        assert(five_tratum_status_register(NULL, &global) == ESP_FAIL && registrations == 2);
    } else if (!strcmp(scenario, "negative-clock")) {
        now_us = -1;
        cJSON *root = get(&request);
        assert(root && request.code == 503);
        cJSON_Delete(root);
    } else assert(0);
    assert(depth == 0 && live_allocations == 0);
    puts("PASS");
    return 0;
}
