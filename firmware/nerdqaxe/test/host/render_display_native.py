#!/usr/bin/env python3
"""Render actual LVGL layouts with test fixtures. Never connects to a miner.

Requires the repository's pinned managed LVGL component and a host CMake/C++
compiler. Pillow is optional: without it, exact framebuffer PPMs still render.
Only the host allocator is substituted; fonts, color depth and layout are the
firmware sources. Outputs are explicitly simulated and not physical evidence.
"""
from pathlib import Path
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / ".cache/display-native"
OUTPUT = BUILD / "output"

def run():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    flags = "-fsanitize=address,undefined -fno-omit-frame-pointer"
    with (BUILD / "configure.log").open("w") as log:
        subprocess.run([
            "cmake", "-S", str(ROOT / "test/host/display_native"), "-B", str(BUILD),
            "-DCMAKE_BUILD_TYPE=Debug", f"-DCMAKE_C_FLAGS={flags}",
            f"-DCMAKE_CXX_FLAGS={flags}", "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined",
        ], check=True, stdout=log, stderr=subprocess.STDOUT)
    with (BUILD / "build.log").open("w") as log:
        subprocess.run(["cmake", "--build", str(BUILD), "--target", "display-native", "-j", "6"],
                       check=True, stdout=log, stderr=subprocess.STDOUT)
    result = subprocess.run([str(BUILD / "display-native"), str(OUTPUT)],
                            check=True, text=True, capture_output=True)
    report = json.loads(result.stdout)
    report["images"] = [str(path) for path in sorted(OUTPUT.glob("*.ppm"))]
    try:
        from PIL import Image
        for image in OUTPUT.glob("*.ppm"):
            # Encoding the native framebuffer unchanged; no mockup/resizing.
            Image.open(image).save(image.with_suffix(".png"))
        report["pngImages"] = [str(path) for path in sorted(OUTPUT.glob("*.png"))]
    except ImportError:
        report["pngImages"] = []
    report["scope"] = "Host LVGL layout and formatting only; not hardware display validation"
    (OUTPUT / "display-native-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))

if __name__ == "__main__":
    try:
        run()
    except subprocess.CalledProcessError as error:
        print(error.stderr or str(error), file=sys.stderr)
        raise
