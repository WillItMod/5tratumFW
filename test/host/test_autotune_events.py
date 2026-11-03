#!/usr/bin/env python3
"""Gamma and Nerd use the same measured decision recorder."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
subprocess.run([sys.executable, str(root / 'firmware/nerdqaxe/test/host/test_autotune_events.py')], check=True)
