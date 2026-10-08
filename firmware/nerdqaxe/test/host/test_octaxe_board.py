#!/usr/bin/env python3
"""Compile actual Oct board/header and base methods with external hardware mocked.

No network, device, firmware SDK build or writes to production source. Parent
profile defaults are fixture boundaries; tested Oct overrides, config loading,
base UART request cadence and report/agent helpers are production code.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def method(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


class OctaxeBoardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="5tfw-oct-board-")
        path = Path(cls.tmp.name)
        for directory in ("drivers", "driver", "freertos"):
            (path / directory).mkdir()
        for name in ("asic.h", "bm1370.h", "board.h", "nerdqaxeplus2.h", "esp_log.h", "esp_timer.h",
                     "drivers/TPS53667.h", "drivers/tmp451_mux.h", "driver/gpio.h", "freertos/FreeRTOS.h", "freertos/task.h"):
            (path / name).write_text('#pragma once\n#include "octaxe_board_support.h"\n')
        for name in ("nerdoctaxegamma.cpp", "nerdoctaxegamma.h", "five_tratum_model_labels.h"):
            shutil.copy(ROOT / "main/boards" / name, path / name)
        parent = (ROOT / "main/boards/nerdqaxeplus.cpp").read_text()
        board = (ROOT / "main/boards/board.cpp").read_text()
        (path / "parent-methods.cpp").write_text(
            '#include "octaxe_board_support.h"\n#include "periodic.hpp"\nstatic const char *TAG="host";\n'
            + method(parent, "void NerdQaxePlus::requestChipTemps()")
            + method(parent, "float NerdQaxePlus::getVRTemp()")
            + method(board, "void Board::loadSettings()"))
        cls.binary = path / "octaxe-board"
        subprocess.run([
            shutil.which("clang++") or shutil.which("g++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-I", str(path),
            "-I", str(ROOT / "test/host"), "-I", str(ROOT / "main"),
            str(path / "nerdoctaxegamma.cpp"), str(path / "parent-methods.cpp"),
            str(ROOT / "test/host/octaxe_board_harness.cpp"), "-o", str(cls.binary),
        ], check=True)
        cls.scenarios = []

    @classmethod
    def tearDownClass(cls):
        proof = ROOT / ".cache/octaxe-20261008"
        proof.mkdir(parents=True, exist_ok=True)
        proof.chmod(0o700)
        files = ("main/boards/nerdoctaxegamma.cpp", "main/boards/nerdoctaxegamma.h",
                 "main/boards/five_tratum_model_labels.h", "main/boards/board.cpp",
                 "main/boards/nerdqaxeplus.cpp", "main/periodic.hpp", "main/stratum/native_pool_stream.h",
                 "test/host/octaxe_board_support.h", "test/host/octaxe_board_harness.cpp", "test/host/test_octaxe_board.py")
        (proof / "board-host-proof.json").write_text(json.dumps({
            "hardwareActions": False, "sanitizers": "ASan/UBSan", "scenarios": cls.scenarios,
            "sourceFiles": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in files},
            "boundary": "Actual Oct cpp/header and production base methods; external GPIO, TMP451, regulator, config and inherited profile defaults mocked",
            "physicalRevisionQualified": False, "powerControlQualified": False,
        }, indent=2) + "\n")
        os.chmod(proof / "board-host-proof.json", 0o600)
        cls.tmp.cleanup()

    def run_case(self, scenario):
        run = subprocess.run([str(self.binary), scenario], capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stderr)
        report = json.loads(run.stdout)
        self.assertTrue(report["passed"])
        self.assertEqual(report["count"], 8)
        self.assertTrue(report["agent0"].endswith(":0/work-context-v2"))
        self.assertTrue(report["agent1"].endswith(":1/work-context-v2"))
        self.scenarios.append(scenario)

    def test_stock_four_phase_limits_settings_and_ascii_agent(self): self.run_case("four-phase")
    def test_six_phase_selection_calibration_and_old_object_retirement(self): self.run_case("six-phase")
    def test_old_board_uses_actual_uart_poll_cadence(self): self.run_case("fallback")
    def test_missing_asic_does_not_issue_a_temperature_request(self): self.run_case("missing-asic")
    def test_all_eight_mux_channels_keep_actual_readings(self): self.run_case("mux")
    def test_invalid_reads_clear_stale_values_but_hot_readings_stay(self): self.run_case("invalid-mux")
    def test_partial_mux_does_not_reuse_missing_group(self): self.run_case("partial-mux")
    def test_shutdown_clears_samples_without_sensor_or_uart_ops(self): self.run_case("shutdown")
    def test_parent_failure_stops_mux_probe_and_clears_samples(self): self.run_case("parent-failure")
    def test_reinit_without_mux_retires_samples_and_restores_uart(self): self.run_case("reinit-no-mux")
    def test_failed_reinit_does_not_reuse_mux_presence_or_cache(self): self.run_case("reinit-failure")


if __name__ == "__main__": unittest.main(verbosity=2)
