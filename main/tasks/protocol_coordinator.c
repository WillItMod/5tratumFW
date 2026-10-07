#include "esp_log.h"
#include "esp_timer.h"
#include "esp_transport.h"
#include "esp_transport_tcp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "protocol_coordinator.h"
#include "stratum_v1_task.h"
#include "mux_peer_status.h"
#include "stratum_v2_task.h"
#include "connect.h"
#include "system.h"
#include "nvs_config.h"
#include "pool_reload.h"
#include "http_server/operating_profiles.h"
#include <stdatomic.h>

#include <string.h>

// Internal coordinator states
typedef enum {
    COORD_STATE_IDLE = 0,
    COORD_STATE_RUNNING_PRIMARY,
    COORD_STATE_RUNNING_FALLBACK,
    // Entered when consecutive pool failures hit the configured threshold.
    // No protocol task is running; the coordinator probes pools periodically
    // and resumes mining as soon as one is reachable. While in this state,
    // pools_unavailable is true so power management cuts ASIC power.
    COORD_STATE_PAUSED,
} coordinator_state_t;

// Internal event types
typedef enum {
    COORD_EVENT_PROTOCOL_FAILED = 0,
    COORD_EVENT_PROTOCOL_SUCCESS,
    COORD_EVENT_V1_TASK_EXITED,
    COORD_EVENT_V2_TASK_EXITED,
    COORD_EVENT_CONFIG_RELOAD,
} coordinator_event_type_t;
typedef struct { coordinator_event_type_t type; unsigned generation; } coordinator_event_t;

#define TRANSPORT_TIMEOUT_MS 5000
#define HEARTBEAT_INTERVAL_MS 60000
#define INITIAL_HEARTBEAT_DELAY_MS 10000
// While paused (all pools unreachable), probe again on this cadence.
#define RECOVERY_PROBE_INTERVAL_MS 30000
#define BUFFER_SIZE 1024

static const char *TAG = "protocol_coordinator";

static GlobalState *s_global_state = NULL;
static coordinator_state_t s_state = COORD_STATE_IDLE;
static QueueHandle_t s_event_queue = NULL;
static atomic_bool s_v1_should_shutdown, s_v2_should_shutdown;
static atomic_bool s_v1_active, s_v2_active;
static atomic_uint s_running_generation, s_work_generation, s_reload_requested;
static unsigned s_reload_applied;

// Protocol tracking
static stratum_protocol_t s_primary_protocol;
static stratum_protocol_t s_fallback_protocol;
static stratum_protocol_t s_running_protocol;
static bool s_heartbeat_enabled = false;

// Primary pool info (saved at startup for heartbeat probing)
static const char *s_primary_url = NULL;
static uint16_t s_primary_port = 0;

// Number of consecutive pools (primary and/or fallback) that have exhausted
// their retry budget without a successful setup. When this reaches
// pool_failure_threshold(), we enter COORD_STATE_PAUSED and set
// pools_unavailable so power management cuts ASIC power.
// Reset on COORD_EVENT_PROTOCOL_SUCCESS.
static int s_consecutive_pool_failures = 0;

void protocol_coordinator_init(GlobalState *gs)
{
    s_global_state = gs;
    s_event_queue = xQueueCreate(8, sizeof(coordinator_event_t));
    atomic_store(&s_v1_should_shutdown, false);
    atomic_store(&s_v2_should_shutdown, false);
    atomic_store(&s_v1_active, false);
    atomic_store(&s_v2_active, false);
    s_heartbeat_enabled = false;
    s_consecutive_pool_failures = 0;
}

void protocol_coordinator_request_primary_reload(GlobalState *gs)
{
    (void)gs;
    atomic_fetch_add(&s_reload_requested, 1);
    coordinator_event_t event = {.type = COORD_EVENT_CONFIG_RELOAD};
    if (s_event_queue) xQueueSend(s_event_queue, &event, 0);
}
unsigned protocol_coordinator_work_generation(void) { return atomic_load(&s_work_generation); }
void protocol_coordinator_invalidate_work(void) { atomic_fetch_add(&s_work_generation, 1); }
static void send_protocol_event(coordinator_event_type_t type, bool exiting)
{
    // Capture generation before marking the task quiescent. Late old-connection
    // events cannot affect a task started after a config reload.
    coordinator_event_t event = {.type = type, .generation = atomic_load(&s_running_generation)};
    if (exiting) {
        if (s_running_protocol == STRATUM_PROTOCOL_V2) atomic_store(&s_v2_active, false);
        else atomic_store(&s_v1_active, false);
    }
    if (s_event_queue) xQueueSend(s_event_queue, &event, 0);
}
void protocol_coordinator_notify_failure(void) { send_protocol_event(COORD_EVENT_PROTOCOL_FAILED, true); }
void protocol_coordinator_notify_success(void) { send_protocol_event(COORD_EVENT_PROTOCOL_SUCCESS, false); }
bool protocol_coordinator_v1_should_shutdown(void) { return atomic_load(&s_v1_should_shutdown); }
void protocol_coordinator_v1_exited(void) { send_protocol_event(COORD_EVENT_V1_TASK_EXITED, true); }
bool protocol_coordinator_v2_should_shutdown(void) { return atomic_load(&s_v2_should_shutdown); }
void protocol_coordinator_v2_exited(void) { send_protocol_event(COORD_EVENT_V2_TASK_EXITED, true); }

static void reset_share_stats(GlobalState *gs)
{
    for (int i = 0; i < gs->SYSTEM_MODULE.rejected_reason_stats_count; i++) {
        gs->SYSTEM_MODULE.rejected_reason_stats[i].count = 0;
        gs->SYSTEM_MODULE.rejected_reason_stats[i].message[0] = '\0';
    }
    gs->SYSTEM_MODULE.rejected_reason_stats_count = 0;
    gs->SYSTEM_MODULE.shares_accepted = 0;
    gs->SYSTEM_MODULE.shares_rejected = 0;
    gs->SYSTEM_MODULE.work_received = 0;
}

static bool has_fallback_pool(GlobalState *gs)
{
    return (gs->SYSTEM_MODULE.fallback_pool_url != NULL &&
            gs->SYSTEM_MODULE.fallback_pool_url[0] != '\0');
}

// Start the V1 stratum task (for primary V1 or fallback)
static void start_v1_task(GlobalState *gs)
{
    atomic_store(&s_v1_should_shutdown, false);
    atomic_store(&s_v1_active, true);
    if (xTaskCreate(stratum_v1_task, "stratum v1", 8192, (void *)gs, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create V1 stratum task");
        protocol_coordinator_notify_failure();
    }
}

// Start the V2 stratum task
static void start_v2_task(GlobalState *gs)
{
    atomic_store(&s_v2_should_shutdown, false);
    atomic_store(&s_v2_active, true);
    if (xTaskCreate(stratum_v2_task, "stratum v2", 12288, (void *)gs, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create V2 stratum task");
        protocol_coordinator_notify_failure();
    }
}

// Start a task for the given protocol
static void start_protocol_task(GlobalState *gs, stratum_protocol_t protocol)
{
    SYSTEM_stratum_io_lock();
    atomic_fetch_add(&s_running_generation, 1);
    protocol_coordinator_invalidate_work();
    SYSTEM_stratum_io_unlock();
    if (protocol == STRATUM_PROTOCOL_V2) {
        start_v2_task(gs);
    } else {
        start_v1_task(gs);
    }
}

// Wait for actual task quiescence; a timeout never permits pointer replacement.
static bool stop_running_task(GlobalState *gs)
{
    bool v2 = s_running_protocol == STRATUM_PROTOCOL_V2;
    atomic_bool *active = v2 ? &s_v2_active : &s_v1_active;
    unsigned generation = atomic_load(&s_running_generation);
    if (!atomic_load(active)) return true;
    if (!v2) mux_peer_status_disconnect();
    atomic_store(v2 ? &s_v2_should_shutdown : &s_v1_should_shutdown, true);
    SYSTEM_stratum_io_lock();
    if (gs->transport) esp_transport_close(gs->transport);
    SYSTEM_stratum_io_unlock();
    bool acknowledged = false;
    for (unsigned i = 0; i < 150; i++) {
        coordinator_event_t event;
        if (xQueueReceive(s_event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE && event.generation == generation &&
            (event.type == COORD_EVENT_PROTOCOL_FAILED || event.type == (v2 ? COORD_EVENT_V2_TASK_EXITED : COORD_EVENT_V1_TASK_EXITED)))
            acknowledged = true;
        if (acknowledged && !atomic_load(active)) return true;
    }
    ESP_LOGW(TAG, "Protocol shutdown timed out; live pool config retained");
    return false;
}

// TCP connect probe (used for SV2 — full noise handshake is too expensive)
static bool probe_pool_sv2(const char *url, uint16_t port)
{
    if (url == NULL || url[0] == '\0' || port == 0) return false;

    esp_transport_handle_t probe = esp_transport_tcp_init();
    if (!probe) return false;

    esp_err_t err = esp_transport_connect(probe, url, port, TRANSPORT_TIMEOUT_MS);
    esp_transport_close(probe);
    esp_transport_destroy(probe);

    return (err == ESP_OK);
}

// Subscribe/authorize probe for V1 — succeeds only if the pool responds with
// a mining.notify line, confirming it's actually serving work.
static bool probe_pool_v1(GlobalState *gs, const char *url, uint16_t port,
                          tls_mode tls, char *cert, const char *user, const char *pass)
{
    if (url == NULL || url[0] == '\0' || port == 0) return false;

    esp_transport_handle_t transport = STRATUM_V1_transport_init(tls, cert);
    if (!transport) return false;

    esp_err_t err = esp_transport_connect(transport, url, port, TRANSPORT_TIMEOUT_MS);
    if (err != ESP_OK) {
        esp_transport_close(transport);
        esp_transport_destroy(transport);
        return false;
    }

    int send_uid = 1;
    STRATUM_V1_subscribe(transport, send_uid++, gs->DEVICE_CONFIG.family.asic.name);
    STRATUM_V1_authorize(transport, send_uid++, user, pass);

    char recv_buffer[BUFFER_SIZE];
    memset(recv_buffer, 0, BUFFER_SIZE);
    int bytes_received = esp_transport_read(transport, recv_buffer, BUFFER_SIZE - 1, TRANSPORT_TIMEOUT_MS);

    esp_transport_close(transport);
    esp_transport_destroy(transport);

    return (bytes_received > 0 && strstr(recv_buffer, "mining.notify") != NULL);
}

// Probe a pool using the appropriate protocol for it.
static bool probe_pool(GlobalState *gs, bool use_fallback)
{
    stratum_protocol_t protocol = use_fallback ? s_fallback_protocol : s_primary_protocol;
    const char *url   = use_fallback ? gs->SYSTEM_MODULE.fallback_pool_url   : gs->SYSTEM_MODULE.pool_url;
    uint16_t    port  = use_fallback ? gs->SYSTEM_MODULE.fallback_pool_port  : gs->SYSTEM_MODULE.pool_port;

    if (protocol == STRATUM_PROTOCOL_V2) {
        return probe_pool_sv2(url, port);
    }

    tls_mode tls       = use_fallback ? gs->SYSTEM_MODULE.fallback_pool_tls  : gs->SYSTEM_MODULE.pool_tls;
    char     *cert     = use_fallback ? gs->SYSTEM_MODULE.fallback_pool_cert : gs->SYSTEM_MODULE.pool_cert;
    const char *user   = use_fallback ? gs->SYSTEM_MODULE.fallback_pool_user : gs->SYSTEM_MODULE.pool_user;
    const char *pass   = use_fallback ? gs->SYSTEM_MODULE.fallback_pool_pass : gs->SYSTEM_MODULE.pool_pass;
    return probe_pool_v1(gs, url, port, tls, cert, user, pass);
}

// Switch from primary to fallback pool.
// The failed task has already exited (it sent PROTOCOL_FAILED then deleted itself).
static void switch_to_fallback(GlobalState *gs)
{
    queue_clear(&gs->stratum_queue);
    reset_share_stats(gs);

    gs->SYSTEM_MODULE.is_using_fallback = true;
    gs->stratum_protocol = s_fallback_protocol;
    s_running_protocol = s_fallback_protocol;
    s_state = COORD_STATE_RUNNING_FALLBACK;

    ESP_LOGI(TAG, "Switching to fallback pool (%s)",
             s_fallback_protocol == STRATUM_PROTOCOL_V2 ? STRATUM_V2 : STRATUM_V1);

    start_protocol_task(gs, s_fallback_protocol);

    // Only enable heartbeat if this was an automatic failover (not user choice)
    s_heartbeat_enabled = !gs->SYSTEM_MODULE.use_fallback_stratum;
}

// Switch from fallback back to primary pool.
// Must stop the running fallback task first.
static void switch_to_primary(GlobalState *gs)
{
    ESP_LOGI(TAG, "Primary pool is back! Switching from fallback.");

    if (!stop_running_task(gs)) return;

    queue_clear(&gs->stratum_queue);
    reset_share_stats(gs);

    gs->SYSTEM_MODULE.is_using_fallback = false;
    gs->stratum_protocol = s_primary_protocol;
    s_running_protocol = s_primary_protocol;
    s_state = COORD_STATE_RUNNING_PRIMARY;

    start_protocol_task(gs, s_primary_protocol);

    s_heartbeat_enabled = false;
}

// Non-blocking heartbeat probe — called when the heartbeat timer expires
static void do_heartbeat_probe(GlobalState *gs)
{
    // Never auto-switch back if user explicitly chose fallback
    if (gs->SYSTEM_MODULE.use_fallback_stratum) {
        s_heartbeat_enabled = false;
        return;
    }

    if (!wifi_is_connected()) {
        return;
    }

    ESP_LOGD(TAG, "Heartbeat: probing primary pool %s:%d", s_primary_url, s_primary_port);

    if (probe_pool(gs, /*use_fallback=*/false)) {
        switch_to_primary(gs);
    } else {
        ESP_LOGD(TAG, "Primary pool still unreachable");
    }
}

// Number of consecutive pool failures that triggers entering the paused state.
// With both primary and fallback configured we tolerate one failure per pool;
// with only one pool configured we pause on its first exhaustion.
static int pool_failure_threshold(GlobalState *gs)
{
    return has_fallback_pool(gs) ? 2 : 1;
}

// All configured pools have exhausted retries. Set pools_unavailable so power
// management cuts ASIC power, and park the coordinator until a probe succeeds.
static void enter_paused_state(GlobalState *gs)
{
    s_state = COORD_STATE_PAUSED;
    gs->SYSTEM_MODULE.pools_unavailable = true;
    s_heartbeat_enabled = false;
    ESP_LOGW(TAG, "All configured pools unreachable, pausing mining to conserve power.");
}

// Resume mining on the given pool after a successful recovery probe.
// Used only from the paused state — caller must have already verified the
// pool is reachable.
static void resume_on_pool(GlobalState *gs, bool use_fallback)
{
    s_consecutive_pool_failures = 0;
    gs->SYSTEM_MODULE.pools_unavailable = false;
    gs->SYSTEM_MODULE.is_using_fallback = use_fallback;

    stratum_protocol_t proto = use_fallback ? s_fallback_protocol : s_primary_protocol;
    gs->stratum_protocol = proto;
    s_running_protocol = proto;
    s_state = use_fallback ? COORD_STATE_RUNNING_FALLBACK : COORD_STATE_RUNNING_PRIMARY;

    queue_clear(&gs->stratum_queue);
    reset_share_stats(gs);

    ESP_LOGI(TAG, "Pool recovery: %s pool reachable, resuming mining (%s)",
             use_fallback ? "fallback" : "primary",
             proto == STRATUM_PROTOCOL_V2 ? STRATUM_V2 : STRATUM_V1);

    start_protocol_task(gs, proto);

    // Only run the auto-switch-back heartbeat for *automatic* failovers
    // (user did not explicitly choose the fallback pool).
    s_heartbeat_enabled = use_fallback && !gs->SYSTEM_MODULE.use_fallback_stratum;
}

// Probe pools while paused. Tries the user-preferred pool first, then the other.
// Resumes mining as soon as one is reachable.
static void try_resume_from_paused(GlobalState *gs)
{
    if (!wifi_is_connected()) {
        return;
    }

    bool prefer_fallback = gs->SYSTEM_MODULE.use_fallback_stratum && has_fallback_pool(gs);

    if (prefer_fallback) {
        if (probe_pool(gs, /*use_fallback=*/true)) {
            resume_on_pool(gs, /*use_fallback=*/true);
            return;
        }
        if (probe_pool(gs, /*use_fallback=*/false)) {
            resume_on_pool(gs, /*use_fallback=*/false);
            return;
        }
    } else {
        if (probe_pool(gs, /*use_fallback=*/false)) {
            resume_on_pool(gs, /*use_fallback=*/false);
            return;
        }
        if (has_fallback_pool(gs) && probe_pool(gs, /*use_fallback=*/true)) {
            resume_on_pool(gs, /*use_fallback=*/true);
            return;
        }
    }

    ESP_LOGD(TAG, "Recovery probe: no pool reachable, staying paused");
}

// Handle an event from the event queue
static void handle_event(GlobalState *gs, coordinator_event_t evt)
{
    if (evt.type == COORD_EVENT_CONFIG_RELOAD || evt.generation != atomic_load(&s_running_generation)) return;
    switch (evt.type) {
        case COORD_EVENT_PROTOCOL_FAILED: {
            if (s_state == COORD_STATE_PAUSED) {
                // Stray failure from a task that exited after we already paused — ignore.
                break;
            }
            s_consecutive_pool_failures++;
            int threshold = pool_failure_threshold(gs);
            ESP_LOGW(TAG, "Protocol failure reported (state=%d, failures=%d/%d)",
                     s_state, s_consecutive_pool_failures, threshold);

            if (s_consecutive_pool_failures >= threshold) {
                enter_paused_state(gs);
                break;
            }

            // Below threshold — try the other pool. This only fires when a
            // fallback exists (otherwise threshold=1 and we paused above).
            if (s_state == COORD_STATE_RUNNING_PRIMARY) {
                switch_to_fallback(gs);
            } else if (s_state == COORD_STATE_RUNNING_FALLBACK) {
                ESP_LOGI(TAG, "Fallback failed, trying primary");
                queue_clear(&gs->stratum_queue);
                reset_share_stats(gs);
                gs->SYSTEM_MODULE.is_using_fallback = false;
                gs->stratum_protocol = s_primary_protocol;
                s_running_protocol = s_primary_protocol;
                s_state = COORD_STATE_RUNNING_PRIMARY;
                start_protocol_task(gs, s_primary_protocol);
                s_heartbeat_enabled = false;
            }
            break;
        }

        case COORD_EVENT_PROTOCOL_SUCCESS:
            if (s_consecutive_pool_failures > 0 || gs->SYSTEM_MODULE.pools_unavailable) {
                ESP_LOGI(TAG, "Pool connection succeeded — clearing failure state");
            }
            s_consecutive_pool_failures = 0;
            gs->SYSTEM_MODULE.pools_unavailable = false;
            break;

        case COORD_EVENT_V1_TASK_EXITED:
        case COORD_EVENT_V2_TASK_EXITED:
            // These come from clean coordinator-requested shutdowns (via stop functions).
            // They're consumed by stop_v1_task/stop_v2_task during switch_to_primary.
            // If we receive one here unexpectedly, just log it.
            ESP_LOGI(TAG, "Task exited event received (evt=%d, state=%d)", evt.type, s_state);
            break;
        case COORD_EVENT_CONFIG_RELOAD:
            break;
    }
}

static esp_err_t reload_snapshot(void *value)
{
    return SYSTEM_reload_primary_pool(value, &s_primary_protocol);
}
static bool apply_primary_reload(GlobalState *gs)
{
    unsigned requested = atomic_load(&s_reload_requested);
    if (requested == s_reload_applied) return true;
    bool restart_primary = s_state == COORD_STATE_RUNNING_PRIMARY;
    if (restart_primary && !stop_running_task(gs)) return false;
    esp_err_t error = operating_profiles_with_pool_slots(0, reload_snapshot, gs);
    if (error == ESP_OK) {
        s_primary_url = gs->SYSTEM_MODULE.pool_url;
        s_primary_port = gs->SYSTEM_MODULE.pool_port;
        s_reload_applied = requested;
    } else ESP_LOGW(TAG, "Primary pool snapshot unavailable; retrying without reboot");
    if (restart_primary) {
        SYSTEM_clean_jobs_queue(gs);
        gs->stratum_protocol = s_primary_protocol;
        s_running_protocol = s_primary_protocol;
        start_protocol_task(gs, s_primary_protocol);
    }
    // Running fallback stays connected. The existing power/recovery policy
    // retains ownership of pause/unreachable state; reload never clears it.
    return error == ESP_OK;
}

void protocol_coordinator_task(void *pvParameters)
{
    GlobalState *gs = (GlobalState *)pvParameters;

    s_primary_url = gs->SYSTEM_MODULE.pool_url;
    s_primary_port = gs->SYSTEM_MODULE.pool_port;
    s_primary_protocol = gs->stratum_protocol;
    s_fallback_protocol = gs->SYSTEM_MODULE.fallback_pool_protocol;

    // HTTP may apply a profile before the coordinator task starts.
    apply_primary_reload(gs);

    // Start initial protocol task
    if (gs->SYSTEM_MODULE.is_using_fallback) {
        // User explicitly selected fallback — use fallback protocol
        gs->stratum_protocol = s_fallback_protocol;
        s_running_protocol = s_fallback_protocol;
        s_state = COORD_STATE_RUNNING_FALLBACK;
        start_protocol_task(gs, s_fallback_protocol);
        // User chose fallback, no heartbeat
        s_heartbeat_enabled = false;
    } else {
        gs->stratum_protocol = s_primary_protocol;
        s_running_protocol = s_primary_protocol;
        s_state = COORD_STATE_RUNNING_PRIMARY;
        start_protocol_task(gs, s_primary_protocol);
    }

    ESP_LOGI(TAG, "Protocol coordinator started (primary: %s, fallback: %s, state: %d)",
             s_primary_protocol == STRATUM_PROTOCOL_V2 ? STRATUM_V2 : STRATUM_V1,
             s_fallback_protocol == STRATUM_PROTOCOL_V2 ? STRATUM_V2 : STRATUM_V1,
             s_state);

    // Heartbeat initial delay state — give fallback connection time to establish
    // before probing primary pool
    bool heartbeat_initial_delay = false;
    int64_t heartbeat_delay_start = 0;

    // Main non-blocking event loop
    while (1) {
        bool reload_ready = apply_primary_reload(gs);
        coordinator_event_t evt;
        TickType_t wait;
        if (!reload_ready || atomic_load(&s_reload_requested) != s_reload_applied) {
            wait = pdMS_TO_TICKS(5000);
        } else if (s_state == COORD_STATE_PAUSED) {
            wait = pdMS_TO_TICKS(RECOVERY_PROBE_INTERVAL_MS);
        } else if (s_heartbeat_enabled) {
            wait = pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS);
        } else {
            wait = portMAX_DELAY;
        }

        bool was_heartbeat_enabled = s_heartbeat_enabled;

        if (xQueueReceive(s_event_queue, &evt, wait) == pdTRUE) {
            handle_event(gs, evt);

            // Detect heartbeat disabled→enabled transition, reset initial delay
            if (s_heartbeat_enabled && !was_heartbeat_enabled) {
                heartbeat_initial_delay = true;
                heartbeat_delay_start = esp_timer_get_time();
            }
        } else if (!reload_ready) {
            continue;
        } else if (s_state == COORD_STATE_PAUSED) {
            // Recovery probe — try to bring a pool back online.
            try_resume_from_paused(gs);
        } else if (s_heartbeat_enabled) {
            // Timeout expired — time for a heartbeat probe
            if (heartbeat_initial_delay) {
                int64_t elapsed_ms = (esp_timer_get_time() - heartbeat_delay_start) / 1000;
                if (elapsed_ms < INITIAL_HEARTBEAT_DELAY_MS) {
                    continue;
                }
                heartbeat_initial_delay = false;
            }
            do_heartbeat_probe(gs);
        }
    }

    vTaskDelete(NULL);
}
