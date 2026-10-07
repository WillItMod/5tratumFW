#!/usr/bin/env python3
"""Check production rate metadata without ESP-IDF, hardware, or network calls."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HOST = Path(__file__).resolve().parent


class HashrateDisplayTests(unittest.TestCase):
    def test_production_counter_and_snapshot_freshness(self):
        with tempfile.TemporaryDirectory(prefix="5tratumfw-hashrate-") as temporary:
            directory = Path(temporary)
            (directory / "esp_err.h").write_text("typedef int esp_err_t;\n")
            for name in ("esp_heap_caps.h", "esp_log.h", "esp_timer.h", "asic.h", "utils.h"):
                (directory / name).write_text('#include "hashrate_runtime_stubs.h"\n')
            binary = directory / "hashrate-test"
            command = shlex.split(os.environ.get("CC", "cc")) + [
                "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare",
                "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                "-I", str(directory), "-I", str(HOST), "-I", str(ROOT / "main"),
                "-I", str(ROOT / "components/asic/include"),
                "-include", str(HOST / "hashrate_runtime_stubs.h"),
                str(HOST / "hashrate_runtime_test.c"), "-pthread", "-lm", "-o", str(binary),
            ]
            built = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stderr)
            env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:halt_on_error=1",
                       UBSAN_OPTIONS="halt_on_error=1")
            tested = subprocess.run([str(binary)], capture_output=True, text=True, env=env)
            self.assertEqual(tested.returncode, 0, tested.stderr or tested.stdout)
            self.assertIn("PASS: production counter metadata", tested.stdout)


if __name__ == "__main__":
    unittest.main()
