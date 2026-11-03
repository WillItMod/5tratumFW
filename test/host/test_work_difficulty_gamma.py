#!/usr/bin/env python3
"""Real Gamma work metadata, queued receipts, constructors and HTTP serializer.
No miner/network I/O. Cryptographic math and UART are controlled test boundaries.
"""
from pathlib import Path
import os,shlex,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[2]
IMAGE='espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805'

def method(source,signature):
    start=source.index(signature);brace=source.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]+'\n'

class WorkDifficultyGammaTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory(prefix='5fw-gamma-work-difficulty-');cls.addClassCleanup(cls.temp.cleanup);p=Path(cls.temp.name)
        (p/'freertos').mkdir()
        (p/'freertos/FreeRTOS.h').write_text('#pragma once\n#include <pthread.h>\ntypedef pthread_mutex_t portMUX_TYPE;\n#define portMUX_INITIALIZER_UNLOCKED PTHREAD_MUTEX_INITIALIZER\n#define taskENTER_CRITICAL(p) pthread_mutex_lock(p)\n#define taskEXIT_CRITICAL(p) pthread_mutex_unlock(p)\n')
        (p/'freertos/portmacro.h').write_text('#include "FreeRTOS.h"\n')
        (p/'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n#define ESP_ERR_INVALID_ARG 0x102\n')
        (p/'esp_transport.h').write_text('#pragma once\ntypedef void *esp_transport_handle_t;\n')
        (p/'esp_timer.h').write_text('#pragma once\n#include <stdint.h>\nint64_t esp_timer_get_time(void);\n')
        (p/'esp_http_server.h').write_text('#pragma once\n#include "esp_err.h"\ntypedef void *httpd_handle_t;\ntypedef struct {int status;char *body;}httpd_req_t;\ntypedef struct {const char *uri;int method;esp_err_t(*handler)(httpd_req_t*);void *user_ctx;}httpd_uri_t;\n#define HTTP_GET 0\n#define HTTPD_401_UNAUTHORIZED 401\n#define HTTPD_500_INTERNAL_SERVER_ERROR 500\nesp_err_t httpd_resp_set_type(httpd_req_t*,const char*);\nesp_err_t httpd_resp_set_hdr(httpd_req_t*,const char*,const char*);\nesp_err_t httpd_resp_send_500(httpd_req_t*);\nesp_err_t httpd_resp_send_err(httpd_req_t*,int,const char*);\nesp_err_t httpd_resp_sendstr(httpd_req_t*,const char*);\nesp_err_t httpd_register_uri_handler(httpd_handle_t,const httpd_uri_t*);\n')
        (p/'global_state.h').write_text('#pragma once\n#include <stdbool.h>\n#include <stdint.h>\n#include "sv2_protocol.h"\n#define QUEUE_SIZE 12\ntypedef struct {int count;void*last;}FakeQueue;\ntypedef struct GlobalState {bool allowed;uint32_t version_mask;int extranonce_2_len;char *extranonce_str;sv2_conn_t *sv2_conn;FakeQueue stratum_queue;struct {unsigned work_received;}SYSTEM_MODULE;}GlobalState;\n')
        (p/'mining_state.h').write_text('#pragma once\n#include "global_state.h"\nstatic inline bool mining_state_work_allowed(const GlobalState*g){return g->allowed;}\n')
        mining=(ROOT/'components/stratum/mining.c').read_text();create=(ROOT/'main/tasks/create_jobs_task.c').read_text();v2=(ROOT/'main/tasks/stratum_v2_task.c').read_text();v1=(ROOT/'main/tasks/stratum_v1_task.c').read_text()
        methods=method(mining,'void construct_bm_job(')
        for signature in ['static void generate_work(GlobalState *GLOBAL_STATE, mining_notify *notification, uint64_t extranonce_2, double difficulty)\n{','static void generate_work_sv2(GlobalState *GLOBAL_STATE, sv2_job_t *sv2_job, double difficulty)\n{','static void generate_work_sv2_ext(GlobalState *GLOBAL_STATE, sv2_ext_job_t *ext_job,\n                                   double difficulty, uint64_t extranonce_2_counter)\n{']:
            methods+=method(create,signature)
        for signature in ['static bool add_active_job_id(','static void clear_active_job_ids(','static void stratum_v2_enqueue_job(','static void stratum_v2_enqueue_ext_job(']:methods+=method(v2,signature)
        methods+=method(v1,'static bool add_active_job_id(').replace('add_active_job_id','v1_add_active_job_id')
        methods+=method(v1,'static void clear_active_job_ids(').replace('clear_active_job_ids','v1_clear_active_job_ids')
        body=method(v1,'case MINING_NOTIFY:');body=body[body.index('{'):].replace('add_active_job_id','v1_add_active_job_id').replace('clear_active_job_ids','v1_clear_active_job_ids')
        methods+='static void accept_real_v1(GlobalState *GLOBAL_STATE,mining_notify *notify,unsigned work_pool,uint64_t work_session,uint64_t receive_time_us){static char *active_job_ids[16];static int active_job_ids_count;StratumApiV1Message stratum_api_v1_message={.mining_notification=notify};'+body+'}\n'
        (p/'real_work_functions.inc').write_text(methods)
        # Match the other host API tests: always compile the real implementation
        # from the same pinned SDK, including on a clean checkout before IDF setup.
        cjson=p/'cJSON';cjson.mkdir()
        for name in ('cJSON.h','cJSON.c'):
            (cjson/name).write_bytes(subprocess.run(['docker','run','--rm','--network','none',
                '--entrypoint','cat',IMAGE,'/opt/esp/idf/components/json/cJSON/'+name],
                check=True,capture_output=True).stdout)
        cls.binary=p/'work-difficulty-gamma'
        command=shlex.split(os.environ.get('CC','cc'))+['-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-deprecated-declarations','-Wno-unused-const-variable','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(p),'-I',str(ROOT/'main/http_server'),'-I',str(ROOT/'main'),'-I',str(ROOT/'components/stratum/include'),'-I',str(ROOT/'components/stratum_v2/include'),'-I',str(cjson),str(ROOT/'main/work_difficulty.c'),str(ROOT/'main/http_server/work_difficulty_api.c'),str(ROOT/'test/host/work_difficulty_gamma_harness.c'),str(cjson/'cJSON.c'),'-pthread','-lm','-o',str(cls.binary)]
        result=subprocess.run(command,capture_output=True,text=True)
        if result.returncode:raise RuntimeError(result.stderr)
    def run_case(self,name):
        result=subprocess.run([str(self.binary),name],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
        self.assertEqual(result.returncode,0,result.stderr or result.stdout);self.assertEqual(result.stdout.strip(),'PASS')
    def test_exact_compact_target_bitcoin_normalization_and_invalid_bounds(self):self.run_case('compact')
    def test_independent_selected_pool_lifecycle_and_600_second_current_job(self):self.run_case('lifecycle')
    def test_old_build_epoch_connection_and_reset_cannot_relabel_work(self):self.run_case('race')
    def test_invalid_new_dispatched_values_clear_previous_visibility(self):self.run_case('invalid')
    def test_actual_v1_sv2_constructors_and_uart_failure_fences(self):self.run_case('constructors')
    def test_actual_notify_wrappers_attach_timestamps_once_not_on_duplicates(self):self.run_case('enqueue')
    def test_actual_http_schema_authorization_nulls_and_no_age_renewal(self):self.run_case('http')
    def test_concurrent_snapshot_never_combines_job_epochs_or_values(self):self.run_case('threaded')
    def test_lifecycle_hooks_are_on_real_connection_and_reset_paths(self):
        coordinator=(ROOT/'main/tasks/protocol_coordinator.c').read_text();system=(ROOT/'main/system.c').read_text();v1=(ROOT/'main/tasks/stratum_v1_task.c').read_text();v2=(ROOT/'main/tasks/stratum_v2_task.c').read_text()
        self.assertIn('work_difficulty_disconnect()',method(coordinator,'void protocol_coordinator_invalidate_work('))
        self.assertIn('work_difficulty_invalidate_dispatch()',method(system,'void SYSTEM_clean_jobs_queue('))
        self.assertEqual(v1.count('work_difficulty_received('),1);self.assertEqual(v2.count('work_difficulty_received('),2)
        self.assertIn('work_difficulty_forget_job(work_pool,work_session)',v1)
        self.assertIn('work_difficulty_forget_job(conn->work_source_pool,conn->work_source_session)',method(v2,'static void stratum_v2_handle_set_new_prev_hash('))
        self.assertNotIn('work_difficulty_received(',method(v2,'static void stratum_v2_handle_set_target('))
if __name__=='__main__':unittest.main()
