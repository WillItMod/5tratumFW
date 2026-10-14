#!/usr/bin/env python3
"""Regenerate both native display formats from the approved donut originals."""
from pathlib import Path
import importlib.util

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "firmware/nerdqaxe/tools/generate_candidate_assets.py"

if __name__ == "__main__":
    spec = importlib.util.spec_from_file_location("candidate_assets", SOURCE)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.main(ROOT)
