#!/usr/bin/env python3
"""Real boot history/startup gate/idle-brand policy with host sanitizers."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class BootStatusTests(unittest.TestCase):
    def test_actual_history_bounds_startup_order_and_brand_safety_guards(self):
        with tempfile.TemporaryDirectory(prefix="5tratum-boot-status-") as tmp:
            binary = Path(tmp) / "boot-status"
            subprocess.run([
                shutil.which("clang++") or shutil.which("g++"), "-std=c++17",
                "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                "-fno-omit-frame-pointer", "-I", str(ROOT / "main"),
                str(ROOT / "test/host/boot_status_harness.cpp"), "-o", str(binary),
            ], check=True)
            subprocess.run([str(binary)], check=True)

if __name__ == "__main__":
    unittest.main(verbosity=2)
