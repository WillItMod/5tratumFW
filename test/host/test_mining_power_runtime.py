#!/usr/bin/env python3
"""Exercise real power transitions with fake drivers; no miner/network access."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HOST = Path(__file__).resolve().parent


class MiningPowerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="5tratumfw-power-")
        cls.addClassCleanup(cls.temp.cleanup)
        directory = Path(cls.temp.name)
        for name in ("esp_err.h", "esp_log.h", "freertos/FreeRTOS.h", "freertos/task.h",
                     "vcore.h", "thermal.h", "power.h", "asic.h", "utils.h", "asic_init.h",
                     "asic_reset.h", "driver/uart.h", "serial.h", "mining_schedule.h"):
            path = directory / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('#include "power_runtime_stubs.h"\n')
        cls.binary = directory / "power-runtime-test"
        command = shlex.split(os.environ.get("CC", "cc")) + [
            "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-const-variable",
            "-I", str(directory), "-I", str(HOST), "-I", str(ROOT / "main"),
            "-include", str(HOST / "power_runtime_stubs.h"), str(HOST / "power_runtime_test.c"),
            "-lm", "-o", str(cls.binary),
        ]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError("Power host compilation failed:\n" + result.stderr)

    def run_case(self, case):
        result = subprocess.run([str(self.binary), case], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr or result.stdout)
        self.assertEqual(result.stdout.strip(), "PASS")

    def test_pause_cuts_power_and_reset_without_changing_saved_clocks(self):
        self.run_case("stop")

    def test_cutoff_and_reset_failures_never_claim_applied_pause(self):
        self.run_case("stop-fail")
        self.run_case("reset-fail")

    def test_resume_restores_exact_settings_without_flushing_uninitialized_uart(self):
        self.run_case("start")

    def test_resume_regulator_and_asic_failures_cut_power_and_latch_fault(self):
        self.run_case("restore-fail")
        self.run_case("init-fail")

    def test_startup_waits_for_ready_workers_before_powering_asic(self):
        self.run_case("startup-not-ready")
        self.run_case("startup-resume")

    def test_pause_during_thermal_cooldown_prevents_restart(self):
        self.run_case("cooldown-pause")

    def test_canceled_or_failed_thermal_resume_cannot_restore_voltage(self):
        self.run_case("cooldown-resume-pause")
        self.run_case("cooldown-init-fail")

    def test_pause_during_resume_cancels_before_init_or_while_settling(self):
        self.run_case("pause-before-init")
        self.run_case("pause-settling")

    def test_failed_resume_and_failed_cutoff_retry_cutoff(self):
        self.run_case("retry-cutoff")

    def test_workers_gate_on_current_stop_reasons_before_uart_operations(self):
        self.run_case("worker-gates")
        jobs = (ROOT / "main/tasks/create_jobs_task.c").read_text()
        self.assertEqual(jobs.count("if (!mining_state_work_allowed(GLOBAL_STATE))"), 3)
        self.assertIn("new_stratum_version_rolling_msg && mining_state_work_allowed", jobs)
        results = (ROOT / "main/tasks/asic_result_task.c").read_text()
        self.assertLess(results.index("if (!mining_state_work_allowed"), results.index("ASIC_process_work(GLOBAL_STATE)"))
        monitor = (ROOT / "main/tasks/hashrate_monitor_task.c").read_text()
        self.assertLess(monitor.index("is_asic_initialized = mining_state_work_allowed"), monitor.index("ASIC_read_registers(GLOBAL_STATE)"))

    def test_startup_readiness_follows_coordinator_creation_and_failure_gate(self):
        source = (ROOT / "main/main.c").read_text()
        coordinator = source.index("xTaskCreate(protocol_coordinator_task")
        readiness = source.index("mining_runtime_ready = system_init_ret == ESP_OK")
        self.assertLess(coordinator, readiness)
        self.assertIn('POWER_MANAGEMENT_stop_for_fault(&GLOBAL_STATE, "Protocol coordinator task could not start")', source[coordinator:readiness])


if __name__ == "__main__":
    unittest.main(verbosity=2)
