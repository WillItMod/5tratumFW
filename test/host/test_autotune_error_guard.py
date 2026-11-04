#!/usr/bin/env python3
"""Run the shared fast-error regression for the Gamma host/CI entrypoint."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
subprocess.run([sys.executable, str(root / 'firmware/nerdqaxe/test/host/test_autotune_error_guard.py')], check=True)
