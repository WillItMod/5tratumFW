#!/usr/bin/env python3
"""Compile real LCD profiles and configure their real CMake admission policy.

No ESP-IDF, miner, network, flash or release output is used. These checks do
not identify an LCD controller or qualify a physical 480x320 assembly.
"""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
DISPLAY = ROOT / "main/displays"
HELPER = ROOT / "cmake/display_profile.cmake"
HARNESS = ROOT / "test/host/display_profile_harness.cpp"
STOCK = {
    "profile": "tdisplay-s3-320x170", "width": 320, "height": 170,
    "pixelClock": 6528000, "bufferPixels": 13600, "bufferBytes": 27200,
    "bgr": False, "gapY": 35, "experimental": False,
    "mirrorNormal": [True, False], "mirrorFlipped": [False, True],
}
RGB = {
    "profile": "experimental-qaxe-480x320-rgb", "width": 480, "height": 320,
    "pixelClock": 12288000, "bufferPixels": 25600, "bufferBytes": 51200,
    "bgr": False, "gapY": 0, "experimental": True,
    "mirrorNormal": [False, False], "mirrorFlipped": [True, True],
}
BGR = {
    "profile": "experimental-qaxe-480x320-bgr", "width": 480, "height": 320,
    "pixelClock": 12000000, "bufferPixels": 25600, "bufferBytes": 51200,
    "bgr": True, "gapY": 0, "experimental": True,
    "mirrorNormal": [False, False], "mirrorFlipped": [True, True],
}


class DisplayProfileTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cmake = shutil.which("cmake")
        cls.cxx = shutil.which("c++")
        if not cls.cmake or not cls.cxx:
            raise RuntimeError("Display profile checks require CMake and a native C++ compiler")

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="display-profile-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.source = self.directory / "source"
        self.source.mkdir()
        # Include the production helper, rather than reimplementing its guard.
        (self.source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.16)\n"
            "project(DisplayProfileChecks LANGUAGES CXX)\n"
            "set(CMAKE_CXX_STANDARD 17)\n"
            "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
            f'include("{HELPER.as_posix()}")\n'
            f'add_executable(profile "{HARNESS.as_posix()}")\n'
            f'target_include_directories(profile PRIVATE "{DISPLAY.as_posix()}")\n')

    def configure(self, *, board="NERDQAXEPLUS2", profile=None, opt_in=None,
                  public=None, build="build"):
        environment = os.environ.copy()
        if board is None:
            environment.pop("BOARD", None)
        else:
            environment["BOARD"] = board
        command = [self.cmake, "-S", str(self.source), "-B", str(self.directory / build)]
        if profile is not None:
            command.append(f"-DFIVETRATUM_DISPLAY_PROFILE={profile}")
        if opt_in is not None:
            command.append(f"-DFIVETRATUM_EXPERIMENTAL_DISPLAY={opt_in}")
        if public is not None:
            command.append(f"-DFIVETRATUM_RELEASE_PROFILE={public}")
        return subprocess.run(command, env=environment, capture_output=True, text=True)

    def compile_and_read(self, build="build"):
        directory = self.directory / build
        # Makefile generators can miss a flag-file change within the same
        # timestamp tick; compile each selected configuration afresh while
        # retaining its actual CMake cache for the reconfiguration check.
        result = subprocess.run([self.cmake, "--build", str(directory), "--clean-first"],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result = subprocess.run([str(directory / "profile")],
                                capture_output=True, text=True, check=True)
        return json.loads(result.stdout)

    def assert_configured_profile(self, expected, **arguments):
        result = self.configure(**arguments)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        actual = self.compile_and_read(arguments.get("build", "build"))
        runtime = arguments.get("board", "NERDQAXEPLUS2") in ("NERDQAXEPLUS2", "NERDOCTAXEGAMMA")
        self.assertEqual(actual.pop("runtimeDisplaySelector"), runtime)
        cache = (self.directory / arguments.get("build", "build") / "CMakeCache.txt").read_text()
        self.assertIn("FIVETRATUM_RUNTIME_DISPLAY_SELECTOR:BOOL=" + ("ON" if runtime else "OFF"), cache)
        self.assertEqual(actual, expected)

    def test_default_cmake_selection_preserves_stock(self):
        self.assert_configured_profile(STOCK)

    def test_explicit_stock_and_public_families_preserve_stock(self):
        for board in ["NERDQAXEPLUS2", "NERDOCTAXEGAMMA"]:
            with self.subTest(board=board):
                self.assert_configured_profile(
                    STOCK, board=board, profile="TDISPLAY_S3_320X170",
                    public=board, build=board)

    def test_other_and_missing_targets_keep_stock_without_runtime_wide_profiles(self):
        for board in ["NERDOCTAXEPLUS", "NERDQAXEPLUS", "NERDQAXEPLUS2-extra",
                      "NERDOCTAXEGAMMA-extra", "", None]:
            with self.subTest(board=board):
                self.assert_configured_profile(STOCK, board=board, build=f"stock-{board}")

    def test_board_reconfiguration_resets_runtime_capability(self):
        for board in ["NERDQAXEPLUS2", "NERDOCTAXEGAMMA", "NERDOCTAXEPLUS",
                      "NERDOCTAXEGAMMA", None]:
            with self.subTest(board=board):
                self.assert_configured_profile(STOCK, board=board)

    def test_explicit_private_rgb_and_bgr_profiles(self):
        for profile, expected in [("QAXE_480X320_RGB", RGB), ("QAXE_480X320_BGR", BGR)]:
            with self.subTest(profile=profile):
                self.assert_configured_profile(expected, profile=profile,
                                               opt_in="ON", build=profile)

    def test_wide_requires_explicit_opt_in(self):
        for profile in ["QAXE_480X320_RGB", "QAXE_480X320_BGR"]:
            for opt_in in [None, "OFF"]:
                with self.subTest(profile=profile, opt_in=opt_in):
                    result = self.configure(profile=profile, opt_in=opt_in,
                                            build=f"{profile}-{opt_in}")
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("explicit experimental opt-in", result.stdout + result.stderr)

    def test_wide_rejects_wrong_or_missing_board(self):
        for profile in ["QAXE_480X320_RGB", "QAXE_480X320_BGR"]:
            for board in ["NERDOCTAXEGAMMA", "NERDOCTAXEPLUS", "NERDQAXEPLUS",
                          "NERDQAXEPLUS2-extra", "NERDOCTAXEGAMMA-extra", None]:
                with self.subTest(board=board, profile=profile):
                    result = self.configure(board=board, profile=profile,
                                            opt_in="ON", build=f"wrong-{profile}-{board}")
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("BOARD=NERDQAXEPLUS2", result.stdout + result.stderr)

    def test_public_build_cannot_admit_wide_even_with_opt_in(self):
        for profile in ["QAXE_480X320_RGB", "QAXE_480X320_BGR"]:
            for public in ["NERDQAXEPLUS2", ""]:
                with self.subTest(profile=profile, public=public):
                    result = self.configure(profile=profile, opt_in="ON", public=public,
                                            build=f"public-{profile}-{bool(public)}")
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("not admitted by the public OTA packaging/updater policy",
                                  " ".join((result.stdout + result.stderr).split()))

    def test_unknown_and_empty_display_profiles_fail_configuration(self):
        for profile in ["", "QAXE_480X320", "TDISPLAY_S3_320X170-extra"]:
            with self.subTest(profile=profile):
                result = self.configure(profile=profile, opt_in="ON", build=f"invalid-{profile}")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Unknown 5tratumFW display profile", result.stdout + result.stderr)

    def test_same_build_reconfiguration_does_not_keep_wide_definitions(self):
        self.assert_configured_profile(RGB, profile="QAXE_480X320_RGB", opt_in="ON")
        self.assert_configured_profile(BGR, profile="QAXE_480X320_BGR", opt_in="ON")
        self.assert_configured_profile(STOCK, profile="TDISPLAY_S3_320X170")

    def test_header_alone_defaults_to_original_profile(self):
        binary = self.directory / "header-default"
        subprocess.run([self.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-I", str(DISPLAY), str(HARNESS), "-o", str(binary)],
                       capture_output=True, text=True, check=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True, check=True)
        actual = json.loads(result.stdout)
        self.assertFalse(actual.pop("runtimeDisplaySelector"))
        self.assertEqual(actual, STOCK)

    def test_header_only_runtime_support_requires_an_exact_admitted_target(self):
        for board in ["NERDQAXEPLUS2", "NERDOCTAXEGAMMA", "NERDOCTAXEPLUS",
                      "NERDQAXEPLUS", "NERDQAXEPLUS2_EXTRA", "NERDOCTAXEGAMMA_EXTRA"]:
            with self.subTest(board=board):
                binary = self.directory / f"header-{board}"
                subprocess.run([self.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                                f"-D{board}", "-I", str(DISPLAY), str(HARNESS),
                                "-o", str(binary)], capture_output=True, text=True, check=True)
                result = subprocess.run([str(binary)], capture_output=True, text=True, check=True)
                actual = json.loads(result.stdout)
                self.assertEqual(actual.pop("runtimeDisplaySelector"),
                                 board in ("NERDQAXEPLUS2", "NERDOCTAXEGAMMA"))
                self.assertEqual(actual, STOCK)

    def test_header_rejects_invalid_compiler_profile(self):
        result = subprocess.run(
            [self.cxx, "-std=c++17", "-DFIVETRATUM_LCD_PROFILE=3", "-I", str(DISPLAY),
             str(HARNESS), "-o", str(self.directory / "invalid")],
            capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Unknown 5tratumFW LCD profile", result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
