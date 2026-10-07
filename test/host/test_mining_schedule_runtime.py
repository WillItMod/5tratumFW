#!/usr/bin/env python3
"""Execute actual scheduler/API C with fake storage, time and HTTP. No miner I/O."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HOST = Path(__file__).resolve().parent
IDF_IMAGE = "espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805"


class ScheduleRuntimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="5tratumfw-schedule-")
        cls.addClassCleanup(cls.temp.cleanup)
        directory = Path(cls.temp.name)
        # Reuse the exact JSON implementation from the pinned build SDK. Docker
        # only reads these two SDK files; the actual tests run with the host cc.
        for name in ("cJSON.h", "cJSON.c"):
            source = subprocess.run(
                ["docker", "run", "--rm", "--entrypoint", "cat", IDF_IMAGE,
                 "/opt/esp/idf/components/json/cJSON/" + name],
                check=True, capture_output=True,
            ).stdout
            (directory / name).write_bytes(source)
        for name in ("esp_err.h", "esp_log.h", "nvs.h", "esp_http_server.h", "esp_netif_sntp.h"):
            (directory / name).write_text('#include "schedule_runtime_stubs.h"\n')
        cls.binary = directory / "schedule-runtime-test"
        command = shlex.split(os.environ.get("CC", "cc")) + [
            "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-const-variable",
            "-I", str(directory), "-I", str(HOST), "-I", str(ROOT / "main"),
            "-I", str(ROOT / "main/http_server"), "-include", str(HOST / "schedule_runtime_stubs.h"),
            str(ROOT / "main/mining_schedule.c"), str(ROOT / "main/mining_schedule_policy.c"),
            str(ROOT / "main/http_server/mining_schedule_api.c"),
            str(HOST / "schedule_runtime_test.c"), str(directory / "cJSON.c"), "-lm", "-o", str(cls.binary),
        ]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError("Runtime host compilation failed:\n" + result.stderr)

    def run_case(self, scenario):
        result = subprocess.run([str(self.binary), scenario], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr or result.stdout)
        self.assertEqual(result.stdout.strip(), "PASS")

    def test_missing_key_defaults_disabled_without_writes(self):
        self.run_case("missing")

    def test_corrupt_wrong_type_unknown_zone_and_trailing_data_fail_closed(self):
        for scenario in ("corrupt", "wrong-type", "bad-zone", "trailing"):
            with self.subTest(scenario=scenario):
                self.run_case(scenario)

    def test_http_validation_authorization_and_chunked_body(self):
        self.run_case("validation")

    def test_json_allocation_failures_never_commit_a_partial_schedule(self):
        self.run_case("serialize-oom")

    def test_independent_clock_required_and_disable_latches_request(self):
        self.run_case("clock")

    def test_later_invalid_sync_and_epoch_reset_force_pause_until_sync(self):
        self.run_case("clock-reset")

    def test_override_expires_at_next_union_boundary(self):
        self.run_case("override")

    def test_forward_jump_across_two_boundaries_expires_override(self):
        self.run_case("jump")

    def test_dst_spring_gap_and_fall_repeated_hour_deadlines(self):
        self.run_case("dst-spring")
        self.run_case("dst-fall")

    def test_requested_applied_transition_and_fault_are_distinct(self):
        self.run_case("status")


if __name__ == "__main__":
    unittest.main(verbosity=2)
