#!/usr/bin/env python3
"""Run actual firmware settings/profile code against a native fake NVS store."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HOST = Path(__file__).resolve().parent


class GammaBootTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="5tratumfw-host-")
        cls.addClassCleanup(cls.temp.cleanup)
        directory = Path(cls.temp.name)
        stubs = directory / "stubs"
        for name in (
            "esp_err.h", "esp_log.h", "esp_http_server.h", "nvs.h", "nvs_flash.h",
            "sv2_protocol.h", "freertos/FreeRTOS.h", "freertos/queue.h",
            "freertos/task.h", "freertos/semphr.h",
        ):
            path = stubs / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('#include "sdk_stubs.h"\n')
        cls.binary = directory / "gamma-boot-test"
        command = shlex.split(os.environ.get("CC", "cc")) + [
            "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare",
            "-Wno-unused-parameter", "-Wno-unused-const-variable",
            "-I", str(stubs), "-I", str(HOST), "-I", str(ROOT / "main"),
            "-I", str(ROOT / "main/tasks"), "-I", str(ROOT / "main/http_server"),
            "-include", str(HOST / "sdk_stubs.h"),
            str(ROOT / "main/nvs_config.c"), str(ROOT / "main/device_config.c"),
            str(HOST / "gamma_boot_test.c"), "-lm", "-o", str(cls.binary),
        ]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(f"Host test compilation failed:\n{result.stderr}")

    def run_case(self, scenario, value):
        result = subprocess.run([str(self.binary), scenario, value], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr or result.stdout)
        self.assertEqual(result.stdout.strip(), "PASS")

    def test_existing_601_602_settings_preserved(self):
        for board in ("601", "602"):
            with self.subTest(board=board):
                self.run_case("current", board)

    def test_legacy_settings_interpreted_without_migration(self):
        for board in ("601", "602"):
            with self.subTest(board=board):
                self.run_case("legacy", board)

    def test_coinbase_decode_defaults_off_without_writing_absent_keys(self):
        for board in ("601", "602"):
            with self.subTest(board=board):
                self.run_case("current", board)

    def test_saved_coinbase_decode_values_preserved_without_writes(self):
        for board in ("601", "602"):
            for values in ("00", "01", "10", "11"):
                with self.subTest(board=board, saved_values=values):
                    self.run_case("decode-saved-" + values, board)

    def test_schedule_storage_distinguishes_absence_corruption_and_wrong_type(self):
        for scenario in ("schedule-missing", "schedule-malformed", "schedule-wrong-type"):
            with self.subTest(scenario=scenario):
                self.run_case(scenario, "601")

    def test_schedule_save_commits_one_key_and_retains_cache_on_failure(self):
        for scenario in ("schedule-save", "schedule-save-failure", "schedule-save-commit-failure"):
            with self.subTest(scenario=scenario):
                self.run_case(scenario, "602")

    def test_all_flash_init_errors_stop_without_erasing_or_opening(self):
        for error in ("-1", "0x101", "0x110d", "0x1110", "0x1102"):
            with self.subTest(error=error):
                self.run_case("flash-error", error)

    def test_namespace_unavailable_stops_before_writer(self):
        self.run_case("open-error", "601")
        self.run_case("write-open-error", "601")

    def test_stored_identity_required(self):
        self.run_case("missing", "601")
        self.run_case("wrong-type", "601")

    def test_unsupported_identity_cannot_start_writer(self):
        for board in ("000", "", "600", "603", "650", "800", "801", "nerd", "601x", "601\n"):
            with self.subTest(board=board):
                self.run_case("unsupported", board)

    def test_device_profile_guard_rejects_unsupported_cached_identity(self):
        self.run_case("profile-guard", "601")

    def test_hardware_calls_follow_both_identity_guards(self):
        # A structural boot-order contract supplements executable NVS/profile tests.
        source = (ROOT / "main/main.c").read_text()
        nvs_guard = source.index("if (nvs_config_init() != ESP_OK)")
        board_guard = source.index("if (device_config_init(&GLOBAL_STATE) != ESP_OK)")
        self.assertLess(nvs_guard, board_guard)
        for hardware in ("i2c_bitaxe_init()", "asic_hold_reset_low()", "ADC_init()",
                         "self_test_init(&GLOBAL_STATE)", "SYSTEM_init_peripherals(&GLOBAL_STATE)",
                         "asic_initialize(&GLOBAL_STATE, ASIC_INIT_COLD_BOOT, 0)"):
            with self.subTest(hardware=hardware):
                self.assertLess(board_guard, source.index(hardware))
        guarded_block = source[board_guard:source.index("i2c_bitaxe_init()")]
        self.assertIn("return;", guarded_block)


if __name__ == "__main__":
    unittest.main(verbosity=2)
