#!/usr/bin/env python3
"""Verify a two-frame fixture from the actual MUX sender; no miner access."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--expect-coin", required=True)
    parser.add_argument("--expect-height", type=int, required=True)
    parser.add_argument("--expect-difficulty", type=float, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="5tratum-work-wire-") as tmp:
        binary = Path(tmp) / "work-wire"
        subprocess.run([
            shutil.which("clang++") or shutil.which("g++"), "-std=c++17",
            "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
            "-fno-omit-frame-pointer", "-I", str(ROOT / "components/arduinojson"),
            "-I", str(ROOT / "main"), "-I", str(ROOT / "main/http_server"),
            str(ROOT / "main/http_server/status_report.cpp"),
            str(ROOT / "test/host/work_context_wire_harness.cpp"), "-o", str(binary),
        ], check=True)
        result = subprocess.run([str(binary), str(args.fixture.resolve())],
                                check=True, capture_output=True, text=True)
    report = json.loads(result.stdout)
    assert report["coin"]["ticker"] == args.expect_coin
    assert report["workContext"]["height"] == args.expect_height
    assert abs(report["workContext"]["networkDifficulty"] - args.expect_difficulty) <= max(1e-9, args.expect_difficulty * 1e-9)
    assert report["workContext"] == report["mux"]["pools"][0]["workContext"]
    assert report["mux"]["pools"][1]["workContext"] is None
    assert not report["work"]["independentAssignment"]
    assert report["hostDisplayProof"]["fixtureOnly"]
    print(json.dumps({"passed": True, "scope": "Actual MUX sender fixture -> firmware parser/state/status/display formatters; no device I/O", "report": report}, indent=2))

if __name__ == "__main__":
    main()
