#!/usr/bin/env python3
"""Actual production control core; no USB, network or miner writes."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="5tratum-mining-control-") as directory:
    binary = Path(directory) / "control"
    subprocess.run([shutil.which("clang++") or shutil.which("g++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-pthread", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-I", str(ROOT / "main/tasks"),
                    str(ROOT / "test/host/mining_control_harness.cpp"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
