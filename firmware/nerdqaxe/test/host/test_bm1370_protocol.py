#!/usr/bin/env python3
"""Compile and test the pure production BM1370 codec against stock source.

Published captured RX bytes are third-party format evidence, not a capture
from our QAxe. No hardware, UART device, clock, initialization or OTA calls.
"""
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
COMPONENT = ROOT / "components/bm1397"


def method(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


def stock_source():
    asic = (COMPONENT / "asic.cpp").read_text()
    bm1370 = (COMPONENT / "bm1370.cpp").read_text()
    header = (COMPONENT / "include/asic.h").read_text()
    packed = header[header.index("typedef struct __attribute__((__packed__))"):header.index("} BM1368_job;") + len("} BM1368_job;")]
    definitions = "\n".join(line for line in header.splitlines() if re.match(r"#define (TYPE_JOB|TYPE_CMD|GROUP_SINGLE|GROUP_ALL|CMD_WRITE|CMD_READ)\s", line))
    return """#pragma once
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "crc.h"
enum packet_type_t { JOB_PACKET, CMD_PACKET };
struct bm_job { uint32_t starting_nonce, target, ntime, version; uint8_t merkle_root_be[32], prev_block_hash_be[32]; };
static std::array<uint8_t,88> stockBytes{};
static int stockLength = 0;
static int SERIAL_send(uint8_t *bytes, int length) {
    assert(length >= 0 && length <= 88); stockLength = length;
    std::memcpy(stockBytes.data(), bytes, length); return length;
}
class StockAsic {
public:
    bool m_transportFailed = false;
    void send(uint8_t, uint8_t *, uint8_t);
    void send2(uint8_t, uint8_t, uint8_t);
    uint8_t sendWork(uint32_t, bm_job *);
    uint8_t jobToAsicId(uint8_t);
    void read(uint8_t address, uint8_t reg) { send2(0x42, address, reg); }
};
""" + packed + "\n" + definitions + "\n" + "\n".join(
        method(source, signature).replace("Asic::", "StockAsic::").replace("BM1370::", "StockAsic::")
        for source, signature in (
            (asic, "void Asic::send("), (asic, "void Asic::send2("),
            (asic, "uint8_t Asic::sendWork("), (bm1370, "uint8_t BM1370::jobToAsicId(")))


class ProtocolTests(unittest.TestCase):
    def test_actual_codec_captured_frames_stock_wire_and_stream_safety(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        self.assertIsNotNone(compiler, "C++ compiler required")
        with tempfile.TemporaryDirectory(prefix="5tratum-bm1370-codec-") as directory:
            path = Path(directory)
            reference = stock_source()
            (path / "stock_source.h").write_text(reference)
            binary = path / "codec-tests"
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Wno-vla-cxx-extension",
                "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-I", str(path),
                "-I", str(COMPONENT / "include"), str(COMPONENT / "bm1370_protocol.cpp"),
                str(COMPONENT / "crc.cpp"), str(ROOT / "test/host/bm1370_protocol_harness.cpp"), "-o", str(binary)], check=True)
            report = json.loads(subprocess.run([str(binary)], check=True, capture_output=True, text=True).stdout)
            self.assertEqual(report["groupsPassed"], 8)
            self.assertEqual(report["rejectedSingleBitMutations"], 360)
            self.assertFalse(report["heapAllocationAllowed"])
            self.assertFalse(report["ownQAxeHardwareTested"])
            self.assertFalse(report["independentWorkAssignment"])
            proof = ROOT / ".cache/bm1370-protocol"
            proof.mkdir(parents=True, exist_ok=True)
            (proof / "stock-source-used.h").write_text(reference)
            report.update({"sanitizers": ["address", "undefined"],
                "stockSourceSha256": hashlib.sha256(reference.encode()).hexdigest(),
                "capturedFixtureRevision": "980128574d8bb8b9619b0081b26e18eb4d0c5e31",
                "capturedFixtureScope": "published third-party BM1370 responses; no own-device or work-isolation proof",
                "ownedManifest": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in (
                    "components/bm1397/include/bm1370_protocol.h", "components/bm1397/bm1370_protocol.cpp",
                    "test/host/bm1370_protocol_harness.cpp", "test/host/test_bm1370_protocol.py")}})
            (proof / "proof.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    unittest.main()
