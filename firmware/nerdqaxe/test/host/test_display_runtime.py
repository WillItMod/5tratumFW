#!/usr/bin/env python3
"""Run production boot-latched profiles/metadata with real ArduinoJson.

No network, hardware, settings writes or panel qualification is performed.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class DisplayRuntimeTests(unittest.TestCase):
    def test_actual_profile_selection_latch_rotation_inversion_and_metadata(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            raise RuntimeError("Runtime LCD tests require a C++17 compiler")
        with tempfile.TemporaryDirectory(prefix="display-runtime-") as temporary:
            for board in ("NERDQAXEPLUS2", "NERDOCTAXEGAMMA"):
                with self.subTest(board=board):
                    binary = Path(temporary) / board
                    subprocess.run([
                        compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        f"-D{board}", "-I", str(ROOT / "main/displays"),
                        "-I", str(ROOT / "components/arduinojson"),
                        str(ROOT / "test/host/display_runtime_harness.cpp"),
                        "-o", str(binary),
                    ], check=True)
                    environment = os.environ.copy()
                    environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
                    subprocess.run([str(binary)], check=True, env=environment)


if __name__ == "__main__":
    unittest.main(verbosity=2)
