#!/usr/bin/env python3
"""Actual Gamma Ethernet settings validation/storage; no network/device writes."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile
import unittest

from test_gamma_display_guard import method

ROOT = Path(__file__).resolve().parents[2]
HOST = Path(__file__).resolve().parent
IDF = "espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805"


class GammaEthernetSettingsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="gamma-ethernet-settings-")
        cls.addClassCleanup(cls.temp.cleanup)
        temporary = Path(cls.temp.name)
        for name in ("esp_err.h", "esp_log.h", "esp_http_server.h", "nvs.h", "nvs_flash.h", "sv2_protocol.h",
                     "freertos/FreeRTOS.h", "freertos/queue.h", "freertos/task.h", "freertos/semphr.h"):
            header = temporary / name
            header.parent.mkdir(parents=True, exist_ok=True)
            header.write_text('#include "sdk_stubs.h"\n')
        connect = (ROOT / "components/connect/connect.c").read_text()
        production = "\n".join((
            method(connect, "bool network_ethernet_supported("),
            method(connect, "static bool ethernet_reserved_pin("),
            method(connect, "bool network_ethernet_pins_available("),
            method((ROOT / "main/display.c").read_text(), "const DisplayConfig * get_display_config("),
            method((ROOT / "main/http_server/http_server.c").read_text(), "bool check_settings_and_update("),
        ))
        (temporary / "ethernet_settings_production.inc").write_text(production)
        get_source = (ROOT / "main/http_server/system_api_json.c").read_text()
        get_block = method(get_source, "    if (network_ethernet_supported(g)) {")
        (temporary / "ethernet_system_json.inc").write_text(
            "static void system_ethernet_json(cJSON *root, GlobalState *g) {\n" + get_block + "}\n")
        cls.binary = temporary / "ethernet-settings"
        cjson = ROOT / "managed_components/espressif__cjson/cJSON"
        if not all((cjson / name).is_file() for name in ("cJSON.h", "cJSON.c")):
            # Host CI precedes IDF dependency installation. Use the same pinned
            # cJSON source already used by the existing Gamma host suites.
            cjson = temporary
            for name in ("cJSON.h", "cJSON.c"):
                (temporary / name).write_bytes(subprocess.run(
                    ["docker", "run", "--rm", "--entrypoint", "cat", IDF,
                     "/opt/esp/idf/components/json/cJSON/" + name], check=True, capture_output=True).stdout)
        # cJSON is an existing project dependency, unrelated to W5500 protocol.
        command = shlex.split(os.environ.get("CC", "cc")) + [
            "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare", "-Wno-unused-parameter", "-Wno-unused-const-variable", "-Wno-deprecated-declarations",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g",
            "-I", str(temporary), "-I", str(HOST), "-I", str(ROOT / "main"), "-I", str(cjson),
            "-I", str(ROOT / "main/http_server"), "-I", str(ROOT / "main/tasks"),
            "-include", str(HOST / "sdk_stubs.h"),
            str(ROOT / "main/nvs_config.c"), str(ROOT / "main/device_config.c"),
            str(HOST / "gamma_ethernet_settings_test.c"), str(cjson / "cJSON.c"), "-lm", "-o", str(cls.binary)]
        result = subprocess.run(command, text=True, capture_output=True)
        if result.returncode:
            raise RuntimeError(result.stderr)

    def run_case(self, scenario):
        for board in ("601", "602"):
            with self.subTest(scenario=scenario, board=board):
                # Reinitialization models reboot using the same fake persistent
                # store; the SDK's task/cache lifetime is process-wide here.
                result = subprocess.run([str(self.binary), scenario, board], text=True, capture_output=True,
                                        env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"}, timeout=15)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout.strip(), "PASS")

    def test_saved_flag_only_commits_once_and_survives_reboot_without_other_changes(self):
        self.run_case("success")

    def test_strict_json_types_case_duplicates_identity_pin_conflicts_and_validate_before_write(self):
        self.run_case("reject")

    def test_open_set_and_commit_failures_keep_cache_persistence_and_mixed_fields_unchanged(self):
        self.run_case("failure")

    def test_missing_wrong_type_and_invalid_persisted_flag_fail_closed(self):
        for raw in ("0", "1", "2", "65535", "string"):
            self.run_case("raw-" + raw)

    def test_system_json_reports_network_snapshot_once_only_on_supported_gamma(self):
        self.run_case("get")

    def test_operating_journal_ids_remain_compatible(self):
        header = (ROOT / "main/nvs_config.h").read_text()
        keys = re.findall(r"\bNVS_CONFIG_[A-Z0-9_]+\b", header[header.index("typedef enum {"):header.index("} NvsConfigKey;")])
        # These are persisted v1 operating-journal IDs and extension boundaries.
        expected = {"NVS_CONFIG_STRATUM_PROTOCOL":3, "NVS_CONFIG_FALLBACK_STRATUM_PROTOCOL":15,
                    "NVS_CONFIG_ASIC_FREQUENCY":28, "NVS_CONFIG_ASIC_VOLTAGE":29,
                    "NVS_CONFIG_MINING_SCHEDULE":68, "NVS_CONFIG_POOL":69,
                    "NVS_CONFIG_SV2_REQUIRE_AUTH":94, "NVS_CONFIG_FALLBACK_SV2_REQUIRE_AUTH":95}
        for name, value in expected.items():
            self.assertEqual(keys.index(name), value, name)
        self.assertEqual(keys[96:], ["NVS_CONFIG_ETHERNET_ENABLED", "NVS_CONFIG_COUNT"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
