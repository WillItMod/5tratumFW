#!/usr/bin/env bash
set -euo pipefail

FW_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$FW_ROOT"
# A new SDK must never reuse an older CMake cache or rewrite the root sdkconfig.
FW_BUILD_DIR=$(python3 - "${FW_BUILD_DIR:-build/gamma-public}" <<'PY'
from pathlib import Path
import sys
root = Path.cwd().resolve()
candidate = Path(sys.argv[1])
if candidate.is_symlink():
    raise SystemExit('FW_BUILD_DIR must not be a symlink')
build = candidate.resolve()
try:
    relative = build.relative_to(root)
except ValueError:
    raise SystemExit('FW_BUILD_DIR must be inside this checkout for the container build')
if not relative.parts or build.exists() or build.is_symlink():
    raise SystemExit('FW_BUILD_DIR must name a new directory; select another path instead of reusing a build')
print(relative.as_posix())
PY
)
mkdir -p "$(dirname "$FW_BUILD_DIR")"
mkdir "$FW_BUILD_DIR"
IDF_IMAGE=espressif/idf:v6.0.2@sha256:0d8c9773d48a327233f9c1d7c654ff0bcf133ae24503ea2e97a57cfe02b8cb67
mkdir -p artifacts

if [[ -e .git ]]; then
    git submodule update --init --recursive
fi
node -e 'if (process.version !== "v24.14.0") { throw new Error("Use pinned Node v24.14.0, matching upstream release CI") }'
(
    cd main/http_server/axe-os
    npm ci --no-audit --no-fund
    npm run build
    npm run test:ci
)
python3 tools/check_web_budget.py --report "$FW_BUILD_DIR/web-budget.json"
python3 tools/test_gamma_host.py
python3 tools/test_ca_bundle.py

if [[ -e .git ]]; then
    git rev-parse HEAD > "$FW_BUILD_DIR/build-source-commit.txt"
fi
docker run --rm -e GITHUB_ACTIONS=true -e "FW_BUILD_DIR=$FW_BUILD_DIR" -e "FW_IDF_IMAGE=$IDF_IMAGE" \
    --mount "type=bind,source=$FW_ROOT,target=/project" -w /project \
    "$IDF_IMAGE" \
    bash -euc 'git config --global --add safe.directory /project
test "$(idf.py --version)" = "ESP-IDF v6.0.2"
idf.py -B "$FW_BUILD_DIR" -D SDKCONFIG="/project/$FW_BUILD_DIR/sdkconfig" -D IDF_TARGET=esp32s3 build
python tools/test_ca_bundle.py --build-dir "$FW_BUILD_DIR"
python - "$FW_BUILD_DIR" <<'"'"'PY'"'"'
import json, os, subprocess, sys
from pathlib import Path
source = Path(os.environ["IDF_PATH"])
receipt = {"idf_version": "v6.0.2", "container_image": os.environ["FW_IDF_IMAGE"],
           "idf_source": {"repository": "https://github.com/espressif/esp-idf",
                          "commit": subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()}}
(Path(sys.argv[1]) / "build-toolchain.json").write_text(json.dumps(receipt, indent=2) + "\n")
PY'

python3 test/host/render_oled_native.py
python3 tools/package_5tratumfw.py --build-dir "$FW_BUILD_DIR"
