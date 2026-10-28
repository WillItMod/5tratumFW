#!/usr/bin/env python3
"""Run the actual legacy/v2 PATCH bodies against production display validation.

Only platform services are faked. No SDK, network, miner or flash access occurs.
The mixed payloads prove invalid profiles return before any existing mutation,
including network/ASIC/fan/pool setters, verification, flush and reload paths.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HOST = ROOT / "test/host"


def function_body(source: str, signature: str) -> str:
    """Copy one complete production function, preserving its body verbatim."""
    start = source.index(signature)
    opening = source.index("{", start + len(signature))
    depth, state, index = 1, "code", opening + 1
    while index < len(source):
        char = source[index]
        pair = source[index:index + 2]
        if state in ('"', "'"):
            if char == "\\":
                index += 2
                continue
            if char == state:
                state = "code"
        elif state == "line-comment":
            if char == "\n":
                state = "code"
        elif state == "block-comment":
            if pair == "*/":
                state = "code"
                index += 2
                continue
        elif pair in ("//", "/*"):
            state = "line-comment" if pair == "//" else "block-comment"
            index += 2
            continue
        elif char in ('"', "'"):
            state = char
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
        index += 1
    raise ValueError(f"Unterminated production function: {signature}")


class DisplaySettingsApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiler = shutil.which("clang++") or shutil.which("g++")
        if not cls.compiler:
            raise RuntimeError("Display settings API tests require a C++17 compiler")

    def test_real_patch_handlers_validate_profiles_before_every_mutation(self):
        with tempfile.TemporaryDirectory(prefix="display-settings-api-") as temporary:
            temporary = Path(temporary)
            actual = temporary / "actual_settings_handlers.cpp"
            handlers = [
                (ROOT / "main/http_server/handler_system.cpp", "esp_err_t PATCH_update_settings(httpd_req_t *req)"),
                (ROOT / "main/http_server/v2/handler_v2_settings.cpp", "esp_err_t PATCH_V2_settings(httpd_req_t *req)"),
                (ROOT / "main/http_server/v2/handler_v2_settings.cpp", "esp_err_t GET_V2_settings(httpd_req_t *req)"),
                (ROOT / "main/http_server/v2/handler_v2_system.cpp", "esp_err_t GET_V2_system(httpd_req_t *req)"),
            ]
            adapter = (ROOT / "main/http_server/display_settings.h").read_text()
            adapters = "\n".join(function_body(adapter, signature) for signature in (
                "inline void appendSavedDisplayProfile(JsonDocument &doc)",
                "inline void appendDisplayRuntimeMetadata(JsonDocument &doc)",
            ))
            actual.write_text(
                '#include "platform.h"\n'
                '#include "display_profile_json.h"\n'
                '#define VR_FREQUENCY_ENABLED\n'
                'namespace FiveTratumDisplay {\n' + adapters + '\n}\n' +
                "\n\n".join(function_body(path.read_text(), signature)
                               for path, signature in handlers) + "\n")
            for board in ("NERDQAXEPLUS2", "NERDOCTAXEGAMMA", "NERDOCTAXEPLUS",
                          "NERDQAXEPLUS", "NERDOCTAXEGAMMA_EXTRA", None):
                with self.subTest(board=board):
                    binary = temporary / (board or "no-board")
                    subprocess.run([
                        self.compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        *([f"-D{board}"] if board else []),
                        "-I", str(HOST / "display_settings_api_stubs"),
                        "-I", str(ROOT / "components/arduinojson"),
                        "-I", str(ROOT / "main/displays"),
                        str(actual), str(HOST / "display_settings_api_harness.cpp"),
                        "-o", str(binary),
                    ], check=True)
                    environment = os.environ.copy()
                    environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
                    subprocess.run([str(binary)], check=True, env=environment)


if __name__ == "__main__":
    unittest.main(verbosity=2)
