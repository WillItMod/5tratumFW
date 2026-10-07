#!/usr/bin/env python3
"""Offline checker regressions plus actual C++ sender/decoder source vectors.

Synthetic vectors and published captured BM1370 CRC fixtures are distinct.
UART, RTOS and logging boundaries are stubbed; captured-format CRC validation
does not qualify our physical QAxe or establish independent work isolation.
"""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
COMPONENT = ROOT / "components/bm1397"
spec = importlib.util.spec_from_file_location("asic_capture_check", ROOT / "tools/asic_capture_check.py")
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


def method(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


def source_vectors(diagnostic=False):
    with tempfile.TemporaryDirectory(prefix="5tratum-capture-vectors-") as directory:
        path = Path(directory)
        (path / "driver").mkdir()
        (path / "rom").mkdir()
        for name in ("driver/gpio.h", "rom/gpio.h"):
            (path / name).write_text("#pragma once\n")
        mining = (COMPONENT / "include/mining.h").read_text()
        start = mining.index("typedef struct")
        end = mining.index("} bm_job;", start) + len("} bm_job;")
        stratum = (ROOT / "main/stratum/stratum_api.h").read_text()
        begin = stratum.index("typedef struct")
        finish = stratum.index("} mining_notify;", begin) + len("} mining_notify;")
        (path / "mining.h").write_text("#pragma once\n#include <cstdint>\n#include <cstddef>\n#define HASH_SIZE 32\n#define MAX_MERKLE_BRANCHES 32\n" +
            stratum[begin:finish] + "\n" + mining[start:end] + "\n")
        for name in ("asic.h", "bm1368.h", "bm1370.h"):
            (path / name).write_text((COMPONENT / "include" / name).read_text())
        sources = []
        for name in ("asic.cpp", "bm1370.cpp"):
            source = (COMPONENT / name).read_text()
            source = re.sub(r'^#include "[^"\n]+"\n', '', source, flags=re.M)
            source = source.replace('#include <endian.h>', '')
            target = path / name
            target.write_text('#include "asic_capture_vector_support.h"\n' + source)
            sources.append(str(target))
        helpers = (COMPONENT / "mining_utils.cpp").read_text()
        helper_body = '#include "asic_capture_vector_support.h"\n'
        for signature in ("uint8_t hex2val(", "size_t hex2bin(", "void swap_endian_words_bin(",
                          "void swap_endian_words(", "void reverse_bytes(", "unsigned char _reverse_bits(", "int _largest_power_of_two("):
            helper_body += method(helpers, signature)
        helper_body += method((COMPONENT / "mining.cpp").read_text(), "void construct_bm_job(")
        (path / "helpers.cpp").write_text(helper_body)
        binary = path / "vectors"
        compiler = shutil.which("clang++") or shutil.which("g++")
        if compiler is None:
            raise RuntimeError("C++ compiler required for actual-source cross checks")
        subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare",
            "-Wno-vla-cxx-extension", "-Wno-unused-const-variable", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            f"-DFIVETRATUM_BM1370_DIAGNOSTIC_DRIVER={int(diagnostic)}",
            "-I", str(path), "-I", str(ROOT / "test/host"), "-I", str(COMPONENT / "include"),
            *sources, str(COMPONENT / "crc.cpp"), str(COMPONENT / "bm1370_protocol.cpp"), str(path / "helpers.cpp"),
            str(ROOT / "test/host/asic_capture_vector_harness.cpp"), "-o", str(binary)], check=True)
        return json.loads(subprocess.run([str(binary)], check=True, capture_output=True, text=True).stdout)


def frame(header, payload):
    # Synthetic mutations only; primary positive vectors come from C++ source.
    prefix = bytes((0x55, 0xAA, header, len(payload) + (4 if header & 0x20 else 3))) + payload
    return prefix + (checker.crc16_false_source(prefix[2:]).to_bytes(2, "big") if header & 0x20 else bytes((checker.crc5_source(prefix[2:]),)))


def crc5_polynomial_reference(data):
    # Independent integer-register implementation of the pinned primary
    # source's poly05/init1f/MSB/no-reflection/no-xor parameters.
    register = 0x1F
    for byte in data:
        for bit in range(7, -1, -1):
            feedback = ((register >> 4) ^ (byte >> bit)) & 1
            register = (register << 1) & 0x1F
            if feedback:
                register ^= 0x05
    return register


class CaptureChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.vectors = source_vectors()

    def capture(self):
        return {"captureVersion": 1, "provenance": "simulated", "asicModel": "BM1370",
                "attribution": {"mode": "nerd-bm1370-v1", "addresses": [0, 64, 128, 192], "source": "simulated"},
                "works": [{"id": name, "headerHex": self.vectors["header" + name], "versionMask": 0x1FFFE000,
                           "ticketDifficulty": "1e-12"} for name in ("A", "B")], "events": []}

    def tx(self, logical=0, at=0, generation=1):
        source = self.vectors["jobs"][logical]
        return {"type": "tx", "atUs": at, "packetHex": source["packetHex"], "work": source["work"],
                "logicalJobCounter": logical, "generation": generation}

    def rx(self, logical=0, chip=0, at=1, candidate=False):
        source = self.vectors["responses"][logical * 4 + chip]
        event = {"type": "rx", "atUs": at, "packetHex": source["packetHex"]}
        if candidate:
            work = self.vectors["jobs"][logical]["work"]
            template = bytearray.fromhex(self.vectors["header" + work])
            version = int.from_bytes(template[:4], "little") | source["rolledVersionBits"]
            template[:4] = version.to_bytes(4, "little")
            template[76:] = source["nonce"].to_bytes(4, "little")
            event["candidateHeaders"] = [{"work": work, "headerHex": template.hex()}]
        return event

    def test_actual_transmit_vectors_and_header_transform(self):
        capture = self.capture()
        capture["events"] = [self.tx(i, i) for i in range(16)] + [
            {"type": "tx", "atUs": 16, "packetHex": self.vectors["addressFrame"]},
            {"type": "tx", "atUs": 17, "packetHex": self.vectors["writeFrame"]}]
        report = checker.analyze_capture(capture)
        self.assertEqual(report["summary"]["eventsWithErrors"], 0)
        self.assertTrue(all(event["checksPassed"] and event["crc"]["verified"] for event in report["events"]))
        self.assertEqual([event["job"]["wireJobId"] for event in report["events"][:16]], [job["wire"] for job in self.vectors["jobs"]])
        self.assertEqual(report["events"][-1]["commandSemantics"], "register-write-single")
        self.assertFalse(report["verification"]["independentWorkAssignment"])

    def test_diagnostic_driver_retains_actual_source_wire_and_decode_vectors(self):
        # Both variants compile the production BM1370 subclass and execute its
        # real virtual methods. Enumeration/UART are declared host fixtures,
        # so equality does not qualify our physical chain or chip isolation.
        self.assertEqual(source_vectors(diagnostic=True), self.vectors)

    def test_actual_receive_decoder_all_ids_and_chips(self):
        for source in self.vectors["responses"]:
            decoded = checker.decode_rx(bytes.fromhex(source["packetHex"]), self.capture()["attribution"])
            for field in ("nonce", "wireJobId", "rolledVersionBits", "sourceAsicIndex"):
                self.assertEqual(decoded[field], source[field])
            self.assertEqual(decoded["discardedJobBits"], 15)
            self.assertEqual(decoded["responseCrc"], checker.RX_CRC_VALID)
            self.assertTrue(decoded["crc"]["verified"])
            self.assertEqual(crc5_polynomial_reference(bytes.fromhex(source["packetHex"])[2:]), 0)
        source = self.vectors["registerResponse"]
        decoded = checker.decode_rx(bytes.fromhex(source["packetHex"]), self.capture()["attribution"])
        for field in ("data", "register", "sourceAsicIndex"):
            self.assertEqual(decoded[field], source[field])

    def test_crc5_matches_actual_source_at_all_nonoverflowing_lengths(self):
        # Current sender fixtures exercise the actual 4- and 8-byte CRC inputs.
        for field in ("addressFrame", "writeFrame"):
            packet = bytes.fromhex(self.vectors[field])
            self.assertEqual(checker.crc5_source(packet[2:-1]), packet[-1])
        for vector in self.vectors["crc5Vectors"]:
            self.assertEqual(checker.crc5_source(bytes.fromhex(vector["bytes"])), vector["crc"])
        self.assertEqual(checker.crc16_false_source(b"123456789"), 0x29B1)
        with self.assertRaises(ValueError):
            checker.crc5_source(bytes(32))

    def test_crc_length_and_preamble_failures(self):
        for index in (0, 3, 5, -1):
            capture = self.capture()
            packet = bytearray.fromhex(self.tx()["packetHex"])
            packet[index] ^= 1
            event = self.tx()
            event["packetHex"] = packet.hex()
            capture["events"] = [event]
            self.assertGreater(checker.analyze_capture(capture)["summary"]["eventsWithErrors"], 0)

    def test_unknown_job_header_never_means_targeting(self):
        capture = self.capture()
        packet = bytes.fromhex(self.tx()["packetHex"])
        capture["events"] = [{"type": "tx", "atUs": 0, "packetHex": frame(0x31, packet[4:-2]).hex()}]
        decoded = checker.analyze_capture(capture)["events"][0]
        self.assertTrue(decoded["crc"]["verified"])
        self.assertNotIn("job", decoded)
        self.assertEqual(decoded["jobSemantics"], "unknown-header-no-targeting-claim")
        capture["events"][0].update(work="A", generation=1)
        self.assertFalse(checker.analyze_capture(capture)["events"][0]["checksPassed"])

    def test_long_command_crc_stays_unverified(self):
        packet = bytes((0x55, 0xAA, 0x51, 33)) + bytes(30) + b"\x00"
        decoded = checker.decode_tx(packet)
        self.assertFalse(decoded["crc"]["verified"])
        self.assertEqual(decoded["crc"]["status"], "unverified-source-length-counter-overflow")

    def test_template_binding_and_counter_must_match(self):
        for mutation in ("header", "counter", "start"):
            capture = self.capture()
            event = self.tx()
            if mutation == "header":
                capture["works"][0]["headerHex"] = self.vectors["headerB"]
            elif mutation == "counter":
                event["logicalJobCounter"] = 1
            else:
                capture["works"][0]["startingNonce"] = 99
            capture["events"] = [event]
            decoded = checker.analyze_capture(capture)["events"][0]
            self.assertFalse(decoded["checksPassed"])
            self.assertNotIn("workBinding", decoded)

    def test_invalid_rx_crc_rejected_before_mapping_and_hash_binding(self):
        capture = self.capture()
        event = self.rx(candidate=True)
        packet = bytearray.fromhex(event["packetHex"])
        packet[-1] ^= 1
        event["packetHex"] = packet.hex()
        capture["events"] = [self.tx(), event]
        report = checker.analyze_capture(capture)
        result = report["events"][-1]
        self.assertFalse(result["checksPassed"])
        self.assertEqual(result["responseCrc"], checker.RX_CRC_FAILED)
        self.assertIn("rx-crc-mismatch", result["errors"])
        self.assertNotIn("workMapping", result)
        self.assertNotIn("fullVersion", result)
        self.assertFalse(result["candidateChecks"][0]["hashAndBindingPass"])
        self.assertEqual(report["summary"]["rxCrcFailures"], 1)
        self.assertFalse(report["verification"]["physicalDeviceQualification"])

    def test_pinned_captured_bm1370_vectors_and_every_single_bit_mutation(self):
        fixture = json.loads((ROOT / "test/host/fixtures/bm1370_captured_rx.json").read_text())
        self.assertEqual(fixture["revision"], checker.RX_CRC_EVIDENCE["revision"])
        self.assertEqual(fixture["evidenceKind"], "third-party-captured-bm1370-frames")
        self.assertEqual([case["packetHex"] for case in fixture["cases"]],
                         [case["packetHex"] for case in self.vectors["capturedRx"]])
        rejected = 0
        for case, actual in zip(fixture["cases"], self.vectors["capturedRx"]):
            packet = bytes.fromhex(case["packetHex"])
            self.assertEqual(actual["crcResidue"], 0)
            self.assertEqual(crc5_polynomial_reference(packet[2:]), 0)
            capture = {"captureVersion": 1, "provenance": "observed", "asicModel": "BM1370", "attribution": None,
                       "works": [], "events": [{"type": "rx", "atUs": 0, "packetHex": case["packetHex"]}]}
            report = checker.analyze_capture(capture)
            decoded = report["events"][0]
            self.assertTrue(decoded["checksPassed"])
            self.assertEqual(decoded["responseCrc"], checker.RX_CRC_VALID)
            self.assertIsNone(decoded["sourceAsicIndex"])
            for field in (("data", "register") if actual["isRegister"] else ("nonce", "wireJobId", "rolledVersionBits")):
                self.assertEqual(decoded[field], actual[field])
            self.assertFalse(report["verification"]["physicalDeviceQualification"])
            self.assertFalse(report["verification"]["isolationVerified"])
            for bit in range(72):
                mutated = bytearray(packet)
                mutated[2 + bit // 8] ^= 1 << (bit % 8)
                self.assertNotEqual(crc5_polynomial_reference(mutated[2:]), 0)
                rejected_result = checker.decode_rx(mutated, None)
                self.assertFalse(rejected_result["checksPassed"])
                self.assertEqual(rejected_result["responseCrc"], checker.RX_CRC_FAILED)
                rejected += 1
        self.assertEqual(rejected, 360)

    def test_response_framing_and_missing_enumeration(self):
        packet = bytes.fromhex(self.rx()["packetHex"])
        self.assertFalse(checker.decode_rx(packet[:8], None)["checksPassed"])
        self.assertFalse(checker.decode_rx(b"\x55\xaa" + packet[2:], None)["checksPassed"])
        decoded = checker.decode_rx(packet, None)
        self.assertIsNone(decoded["sourceAsicIndex"])
        self.assertIn("no-declared-enumeration", decoded["warnings"])

    def test_id_reuse_stays_ambiguous_even_with_latest_generation(self):
        capture = self.capture()
        capture["events"] = [self.tx(generation=1), self.tx(at=2, generation=2), self.rx(at=3, candidate=True)]
        result = checker.analyze_capture(capture)["events"][-1]
        self.assertEqual(result["workMapping"]["status"], "ambiguous-id-reuse")
        self.assertNotIn("fullVersion", result)
        self.assertEqual(len(result["workMapping"]["candidates"]), 2)
        self.assertEqual(result["workMapping"]["candidateCount"], 2)
        self.assertFalse(result["workMapping"]["candidatesTruncated"])
        self.assertFalse(checker.analyze_capture(capture)["verification"]["isolationVerified"])

    def test_maximum_reused_id_capture_has_bounded_report(self):
        capture = self.capture()
        tx_count = checker.MAX_EVENTS // 2
        capture["events"] = [self.tx(at=index, generation=index + 1) for index in range(tx_count)] + [
            self.rx(at=tx_count + index) for index in range(checker.MAX_EVENTS - tx_count)]
        # This is the previous quadratic case at the full 4096-event bound.
        # Header checking against a later generation must retain its work
        # membership without expanding the full history for each candidate.
        late_generation = self.tx(at=tx_count - 1, generation=tx_count)
        late_generation["work"] = "B"
        late_payload = bytearray.fromhex(self.tx(1)["packetHex"])[4:-2]
        late_payload[0] = 0
        late_generation["packetHex"] = frame(0x21, late_payload).hex()
        late_generation.pop("logicalJobCounter")
        capture["events"][tx_count - 1] = late_generation
        explicit = self.rx(candidate=True)["candidateHeaders"][0]
        explicit["work"] = "B"
        candidate_header = bytearray.fromhex(explicit["headerHex"])
        candidate_header[4:76] = bytes.fromhex(self.vectors["headerB"])[4:76]
        explicit["headerHex"] = candidate_header.hex()
        capture["events"][-1]["candidateHeaders"] = [explicit]
        self.assertLess(len(json.dumps(capture)), checker.MAX_FILE_BYTES)
        report = checker.analyze_capture(capture)
        responses = report["events"][tx_count:]
        self.assertEqual(len(responses), 2048)
        for response in responses:
            mapping = response["workMapping"]
            self.assertEqual(mapping["status"], "ambiguous-id-reuse")
            self.assertEqual(mapping["candidateCount"], tx_count)
            self.assertTrue(mapping["candidatesTruncated"])
            self.assertEqual(len(mapping["candidates"]), checker.MAX_MAPPING_PREVIEW)
            self.assertEqual([entry["generation"] for entry in mapping["candidates"]], list(range(1, 9)))
        self.assertEqual(sum(len(response["workMapping"]["candidates"]) for response in responses), 16384)
        self.assertLess(len(json.dumps(report, indent=2)), 12 * 1024 * 1024)
        self.assertTrue(responses[-1]["candidateChecks"][0]["hashAndBindingPass"])
        self.assertFalse(report["verification"]["independentWorkAssignment"])
        self.assertFalse(report["verification"]["isolationVerified"])

    def test_unbound_response_is_not_assigned_heuristically(self):
        capture = self.capture()
        capture["events"] = [self.rx(candidate=True)]
        result = checker.analyze_capture(capture)["events"][0]
        self.assertEqual(result["workMapping"]["status"], "unbound")
        self.assertIn("candidate-work-not-bound-to-prior-tx-id", result["candidateChecks"][0]["bindingErrors"])

    def test_explicit_hash_header_and_version_checks(self):
        capture = self.capture()
        capture["events"] = [self.tx(), self.rx(candidate=True)]
        result = checker.analyze_capture(capture)["events"][-1]
        self.assertTrue(result["candidateChecks"][0]["hashAndBindingPass"])
        self.assertEqual(result["fullVersion"], 0x20000000 | 0x0201 << 13)
        for byte_index, expected in ((0, "candidate-version-mismatch"), (20, "candidate-immutable-header-mismatch"), (76, "candidate-nonce-mismatch")):
            broken = copy.deepcopy(capture)
            header = bytearray.fromhex(broken["events"][-1]["candidateHeaders"][0]["headerHex"])
            header[byte_index] ^= 1
            broken["events"][-1]["candidateHeaders"][0]["headerHex"] = header.hex()
            self.assertIn(expected, checker.analyze_capture(broken)["events"][-1]["candidateChecks"][0]["bindingErrors"])

    def test_ticket_and_required_version_constraints(self):
        capture = self.capture()
        capture["events"] = [self.tx(), self.rx(candidate=True)]
        capture["works"][0]["ticketDifficulty"] = "1e64"
        self.assertFalse(checker.analyze_capture(capture)["events"][-1]["candidateChecks"][0]["meetsDeclaredTicket"])
        capture["works"][0].update(ticketDifficulty="1e-12", versionMask=0)
        self.assertIn("version-change-outside-declared-mask", checker.analyze_capture(capture)["events"][-1]["candidateChecks"][0]["bindingErrors"])
        capture["works"][0].update(versionMask=0x1FFFE000, requiredVersionMask=1, requiredVersionValue=1)
        self.assertIn("required-version-bits-mismatch", checker.analyze_capture(capture)["events"][-1]["candidateChecks"][0]["bindingErrors"])

    def test_sha256d_independent_openssl_zero_header(self):
        # Deliberately synthetic 80-zero-byte input, not a valid chain header.
        header = bytes(80)
        openssl = shutil.which("openssl")
        self.assertIsNotNone(openssl, "OpenSSL is needed for independent hash cross check")
        first = subprocess.run([openssl, "dgst", "-sha256", "-binary"], input=header, check=True, capture_output=True).stdout
        second = subprocess.run([openssl, "dgst", "-sha256", "-binary"], input=first, check=True, capture_output=True).stdout
        work = {"header": header, "versionMask": 0, "requiredVersionMask": 0, "requiredVersionValue": 0,
                "difficulty": checker.difficulty("1e-64")}
        response = checker.decode_rx(bytes.fromhex(self.vectors["zeroNonceResponse"]), None)
        result = checker.check_candidate({"work": "zero", "headerHex": header.hex()}, response, {"zero": work}, {"zero"})
        self.assertEqual(result["hashHex"], second[::-1].hex())
        self.assertTrue(result["hashAndBindingPass"])
        self.assertEqual(result["hashInteger"], str(int.from_bytes(second, "little")))

    def negative_capture(self):
        capture = self.capture()
        capture["events"] = [self.tx(), self.tx(1, at=100)] + [self.rx(1, chip, 101 + chip, candidate=True) for chip in range(4)]
        capture["negativeControl"] = {"firstWork": "A", "secondWork": "B", "secondTxEventIndex": 1,
            "windowStartUs": 101, "windowEndUs": 104, "minPerAsic": 1, "drainBasis": None}
        return capture

    def test_broadcast_negative_control_never_proves_isolation(self):
        report = checker.analyze_capture(self.negative_capture())
        negative = report["negativeControl"]
        self.assertEqual(negative["observation"], "consistent-with-chain-broadcast")
        self.assertEqual(negative["sourceDecodedSecondWorkByAsic"], [1, 1, 1, 1])
        self.assertEqual(negative["hashCheckedSecondWorkByAsic"], [1, 1, 1, 1])
        self.assertFalse(negative["isolationVerified"])
        self.assertFalse(negative["drainVerified"])
        self.assertEqual(report["provenance"], "simulated")
        self.assertFalse(report["provenanceAttested"])
        self.assertEqual(negative["evidenceKind"], "simulated-fixture")

    def test_silence_wrong_cutover_and_id_reuse_are_inconclusive(self):
        capture = self.negative_capture()
        capture["events"].pop()
        self.assertEqual(checker.analyze_capture(capture)["negativeControl"]["observation"], "inconclusive")
        capture = self.negative_capture()
        capture["negativeControl"]["secondTxEventIndex"] = 0
        self.assertEqual(checker.analyze_capture(capture)["negativeControl"]["observation"], "inconclusive")
        capture = self.negative_capture()
        payload = bytearray.fromhex(capture["events"][1]["packetHex"])[4:-2]
        payload[0] = 0
        capture["events"][1]["packetHex"] = frame(0x21, payload).hex()
        capture["events"][1].pop("logicalJobCounter")
        self.assertIn("negative-control-wire-id-reused", checker.analyze_capture(capture)["negativeControl"]["issues"])

    def test_strict_schema_types_bounds_and_monotonic_time(self):
        capture = self.capture()
        capture["events"] = [self.tx(), self.rx()]
        mutations = [lambda c: c.update(responseCrcVerified=True), lambda c: c.update(captureVersion=True),
            lambda c: c.update(asicModel="BM1368"), lambda c: c["events"][0].update(atUs=True),
            lambda c: c["events"][1].update(atUs=-1), lambda c: c["events"][0].update(generation=0),
            lambda c: c["events"][0].update(atUs=20),
            lambda c: c["events"][0].update(logicalJobCounter=1 << 32),
            lambda c: c["works"][0].update(ticketDifficulty="NaN"), lambda c: c["works"][0].update(ticketDifficulty=1.0),
            lambda c: c["works"][0].update(headerHex="00"), lambda c: c["events"][0].update(packetHex="AA55"),
            lambda c: c["attribution"].update(addresses=[0, 64, 64, 192]),
            lambda c: c["attribution"].update(source="observed-enumeration"),
            lambda c: c["events"][0].update(work="missing"), lambda c: c.update(works=c["works"] * 9),
            lambda c: c.update(events=c["events"] * 2049), lambda c: c["works"][0].update(requiredVersionValue=1),
            lambda c: c["events"][0].update(packetHex="00" * 256)]
        for mutation in mutations:
            changed = copy.deepcopy(capture)
            mutation(changed)
            with self.assertRaises(checker.InputError):
                checker.analyze_capture(changed)
        for key in ("firstWork", "secondWork", "secondTxEventIndex", "windowStartUs", "windowEndUs", "minPerAsic", "drainBasis"):
            changed = self.negative_capture()
            changed["negativeControl"][key] = []
            with self.assertRaises(checker.InputError):
                checker.analyze_capture(changed)

    def test_duplicate_nonfinite_oversized_input_and_cli_exit_codes(self):
        with tempfile.TemporaryDirectory(prefix="5tratum-capture-json-") as directory:
            path = Path(directory) / "capture.json"
            for raw in ('{"captureVersion":1,"captureVersion":1}', '{"value":NaN}', '{"value":Infinity}'):
                path.write_text(raw)
                with self.assertRaises(checker.InputError):
                    checker.read_capture(path)
            path.write_text('{"value":' + "1" * 10000 + '}')
            with self.assertRaises(checker.InputError):
                checker.read_capture(path)
            path.write_bytes(b" " * (checker.MAX_FILE_BYTES + 1))
            with self.assertRaises(checker.InputError):
                checker.read_capture(path)
            capture = self.capture()
            capture["events"] = [self.tx(), self.rx()]
            path.write_text(json.dumps(capture))
            command = [sys.executable, str(ROOT / "tools/asic_capture_check.py"), str(path)]
            process = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(process.returncode, 0, process.stderr)
            report = json.loads(process.stdout)
            self.assertEqual(report["captureSha256"], hashlib.sha256(path.read_bytes()).hexdigest())
            self.assertEqual(report["events"][-1]["hashCheck"], "skipped-no-explicit-80-byte-candidate")
            capture["events"][0]["packetHex"] = "55aa21000000"
            path.write_text(json.dumps(capture))
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 1)
            path.write_text('{"bad":"schema"}')
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
