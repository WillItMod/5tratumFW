#!/usr/bin/env python3
"""Actual strict notification parser/state under host sanitizers; no device I/O."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class MuxStatusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="5tratum-mux-status-")
        cls.binary = Path(cls.tmp.name) / "mux-status"
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            raise RuntimeError("A C++17 host compiler is required")
        subprocess.run([
            compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            "-I", str(ROOT / "components/arduinojson"),
            "-I", str(ROOT / "main/stratum"),
            str(ROOT / "test/host/mux_status_harness.cpp"),
            "-o", str(cls.binary),
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def scenario(self, name):
        subprocess.run([str(self.binary), name], check=True, capture_output=True)

    def test_exact_ttl_boundary_renewal_and_monotonic_rollback(self):
        self.scenario("freshness")

    def test_reconnect_clears_and_old_connection_cannot_renew_or_invalidate(self):
        self.scenario("reconnect")

    def test_primary_and_fallback_status_are_independent(self):
        self.scenario("fallback")

    def test_standard_jobs_and_share_results_are_not_intercepted(self):
        self.scenario("unrelated")

    def test_malformed_oversized_and_false_capabilities_fail_closed_but_are_consumed(self):
        self.scenario("invalid")

    def test_coin_context_clears_on_unknown_expiry_disconnect_and_reconnect_and_keeps_pools_distinct(self):
        self.scenario("coin")

    def test_coin_context_limits_types_controls_extra_fields_and_exact_boundaries(self):
        self.scenario("coin-invalid")

    def test_canonical_digit_leading_coin_route_changes_keep_ack_and_strict_job_binding(self):
        self.scenario("coin-leading-digit")

    def test_work_context_legacy_null_and_nullable_numeric_fields(self):
        self.scenario("work")

    def test_work_context_matches_latest_received_job_and_target_and_connection(self):
        self.scenario("work-binding")

    def test_work_age_heartbeat_regression_and_exact_expiry(self):
        self.scenario("work-age")

    def test_work_pools_do_not_invent_a_dual_aggregate(self):
        self.scenario("work-pools")

    def test_work_schema_limits_types_and_raw_notification_bound(self):
        self.scenario("work-invalid")


if __name__ == "__main__":
    unittest.main()
