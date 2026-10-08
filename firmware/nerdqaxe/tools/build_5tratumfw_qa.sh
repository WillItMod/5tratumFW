#!/usr/bin/env bash
# Reproducible model-specific paired OTA build. Never connects to or flashes a miner.
set -euo pipefail

QA_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$QA_ROOT"
QA_IDF_IMAGE='espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805'
QA_BUILD_REL=${QA_BUILD_DIR:-build/qa-public}
QA_TEST_PYTHON=${QA_TEST_PYTHON:-python3}
if [[ -n "${BOARD:-}" && "$BOARD" != NERDQAXEPLUS2 ]]; then
  echo 'This public helper supports BOARD=NERDQAXEPLUS2 only.' >&2
  exit 1
fi
export BOARD=NERDQAXEPLUS2
node -e 'if (process.version !== "v24.14.0") throw new Error("Use pinned Node v24.14.0")'

QA_VERSION=$(python3 - <<'PY'
from pathlib import Path
import re
v=Path('version.txt').read_text().strip()
s=Path('main/http_server/axe-os/src/app/firmware-web-version.ts').read_text()
m=re.search(r"export\s+const\s+FIRMWARE_WEB_VERSION\s*=\s*'([^']+)'\s*;",s)
if not re.fullmatch(r'5tratumFW-qa-\d+\.\d+\.\d+-beta\.\d+',v) or len(v.encode('ascii'))>31 or not m or m.group(1)!=v:
 raise SystemExit('version.txt and FIRMWARE_WEB_VERSION must use the same QAxe-specific BETA version')
print(v)
PY
)
QA_BUILD_ABS=$(python3 - "$QA_BUILD_REL" <<'PY'
from pathlib import Path
import sys
root=Path.cwd(); relative=Path(sys.argv[1])
if relative.is_absolute() or '..' in relative.parts:
 raise SystemExit('QA_BUILD_DIR must be a relative path under build/')
p=(root/relative).resolve()
p.relative_to(root/'build')
if p.exists() and any(p.iterdir()):
 raise SystemExit('Use a new empty build directory under build/; existing build/configuration is never erased')
print(p)
PY
)
if git rev-parse --show-toplevel >/dev/null 2>&1; then
  git diff --quiet --ignore-submodules=none HEAD -- .
  # This scoped update is a dependency checkout operation, not firmware installation.
  git submodule update --init --recursive
fi
# Validate the reviewed source inventory before building; arbitrary untracked files never enter the package.
QA_SOURCE_COMMIT=$(python3 - <<'PY'
import importlib.util
from pathlib import Path
import sys
p=Path('tools/package_5tratumfw_qa.py')
sys.path.insert(0,str(p.parent.resolve()))
s=importlib.util.spec_from_file_location('qa_package',p); m=importlib.util.module_from_spec(s); s.loader.exec_module(m)
print(m.source_receipt()[0])
PY
)
(
  cd main/http_server/axe-os
  npm ci --no-audit --no-fund
  npm run build
  npm test -- --watch=false --browsers=ChromeHeadless --progress=false
)
node tools/test_web_controls.mjs
python3 tools/package_5tratumfw_qa.py --record-web-build
# Host regressions execute production code through bounded fake hardware/RTOS boundaries.
"$QA_TEST_PYTHON" - <<'PY'
from pathlib import Path
import subprocess
import sys
for script in sorted(Path('test/host').glob('test_*.py')):
 subprocess.run([sys.executable,str(script)],check=True)
PY
mkdir -p "$QA_BUILD_ABS"
python3 - "$QA_BUILD_ABS/build-source-receipt.json" "$QA_SOURCE_COMMIT" "$QA_VERSION" "$QA_IDF_IMAGE" <<'PY'
import json
from pathlib import Path
import sys
path,commit,version,image=sys.argv[1:]
Path(path).write_text(json.dumps({'sourceCommit':commit,'boardProfile':'NERDQAXEPLUS2','version':version,'idfImage':image},sort_keys=True)+'\n')
PY
QA_DEFAULT_CONFIG=/project/sdkconfig.defaults
if [[ -f BUILD_CONFIG/sdkconfig ]]; then
  # The corresponding source archive includes the exact generated build configuration.
  QA_DEFAULT_CONFIG=/project/BUILD_CONFIG/sdkconfig
fi
docker run --rm -e BOARD=NERDQAXEPLUS2 \
  --mount "type=bind,source=$QA_ROOT,target=/project" -w /project \
  "$QA_IDF_IMAGE" \
  bash -c 'git config --global --add safe.directory /project
    idf.py -B "$1" -D IDF_TARGET=esp32s3 -D SDKCONFIG="$1/sdkconfig" \
      -D SDKCONFIG_DEFAULTS="$3" -D BUILD_WEB=OFF -D PROJECT_VER="$2" \
      -D FIVETRATUM_RELEASE_PROFILE=NERDQAXEPLUS2 \
      -D FIVETRATUM_ASIC_CAPTURE_LOGS=OFF \
      -D FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER=OFF \
      -D FIVETRATUM_BM1370_CAPTURE=OFF build' \
  qa-build "/project/$QA_BUILD_REL" "$QA_VERSION" "$QA_DEFAULT_CONFIG"
"$QA_TEST_PYTHON" test/host/render_display_native.py
python3 tools/package_5tratumfw_qa.py --build-dir "$QA_BUILD_REL"
