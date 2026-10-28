#!/usr/bin/env python3
"""Compile the original W5500 engine against a deterministic register simulator.

No network, GPIO, miner or external W5500 implementation is used. A sanitizer
failure is a test failure. This does not qualify an actual Ethernet module.
"""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMPONENT = ROOT / "components/gamma_ethernet"


class GammaEthernetTests(unittest.TestCase):
    def test_original_macraw_protocol_under_sanitizers(self):
        with tempfile.TemporaryDirectory(prefix="gamma-ethernet-") as temporary:
            binary = Path(temporary) / "w5500-core"
            subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                            "-g", "-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                            "-I", str(COMPONENT / "include"), str(COMPONENT / "gamma_w5500.c"),
                            str(COMPONENT / "test/test_w5500_core.c"), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=15)

    def test_adapter_lifecycle_and_stack_reentry_under_sanitizers(self):
        with tempfile.TemporaryDirectory(prefix="gamma-ethernet-adapter-") as temporary:
            temporary = Path(temporary)
            for name in ("esp_err.h", "esp_netif.h", "driver/spi_master.h", "esp_intr_alloc.h", "sdkconfig.h", "esp_heap_caps.h", "esp_mac.h",
                         "esp_netif_defaults.h", "esp_timer.h", "freertos/FreeRTOS.h", "freertos/semphr.h", "freertos/task.h"):
                header = temporary / name
                header.parent.mkdir(parents=True, exist_ok=True)
                header.write_text('#include "gamma_eth_platform.h"\n')
            binary = temporary / "ethernet-adapter"
            subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-D_POSIX_C_SOURCE=200809L", "-pthread",
                            "-Wall", "-Wextra", "-Werror", "-pedantic", "-g", "-O1",
                            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                            "-I", str(temporary), "-I", str(COMPONENT / "test"), "-I", str(COMPONENT / "include"),
                            str(COMPONENT / "gamma_w5500.c"), str(COMPONENT / "gamma_ethernet.c"),
                            str(COMPONENT / "test/test_ethernet_adapter.c"), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=15)


if __name__ == "__main__":
    unittest.main()
