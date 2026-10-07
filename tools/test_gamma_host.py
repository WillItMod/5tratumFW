#!/usr/bin/env python3
"""Run every Gamma host regression entry point, including plain script tests."""

from pathlib import Path
import subprocess
import sys


def main():
    root = Path(__file__).resolve().parents[1]
    directory = root / "test/host"
    boot = directory / "run_gamma_boot_tests.py"
    scripts = [boot, *sorted(directory.glob("test_*.py"))]
    if not boot.is_file() or len(scripts) < 2:
        raise SystemExit("Expected Gamma boot tests and test/host/test_*.py scripts")
    for script in scripts:
        if script.is_symlink() or not script.is_file():
            raise SystemExit(f"Expected a regular host test script: {script}")
        print(f"Running {script.relative_to(root)}", flush=True)
        result = subprocess.run([sys.executable, str(script)], cwd=root)
        if result.returncode:
            return result.returncode if result.returncode > 0 else 1
    print(f"PASS: {len(scripts)} Gamma host test entry points", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
