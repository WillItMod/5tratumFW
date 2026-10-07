#!/usr/bin/env python3
"""Exercise Gamma's production MUX parser, dispatch, timing and peer lifetime.

Runs with the pinned IDF image's cJSON and host ASan/UBSan. Platform services
are fake; the notification/share branches are extracted unchanged from the
receive task. No miner or external pool is accessed.
"""
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
IMAGE = 'espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805'
HARNESS = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mux_peer_status.h"
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define BUFFER_SIZE 1024
#define TRANSPORT_TIMEOUT_MS 5000
#define REQUIRE(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void hex2bin(const char *s, unsigned char *out, size_t n) { abort(); }
typedef void *esp_transport_handle_t;
typedef struct { const char *version; } esp_app_desc_t;
static int64_t fake_clock_us;
static unsigned critical_depth;
static bool require_locked_clock;
static void (*before_lock_hook)(void);
static char last_write[BUFFER_SIZE];
void test_enter_critical(int *lock) {
    if (before_lock_hook) {
        void (*hook)(void) = before_lock_hook;
        before_lock_hook = NULL;
        hook();
    }
    ++critical_depth;
}
void test_exit_critical(int *lock) {
    REQUIRE(critical_depth == 1);
    --critical_depth;
}
int64_t esp_timer_get_time(void) {
    if (require_locked_clock) REQUIRE(critical_depth == 1);
    return fake_clock_us;
}
static const esp_app_desc_t *esp_app_get_description(void) {
    static const esp_app_desc_t app = {.version="5tratumFW-0.1.0-beta.1"};
    return &app;
}
static int esp_transport_write(esp_transport_handle_t transport, const char *buffer, int length, int timeout) {
    REQUIRE(length >= 0 && (size_t)length < sizeof(last_write));
    memcpy(last_write,buffer,length);
    last_write[length] = 0;
    return length;
}
'''
DISPATCH_PREFIX = r'''
typedef struct {
    struct { float response_time; unsigned accepted, rejected; } SYSTEM_MODULE;
} TestGlobalState;
static TestGlobalState test_state;
static StratumApiV1Message stratum_api_v1_message;
static void SYSTEM_notify_accepted_share(TestGlobalState *state) { ++state->SYSTEM_MODULE.accepted; }
static void SYSTEM_notify_rejected_share(TestGlobalState *state, const char *reason) { ++state->SYSTEM_MODULE.rejected; }
static void production_dispatch(int64_t receive_time_us, uint64_t mux_generation) {
    TestGlobalState *GLOBAL_STATE = &test_state;
'''
TEST = r'''
static const char *valid = "{\"id\":null,\"method\":\"mining.5tratum.status\",\"params\":[{\"protocolVersion\":1,\"product\":\"5tratMUX\",\"workScope\":\"chain-broadcast\",\"independentWorkAssignment\":false,\"ttlSeconds\":90}]}";
static void parse_reserved(const char *s, bool expected) {
    StratumApiV1Message message = {0};
    STRATUM_V1_parse(&message, s);
    REQUIRE(message.method == MINING_5TRATUM_STATUS);
    REQUIRE(message.mux_status_valid == expected);
    REQUIRE(!message.response_success && message.message_id == -1 && !message.error_str);
    STRATUM_V1_reset_message(&message);
}
static void typed_notification(const char *version, const char *ttl, bool expected) {
    char text[512];
    snprintf(text,sizeof(text),"{\"id\":null,\"method\":\"mining.5tratum.status\",\"params\":[{\"protocolVersion\":%s,\"product\":\"5tratMUX\",\"workScope\":\"chain-broadcast\",\"independentWorkAssignment\":false,\"ttlSeconds\":%s}]}",version,ttl);
    parse_reserved(text,expected);
}
static void notifications(void) {
    parse_reserved(valid,true);
    parse_reserved("{\"jsonrpc\":\"2.0\",\"id\":null,\"method\":\"mining.5tratum.status\",\"params\":[{\"protocol\\u0056ersion\":1,\"product\":\"\\u0035tratMUX\",\"workScope\":\"chain-broadcast\",\"independentWorkAssignment\":false,\"ttlSeconds\":90}]}",true);
    const char *versions[] = {"1.0","1e0","1E+0","01","0001","1.0000000000000001","1.5","\"1\"","true","false","null","-1","4294967296"};
    for (unsigned i=0; i<sizeof(versions)/sizeof(versions[0]); ++i) typed_notification(versions[i],"90",false);
    const char *ttls[] = {"90.0","9e1","9E+1","090","00090","90.000000000000001","90.5","\"90\"","true","false","null","-90","900"};
    for (unsigned i=0; i<sizeof(ttls)/sizeof(ttls[0]); ++i) typed_notification("1",ttls[i],false);
    const char *invalid[] = {
      "{\"id\":7,\"method\":\"mining.5tratum.status\",\"result\":true,\"params\":[]}",
      "{\"id\":null,\"method\":\"mining.5tratum.status\",\"params\":[{\"protocolVersion\":1,\"product\":\"5tratMUX\",\"workScope\":\"chain-broadcast\",\"independentWorkAssignment\":true,\"ttlSeconds\":90}]}",
      "{\"id\":null,\"method\":\"mining.5tratum.status\",\"params\":[{\"protocolVersion\":1,\"product\":\"5tratMUX\\u0000evil\",\"workScope\":\"chain-broadcast\",\"independentWorkAssignment\":false,\"ttlSeconds\":90}]}",
      "{\"id\":null,\"method\":\"mining.5tratum.status\\u0000evil\",\"params\":[]}",
      "{\"id\":7,\"method\":7,\"method\":\"mining.5tratum.status\",\"result\":true}",
      "{\"id\":null,\"id\":7,\"method\":\"mining.5tratum.status\",\"params\":[]}",
      "{\"id\":null,\"method\":\"mining.5tratum.status\",\"params\":[{\"protocolVersion\":1,\"protocolVersion\":2,\"product\":\"5tratMUX\",\"workScope\":\"chain-broadcast\",\"independentWorkAssignment\":false,\"ttlSeconds\":90}]}",
      "{\"id\":null,\"method\":\"mining.5tratum.status\",\"params\":[{},{}]}",
      "{\"id\":null,\"method\":\"mining.5tratum.status\",\"params\":[{\"protocolVersion\":1,\"product\":\"5tratMUX\",\"workScope\":\"chain-broadcast\",\"independentWorkAssignment\":false,\"ttlSeconds\":90,\"extra\":false}]}",
      "{\"method\":\"mining.5tratum.status\",\"params\":[{\"protocolVersion\":1,\"product\":\"5tratMUX\",\"workScope\":\"chain-broadcast\",\"independentWorkAssignment\":false,\"ttlSeconds\":90}]}"
    };
    for (unsigned i=0; i<sizeof(invalid)/sizeof(invalid[0]); ++i) parse_reserved(invalid[i],false);
    const char *suffixes[] = {"garbage","{}","{\"id\":8,\"result\":true}","\v"};
    char text[700];
    for (unsigned i=0; i<sizeof(suffixes)/sizeof(suffixes[0]); ++i) {
        snprintf(text,sizeof(text),"%s%s",valid,suffixes[i]);
        parse_reserved(text,false);
    }
    snprintf(text,sizeof(text)," \t%s\r\n ",valid);
    parse_reserved(text,true);
    memset(text,' ',sizeof(text)); memcpy(text,valid,strlen(valid)); text[512]=0;
    parse_reserved(text,true); // exactly 512 bytes remains bounded
    text[512]=' '; text[513]=0;
    parse_reserved(text,false);
    StratumApiV1Message message = {0};
    STRATUM_V1_parse(&message,"{\"id\":8,\"result\":true,\"error\":null}");
    REQUIRE(message.method == STRATUM_RESULT && message.response_success && !message.mux_status_valid);
    STRATUM_V1_reset_message(&message);
    STRATUM_V1_parse(&message,"{\"id\":8,\"result\":false,\"error\":[23,\"rejected\",null]}");
    REQUIRE(message.method == STRATUM_RESULT && !message.response_success && !message.mux_status_valid);
    STRATUM_V1_reset_message(&message);
    STRATUM_V1_parse(&message,"{\"id\":null,\"method\":\"mining.set_difficulty\",\"params\":[100.5]}");
    REQUIRE(message.method == MINING_SET_DIFFICULTY && message.new_difficulty == 100.5);
    STRATUM_V1_reset_message(&message); // normal protocol numbers retain existing behavior
    puts("PASS: strict reserved envelopes, integer tokens, trailing data and 512-byte boundary");
}
static void lifetime(void) {
    FiveTratumMuxPeer state = {0};
    REQUIRE(!five_tratum_mux_snapshot(&state,0).connected);
    uint64_t first = five_tratum_mux_begin(&state,false);
    REQUIRE(!five_tratum_mux_snapshot(&state,1).acknowledged);
    five_tratum_mux_receive(&state,first,true,100);
    REQUIRE(five_tratum_mux_snapshot(&state,100).acknowledged);
    REQUIRE(five_tratum_mux_snapshot(&state,90000099).acknowledged);
    REQUIRE(five_tratum_mux_snapshot(&state,90000100).expired);
    REQUIRE(!five_tratum_mux_snapshot(&state,90000100).acknowledged);
    REQUIRE(five_tratum_mux_snapshot(&state,90000100).age_seconds == 90);
    REQUIRE(five_tratum_mux_snapshot(&state,99).expired);
    five_tratum_mux_receive(&state,first,true,91000000);
    REQUIRE(five_tratum_mux_snapshot(&state,180999999).acknowledged);
    REQUIRE(five_tratum_mux_snapshot(&state,181000000).expired);
    five_tratum_mux_receive(&state,first,false,181000001);
    REQUIRE(!five_tratum_mux_snapshot(&state,181000002).acknowledged);
    REQUIRE(!five_tratum_mux_snapshot(&state,181000002).expired);
    uint64_t second = five_tratum_mux_begin(&state,true);
    REQUIRE(second != first);
    five_tratum_mux_receive(&state,first,true,200);
    REQUIRE(!five_tratum_mux_snapshot(&state,200).acknowledged);
    five_tratum_mux_receive(&state,second,true,201);
    REQUIRE(five_tratum_mux_snapshot(&state,202).fallback);
    five_tratum_mux_receive(&state,first,false,300);
    REQUIRE(five_tratum_mux_snapshot(&state,301).acknowledged); // stale invalidation cannot revoke
    five_tratum_mux_receive(&state,first,true,90000000);
    REQUIRE(five_tratum_mux_snapshot(&state,90000201).expired); // stale refresh cannot extend TTL
    five_tratum_mux_disconnect(&state);
    five_tratum_mux_receive(&state,second,true,90000202);
    REQUIRE(!five_tratum_mux_snapshot(&state,90000203).connected);
    REQUIRE(!five_tratum_mux_snapshot(&state,90000203).acknowledged);
    REQUIRE(!five_tratum_mux_snapshot(&state,90000203).expired);
    state.generation = UINT64_MAX;
    REQUIRE(five_tratum_mux_begin(&state,false) == 1);
    five_tratum_mux_receive(&state,1,true,-1);
    REQUIRE(!five_tratum_mux_snapshot(&state,1).acknowledged);
    five_tratum_mux_receive(&state,1,true,0);
    REQUIRE(five_tratum_mux_snapshot(&state,INT64_MAX).age_seconds == UINT32_MAX);
    puts("PASS: exact TTL, renewal, invalidation, reconnect generation, fallback and monotonic bounds");
}
static void parse_and_dispatch(const char *text, uint64_t generation, int64_t now) {
    STRATUM_V1_parse(&stratum_api_v1_message,text);
    production_dispatch(now,generation);
    STRATUM_V1_reset_message(&stratum_api_v1_message);
}
static void share_interception(void) {
    memset(&test_state,0,sizeof(test_state));
    memset(request_timings,0,sizeof(request_timings));
    uint64_t generation = mux_peer_status_begin(false);
    stamp_tx(8,1000000);
    parse_and_dispatch(valid,generation,1000001);
    REQUIRE(test_state.SYSTEM_MODULE.accepted == 0 && test_state.SYSTEM_MODULE.rejected == 0);
    REQUIRE(request_timings[8].tracking);
    fake_clock_us = 1000002;
    REQUIRE(mux_peer_status_snapshot().acknowledged);
    parse_and_dispatch("{\"id\":8,\"method\":\"mining.5tratum.status\",\"result\":true}",generation,1000003);
    REQUIRE(test_state.SYSTEM_MODULE.accepted == 0 && test_state.SYSTEM_MODULE.rejected == 0);
    REQUIRE(request_timings[8].tracking);
    REQUIRE(!mux_peer_status_snapshot().acknowledged); // invalid advertisement revokes the badge
    parse_and_dispatch("{\"id\":8,\"method\":\"mining.5tratum.status\",\"result\":false,\"error\":[23,\"fake reject\",null]}junk",generation,1000004);
    REQUIRE(test_state.SYSTEM_MODULE.accepted == 0 && test_state.SYSTEM_MODULE.rejected == 0);
    REQUIRE(request_timings[8].tracking);
    parse_and_dispatch("{\"id\":8,\"result\":true,\"error\":null}",generation,1005000);
    REQUIRE(test_state.SYSTEM_MODULE.accepted == 1 && test_state.SYSTEM_MODULE.rejected == 0);
    REQUIRE(test_state.SYSTEM_MODULE.response_time == 5.0f && !request_timings[8].tracking);
    parse_and_dispatch("{\"id\":8,\"result\":true,\"error\":null}",generation,1006000);
    REQUIRE(test_state.SYSTEM_MODULE.accepted == 1); // duplicate result cannot consume a submit twice
    stamp_tx(9,2000000);
    parse_and_dispatch("{\"id\":9,\"method\":\"mining.5tratum.status\",\"result\":false}",generation,2000001);
    REQUIRE(request_timings[9].tracking && test_state.SYSTEM_MODULE.rejected == 0);
    parse_and_dispatch("{\"id\":9,\"result\":false,\"error\":[23,\"rejected\",null]}",generation,2005000);
    REQUIRE(test_state.SYSTEM_MODULE.accepted == 1 && test_state.SYSTEM_MODULE.rejected == 1);
    REQUIRE(!request_timings[9].tracking);
    puts("PASS: actual reserved/share dispatch preserves pending submit timing and accepted/rejected counts");
}
static uint64_t interleaved_generation;
static void receive_before_snapshot_lock(void) {
    fake_clock_us = 201;
    mux_peer_status_receive(interleaved_generation,true,fake_clock_us);
}
static void locked_snapshot(void) {
    interleaved_generation = mux_peer_status_begin(true);
    fake_clock_us = 200;
    before_lock_hook = receive_before_snapshot_lock;
    require_locked_clock = true;
    FiveTratumMuxSnapshot snapshot = mux_peer_status_snapshot();
    require_locked_clock = false;
    REQUIRE(snapshot.acknowledged && !snapshot.expired && snapshot.fallback && snapshot.age_seconds == 0);
    REQUIRE(critical_depth == 0 && !before_lock_hook);
    puts("PASS: snapshot clock and peer state share a critical section during interleaved receive");
}
static void subscribe_optin(void) {
    int sent = STRATUM_V1_subscribe(NULL,2,"BM1370");
    REQUIRE(sent == (int)strlen(last_write) && last_write[sent-1] == '\n');
    cJSON *json = cJSON_Parse(last_write);
    REQUIRE(json && !strcmp(cJSON_GetObjectItemCaseSensitive(json,"method")->valuestring,"mining.subscribe"));
    REQUIRE(cJSON_GetObjectItemCaseSensitive(json,"id")->valueint == 2);
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(json,"params");
    REQUIRE(cJSON_GetArraySize(params) == 1);
    REQUIRE(!strcmp(cJSON_GetArrayItem(params,0)->valuestring,"5tratumFW/BM1370/5tratumFW-0.1.0-beta.1"));
    cJSON_Delete(json);
    puts("PASS: production SV1 subscribe advertises the explicit 5tratumFW/ opt-in prefix");
}
int main(void) {
    notifications(); lifetime(); share_interception(); locked_snapshot(); subscribe_optin();
    puts("Gamma MUX: 5 production host scenarios passed (ASan/UBSan)");
}
'''


def run():
    source = (ROOT / 'components/stratum/stratum_api.c').read_text()
    start = source.index('void STRATUM_V1_reset_message(')
    stamp = source.index('static void stamp_tx(', start)
    methods = source[start:stamp]
    timing_start = source.index('static RequestTiming request_timings')
    timing_end = source.index('esp_transport_handle_t STRATUM_V1_transport_init', timing_start)
    timing = source[timing_start:timing_end]
    sender = source[stamp:source.index('int STRATUM_V1_suggest_difficulty(', stamp)]
    api = (ROOT / 'components/stratum/include/stratum_api.h').read_text()
    types = api[api.index('#define MAX_MERKLE_BRANCHES'):api.index('esp_transport_handle_t STRATUM_V1_transport_init')]
    task = (ROOT / 'main/tasks/stratum_v1_task.c').read_text()
    reserved_start = task.index('if (stratum_api_v1_message.method == MINING_5TRATUM_STATUS)')
    reserved_end = task.index('} else if (stratum_api_v1_message.method == MINING_NOTIFY)', reserved_start)
    result_start = task.index('} else if (stratum_api_v1_message.method == STRATUM_RESULT)', reserved_end)
    result_end = task.index('} else if (stratum_api_v1_message.method == STRATUM_RESULT_SETUP)', result_start)
    dispatch = DISPATCH_PREFIX + task[reserved_start:reserved_end] + task[result_start:result_end] + '\n}\n}\n'
    (ROOT / 'build').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=ROOT / 'build', prefix='mux-host-') as value:
        folder = Path(value)
        (folder / 'freertos').mkdir()
        (folder / 'freertos/FreeRTOS.h').write_text('''#pragma once
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
void test_enter_critical(int *lock);
void test_exit_critical(int *lock);
#define taskENTER_CRITICAL(lock) test_enter_critical(lock)
#define taskEXIT_CRITICAL(lock) test_exit_critical(lock)
''')
        (folder / 'freertos/task.h').write_text('#include "FreeRTOS.h"\n')
        (folder / 'esp_timer.h').write_text('#include <stdint.h>\nint64_t esp_timer_get_time(void);\n')
        (folder / 'harness.c').write_text(HARNESS + types + '\nvoid STRATUM_V1_free_mining_notify(mining_notify *params);\n' + timing + methods + sender + dispatch + TEST)
        mount = 'type=bind,source=' + str(ROOT) + ',target=/project'
        relative = shlex.quote(str(folder.relative_to(ROOT)))
        script = ('set -eu\n'
                  'cc -std=gnu11 -g -fsanitize=address,undefined -fno-omit-frame-pointer '
                  '-Icomponents/stratum/include -Imain -I' + relative + ' '
                  '-I/opt/esp/idf/components/json/cJSON '
                  + relative + '/harness.c main/mux_peer_status.c '
                  '/opt/esp/idf/components/json/cJSON/cJSON.c -lm -o ' + relative + '/mux-host\n'
                  'ASAN_OPTIONS=detect_leaks=1 ' + relative + '/mux-host')
        subprocess.run(['docker','run','--rm','--entrypoint','/bin/bash','--mount',mount,
                        '-w','/project',IMAGE,'-c',script],check=True)


if __name__ == '__main__':
    run()
