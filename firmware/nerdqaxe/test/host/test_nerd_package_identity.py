#!/usr/bin/env python3
"""Exercise real model/package guards with synthetic images; never build or flash hardware."""
from contextlib import ExitStack
from dataclasses import replace
import gzip
import json
from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import package_5tratumfw_qa as packaging
from nerd_build_identity import QA, OCT, checked_identity

COMMIT = "1" * 40


class PackageFixture:
    def __init__(self, root, identity):
        self.root, self.identity = root, identity
        self.version = f"5tratumFW-{identity.family}-0.1.0-beta.1"
        self.build = root / "build/test-package"
        self.web = root / "main/http_server/axe-os/dist/axe-os"
        self.output = root / "release/test-output"
        self.build.mkdir(parents=True)
        self.web.mkdir(parents=True)
        (self.build / "config").mkdir()
        (root / identity.version_file).write_text(self.version + "\n")
        web_version = root / identity.web_version_file
        web_version.parent.mkdir(parents=True, exist_ok=True)
        web_version.write_text(f"export const FIRMWARE_WEB_VERSION = '{self.version}';\n")
        board_source = root / identity.board_source
        board_source.parent.mkdir(parents=True, exist_ok=True)
        if identity == OCT:
            board_source.write_text('#include "five_tratum_model_labels.h"\n'
                'm_deviceModel = FiveTratumModels::OctaxeGamma;\n'
                'm_miningAgent = FiveTratumModels::OctaxeGammaMiningAgent;\n'
                'm_asicModel = "BM1370";\nm_asicCount = 8;\n')
            labels = root / "main/boards/five_tratum_model_labels.h"
            labels.write_bytes((ROOT / "main/boards/five_tratum_model_labels.h").read_bytes())
        else:
            board_source.write_text(f'm_deviceModel = "{identity.model}";\nm_asicModel = "BM1370";\nm_asicCount = {identity.asic_count};\n')
        (root / "partitions.csv").write_text("www,data,spiffs,0x410000,3M\nota_0,app,ota_0,0x710000,4M\nota_1,app,ota_1,0xb10000,4M\n")
        (root / "LICENSE").write_text("Synthetic license fixture; no actual firmware source packaged.\n")
        (self.build / "sdkconfig").write_text("CONFIG_IDF_TARGET=esp32s3\n")
        (self.build / "project_description.json").write_text(json.dumps({"project_version": self.version,
            "target": "esp32s3", "config_file": str(self.build / "sdkconfig")}))
        (self.build / "config/sdkconfig.json").write_text(json.dumps({"IDF_TARGET": "esp32s3",
            "PARTITION_TABLE_CUSTOM_FILENAME": "partitions.csv", "SPIFFS_PAGE_SIZE": 256,
            "SPIFFS_OBJ_NAME_LEN": 32, "SPIFFS_META_LENGTH": 4}))
        (self.build / "build-source-receipt.json").write_text(json.dumps({"sourceCommit": COMMIT,
            "boardProfile": identity.board, "version": self.version, "idfImage": packaging.IDF_IMAGE}))
        self.cache = {"FIVETRATUM_RELEASE_PROFILE": identity.board,
            "FIVETRATUM_ASIC_CAPTURE_LOGS": "OFF", "FIVETRATUM_BM1370_CAPTURE": "OFF",
            "FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER": "OFF"}
        self.write_cache()
        self.compile_flags(identity.board)
        self.image = bytearray(512)
        self.image[0] = 0xe9
        struct.pack_into("<H", self.image, 12, 9)
        struct.pack_into("<I", self.image, 32, 0xabcd5432)
        self.image[48:80] = self.version.encode().ljust(32, b"\0")
        self.image[144:176] = b"v5.5.3".ljust(32, b"\0")
        (self.build / "esp-miner.bin").write_bytes(self.image)
        (self.build / "www.bin").write_bytes(b"\xff" * packaging.WWW_BYTES)
        (self.web / "index.html.gz").write_bytes(gzip.compress(b"fixture"))
        (self.web / "main.js.gz").write_bytes(gzip.compress(self.version.encode()))
        (self.web / "build-info.json.gz").write_bytes(gzip.compress(json.dumps({"product": "5tratumFW",
            "version": self.version, "boardProfile": identity.board, "nodeVersion": "24.14.0",
            "sourceCommit": COMMIT}).encode()))

    def write_cache(self):
        (self.build / "CMakeCache.txt").write_text("".join(f"{key}:STRING={value}\n" for key, value in self.cache.items()))

    def compile_flags(self, *boards):
        (self.build / "compile_commands.json").write_text(json.dumps([{"file": "/project/main/main.cpp",
            "command": "clang " + " ".join("-D" + board for board in boards) + " -c main.cpp"}]))

    def fake_recreate_spiffs(self, command, **kwargs):
        if command[:3] != ["docker", "run", "--rm"]:
            raise AssertionError("Unexpected fixture command")
        (self.build / "www-verify.bin").write_bytes((self.build / "www.bin").read_bytes())

    def package(self):
        with ExitStack() as stack:
            stack.enter_context(patch.object(packaging, "ROOT", self.root))
            stack.enter_context(patch.object(packaging, "WEB", self.web))
            stack.enter_context(patch.object(packaging, "source_receipt", return_value=(COMMIT,
                {"LICENSE": self.root / "LICENSE"}, {}, "firmware/nerdqaxe")))
            stack.enter_context(patch.object(packaging, "run", return_value="v24.14.0"))
            stack.enter_context(patch.object(packaging.subprocess, "run", side_effect=self.fake_recreate_spiffs))
            return packaging.package(self.build, self.output, self.identity)


class NerdPackageIdentityTests(unittest.TestCase):
    def test_real_version_files_are_separate_with_qa_as_default(self):
        self.assertTrue(packaging.version().startswith("5tratumFW-qa-"))
        self.assertEqual(packaging.version(), (ROOT / QA.version_file).read_text().strip())
        self.assertTrue(packaging.version(OCT).startswith("5tratumFW-oct-"))
        self.assertEqual(packaging.version(OCT), (ROOT / OCT.version_file).read_text().strip())
        self.assertNotEqual(packaging.version(), packaging.version(OCT))
        angular = json.loads((ROOT / "main/http_server/axe-os/angular.json").read_text())
        configs = angular["projects"]["axe-os"]["architect"]["build"]["configurations"]
        self.assertFalse(any(item["replace"].endswith("firmware-web-version.ts") for item in configs["production"]["fileReplacements"]))
        self.assertIn({"replace": "src/app/firmware-web-version.ts", "with": "src/app/firmware-web-version.oct.ts"}, configs["oct-production"]["fileReplacements"])

    def test_modified_or_unknown_build_identity_is_rejected(self):
        for identity in (replace(OCT, board=QA.board), replace(OCT, asic_count=4), replace(QA, model=OCT.model)):
            with self.assertRaisesRegex(ValueError, "identity"):
                checked_identity(identity)

    def test_each_package_uses_its_model_count_version_filename_and_ota_only_geometry(self):
        for identity in (QA, OCT):
            with self.subTest(model=identity.model), tempfile.TemporaryDirectory() as temporary:
                fixture = PackageFixture(Path(temporary), identity)
                fixture.package()
                manifest = json.loads((fixture.output / "manifest.json").read_text())
                self.assertEqual(manifest["boardProfile"], identity.board)
                self.assertEqual(manifest["deviceModel"], identity.model)
                self.assertEqual(manifest["asicCount"], identity.asic_count)
                self.assertEqual(manifest["releaseTag"], identity.tag_prefix + "0.1.0-beta.1")
                self.assertEqual(set(manifest["images"]), {identity.app_name, "www.bin"})
                self.assertFalse(manifest["hardwareTested"])
                self.assertFalse(manifest["independentWorkAssignment"])
                self.assertFalse(manifest["partitionLayout"]["physicalLayoutVerified"])
                self.assertEqual(manifest["partitionLayout"]["flashBytesAssumed"], 16 * 1024 * 1024)
                self.assertEqual({path.name for path in fixture.output.glob("*.bin")}, {identity.app_name, "www.bin"})
                for name, entry in manifest["files"].items():
                    self.assertEqual(packaging.sha256(fixture.output / name), entry["sha256"])

    def test_oct_cannot_be_packaged_from_a_qa_target_or_mixed_board_flags(self):
        for boards in ((QA.board,), (QA.board, OCT.board)):
            with self.subTest(boards=boards), tempfile.TemporaryDirectory() as temporary:
                fixture = PackageFixture(Path(temporary), OCT)
                fixture.compile_flags(*boards)
                with self.assertRaisesRegex(ValueError, "compiler"):
                    fixture.package()
                self.assertFalse(fixture.output.exists())

    def test_actual_oct_source_constant_bindings_package_and_mutants_are_rejected(self):
        for mutation in (None, "model-binding", "agent-binding", "utf8-constant", "agent-constant", "include"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as temporary:
                fixture = PackageFixture(Path(temporary), OCT)
                source = fixture.root / OCT.board_source
                source.write_bytes((ROOT / OCT.board_source).read_bytes())
                labels = fixture.root / "main/boards/five_tratum_model_labels.h"
                if mutation == "model-binding":
                    source.write_text(source.read_text().replace("m_deviceModel = FiveTratumModels::OctaxeGamma;",
                        "m_deviceModel = FiveTratumModels::OctaxeGammaMiningAgent;"))
                elif mutation == "agent-binding":
                    source.write_text(source.read_text().replace("m_miningAgent = FiveTratumModels::OctaxeGammaMiningAgent;",
                        "m_miningAgent = FiveTratumModels::OctaxeGamma;"))
                elif mutation == "utf8-constant":
                    labels.write_text(labels.read_text().replace(r"NerdOCTAXE-\xCE\xB3", r"NerdOCTAXE-\xCE\xB4"))
                elif mutation == "agent-constant":
                    labels.write_text(labels.read_text().replace('"NerdOCTAXE-Gamma"', '"NerdQAxe++"'))
                elif mutation == "include":
                    source.write_text(source.read_text().replace('#include "five_tratum_model_labels.h"', ""))
                if mutation is None:
                    fixture.package()
                    self.assertEqual(json.loads((fixture.output / "manifest.json").read_text())["deviceModel"], OCT.model)
                else:
                    with self.assertRaisesRegex(ValueError, "model/BM1370/count contract"):
                        fixture.package()
                    self.assertFalse(fixture.output.exists())

    def test_qa_requires_its_existing_literal_model_assignment(self):
        with tempfile.TemporaryDirectory() as temporary:
            fixture = PackageFixture(Path(temporary), QA)
            source = fixture.root / QA.board_source
            source.write_text(source.read_text().replace('m_deviceModel = "NerdQAxe++";',
                'm_deviceModel = FiveTratumModels::OctaxeGamma;'))
            with self.assertRaisesRegex(ValueError, "model/BM1370/count contract"):
                fixture.package()
            self.assertFalse(fixture.output.exists())

    def test_diagnostics_and_wrong_cmake_profile_fail_before_output(self):
        for key, value in (("FIVETRATUM_RELEASE_PROFILE", QA.board), ("FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER", "ON"),
                           ("FIVETRATUM_BM1370_CAPTURE", "ON"), ("FIVETRATUM_ASIC_CAPTURE_LOGS", "ON")):
            with self.subTest(key=key), tempfile.TemporaryDirectory() as temporary:
                fixture = PackageFixture(Path(temporary), OCT)
                fixture.cache[key] = value
                fixture.write_cache()
                with self.assertRaises(ValueError): fixture.package()
                self.assertFalse(fixture.output.exists())

    def test_wrong_image_version_count_and_partition_layout_are_rejected(self):
        for mutation in ("version", "count", "partition", "www", "web-version"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as temporary:
                fixture = PackageFixture(Path(temporary), OCT)
                if mutation == "version":
                    fixture.image[48:80] = b"5tratumFW-qa-0.1.0-beta.3".ljust(32, b"\0")
                    (fixture.build / "esp-miner.bin").write_bytes(fixture.image)
                elif mutation == "count":
                    source = fixture.root / OCT.board_source
                    source.write_text(source.read_text().replace("m_asicCount = 8", "m_asicCount = 4"))
                elif mutation == "partition":
                    source = fixture.root / "partitions.csv"
                    source.write_text(source.read_text().replace("0x710000", "0x720000"))
                elif mutation == "www":
                    (fixture.build / "www.bin").write_bytes(b"short")
                else:
                    (fixture.web / "main.js.gz").write_bytes(gzip.compress(b"5tratumFW-qa-0.1.0-beta.3"))
                with self.assertRaises(ValueError): fixture.package()
                self.assertFalse(fixture.output.exists())

    def test_each_public_helper_rejects_the_other_board_before_running_build_tools(self):
        for identity, wrong in ((QA, OCT), (OCT, QA)):
            environment = dict(os.environ, BOARD=wrong.board)
            result = subprocess.run(["bash", str(ROOT / identity.build_helper)], env=environment,
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(f"BOARD={identity.board} only", result.stderr)

    def test_cmake_metadata_guard_rejects_cross_family_board_and_version(self):
        source = (ROOT / "CMakeLists.txt").read_text()
        block = source[source.index("if(DEFINED FIVETRATUM_RELEASE_PROFILE)"):source.index('set(ENV{SOURCE_DATE_EPOCH}')]
        for identity in (QA, OCT):
            for wrong in (False, "board", "version"):
                with self.subTest(model=identity.model, wrong=wrong), tempfile.TemporaryDirectory() as temporary:
                    folder = Path(temporary)
                    version = f"5tratumFW-{identity.family}-0.1.0-beta.1"
                    (folder / identity.version_file).write_text(version)
                    script = folder / "profile.cmake"
                    script.write_text(f'set(FIVETRATUM_RELEASE_PROFILE "{identity.board}")\n'
                                      + (f'set(PROJECT_VER "5tratumFW-wrong-0.1.0-beta.1")\n' if wrong == "version" else "")
                                      + block + '\nmessage(STATUS "bound-version=${PROJECT_VER}")\n')
                    result = subprocess.run(["cmake", "-P", str(script)], capture_output=True, text=True,
                                            env=dict(os.environ, BOARD=QA.board if wrong == "board" and identity == OCT
                                                     else OCT.board if wrong == "board" else identity.board))
                    self.assertEqual(result.returncode == 0, wrong is False)
                    if wrong is False: self.assertIn("bound-version=" + version, result.stdout)


if __name__ == "__main__":
    unittest.main()
