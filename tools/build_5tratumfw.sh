#!/usr/bin/env bash
set -euo pipefail

FW_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$FW_ROOT"
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
python3 tools/check_web_budget.py --report artifacts/web-budget.json
python3 tools/test_gamma_host.py

mkdir -p build
if [[ -e .git ]]; then
    git rev-parse HEAD > build/build-source-commit.txt
fi
docker run --rm -e GITHUB_ACTIONS=true \
    --mount "type=bind,source=$FW_ROOT,target=/project" -w /project \
    espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805 \
    bash -c 'git config --global --add safe.directory /project
idf.py -D IDF_TARGET=esp32s3 build'

python3 test/host/render_oled_native.py
python3 tools/package_5tratumfw.py
