#!/usr/bin/env python3
"""Exercise the actual bounded capture recorder without hardware or a runtime."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
COMPONENT = ROOT / "components/bm1397"
OWNED = (
    "components/bm1397/include/bm1370_capture.h", "components/bm1397/bm1370_capture.cpp",
    "test/host/bm1370_capture_harness.cpp", "test/host/test_bm1370_capture.py")


class CaptureTests(unittest.TestCase):
    def test_actual_recorder_bounded_storage_freeze_and_loss_reporting(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        self.assertIsNotNone(compiler, "C++ compiler required")
        with tempfile.TemporaryDirectory(prefix="5tratum-bm1370-capture-") as directory:
            binary = Path(directory) / "capture-tests"
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-I", str(COMPONENT / "include"),
                str(COMPONENT / "bm1370_capture.cpp"), str(ROOT / "test/host/bm1370_capture_harness.cpp"),
                "-o", str(binary)], check=True)
            report = json.loads(subprocess.run([str(binary)], check=True, capture_output=True, text=True).stdout)
            self.assertEqual(report["groupsPassed"], 8)
            self.assertEqual(report["storageBytes"], 96 * 1024)
            self.assertEqual(report["allCapacitiesTested"], 384)
            self.assertFalse(report["heapAllocationAllowed"])
            self.assertFalse(report["physicalWireComplete"])
            self.assertFalse(report["physicalChipIdentity"])
            self.assertFalse(report["independentWorkAssignment"])
            proof = ROOT / ".cache/bm1370-capture"
            proof.mkdir(parents=True, exist_ok=True)
            report.update({"sanitizers": ["address", "undefined"], "externallySerialized": True,
                "firmwareIntegrationRequired": True, "callerOwnedStorage": True,
                "generationSource": "caller software context; not an on-wire tag",
                "ownedManifest": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in OWNED}})
            (proof / "proof.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    unittest.main()
