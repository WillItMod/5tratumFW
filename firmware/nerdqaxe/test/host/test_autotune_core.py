#!/usr/bin/env python3
"""Exercise the original shared C controller under C and C++ sanitizers."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='5tratum-autotune-core-') as directory:
    for language, names, standard in [('c', ('clang', 'gcc'), 'c11'), ('c++', ('clang++', 'g++'), 'c++17')]:
        compiler = next((shutil.which(name) for name in names if shutil.which(name)), None)
        if not compiler:
            raise RuntimeError(f'A {language} host compiler is required')
        binary = Path(directory) / ('controller-cpp' if language == 'c++' else 'controller-c')
        subprocess.run([compiler, '-x', language, '-std=' + standard, '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
                        '-I', str(root / 'main'), str(root / 'test/host/autotune_core_harness.c'),
                        '-lm', '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
