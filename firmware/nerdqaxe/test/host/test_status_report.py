#!/usr/bin/env python3
"""Exercise the firmware's live-status serializer without device access."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class LiveStatusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="5tratum-status-")
        cls.binary = Path(cls.tmp.name) / "status-report"
        subprocess.run([
            shutil.which("clang++") or shutil.which("g++"), "-std=c++17",
            "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
            "-fno-omit-frame-pointer", "-I", str(ROOT / "components/arduinojson"),
            "-I", str(ROOT / "main"), "-I", str(ROOT / "main/http_server"),
            str(ROOT / "main/http_server/status_report.cpp"),
            str(ROOT / "test/host/status_report_harness.cpp"), "-o", str(cls.binary),
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def report(self, scenario="fresh"):
        run = subprocess.run([str(self.binary), scenario], check=True,
                             capture_output=True, text=True)
        self.assertLessEqual(len(run.stdout.encode()), 24576)
        return json.loads(run.stdout)

    def test_only_actual_fresh_counter_estimates_are_numeric(self):
        d = self.report()
        self.assertEqual([a["hashRateGHs"] for a in d["asics"]], [1000, 1001, 1002, 1003])
        self.assertTrue(all(a["fresh"] for a in d["asics"]))
        self.assertEqual([a["sampleAgeSeconds"] for a in d["asics"]], [5] * 4)
        self.assertFalse(d["work"]["independentAssignment"])
        self.assertEqual(d["work"]["scope"], "chain-broadcast")
        self.assertEqual(d["identity"]["deviceId"], "5tfw:0102030405060708090a0b0c0d0e0f10")
        self.assertEqual(d["hardware"], {"boardModel": "NerdQAxe++", "asicModel": "BM1370", "asicCount": 4})

    def test_measured_zero_is_preserved_and_boundary_is_fresh(self):
        self.assertEqual(self.report("zero")["asics"][0]["hashRateGHs"], 0)
        self.assertTrue(self.report("boundary")["asics"][0]["fresh"])

    def test_real_octaxe_model_retains_all_eight_measured_counters(self):
        d = self.report("octaxe")
        self.assertEqual(d["hardware"], {"boardModel": "NerdOCTAXE-γ", "asicModel": "BM1370", "asicCount": 8})
        self.assertEqual([a["hashRateGHs"] for a in d["asics"]], list(range(1000, 1008)))
        self.assertFalse(d["work"]["independentAssignment"])

    def test_exact_octaxe_exception_does_not_admit_other_unicode_or_field_types(self):
        for scenario in ("octaxe-suffix", "octaxe-truncated", "octaxe-other-unicode",
                         "octaxe-control", "octaxe-version", "octaxe-asic"):
            with self.subTest(scenario=scenario):
                self.assertFalse(self.report(scenario)["built"])

    def test_missing_stale_future_and_invalid_values_are_not_measurements(self):
        d = self.report("unavailable")
        self.assertTrue(all(a["hashRateGHs"] is None and not a["fresh"] for a in d["asics"]))
        self.assertEqual([a["temperatureC"] for a in d["asics"]], [None, None, None, 53])
        self.assertIsNone(d["asics"][2]["sampleAgeSeconds"])

    def test_a_tcp_connection_does_not_identify_mux_or_coin(self):
        d = self.report("tcp-only")
        self.assertTrue(d["mux"]["pools"][0]["transportConnected"])
        self.assertFalse(d["mux"]["pools"][0]["connected"])
        self.assertIsNone(d["coin"]["id"])

    def test_explicit_fresh_route_metadata_and_mixed_pool_ambiguity(self):
        self.assertEqual(self.report("known")["coin"]["id"], "bitcoin")
        mixed = self.report("mixed")
        self.assertIsNone(mixed["coin"]["id"])
        self.assertIsNone(mixed["work"]["activePool"])
        self.assertEqual([p["coin"]["ticker"] for p in mixed["mux"]["pools"]], ["BTC", "DGB"])

    def test_expired_peer_never_retains_coin_information(self):
        d = self.report("expired")
        self.assertTrue(d["mux"]["pools"][0]["expired"])
        self.assertIsNone(d["mux"]["pools"][0]["coin"]["id"])
        self.assertIsNone(d["coin"]["id"])

    def test_maximum_chip_count_is_bounded_and_bad_inputs_fail_cleanly(self):
        self.assertEqual(len(self.report("max")["asics"]), 64)
        for scenario in ("invalid-count", "invalid-version", "invalid-identity", "oom"):
            self.assertFalse(self.report(scenario)["built"])

    def test_work_context_preserves_exact_job_nullable_fields_and_report_strings(self):
        d = self.report("work")
        self.assertEqual(d["workContext"], {
            "jobId": "dgb-job", "height": 20000000, "nBits": "1b0404cb",
            "networkDifficulty": d["workContext"]["networkDifficulty"], "ageSeconds": 89,
            "source": "forwarded-stratum-job",
        })
        self.assertAlmostEqual(d["workContext"]["networkDifficulty"], 16307.420938523983, places=5)
        self.assertEqual(d["mux"]["pools"][0]["workContext"], d["workContext"])
        self.assertIsNone(d["mux"]["pools"][1]["workContext"])
        nullable = self.report("work-nullable")["workContext"]
        self.assertIsNone(nullable["height"])
        self.assertIsNone(nullable["networkDifficulty"])
        self.assertEqual(nullable["nBits"], "1b0404cb")

    def test_work_context_does_not_combine_dual_and_uses_actual_fallback(self):
        d = self.report("work-dual")
        self.assertIsNone(d["workContext"])
        self.assertEqual([p["workContext"]["jobId"] for p in d["mux"]["pools"]], ["dgb-job", "btc-job"])
        fallback = self.report("work-fallback")
        self.assertEqual(fallback["workContext"]["jobId"], "btc-job")
        self.assertEqual(fallback["coin"]["ticker"], "BTC")
        self.assertEqual(fallback["work"]["activePool"], 1)

    def test_expired_and_legacy_status_never_report_old_work(self):
        self.assertIsNone(self.report("work-expired")["workContext"])
        self.assertIsNone(self.report("work-expired")["mux"]["pools"][0]["workContext"])
        self.assertIsNone(self.report("known")["workContext"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
