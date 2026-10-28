#!/usr/bin/env python3
"""Production Gamma saved-tuning scheduler + HTTP + profile journal, no device access."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
HOST=Path(__file__).resolve().parent
IMAGE='espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805'
class TuningScheduleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory(prefix='gamma-tuning-schedule-')
        cls.addClassCleanup(cls.temp.cleanup)
        directory=Path(cls.temp.name)
        for name in ('cJSON.h','cJSON.c'):
            (directory/name).write_bytes(subprocess.run(['docker','run','--rm','--entrypoint','cat',IMAGE,'/opt/esp/idf/components/json/cJSON/'+name],check=True,capture_output=True).stdout)
        for name in ('esp_err.h','esp_log.h','esp_http_server.h','nvs.h','nvs_flash.h','sv2_protocol.h','freertos/FreeRTOS.h','freertos/queue.h','freertos/task.h','freertos/semphr.h','esp_app_desc.h','esp_mac.h','mbedtls/sha256.h','protocol_coordinator.h'):
            path=directory/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('#include "tuning_schedule_stubs.h"\n')
        cls.binary=directory/'tuning-runtime'
        command=shlex.split(os.environ.get('CC','cc'))+['-std=gnu11','-D_GNU_SOURCE','-DTUNING_SCHEDULE_RUNTIME_TEST','-Wall','-Wextra','-Werror','-Wno-sign-compare','-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-misleading-indentation','-Wno-deprecated-declarations','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(directory),'-I',str(HOST),'-I',str(ROOT/'main'),'-I',str(ROOT/'main/tasks'),'-I',str(ROOT/'main/http_server'),'-include',str(HOST/'tuning_schedule_stubs.h'),str(ROOT/'main/nvs_config.c'),str(ROOT/'main/device_config.c'),str(ROOT/'main/pool_schedule.c'),str(ROOT/'main/tuning_schedule.c'),str(ROOT/'main/http_server/tuning_schedule_api.c'),str(ROOT/'main/http_server/operating_profiles.c'),str(HOST/'profiles_runtime_test.c'),str(directory/'cJSON.c'),'-lm','-o',str(cls.binary)]
        result=subprocess.run(command,capture_output=True,text=True)
        if result.returncode:raise RuntimeError(result.stderr)
    def test_production_runtime_scenarios(self):
        for case in ('tuning-read-save-auth','tuning-boundaries','tuning-protection-clock','tuning-validation-failure','tuning-pair-snapshot','tuning-model-support'):
            with self.subTest(case=case):
                result=subprocess.run([str(self.binary),case],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
                self.assertEqual(result.returncode,0,result.stderr)
                self.assertEqual(result.stdout.strip(),'PASS')
    def test_boot_order_and_shared_guarded_apply(self):
        main=(ROOT/'main/main.c').read_text()
        self.assertLess(main.index('operating_profiles_init(&GLOBAL_STATE)'),main.index('tuning_schedule_init(&GLOBAL_STATE)'))
        self.assertLess(main.index('tuning_schedule_init(&GLOBAL_STATE)'),main.index('start_rest_server'))
        runtime=(ROOT/'main/tuning_schedule.c').read_text()
        self.assertIn('operating_profile_apply_tuning_guarded',runtime)
        self.assertNotIn('setenv(',runtime)
        self.assertNotIn('tzset(',runtime)
        self.assertNotIn('esp_restart(',runtime)
if __name__=='__main__':unittest.main(verbosity=2)
