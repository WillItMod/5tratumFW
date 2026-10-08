#!/usr/bin/env python3
"""Actual Gamma GET handler + pure report + locked peer state; platform-only mocks."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HOST = Path(__file__).resolve().parent
IMAGE = "espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805"


def production_binding():
    text = (ROOT / "main/http_server/operating_profiles.c").read_text()
    start = text.index("cJSON *operating_profiles_binding(void)")
    end = text.index("\nstatic cJSON *get_profiles(void)", start)
    return text[start:end]


class GammaStatusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="5tfw-gamma-status-")
        path = Path(cls.temp.name)
        for name in ("cJSON.h", "cJSON.c"):
            result = subprocess.run(["docker", "run", "--rm", "--network", "none", "--entrypoint", "cat",
                                     IMAGE, "/opt/esp/idf/components/json/cJSON/" + name],
                                    check=True, capture_output=True)
            (path / name).write_bytes(result.stdout)
        for name in ("global_state.h", "esp_http_server.h", "protocol_coordinator.h", "esp_timer.h",
                     "freertos/FreeRTOS.h", "freertos/task.h"):
            header = path / name
            header.parent.mkdir(parents=True, exist_ok=True)
            header.write_text('#pragma once\n#include "gamma_status_support.h"\n')
        (path / "binding.c").write_text(
            '#include "gamma_status_support.h"\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n'
            'static GlobalState *state;\nvoid set_binding_state(GlobalState *value){state=value;}\n'
            + production_binding())
        cls.binary = path / "gamma-status"
        # Keep firmware/harness warnings fatal; Apple's libc deprecates sprintf
        # used by the unchanged IDF-pinned third-party cJSON implementation.
        library = path / "cJSON.o"
        subprocess.run([shutil.which("clang") or shutil.which("cc"), "-std=gnu11",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        "-Wno-deprecated-declarations", "-c", str(path / "cJSON.c"),
                        "-o", str(library)], check=True)
        command = [shutil.which("clang") or shutil.which("cc"), "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                   "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g",
                   "-I", str(path), "-I", str(HOST), "-I", str(ROOT / "main"),
                   "-I", str(ROOT / "main/http_server"), "-I", str(ROOT / "components/stratum/include"),
                   str(ROOT / "main/http_server/five_tratum_status.c"),
                   str(ROOT / "main/http_server/five_tratum_status_report.c"),
                   str(ROOT / "main/mux_peer_status.c"), str(path / "binding.c"),
                   str(HOST / "gamma_status_harness.c"), str(library), "-lpthread", "-lm", "-o", str(cls.binary)]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode:
            cls.temp.cleanup()
            raise RuntimeError(result.stderr)
        cls.scenarios = []

    @classmethod
    def tearDownClass(cls):
        folder = ROOT / ".cache/gamma-status-20261008"
        folder.mkdir(parents=True, exist_ok=True)
        folder.chmod(0o700)
        files = ("main/http_server/five_tratum_status.c", "main/http_server/five_tratum_status.h",
                 "main/http_server/five_tratum_status_report.c", "main/http_server/five_tratum_status_report.h",
                 "main/mux_peer_status.c", "components/stratum/include/five_tratum_mux_status.h",
                 "main/http_server/operating_profiles.c", "test/host/gamma_status_support.h",
                 "test/host/gamma_status_harness.c", "test/host/test_gamma_status.py")
        proof = folder / "host-proof.json"
        proof.write_text(json.dumps({"sanitizers": "ASan/UBSan", "hardwareActions": False,
                                    "scenarios": cls.scenarios,
                                    "boundary": "Actual GET/formatter/locked peer runtime and unchanged binding function; SDK/crypto/storage/HTTP platform mocked",
                                    "sourceFiles": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in files}}, indent=2) + "\n")
        os.chmod(proof, 0o600)
        cls.temp.cleanup()

    def run_case(self, scenario):
        result = subprocess.run([str(self.binary), scenario], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "PASS")
        self.scenarios.append(scenario)

    def test_fresh_primary_bound_to_same_profiles_identity(self): self.run_case("primary")
    def test_current_fallback_only_is_advertised(self): self.run_case("fallback")
    def test_gamma_602_binding_is_supported(self): self.run_case("602")
    def test_exact_ttl_disconnect_reconnect_and_old_generation(self): self.run_case("lifetime")
    def test_pool_protocol_and_same_route_epoch_transition_fail_closed(self): self.run_case("transition")
    def test_future_peer_time_is_expired(self): self.run_case("future")
    def test_network_and_cors_fail_before_peer_and_identity_reads(self): self.run_case("auth")
    def test_identity_failure_or_unqualified_board_is_unavailable(self): self.run_case("identity")
    def test_all_json_allocation_failures_release_partial_reports(self): self.run_case("allocation")
    def test_malformed_duplicate_or_secret_binding_fields_are_rejected(self): self.run_case("malformed-binding")
    def test_send_failure_is_returned_and_json_is_released(self): self.run_case("send-failure")
    def test_route_registration_failure_is_returned(self): self.run_case("registration")
    def test_negative_monotonic_time_is_unavailable(self): self.run_case("negative-clock")


if __name__ == "__main__":
    unittest.main(verbosity=2)
