#!/usr/bin/env python3
"""Execute the production Nerd tuning runtime and config transaction with mocked I/O.

The real controller, JSON parser, backup validation, state ownership, persistence
and HTTP handlers run under sanitizers. No hardware or network is accessed.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def method(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'

with tempfile.TemporaryDirectory(prefix='5tfw-autotune-runtime-') as directory:
    path = Path(directory)
    for name in ('esp_cpu.h','esp_memory_utils.h'):
        (path/name).write_text('#pragma once\n')
    config = (ROOT / 'main/nvs_config.cpp').read_text()
    defines = '\n'.join(re.findall(r'^#define NVS_CONFIG_\w+\s+"[^"\n]+"', (ROOT/'main/nvs_config.h').read_text(), re.M))
    keys = config[config.index('static const char *const tuningKeys[]'):config.index('static bool isTuningKey(')]
    pieces = '\n'.join(method(config, signature) for signature in (
        'static bool isTuningKey(', 'uint64_t tuningMutationRevision(',
        'bool captureTuningSettings(', 'bool validTuningSettings(', 'bool restoreTuningSettings(',
        'void cfgSetU16('))
    (path/'config_actual.inc').write_text(defines + '\nnamespace Config {\n' + keys + pieces + '\n}\n')
    runtime = (ROOT / 'main/autotune/runtime.cpp').read_text()
    runtime = re.sub(r'^#include[^\n]*\n', '', runtime, flags=re.M)
    (path/'runtime_actual.inc').write_text(runtime)
    source = '#include "autotune_nerd_support.h"\n#include "config_actual.inc"\n'
    source += method((ROOT/'main/boards/drivers/TPS53647.cpp').read_text(),'bool TPS53647::setTuningVoltage(')
    source += method((ROOT/'main/boards/nerdqaxeplus.cpp').read_text(),'bool NerdQaxePlus::restoreTuningFanPolarity(').replace('NerdQaxePlus::','Board::')
    source += '#include "runtime_actual.inc"\n'
    source += method((ROOT/'components/bm1397/asic.cpp').read_text(),'void Asic::send(')
    source += method((ROOT/'components/bm1397/asic.cpp').read_text(),'bool Asic::recoveryDelay(')
    source += (ROOT/'test/host/autotune_nerd_harness.cpp').read_text()
    (path/'test.cpp').write_text(source)
    binary = path/'test'
    subprocess.run([os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++'), '-std=c++17','-Wall','-Wextra','-Werror','-pthread',
        '-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-Wno-vla-cxx-extension','-I',str(path),'-I',str(ROOT/'test/host'),
        '-I',str(ROOT/'components/arduinojson'),'-I',str(ROOT/'main'),str(path/'test.cpp'),'-o',str(binary)],check=True)
    for scenario in ('qa','oct','waiver','backup-write-failure','backup-readback-failure','backup-identity','backup-settings',
                     'pending-manual','manual-handoff','ota-handoff','restore','restore-reboot','restore-fault',
                     'restore-storage-failure','restore-down','configure-pending','invalid-config','sensor-failure','voltage-failure',
                     'pending-headroom','readback-timeout','start-readback','weak-chip-qa','weak-chip-oct','configure-owned-bounds','restore-hot','restored-status-age','wire-revocation','backup-invalid-initial','backup-cached-deleted','backup-cached-changed','uart-revocation','restore-worker-failure','restore-lock-order','ota-freeze','backup-slow','pending-manual-cache','restore-disabled-protection','polarity-revocation','restore-polarity-failure','backup-revocation','fan-read-failure','pll-strict-sensor','pll-strict-fan','pll-strict-headroom','pll-long-healthy','pll-long-failure','json-points','search-display','storage-bootstrap-context','boot-present-records','boot-missing-records','boot-corrupt-options','boot-corrupt-baseline','boot-read-failure','boot-memory-failure','wrong-context-actions','worker-low-before','worker-low-after-commit','worker-wrong-context'):
        subprocess.run([str(binary), scenario], check=True)
        print(f'Internal-stack/bounded runtime scenario passed: {scenario}',flush=True)
