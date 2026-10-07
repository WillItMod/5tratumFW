#!/usr/bin/env python3
"""Actual Gamma scheduler, HTTP and coordinator C; no device or SDK builds."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HOST = Path(__file__).resolve().parent
IDF = "espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805"


class PoolScheduleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="gamma-pool-schedule-")
        cls.addClassCleanup(cls.temp.cleanup)
        temp = Path(cls.temp.name)
        for name in ("cJSON.c", "cJSON.h"):
            (temp / name).write_bytes(subprocess.run(
                ["docker", "run", "--rm", "--entrypoint", "cat", IDF,
                 "/opt/esp/idf/components/json/cJSON/" + name], check=True, capture_output=True).stdout)
        # Compile the unmodified production close function bodies with fake
        # transports; the rest of each task depends on actual ESP drivers.
        def function(source, signature):
            start = source.index(signature)
            opening = source.index("{", start)
            depth = 1
            end = opening + 1
            while depth:
                depth += (source[end] == "{") - (source[end] == "}")
                end += 1
            return source[start:end]
        v1 = (ROOT / "main/tasks/stratum_v1_task.c").read_text()
        v2 = (ROOT / "main/tasks/stratum_v2_task.c").read_text()
        (temp / "stratum_close_hooks.inc").write_text("\n".join((
            function(v1, "void stratum_v1_close_connection("),
            function(v2, "static void stratum_v2_release_state("),
            function(v2, "void stratum_v2_close_connection("))))
        for name in ("esp_err.h", "esp_log.h", "esp_http_server.h", "esp_netif_sntp.h", "nvs.h",
                     "esp_timer.h", "esp_transport.h", "esp_transport_tcp.h", "connect.h",
                     "freertos/FreeRTOS.h", "freertos/task.h", "freertos/queue.h"):
            p = temp / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text('#include "pool_schedule_stubs.h"\n')
        cls.binary = temp / "pool-schedule-test"
        command = shlex.split(os.environ.get("CC", "cc")) + [
            "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare", "-Wno-unused-const-variable", "-Wno-deprecated-declarations",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g",
            "-I", str(temp), "-I", str(HOST), "-I", str(ROOT / "main"),
            "-I", str(ROOT / "main/tasks"), "-I", str(ROOT / "main/http_server"),
            "-I", str(ROOT / "components/stratum/include"),
            "-include", str(HOST / "pool_schedule_stubs.h"),
            str(ROOT / "main/pool_schedule.c"), str(ROOT / "main/pool_reload.c"),
            str(ROOT / "main/http_server/pool_schedule_api.c"), str(HOST / "pool_schedule_test.c"),
            str(temp / "cJSON.c"), "-lm", "-o", str(cls.binary)]
        result = subprocess.run(command, text=True, capture_output=True)
        if result.returncode:
            raise RuntimeError(result.stderr)

    def test_actual_firmware_scenarios(self):
        for scenario in ("selector", "invalid", "json-oom", "clock", "reconcile", "failure", "http", "corrupt", "reload", "readers", "close-hooks", "coordinator"):
            with self.subTest(scenario=scenario):
                result = subprocess.run([str(self.binary), scenario], text=True, capture_output=True,
                                        env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout.strip(), "PASS")

    def test_boot_order_and_independent_timezone(self):
        main = (ROOT / "main/main.c").read_text()
        self.assertLess(main.index("device_config_init(&GLOBAL_STATE)"), main.index("pool_schedule_init(&GLOBAL_STATE)"))
        self.assertLess(main.index("operating_profiles_init(&GLOBAL_STATE)"), main.index("pool_schedule_init(&GLOBAL_STATE)"))
        self.assertLess(main.index("pool_schedule_init(&GLOBAL_STATE)"), main.index("start_rest_server"))
        self.assertLess(main.index("protocol_coordinator_init(&GLOBAL_STATE)"), main.index("pool_schedule_start()"))
        schedule = (ROOT / "main/pool_schedule.c").read_text()
        self.assertIn("gmtime_r", schedule)
        self.assertNotIn("setenv(", schedule)
        self.assertNotIn("tzset(", schedule)
        self.assertNotIn("esp_restart(", schedule)
        shares = (ROOT / "main/tasks/asic_result_task.c").read_text()
        capture = shares.index("connection_generation = protocol_coordinator_work_generation()")
        snapshot = shares.index("pthread_mutex_lock(&GLOBAL_STATE->valid_jobs_lock)")
        guard = shares.index("connection_generation != protocol_coordinator_work_generation()")
        submit = shares.index("stratum_v2_submit_share_extended(")
        self.assertLess(capture, snapshot)
        self.assertLess(guard, submit)
        self.assertLess(shares.index("SYSTEM_stratum_io_lock()"), guard)
        self.assertGreater(shares.index("SYSTEM_stratum_io_unlock()"), submit)


if __name__ == "__main__":
    unittest.main(verbosity=2)
