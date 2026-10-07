#!/usr/bin/env python3
"""Actual capture runtime + serial source with fake ESP/UART and real pthreads."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
COMPONENT = ROOT / "components/bm1397"


def headers(path):
    files = {
        "esp_timer.h": "#pragma once\n#include <cstdint>\nint64_t esp_timer_get_time();\n",
        "esp_heap_caps.h": "#pragma once\n#include <cstddef>\n#include <cstdint>\n#define MALLOC_CAP_SPIRAM 0x400\n#define MALLOC_CAP_8BIT 0x4\nvoid *heap_caps_malloc(std::size_t,uint32_t);\nvoid *heap_caps_aligned_alloc(std::size_t,std::size_t,uint32_t);\n",
        "freertos/FreeRTOS.h": "#pragma once\n#define pdMS_TO_TICKS(ms) (ms)\n",
        "freertos/task.h": "#pragma once\n#include \"FreeRTOS.h\"\n",
        "esp_log.h": "#pragma once\n#define ESP_LOGI(tag, ...) do { (void)(tag); } while(0)\n",
        "soc/uart_struct.h": "#pragma once\n",
        "asic.h": "#pragma once\n",
        "mining_utils.h": "#pragma once\n",
        "driver/uart.h": """#pragma once
#include <cstddef>
#include <cstdint>
using esp_err_t=int;
using uart_port_t=int;
constexpr int ESP_OK=0,UART_NUM_1=1,UART_DATA_8_BITS=8,UART_PARITY_DISABLE=0;
constexpr int UART_STOP_BITS_1=1,UART_HW_FLOWCTRL_DISABLE=0,UART_PIN_NO_CHANGE=-1;
struct uart_config_t { int baud_rate,data_bits,parity,stop_bits,flow_ctrl,rx_flow_ctrl_thresh; };
esp_err_t uart_param_config(uart_port_t,const uart_config_t *);
esp_err_t uart_set_pin(uart_port_t,int,int,int,int);
esp_err_t uart_driver_install(uart_port_t,int,int,int,void *,int);
esp_err_t uart_set_baudrate(uart_port_t,uint32_t);
int uart_write_bytes(uart_port_t,const void *,std::size_t);
int uart_read_bytes(uart_port_t,void *,uint32_t,uint32_t);
esp_err_t uart_get_buffered_data_len(uart_port_t,std::size_t *);
esp_err_t uart_flush(uart_port_t);
esp_err_t uart_wait_tx_done(uart_port_t,uint32_t);
"""}
    for name, text in files.items():
        target = path / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)


class RuntimeTests(unittest.TestCase):
    def test_actual_runtime_serial_sessions_psram_and_paging(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        self.assertIsNotNone(compiler, "C++ compiler required")
        with tempfile.TemporaryDirectory(prefix="5tratum-capture-runtime-") as directory:
            path = Path(directory)
            headers(path)
            source_names = (
                "components/bm1397/bm1370_capture.cpp", "components/bm1397/include/bm1370_capture.h",
                "components/bm1397/bm1370_capture_runtime.cpp", "components/bm1397/include/bm1370_capture_runtime.h",
                "components/bm1397/serial.cpp", "components/bm1397/include/serial.h")
            source_manifest = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in source_names}
            runtime = (COMPONENT / "bm1370_capture_runtime.cpp").read_bytes()
            (path / "capture_runtime_production.cpp").write_bytes(runtime)
            binary = path / "runtime-tests"
            thread_sanitizer = os.environ.get("BM1370_CAPTURE_TSAN") == "1"
            sanitizer = "thread" if thread_sanitizer else "address,undefined"
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pthread",
                "-fsanitize=" + sanitizer, "-fno-omit-frame-pointer", "-DFIVETRATUM_BM1370_CAPTURE=1",
                "-I", str(path), "-I", str(COMPONENT / "include"), str(COMPONENT / "bm1370_capture.cpp"),
                str(COMPONENT / "serial.cpp"), str(ROOT / "test/host/bm1370_capture_runtime_harness.cpp"),
                "-o", str(binary)], check=True)
            report = json.loads(subprocess.run([str(binary)], check=True, capture_output=True, text=True, timeout=30).stdout)
            self.assertEqual(report["groupsPassed"], 8)
            self.assertTrue(report["delayedOldSessionRejected"])
            self.assertEqual(report["psramAlignmentRequested"], 8)
            self.assertTrue(report["ordinaryFourByteAllocationRejected"])
            self.assertEqual(report["runtimeOrdinaryMallocCalls"], 0)
            self.assertFalse(report["perEventAllocation"])
            self.assertFalse(report["extraUartOperations"])
            self.assertFalse(report["physicalWireComplete"])
            proof = ROOT / ".cache/bm1370-capture-runtime"
            proof.mkdir(parents=True, exist_ok=True)
            self.assertEqual(source_manifest, {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in source_names},
                             "Production sources changed during test; rerun before publishing proof")
            report.update({"sanitizers": ["thread"] if thread_sanitizer else ["address", "undefined"],
                "runtimeBodySha256": hashlib.sha256(runtime).hexdigest(),
                "sourceManifest": source_manifest,
                "ownedManifest": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in (
                    "test/host/bm1370_capture_runtime_harness.cpp", "test/host/test_bm1370_capture_runtime.py")},
                "seams": ["ESP timer", "ESP heap/PSRAM allocator", "UART driver", "deterministic barrier before real runtime mutex acquisition"],
                "physicalLimitations": "Host integration does not establish physical UART losslessness, ASIC identity, timing safety or independent work.",
                "threadSanitizer": "passed" if thread_sanitizer else "separate optional invocation BM1370_CAPTURE_TSAN=1"})
            (proof / ("proof-tsan.json" if thread_sanitizer else "proof.json")).write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    unittest.main()
