#!/usr/bin/env python3
"""Offline raw-recorder regressions; no UART, network or hardware operations.

The positive mining roundtrip is a published third-party capture, pinned to
Mujina 980128574d8bb8b9619b0081b26e18eb4d0c5e31 test_data.rs:119/203.
Mutated headers, transport records, topology and page envelopes are synthetic.
"""
import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stdout, redirect_stderr

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
spec = importlib.util.spec_from_file_location("bm1370_capture_analyze", ROOT / "tools/bm1370_capture_analyze.py")
analyzer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analyzer)

# Exact observed Bitaxe Gamma full-header TX and accepted nonce, not generated
# by our sender or decoder. This does not qualify the user's QAxe.
CAPTURED_TX = bytes.fromhex(
    "55aa2156680100000000043a0217d7685468"
    "5519a7cb044f88726355919e61a98bcf71a0c28795ea54db8c36414b06ddf5f0"
    "0000000000000000965201001d3996bca3f4670dfcd4f201c162b96dfd55646b"
    "00000020721c")
CAPTURED_RX = bytes.fromhex("aa554c0352750cd205a29c")


def canonical_template(packet):
    # Independent, explicit eight-word reversal from the primary wire-layout
    # description; do not call the analyzer's header binding implementation.
    merkle = b"".join(packet[18 + i * 4:22 + i * 4] for i in range(7, -1, -1))
    previous = b"".join(packet[50 + i * 4:54 + i * 4] for i in range(7, -1, -1))
    return packet[82:86] + previous + merkle + packet[14:18] + packet[10:14] + packet[6:10]


TEMPLATE = canonical_template(CAPTURED_TX)


def tx_for_header(header, counter):
    word_reverse = lambda value: b"".join(value[i * 4:i * 4 + 4] for i in range(7, -1, -1))
    body = bytes(((counter * 24) & 127, 1)) + header[76:80] + header[72:76] + header[68:72]
    body += word_reverse(header[36:68]) + word_reverse(header[4:36]) + header[:4]
    before_crc = b"\x55\xaa\x21\x56" + body
    return before_crc + analyzer.source.crc16_false_source(before_crc[2:]).to_bytes(2, "big")


def record(kind, sequence, packet=b"", **extra):
    row = {"sequence": sequence, "timestampUs": 100 + sequence * 10, "generation": 0,
           "kind": kind, "marker": 0, "requestedLength": len(packet), "transportReturned": len(packet),
           "capturedLength": len(packet), "packetHex": packet.hex(), "flags": 0, "auxiliaryValue": 0}
    if kind == "tx-job":
        row.update(generation=1, headerHex=TEMPLATE.hex(), logicalCounter=15,
                   versionMask=0x1FFFE000, ticketDifficulty="8192", poolIndex=0)
    row.update(extra)
    return row


def capture(records=None, **status_changes):
    if records is None:
        records = [record("tx-job", 1, CAPTURED_TX), record("rx-chunk", 2, CAPTURED_RX)]
    status = {"active": False, "frozen": True, "recordCount": len(records), "capacity": 384,
              "incompleteFlags": 0, "freezeReason": "manual", "droppedBusy": 0, "startUs": 100,
              "deadlineUs": 2_000_100, "stoppedUs": 20_000, "physicalWireComplete": False,
              "physicalChipIdentity": False, "independentWorkAssignment": False}
    status.update(status_changes)
    return {"schemaVersion": 1, "captureId": 1, "status": status,
            "deviceBinding": {"firmwareVersion": "5tratumFW-qa-capture-test", "boardModel": "NerdQAxe++",
                              "asicModel": "BM1370", "asicCount": 4, "deviceId": "5tfw:" + "a" * 32,
                              "diagnosticDriver": True, "captureBuild": True}, "records": records}


def crc_rx(packet):
    packet = bytearray(packet)
    type_bits = packet[-1] & 0xE0
    for checksum in range(32):
        packet[-1] = type_bits | checksum
        if analyzer.source.crc5_source(packet[2:]) == 0:
            return bytes(packet)
    raise AssertionError("no CRC trailer")


class RawRecorderChecks(unittest.TestCase):
    def test_published_capture_reconstructs_accepted_header_and_pool(self):
        self.assertEqual(len(CAPTURED_TX), 88)
        self.assertEqual(len(TEMPLATE), 80)
        report = analyzer.analyze_capture(capture())
        self.assertFalse(report["issues"])
        frame = report["rx"][0]
        self.assertEqual(frame["mapping"], "unique-match-within-captured-contexts")
        candidate = frame["matchingContexts"][0]
        header = bytearray(TEMPLATE)
        header[:4] = (0x20B44000).to_bytes(4, "little")
        header[76:] = bytes.fromhex("4c035275")
        digest = hashlib.sha256(hashlib.sha256(header).digest()).digest()
        self.assertEqual(candidate["hashHex"], digest[::-1].hex())
        self.assertAlmostEqual(float(candidate["computedDifficulty"]), 29588, delta=1)
        self.assertEqual(candidate["fullVersion"], 0x20B44000)
        self.assertEqual(frame["wireJobId"], 0x68)
        self.assertEqual(report["summary"]["uniqueHashMatchesByCapturedPool"], {"primary": 1, "secondary": 0, "unknown": 0})
        # Cross-check SHA256d through a separate installed implementation.
        first = subprocess.run(["openssl", "dgst", "-sha256", "-binary"], input=header, capture_output=True, check=True).stdout
        second = subprocess.run(["openssl", "dgst", "-sha256", "-binary"], input=first, capture_output=True, check=True).stdout
        self.assertEqual(second, digest)
        self.assertTrue(frame["preArmWorkUnknown"])
        self.assertFalse(frame["candidateSetComplete"])
        self.assertFalse(report["verification"]["physicalChipIdentity"])
        self.assertFalse(report["verification"]["isolationVerified"])
        self.assertNotIn("sourceAsicIndex", frame)

    def test_every_read_split_noise_and_back_to_back_frames(self):
        for split in range(1, 11):
            with self.subTest(split=split):
                rows = [record("tx-job", 1, CAPTURED_TX), record("rx-chunk", 2, b"\x01\xaa\x00" + CAPTURED_RX[:split]),
                        record("rx-chunk", 3, CAPTURED_RX[split:] + CAPTURED_RX)]
                report = analyzer.analyze_capture(capture(rows))
                self.assertEqual(report["summary"]["nonceFrames"], 2)
                self.assertEqual(report["summary"]["discardedRxBytes"], 3)
                self.assertFalse(report["summary"]["partialRxBytesAtFreeze"])
                self.assertEqual(report["rx"][0]["firstRecordSequence"], 2)
                self.assertEqual(report["rx"][0]["lastRecordSequence"], 3)

    def test_corrupt_crc_resynchronizes_and_never_hashes_bad_frame(self):
        for byte in range(2, 11):
            for bit in range(8):
                bad = bytearray(CAPTURED_RX)
                bad[byte] ^= 1 << bit
                rows = [record("tx-job", 1, CAPTURED_TX), record("rx-chunk", 2, bytes(bad) + CAPTURED_RX)]
                report = analyzer.analyze_capture(capture(rows))
                self.assertEqual(report["summary"]["rxCrcFailures"], 1)
                self.assertEqual(report["summary"]["nonceFrames"], 1)
                self.assertNotIn("matchingContexts", report["rx"][0])

    def test_valid_crc_unknown_response_type_is_not_nonce(self):
        for type_bits in (1, 2, 3, 5, 6, 7):
            unknown = bytearray(CAPTURED_RX)
            unknown[-1] = type_bits << 5
            rows = [record("tx-job", 1, CAPTURED_TX), record("rx-chunk", 2, crc_rx(unknown) + CAPTURED_RX)]
            report = analyzer.analyze_capture(capture(rows))
            self.assertEqual(report["summary"]["unsupportedResponseTypes"], 1)
            self.assertEqual(report["summary"]["nonceFrames"], 1)

    def test_reused_wire_id_keeps_all_pool_contexts_not_latest(self):
        other = bytearray(TEMPLATE)
        other[36] ^= 1
        rows = [record("tx-job", 1, CAPTURED_TX, poolIndex=0),
                record("tx-job", 2, tx_for_header(other, 31), headerHex=other.hex(), logicalCounter=31, poolIndex=1),
                record("rx-chunk", 3, CAPTURED_RX)]
        frame = analyzer.analyze_capture(capture(rows))["rx"][0]
        self.assertTrue(frame["wireIdReused"])
        self.assertEqual(frame["candidateCount"], 2)
        self.assertEqual(frame["matchingContexts"][0]["poolIndex"], 0)
        self.assertEqual(frame["matchingContexts"][0]["txSequence"], 1)

    def test_identical_header_reuse_remains_ambiguous_across_generation(self):
        rows = [record("tx-job", 1, CAPTURED_TX, poolIndex=0),
                record("retirement", 2, marker=1, generation=2),
                record("tx-job", 3, CAPTURED_TX, logicalCounter=31, generation=2, poolIndex=1),
                record("rx-chunk", 4, CAPTURED_RX)]
        report = analyzer.analyze_capture(capture(rows))
        frame = report["rx"][0]
        self.assertEqual(frame["mapping"], "ambiguous-matching-contexts")
        self.assertEqual(frame["matchingContextCount"], 2)
        self.assertTrue(frame["matchingContexts"][0]["softwareRetired"])
        self.assertFalse(frame["matchingContexts"][1]["softwareRetired"])
        self.assertFalse(report["verification"]["pipelineDrainVerified"])

    def test_partial_rx_continues_across_software_annotation_not_flush(self):
        for marker in (1, 2, 3, 4):
            rows = [record("tx-job", 1, CAPTURED_TX), record("rx-chunk", 2, CAPTURED_RX[:4]),
                    record("retirement", 3, marker=marker), record("rx-chunk", 4, CAPTURED_RX[4:])]
            report = analyzer.analyze_capture(capture(rows))
            self.assertEqual(report["summary"]["nonceFrames"], 1)
        rows[2] = record("transport", 3, marker=4, flags=0x201)
        report = analyzer.analyze_capture(capture(rows, incompleteFlags=0x201))
        self.assertEqual(report["summary"]["nonceFrames"], 0)
        self.assertFalse(report["verification"]["softwareObservationComplete"])

    def test_version_mask_violation_cannot_bind_valid_sha(self):
        data = capture()
        data["records"][0]["versionMask"] = 0
        frame = analyzer.analyze_capture(data)["rx"][0]
        self.assertEqual(frame["matchingContextCount"], 0)
        self.assertTrue(frame["candidates"][0]["meetsCapturedTicket"])
        self.assertIn("version-change-outside-captured-mask", frame["candidates"][0]["bindingErrors"])

    def test_header_counter_and_tx_crc_must_all_agree(self):
        variants = []
        data = capture(); data["records"][0]["headerHex"] = "01" + TEMPLATE[1:].hex(); variants.append(data)
        data = capture(); data["records"][0]["logicalCounter"] = 14; variants.append(data)
        data = capture(); data["records"][0]["packetHex"] = CAPTURED_TX[:-1].hex() + "00"; variants.append(data)
        for data in variants:
            report = analyzer.analyze_capture(data)
            self.assertEqual(report["summary"]["jobContexts"], 0)
            self.assertEqual(report["summary"]["txCheckFailures"], 1)
            self.assertEqual(report["rx"][0]["mapping"], "unbound")

    def test_short_and_failed_tx_are_intent_not_transmitted_work(self):
        for count in (-1, 0, 87, 89):
            data = capture(); data["records"][0]["transportReturned"] = count
            report = analyzer.analyze_capture(data)
            self.assertIn("short-or-failed-tx", report["issues"])
            self.assertEqual(report["summary"]["jobContexts"], 0)
            self.assertEqual(report["rx"][0]["mapping"], "unbound")

    def test_more_than_sixteen_headers_and_bounded_ambiguity_preview(self):
        rows = []
        for counter in range(32):
            header = bytearray(TEMPLATE)
            if counter != 15:
                header[36] ^= counter + 1
            rows.append(record("tx-job", len(rows) + 1, tx_for_header(header, counter),
                               headerHex=header.hex(), logicalCounter=counter, poolIndex=counter % 2))
        rows.append(record("rx-chunk", 33, CAPTURED_RX))
        report = analyzer.analyze_capture(capture(rows))
        self.assertEqual(report["summary"]["jobContexts"], 32)
        self.assertEqual(report["rx"][0]["matchingContexts"][0]["txSequence"], 16)
        rows = [record("tx-job", i + 1, CAPTURED_TX, logicalCounter=15 + 16 * i) for i in range(24)]
        rows.append(record("rx-chunk", 25, CAPTURED_RX))
        frame = analyzer.analyze_capture(capture(rows))["rx"][0]
        self.assertEqual(frame["candidateCount"], 24)
        self.assertEqual(frame["matchingContextCount"], 24)
        self.assertEqual(len(frame["candidates"]), 8)
        self.assertEqual(len(frame["matchingContexts"]), 8)
        self.assertTrue(frame["candidatesTruncated"])
        self.assertEqual(frame["mapping"], "ambiguous-matching-contexts")

    def test_partial_frame_and_missing_pre_arm_context_stay_unknown(self):
        report = analyzer.analyze_capture(capture([record("rx-chunk", 1, CAPTURED_RX)]))
        self.assertEqual(report["rx"][0]["mapping"], "unbound")
        report = analyzer.analyze_capture(capture([record("rx-chunk", 1, CAPTURED_RX[:5])]))
        self.assertEqual(report["summary"]["partialRxBytesAtFreeze"], 5)
        self.assertIn("capture-ends-with-partial-rx-frame", report["warnings"])

    def test_capacity_flags_busy_loss_and_clock_sequence_fail_closed(self):
        for change in ({"incompleteFlags": 3, "freezeReason": "capacity"}, {"droppedBusy": 1},
                       {"incompleteFlags": 0x81}, {"freezeReason": "invalid-clock"}):
            self.assertFalse(analyzer.analyze_capture(capture(**change))["verification"]["softwareObservationComplete"])
        for key, value in (("sequence", 4), ("timestampUs", 90), ("flags", 65)):
            data = capture(); data["records"][1][key] = value
            self.assertFalse(analyzer.analyze_capture(data)["verification"]["softwareObservationComplete"])
        data = capture(); data["records"][1]["transportReturned"] = 10
        report = analyzer.analyze_capture(data)
        self.assertIn("rx-chunk-transport-length-mismatch", report["issues"])
        self.assertFalse(report["rx"])

    def test_invalid_order_never_creates_a_prior_context_or_cross_gap_frame(self):
        data = capture(); data["records"][0]["timestampUs"] = 150
        report = analyzer.analyze_capture(data)
        self.assertFalse(report["verification"]["softwareObservationComplete"])
        self.assertFalse(report["rx"])
        data = capture(); data["records"][0]["sequence"] = 3
        report = analyzer.analyze_capture(data)
        self.assertEqual(report["summary"]["jobContexts"], 0)
        self.assertEqual(report["rx"][0]["mapping"], "unbound")
        rows = [record("tx-job", 1, CAPTURED_TX), record("rx-chunk", 2, CAPTURED_RX[:4]),
                record("rx-chunk", 3, b"\x01", transportReturned=-1),
                record("rx-chunk", 4, CAPTURED_RX[4:])]
        report = analyzer.analyze_capture(capture(rows))
        self.assertEqual(report["summary"]["nonceFrames"], 0)

    def test_maximum_capture_is_bounded_and_capacity_is_incomplete(self):
        rows = [record("tx-job", i + 1, CAPTURED_TX, logicalCounter=15 + 16 * i) for i in range(192)]
        chunk = CAPTURED_RX * 8
        rows.extend(record("rx-chunk", i + 1, chunk) for i in range(192, 384))
        report = analyzer.analyze_capture(capture(rows, incompleteFlags=3, freezeReason="capacity"))
        self.assertEqual(report["summary"]["jobContexts"], 192)
        self.assertEqual(report["summary"]["nonceFrames"], 1536)
        self.assertFalse(report["verification"]["softwareObservationComplete"])
        self.assertTrue(all(frame["candidateCount"] == 192 and len(frame["candidates"]) == 8 and
                            frame["matchingContextCount"] == 192 and len(frame["matchingContexts"]) == 8
                            for frame in report["rx"]))
        self.assertLess(len(json.dumps(report).encode()), 12 * 1024 * 1024)

    def pages(self):
        rows = [record("rx-chunk", i + 1, b"\x01") for i in range(10)]
        data = capture(rows)
        heap = dict(captureStorageBytes=98304, freeInternal=100000, largestInternal=50000,
                    minimumInternal=80000, freePsram=6000000, largestPsram=5000000)
        pages = []
        for start in (0, 8):
            page = copy.deepcopy(data)
            page.update(offset=start, nextOffset=min(start + 8, 10), records=rows[start:start + 8], heap=dict(heap))
            pages.append(page)
        return data, pages

    def test_actual_numeric_page_cursor_and_mutable_export_heap(self):
        data, pages = self.pages()
        pages[1]["heap"]["freeInternal"] -= 1000
        merged = analyzer.assemble_pages(pages)
        self.assertEqual(merged["records"], data["records"])
        self.assertNotIn("offset", merged)
        self.assertEqual(merged["heap"], pages[0]["heap"])
        single = capture(); single.update(offset=0, nextOffset=2)
        analyzer.validate_capture(single)

    def test_wrong_session_overlap_gap_or_missing_pages_rejected(self):
        _, pages = self.pages()
        for mutate in (lambda p: p[1].update(captureId=2),
                       lambda p: p[1].update(offset=7), lambda p: p[1].update(offset=9),
                       lambda p: p[1]["deviceBinding"].update(deviceId="5tfw:" + "b" * 32),
                       lambda p: p[1]["status"].update(droppedBusy=1)):
            changed = copy.deepcopy(pages); mutate(changed)
            with self.assertRaises(analyzer.InputError): analyzer.assemble_pages(changed)
        with self.assertRaises(analyzer.InputError): analyzer.assemble_pages(pages[:1])
        with self.assertRaises(analyzer.InputError): analyzer.validate_capture(pages[0])

    def test_closed_schema_types_bounds_and_no_physical_attestation(self):
        mutations = [lambda d: d.update(schemaVersion=True), lambda d: d.update(captureId=0),
                     lambda d: d["status"].update(active=True), lambda d: d["status"].update(physicalChipIdentity=True),
                     lambda d: d["status"].update(deadlineUs=2_000_101), lambda d: d["status"].update(capacity=385),
                     lambda d: d["deviceBinding"].update(asicCount=True), lambda d: d["records"][0].update(poolIndex=2),
                     lambda d: d["records"][0].update(ticketDifficulty="NaN"),
                     lambda d: d["records"][0].update(ticketDifficulty="4294967296"),
                     lambda d: d["records"][0].update(generation=0), lambda d: d["records"][0].update(extra="bad"),
                     lambda d: d["records"][1].update(packetHex="AA"),
                     lambda d: d["records"][1].update(capturedLength=88),
                     lambda d: d["records"][1].update(transportReturned=True), lambda d: d.update(extra="bad")]
        for mutate in mutations:
            data = capture(); mutate(data)
            with self.assertRaises(analyzer.InputError): analyzer.validate_capture(data)

    def test_file_limits_duplicates_nonfinite_pages_and_cli_exit_codes(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.json"
            for raw in (b'{"schemaVersion":1,"schemaVersion":1}', b'{"value":NaN}', b"x" * (analyzer.MAX_FILE_BYTES + 1)):
                path.write_bytes(raw)
                with self.assertRaises(analyzer.InputError): analyzer.read_capture(path)
            path.write_text(json.dumps(capture()))
            data, digest = analyzer.read_capture(path)
            self.assertEqual(digest, hashlib.sha256(path.read_bytes()).hexdigest())
            with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                self.assertEqual(analyzer.main([str(path)]), 0)
                path.write_text(json.dumps(capture(droppedBusy=1)))
                self.assertEqual(analyzer.main([str(path)]), 1)
                path.write_text('{"bad":1}')
                self.assertEqual(analyzer.main([str(path)]), 2)
            _, pages = self.pages(); path.write_text(json.dumps(pages))
            data, _ = analyzer.read_capture(path)
            self.assertEqual(len(data["records"]), 10)


if __name__ == "__main__":
    unittest.main()
