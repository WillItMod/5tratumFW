#!/usr/bin/env python3
"""Run the shared read-only progress projection in C and C++ under sanitizers."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='5tfw-autotune-presentation-') as directory:
    for language, names, standard in [('c', ('clang', 'gcc'), 'c11'), ('c++', ('clang++', 'g++'), 'c++17')]:
        compiler = next((shutil.which(name) for name in names if shutil.which(name)), None)
        if not compiler:
            raise RuntimeError(f'A {language} host compiler is required')
        binary = Path(directory) / ('presentation-cpp' if language == 'c++' else 'presentation-c')
        subprocess.run([compiler, '-x', language, '-std=' + standard, '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
                        '-I', str(root / 'main'), str(root / 'test/host/autotune_presentation_harness.c'),
                        '-lm', '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
