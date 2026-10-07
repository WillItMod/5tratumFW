#!/usr/bin/env python3
"""Compile actual monitor methods against mocked RTOS/hardware boundaries."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
compiler = shutil.which('clang++') or shutil.which('g++')
if not compiler:
    raise RuntimeError('A C++17 host compiler is required')
with tempfile.TemporaryDirectory(prefix='5tratum-counter-monitor-') as directory:
    path = Path(directory)
    (path / 'freertos').mkdir()
    (path / 'freertos/FreeRTOS.h').write_text('''#pragma once
#include <cstdint>
using BaseType_t = int;
using UBaseType_t = unsigned;
using TickType_t = uint64_t;
constexpr int pdPASS = 1;
#define pdMS_TO_TICKS(ms) (ms)
''')
    (path / 'freertos/task.h').write_text('''#pragma once
#include "FreeRTOS.h"
using TaskFunction_t = void (*)(void *);
using TaskHandle_t = void *;
TickType_t xTaskGetTickCount();
void vTaskDelay(TickType_t);
void vTaskDelayUntil(TickType_t *, TickType_t);
void vTaskSuspend(TaskHandle_t);
''')
    (path / 'esp_timer.h').write_text('#pragma once\n#include <cstdint>\nint64_t esp_timer_get_time();\n')
    source = (root / 'main/tasks/hashrate_monitor_task.cpp').read_text()
    # Replace platform include boundaries, leaving every method body intact.
    source = re.sub(r'^#include "[^"\n]+"\n', '', source, flags=re.M)
    (path / 'monitor.cpp').write_text('#include "hashrate_monitor_support.h"\n' + source)
    binary = path / 'monitor'
    subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-pthread',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    '-I', str(path), '-I', str(root / 'main/tasks'),
                    '-I', str(root / 'test/host'), str(path / 'monitor.cpp'),
                    str(root / 'test/host/hashrate_monitor_harness.cpp'),
                    '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
