#!/usr/bin/env python3
"""Test the actual portable registry with simulated drain attestations only.

This runtime foundation is not wired to create_jobs/result tasks, MUX sessions,
or physical UART dispatch. Passing these tests is not hardware isolation proof.
"""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class AsicJobRegistryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="5tratum-job-registry-")
        cls.binary = Path(cls.tmp.name) / "asic-job-registry"
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            raise RuntimeError("A C++17 host compiler is required")
        subprocess.run([
            compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-g",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            "-I", str(ROOT / "main"),
            str(ROOT / "test/host/asic_job_registry_harness.cpp"),
            "-o", str(cls.binary),
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def scenario(self, name):
        result = subprocess.run([str(self.binary), name], check=True, capture_output=True, text=True)
        report = json.loads(result.stdout)
        self.assertTrue(report["passed"])
        self.assertFalse(report["physicalDispatchEnabled"])
        self.assertLessEqual(report["registryBytes"], 32768)

    def test_four_asics_keep_same_wire_id_and_distinct_original_routes(self):
        self.scenario("isolation")

    def test_route_payload_and_fractional_difficulty_are_immutable(self):
        self.scenario("immutable-payload")

    def test_context_is_published_before_immediate_result_dispatch(self):
        self.scenario("publish-before-dispatch")

    def test_startup_requires_explicit_drain_attestation_and_rejects_replay(self):
        self.scenario("startup-barrier")

    def test_reconnect_rejects_copied_results_and_old_socket_callbacks(self):
        self.scenario("reconnect")

    def test_clean_during_build_or_result_clone_cannot_resurrect_stale_jobs(self):
        self.scenario("clean-inflight-race")

    def test_clean_and_disconnect_preserve_other_chips_contexts(self):
        self.scenario("chip-local-clean")

    def test_exhausted_or_cancelled_ids_require_verified_drain_before_reuse(self):
        self.scenario("exhaustion-and-reuse")

    def test_intervening_clean_or_reconnect_invalidates_drain_ticket(self):
        self.scenario("drain-epoch-race")

    def test_duplicate_scope_contains_chip_job_and_rolled_version(self):
        self.scenario("duplicates")

    def test_duplicate_capacity_fails_closed_without_forgetting_old_entries(self):
        self.scenario("bounded-duplicates")

    def test_version_mask_validates_actual_version_including_cleared_base_bits(self):
        self.scenario("version-mask")

    def test_invalid_inventory_ids_difficulty_and_payload_do_not_consume_slots(self):
        self.scenario("invalid-inputs")


if __name__ == "__main__":
    unittest.main(verbosity=2)
