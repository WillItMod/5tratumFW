#!/usr/bin/env python3
"""Exercise firmware job selection with an explicitly simulated transport."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class JobSelectorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="5tratum-job-selector-")
        cls.binary = Path(cls.tmp.name) / "job-selector"
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            raise RuntimeError("C++17 host compiler required")
        subprocess.run([
            compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            "-I", str(ROOT / "main/tasks"),
            str(ROOT / "test/host/asic_job_selector_harness.cpp"),
            "-o", str(cls.binary),
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_scenario(self, scenario):
        return subprocess.run([str(self.binary), scenario], check=True,
                              capture_output=True, text=True).stdout

    def test_four_jobs_keep_independent_routes_headers_and_difficulty(self):
        report = json.loads(self.run_scenario("four-jobs"))
        self.assertEqual(report["source"], "simulation")
        self.assertEqual(report["asicJobs"], 4)
        self.assertFalse(report["independentWorkAssignment"])

    def test_unverified_transport_keeps_all_work_pending(self):
        self.run_scenario("unsupported")

    def test_undrained_or_exhausted_slots_stop_reuse_and_allow_other_chips(self):
        self.run_scenario("exhaustion")

    def test_clean_and_reconnect_reject_staged_old_jobs(self):
        self.run_scenario("clean-before")

    def test_publish_precedes_send_and_clean_retires_synchronous_result(self):
        self.run_scenario("clean-during")

    def test_partial_transport_failure_quarantines_wire_id(self):
        self.run_scenario("transport-failure")

    def test_latest_staged_job_is_an_immutable_copy(self):
        self.run_scenario("immutable")

    def test_incompatible_mask_or_invalid_job_never_reaches_transport(self):
        self.run_scenario("incompatible")


if __name__ == "__main__":
    unittest.main()
