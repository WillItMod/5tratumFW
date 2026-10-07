#!/usr/bin/env python3
"""Exercise the firmware's chip-counter availability/age validator, no device I/O."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
compiler = shutil.which('clang++') or shutil.which('g++')
if not compiler:
    raise RuntimeError('A C++17 host compiler is required')
with tempfile.TemporaryDirectory(prefix='5tratum-chip-rate-') as directory:
    binary = Path(directory) / 'chip-rate'
    subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    '-I', str(root / 'main/tasks'),
                    str(root / 'test/host/chip_hashrate_sample_harness.cpp'),
                    '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
