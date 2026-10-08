#!/usr/bin/env python3
"""Actual StratumApi subscribe/send plus pure agent builder; no device I/O."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class NativePoolStreamTests(unittest.TestCase):
    sources = (
        "main/stratum/stratum_api.cpp", "main/stratum/stratum_api.h",
        "main/stratum/stratum_task.cpp", "main/stratum/native_pool_stream.h",
        "main/boards/five_tratum_model_labels.h", "main/boards/nerdoctaxegamma.cpp",
        "test/host/native_pool_stream_harness.cpp", "test/host/test_native_pool_stream.py",
    )

    @classmethod
    def setUpClass(cls):
        cls.source_hashes = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in cls.sources}
        cls.tmp = tempfile.TemporaryDirectory(prefix="5tratum-native-pool-")
        temporary = Path(cls.tmp.name)
        headers = {
            "esp_transport.h": "#pragma once\nusing esp_transport_handle_t=void*;\nstruct esp_transport_keep_alive_t {};\n",
            "esp_heap_caps.h": "#pragma once\n#include <cstddef>\n#include <cstdlib>\n#define MALLOC_CAP_SPIRAM 0x400\n#define MALLOC_CAP_8BIT 4\n#define MALLOC_CAP_DMA 8\nvoid *heap_caps_malloc(std::size_t,unsigned);\nvoid *heap_caps_calloc(std::size_t,std::size_t,unsigned);\nvoid *heap_caps_realloc(void*,std::size_t,unsigned);\nvoid heap_caps_free(void*);\n",
            "esp_log.h": "#pragma once\n#define ESP_LOGI(tag,...) do{(void)(tag);}while(0)\n#define ESP_LOGE(tag,...) do{(void)(tag);}while(0)\n#define ESP_LOGD(tag,...) do{(void)(tag);}while(0)\n#define ESP_LOGW(tag,...) do{(void)(tag);}while(0)\n#define pdMS_TO_TICKS(x) (x)\ninline void vTaskDelay(int){}\n",
            "esp_ota_ops.h": "#pragma once\nstruct esp_app_desc_t{char version[64];};\nconst esp_app_desc_t *esp_app_get_description();\n",
            "esp_http_server.h": "#pragma once\nusing esp_err_t=int;\nstruct httpd_req_t {};\n",
            "lwip/sockets.h": "#pragma once\n#include <sys/socket.h>\n",
        }
        for name, text in headers.items():
            target = temporary / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(text)
        cls.binary = temporary / "native-pool"
        subprocess.run([
            shutil.which("clang++") or shutil.which("g++"), "-std=c++17",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-pthread",
            "-I", str(temporary), "-I", str(ROOT / "components/arduinojson"),
            "-I", str(ROOT / "main"), "-I", str(ROOT / "main/stratum"),
            str(ROOT / "main/stratum/stratum_api.cpp"),
            str(ROOT / "test/host/native_pool_stream_harness.cpp"), "-o", str(cls.binary),
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_scenario(self, scenario):
        result = subprocess.run([str(self.binary), scenario], check=True, capture_output=True, text=True)
        self.assertLess(len(result.stdout), 1024)
        return json.loads(result.stdout)

    def test_actual_two_apis_send_distinct_stream_hints_same_stable_parent(self):
        a, b = self.run_scenario("two-streams")
        for index, frame in enumerate((a, b)):
            self.assertEqual(frame["id"], 1)
            self.assertEqual(frame["method"], "mining.subscribe")
            self.assertEqual(frame["params"], [
                "5tratumFW/NerdQAxe++/BM1370/5tratumFW-qa-web-a10/"
                f"native-pool-v1:0102030405060708090a0b0c0d0e0f10:{index}/work-context-v2"])
            self.assertLessEqual(len(frame["params"][0]), 192)
        self.assertEqual(self.source_hashes,
            {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in self.sources})
        proof = ROOT / ".cache/native-pool-stream"
        proof.mkdir(parents=True, exist_ok=True)
        (proof / "subscribe-wire.jsonl").write_text("".join(json.dumps(frame) + "\n" for frame in (a, b)))
        (proof / "proof.json").write_text(json.dumps({
            "sourceManifest": self.source_hashes,
            "integration": "Actual StratumApi.cpp subscribe/send with ASan/UBSan",
            "agentLimitBytes": 192,
            "streamIndices": [0, 1],
            "identitySource": "Mock of existing stable opaque ID getter",
            "transport": "Mock connected/partial-write/error socket",
            "limitations": "No device, UART, public pool compatibility, identity attestation or ASIC isolation proof.",
        }, indent=2) + "\n")

    def test_legacy_default_and_invalid_or_unavailable_identity_remain_legacy(self):
        for scenario in ("legacy", "invalid-index", "unavailable-id", "zero-id", "uppercase-id", "short-id"):
            with self.subTest(scenario=scenario):
                agent = self.run_scenario(scenario)["params"][0]
                self.assertEqual(agent, "5tratumFW/NerdQAxe++/BM1370/5tratumFW-qa-web-a10/work-context-v2")

    def test_actual_octaxe_subscriptions_use_ascii_alias_with_one_parent_and_two_streams(self):
        frames = self.run_scenario("oct-two-streams")
        self.assertEqual(len(frames), 2)
        for index, frame in enumerate(frames):
            self.assertEqual(frame["id"], 1)
            self.assertEqual(frame["method"], "mining.subscribe")
            self.assertEqual(frame["params"], [
                "5tratumFW/NerdOCTAXE-Gamma/BM1370/5tratumFW-oct-0.1.0-beta.1/"
                f"native-pool-v1:0102030405060708090a0b0c0d0e0f10:{index}/work-context-v2"])
            self.assertLessEqual(len(frame["params"][0].encode("ascii")), 192)
        source = (ROOT / "main/boards/nerdoctaxegamma.cpp").read_text()
        self.assertIn("m_miningAgent = FiveTratumModels::OctaxeGammaMiningAgent;", source)
        self.assertIn("m_deviceModel = FiveTratumModels::OctaxeGamma;", source)
        self.assertEqual(self.source_hashes,
            {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in self.sources})
        proof = ROOT / ".cache/native-pool-stream"
        proof.mkdir(parents=True, exist_ok=True)
        (proof / "octaxe-subscribe-wire.jsonl").write_text(
            "".join(json.dumps(frame) + "\n" for frame in frames))
        (proof / "octaxe-proof.json").write_text(json.dumps({
            "sourceManifest": self.source_hashes,
            "integration": "Actual StratumApi.cpp subscribe/send with ASan/UBSan",
            "model": "NerdOCTAXE-γ",
            "miningAgent": "NerdOCTAXE-Gamma",
            "streamIndices": [0, 1],
            "agentLimitBytes": 192,
            "transport": "Mock connected socket with seven-byte partial writes",
            "limitations": "No device, MUX server, UART, identity attestation or ASIC isolation proof.",
        }, indent=2) + "\n")

    def test_octaxe_identity_failure_stays_legacy_and_utf8_model_cannot_bypass_agent_guard(self):
        legacy = self.run_scenario("oct-unavailable-id")
        self.assertEqual(legacy["params"], [
            "5tratumFW/NerdOCTAXE-Gamma/BM1370/5tratumFW-oct-0.1.0-beta.1/work-context-v2"])
        retried = self.run_scenario("oct-utf8-model")
        self.assertEqual(retried["id"], 1)
        self.assertEqual(retried, self.run_scenario("oct-two-streams")[0])

    def test_bounded_builder_never_writes_broken_json_or_overruns(self):
        self.assertTrue(self.run_scenario("bounds")["bounded"])
        self.assertEqual(self.run_scenario("unsafe")["id"], 1)

    def test_actual_send_retains_connection_and_failure_semantics(self):
        for scenario in ("disconnected", "send-failure"):
            self.assertFalse(self.run_scenario(scenario)["sent"])

    def test_real_pool_task_passes_its_own_index(self):
        source = (ROOT / "main/stratum/stratum_task.cpp").read_text()
        self.assertIn("subscribe(m_transport, board->getMiningAgent(), board->getAsicModel(), m_index)", source)


if __name__ == "__main__":
    unittest.main()
