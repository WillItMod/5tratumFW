#!/usr/bin/env python3
"""Gamma presents the same canonical measured progress as the Nerd families."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
subprocess.run([sys.executable, str(root / 'firmware/nerdqaxe/test/host/test_autotune_presentation.py')], check=True)
