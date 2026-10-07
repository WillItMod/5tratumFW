#!/usr/bin/env python3
"""Display data tests only; no device, ASIC dispatch or network operations."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class DisplayDataTests(unittest.TestCase):
    def test_unavailable_cached_values_units_mux_and_measured_rate_freshness(self):
        with tempfile.TemporaryDirectory(prefix="5tratum-display-data-") as tmp:
            binary = Path(tmp) / "display-data"
            subprocess.run([
                shutil.which("clang++") or shutil.which("g++"), "-std=c++17",
                "-Wall", "-Wextra", "-Werror", "-pthread", "-fsanitize=address,undefined",
                "-fno-omit-frame-pointer", "-I", str(ROOT / "main"),
                str(ROOT / "test/host/display_data_harness.cpp"), "-o", str(binary)
            ], check=True)
            subprocess.run([str(binary)], check=True)

if __name__ == "__main__":
    unittest.main(verbosity=2)
