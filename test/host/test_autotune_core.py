#!/usr/bin/env python3
"""Gamma uses the same canonical core shipped in the standalone Nerd source."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
subprocess.run([sys.executable, str(root / 'firmware/nerdqaxe/test/host/test_autotune_core.py')], check=True)
