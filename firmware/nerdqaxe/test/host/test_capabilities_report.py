#!/usr/bin/env python3
"""Compile and exercise the actual bounded firmware JSON serializer, no device I/O."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class CapabilitiesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="5tratum-capabilities-")
        cls.binary = Path(cls.tmp.name) / "capabilities-report"
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            raise RuntimeError("A C++17 host compiler is required")
        subprocess.run([
            compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-I", str(ROOT / "components/arduinojson"),
            "-I", str(ROOT / "main/http_server"),
            "-I", str(ROOT / "main"),
            str(ROOT / "main/http_server/capabilities_report.cpp"),
            str(ROOT / "test/host/capabilities_report_harness.cpp"),
            "-o", str(cls.binary),
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    @classmethod
    def report(cls, scenario="stock"):
        result = subprocess.run([str(cls.binary), scenario], check=True, capture_output=True, text=True)
        if len(result.stdout.encode()) > 32768:
            raise AssertionError("Response exceeded discovery bound")
        return json.loads(result.stdout)

    def assert_children(self, report):
        chips = report["asics"]
        self.assertEqual(len(chips), report["hardware"]["asicCount"])
        self.assertEqual([c["asicIndex"] for c in chips], list(range(len(chips))))
        for chip in chips:
            self.assertEqual(chip["logicalMinerId"], f'{report["identity"]["deviceId"]}/asic/{chip["asicIndex"]}')
            self.assertEqual(chip["asicModel"], report["hardware"]["asicModel"])
            self.assertIsNone(chip["power"]["valueW"])

    def test_stock_four_descriptors_shared_controls_and_preserved_settings(self):
        report = self.report()
        self.assert_children(report)
        self.assertEqual(report["operatingPoint"]["configuredFrequencyMHz"], 500)
        self.assertEqual(report["operatingPoint"]["configuredCoreVoltageMv"], 1130)
        self.assertIsNone(report["operatingPoint"]["appliedFrequencyMHz"])
        self.assertIsNone(report["operatingPoint"]["appliedCoreVoltageMv"])
        self.assertEqual([x["chainAddress"] for x in report["asics"]], [0, 64, 128, 192])
        self.assertFalse(report["capabilities"]["independentWorkAssignment"])
        self.assertEqual(report["capabilities"]["workTargeting"], "chain-broadcast")
        for key in ("independentFrequencyControl", "independentVoltageControl", "perAsicPowerTelemetry"):
            self.assertFalse(report["capabilities"][key]["available"])
        self.assertEqual(report["capabilities"]["voltageControl"]["scope"], "shared-board")
        self.assertIsNone(report["hardware"]["pcbRevision"])

    def test_absent_saved_values_do_not_substitute_source_defaults(self):
        point = self.report("missing-settings")["operatingPoint"]
        self.assertEqual(point["source"], "unavailable")
        self.assertIsNone(point["configuredFrequencyMHz"])
        self.assertIsNone(point["configuredCoreVoltageMv"])

    def test_saved_values_outside_presets_are_not_clamped_or_replaced(self):
        point = self.report("saved-outside-presets")["operatingPoint"]
        self.assertEqual(point["configuredFrequencyMHz"], 725)
        self.assertEqual(point["configuredCoreVoltageMv"], 1199)

    def test_invalid_saved_values_are_null(self):
        point = self.report("invalid-settings")["operatingPoint"]
        self.assertIsNone(point["configuredFrequencyMHz"])
        self.assertIsNone(point["configuredCoreVoltageMv"])

    def test_zero_nonfinite_negative_temperatures_are_unavailable(self):
        report = self.report("mixed-temperatures")
        readings = [c["temperature"] for c in report["asics"]]
        self.assertEqual([x["valueC"] for x in readings], [None, None, None, 52.5])
        self.assertEqual(readings[0]["reason"], "stock-api-zero-is-not-valid")
        self.assertTrue(report["capabilities"]["perAsicTemperatureTelemetry"]["available"])

    def test_uninitialized_temperatures_stay_unavailable(self):
        report = self.report("uninitialized")
        self.assertTrue(all(c["temperature"]["valueC"] is None for c in report["asics"]))
        self.assertFalse(report["capabilities"]["perAsicTemperatureTelemetry"]["available"])

    def test_other_chip_counts_are_not_reported_as_four(self):
        report = self.report("octaxe")
        self.assert_children(report)
        self.assertEqual(report["hardware"]["boardModel"], "NerdOCTAXE-γ")
        self.assertEqual(report["hardware"]["boardProfile"], "NERDOCTAXEGAMMA")
        self.assertEqual(len(report["asics"]), 8)
        self.assertEqual([x["chainAddress"] for x in report["asics"]], list(range(0, 256, 32)))
        self.assertFalse(report["capabilities"]["independentWorkAssignment"])
        one = self.report("unknown-asic")
        self.assertEqual(len(one["asics"]), 1)
        self.assertFalse(one["capabilities"]["registerAddressing"]["available"])
        self.assertIsNone(one["asics"][0]["chainAddress"])

    def test_only_exact_known_octaxe_model_admits_the_gamma_character(self):
        for scenario in ("octaxe-suffix", "octaxe-truncated", "octaxe-other-unicode",
                         "octaxe-control", "octaxe-version", "octaxe-profile"):
            with self.subTest(scenario=scenario):
                self.assertEqual(self.report(scenario), {"built": False})

    def test_identity_is_bounded_deterministic_and_contains_no_raw_mac_or_endpoints(self):
        report = self.report()
        self.assertEqual(report["identity"]["deviceId"], "5tfw:0102030405060708090a0b0c0d0e0f10")
        self.assertEqual(report["identity"]["deviceId"], self.report()["identity"]["deviceId"])
        forbidden = {"macAddr", "mac", "ip", "hostip", "hostname", "stratumUser", "stratumPass", "password", "credentials", "endpoint"}
        def visit(value):
            if isinstance(value, dict):
                self.assertFalse(set(value) & forbidden)
                for child in value.values(): visit(child)
            elif isinstance(value, list):
                for child in value: visit(child)
        visit(report)
        self.assertFalse(self.report("zero-digest")["built"])

    def test_prototype_identity_distinct_from_upstream_product_version(self):
        report = self.report()
        self.assertEqual(report["firmware"]["product"], "ESP-Miner-NerdQAxePlus")
        self.assertEqual(report["firmware"]["version"], "v1.1.0-rc1-test1")
        self.assertIsNone(report["firmware"]["sourceRevision"])
        self.assertEqual(report["integration"]["status"], "capability-prototype")
        self.assertEqual(report["integration"]["extensionBuild"]["product"], "5tratumFW capability prototype")

    def test_invalid_count_label_identity_and_oom_fail_without_partial_report(self):
        for scenario in ("zero-count", "excessive-count", "invalid-label", "invalid-id", "oom"):
            with self.subTest(scenario=scenario):
                self.assertEqual(self.report(scenario), {"built": False})

    def test_maximum_count_cannot_emit_oversize_or_partial_report(self):
        report = self.report("max-count")
        self.assertEqual(report, {"built": False})

    def test_actual_strict_cached_integer_helper_rejects_coercion_without_mutating_cache(self):
        valid = self.report("strict-integer")
        self.assertEqual(valid, {"accepted": True, "value": 500, "cacheUnchanged": True})
        for suffix in ("true", "false", "string", "bad-string", "fraction", "float", "null", "negative", "overflow", "malformed", "missing"):
            with self.subTest(raw_type=suffix):
                self.assertEqual(self.report("strict-" + suffix), {"accepted": False, "value": 77, "cacheUnchanged": True})


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--emit", type=Path, help="Write the actual four-ASIC prototype report for schema/MUX review")
    args, unittest_args = parser.parse_known_args()
    if args.emit:
        CapabilitiesTests.setUpClass()
        try:
            args.emit.write_text(json.dumps(CapabilitiesTests.report(), indent=2) + "\n")
        finally:
            CapabilitiesTests.tearDownClass()
    else:
        unittest.main(argv=[sys.argv[0], *unittest_args])
