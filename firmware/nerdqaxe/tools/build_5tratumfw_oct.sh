#!/usr/bin/env bash
# Reproducible model-specific paired OTA build. Never connects to or flashes a miner.
set -euo pipefail

OCT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$OCT_ROOT"
OCT_IDF_IMAGE='espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805'
OCT_BUILD_REL=${OCT_BUILD_DIR:-build/oct-public}
OCT_TEST_PYTHON=${OCT_TEST_PYTHON:-python3}
if [[ -n "${BOARD:-}" && "$BOARD" != NERDOCTAXEGAMMA ]]; then
  echo 'This public helper supports BOARD=NERDOCTAXEGAMMA only.' >&2
  exit 1
fi
export BOARD=NERDOCTAXEGAMMA
node -e 'if (process.version !== "v24.14.0") throw new Error("Use pinned Node v24.14.0")'

OCT_VERSION=$(python3 - <<'PY'
from pathlib import Path
import re
v=Path('version-oct.txt').read_text().strip()
s=Path('main/http_server/axe-os/src/app/firmware-web-version.oct.ts').read_text()
m=re.search(r"export\s+const\s+FIRMWARE_WEB_VERSION\s*=\s*'([^']+)'\s*;",s)
if not re.fullmatch(r'5tratumFW-oct-\d+\.\d+\.\d+-beta\.\d+',v) or len(v.encode('ascii'))>31 or not m or m.group(1)!=v:
 raise SystemExit('version-oct.txt and its FIRMWARE_WEB_VERSION must use the same OctAxe-specific BETA version')
print(v)
PY
)
OCT_BUILD_ABS=$(python3 - "$OCT_BUILD_REL" <<'PY'
from pathlib import Path
import sys
root=Path.cwd(); relative=Path(sys.argv[1])
if relative.is_absolute() or '..' in relative.parts:
 raise SystemExit('OCT_BUILD_DIR must be a relative path under build/')
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
OCT_SOURCE_COMMIT=$(python3 - <<'PY'
import importlib.util
from pathlib import Path
import sys
p=Path('tools/package_5tratumfw_qa.py')
sys.path.insert(0,str(p.parent.resolve()))
s=importlib.util.spec_from_file_location('qa_package',p); m=importlib.util.module_from_spec(s); s.loader.exec_module(m)
print(m.source_receipt(m.OCT)[0])
PY
)
(
  cd main/http_server/axe-os
  npm ci --no-audit --no-fund
  npm run build:oct
  npm test -- --watch=false --browsers=ChromeHeadless --progress=false
)
node tools/test_web_controls.mjs
python3 tools/package_5tratumfw_oct.py --record-web-build
# Host regressions execute production code through bounded fake hardware/RTOS boundaries.
"$OCT_TEST_PYTHON" - <<'PY'
from pathlib import Path
import subprocess
import sys
for script in sorted(Path('test/host').glob('test_*.py')):
 subprocess.run([sys.executable,str(script)],check=True)
PY
mkdir -p "$OCT_BUILD_ABS"
python3 - "$OCT_BUILD_ABS/build-source-receipt.json" "$OCT_SOURCE_COMMIT" "$OCT_VERSION" "$OCT_IDF_IMAGE" <<'PY'
import json
from pathlib import Path
import sys
path,commit,version,image=sys.argv[1:]
Path(path).write_text(json.dumps({'sourceCommit':commit,'boardProfile':'NERDOCTAXEGAMMA','version':version,'idfImage':image},sort_keys=True)+'\n')
PY
OCT_DEFAULT_CONFIG=/project/sdkconfig.defaults
if [[ -f BUILD_CONFIG/sdkconfig ]]; then
  # The corresponding source archive includes the exact generated build configuration.
  OCT_DEFAULT_CONFIG=/project/BUILD_CONFIG/sdkconfig
fi
docker run --rm -e BOARD=NERDOCTAXEGAMMA \
  --mount "type=bind,source=$OCT_ROOT,target=/project" -w /project \
  "$OCT_IDF_IMAGE" \
  bash -c 'git config --global --add safe.directory /project
    idf.py -B "$1" -D IDF_TARGET=esp32s3 -D SDKCONFIG="$1/sdkconfig" \
      -D SDKCONFIG_DEFAULTS="$3" -D BUILD_WEB=OFF -D PROJECT_VER="$2" \
      -D FIVETRATUM_RELEASE_PROFILE=NERDOCTAXEGAMMA \
      -D FIVETRATUM_ASIC_CAPTURE_LOGS=OFF \
      -D FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER=OFF \
      -D FIVETRATUM_BM1370_CAPTURE=OFF build' \
  oct-build "/project/$OCT_BUILD_REL" "$OCT_VERSION" "$OCT_DEFAULT_CONFIG"
"$OCT_TEST_PYTHON" test/host/render_display_native.py
python3 tools/package_5tratumfw_oct.py --build-dir "$OCT_BUILD_REL"
