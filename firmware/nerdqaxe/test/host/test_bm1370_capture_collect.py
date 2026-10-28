#!/usr/bin/env python3
"""Bounded collector tests with synthetic HTTP responses; no miner access.

The accepted nonce/header vector comes from the existing analyzer's published
third-party capture fixture. Simulated paging/authentication does not qualify
physical targeting, chip identity, transport completeness or pipeline drain.
"""
import copy
from email.message import Message
import io
import json
from pathlib import Path
import stat
import sys
import tempfile
import unittest
import urllib.error
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import bm1370_capture_collect as collector
from test_bm1370_capture_analyze import capture, record, CAPTURED_TX, CAPTURED_RX

DEVICE_ID = "5tfw:" + "a" * 32


def status_for(data):
    result = copy.deepcopy(data)
    result["records"] = []
    return result


def pages_for(data):
    rows = data["records"]
    output = []
    for offset in range(0, max(1, len(rows)), 8):
        page = copy.deepcopy(data)
        page.update(offset=offset, nextOffset=min(offset + 8, len(rows)), records=rows[offset:offset + 8])
        output.append(page)
    return output


def disabled():
    result = status_for(capture())
    result["captureId"] = 0
    result["status"].update(frozen=False, recordCount=0, capacity=0, freezeReason="none",
                            startUs=0, deadlineUs=0, stoppedUs=0)
    return result


class FakeClient:
    def __init__(self, responses):
        self.responses = responses
        self.calls = []

    def request(self, query="", body=None):
        self.calls.append((query, body))
        if not self.responses:
            raise AssertionError("unexpected extra HTTP request")
        response = self.responses.pop(0)
        if isinstance(response, Exception):
            raise response
        return copy.deepcopy(response)


class FakeResponse(io.BytesIO):
    def __init__(self, raw, content_type="application/json", status=200):
        super().__init__(raw)
        self.status = status
        self.headers = Message()
        self.headers["Content-Type"] = content_type


class CollectorChecks(unittest.TestCase):
    def test_existing_frozen_capture_is_read_only_and_hash_checked(self):
        data = capture()
        client = FakeClient([status_for(data), *pages_for(data)])
        pages, report = collector.collect_capture(client, DEVICE_ID, capture_id=1)
        self.assertEqual(client.calls, [("", None), ("?captureId=1&offset=0", None)])
        self.assertEqual(pages, pages_for(data))
        self.assertEqual(report["summary"]["uniqueHashMatchesByCapturedPool"]["primary"], 1)
        self.assertFalse(report["verification"]["independentWorkAssignment"])
        self.assertFalse(report["verification"]["isolationVerified"])

    def test_arm_is_sent_exactly_once_then_export_waits_for_frozen_snapshot(self):
        data = capture()
        armed = status_for(data)
        armed["status"].update(active=True, frozen=False, recordCount=0, freezeReason="none", stoppedUs=0)
        client = FakeClient([disabled(), armed, status_for(data), *pages_for(data)])
        sleeps = []
        collector.collect_capture(client, DEVICE_ID, duration_ms=2000, sleep=sleeps.append)
        self.assertEqual(sleeps, [2.05])
        self.assertEqual(client.calls, [("", None), ("", {"action": "arm", "durationMs": 2000}),
                                        ("", None), ("?captureId=1&offset=0", None)])

    def test_all_48_pages_are_collected_without_inventing_a_partial_success(self):
        rows = [record("tx-job", 1, CAPTURED_TX)]
        rows.extend(record("rx-chunk", index, CAPTURED_RX) for index in range(2, 385))
        data = capture(rows, freezeReason="capacity", incompleteFlags=3)
        client = FakeClient([status_for(data), *pages_for(data)])
        pages, report = collector.collect_capture(client, DEVICE_ID, capture_id=1)
        self.assertEqual(len(pages), 48)
        self.assertEqual(report["summary"]["records"], 384)
        self.assertIn("capture-incomplete-flags", report["issues"])
        self.assertEqual(client.calls[-1], ("?captureId=1&offset=376", None))

    def test_wrong_identity_model_build_or_active_capture_prevents_arm(self):
        for key, value in [("deviceId", "5tfw:" + "b" * 32), ("boardModel", "NerdOCTAXE-γ"),
                           ("asicModel", "BM1368"), ("asicCount", True),
                           ("diagnosticDriver", False), ("captureBuild", False)]:
            with self.subTest(key=key):
                initial = disabled()
                initial["deviceBinding"][key] = value
                client = FakeClient([initial])
                with self.assertRaises((collector.CollectionError, collector.analyzer.InputError)):
                    collector.collect_capture(client, DEVICE_ID, duration_ms=2000)
                self.assertEqual(client.calls, [("", None)])
        active = status_for(capture())
        active["status"].update(active=True, frozen=False, freezeReason="none", stoppedUs=0)
        client = FakeClient([active])
        with self.assertRaisesRegex(collector.CollectionError, "another capture is active"):
            collector.collect_capture(client, DEVICE_ID, duration_ms=2000)
        self.assertEqual(client.calls, [("", None)])

    def test_unknown_arm_outcome_is_never_retried(self):
        client = FakeClient([disabled(), collector.CollectionError("transport failed")])
        with self.assertRaisesRegex(collector.CollectionError, "transport failed"):
            collector.collect_capture(client, DEVICE_ID, duration_ms=2000)
        self.assertEqual(len(client.calls), 2)

    def test_unconfirmed_reused_wrong_duration_and_changed_binding_arm_fail(self):
        for mutate in (lambda data: data.update(captureId=0),
                       lambda data: data["status"].update(frozen=False, active=False),
                       lambda data: data["status"].update(deadlineUs=data["status"]["startUs"] + 1000),
                       lambda data: data["deviceBinding"].update(firmwareVersion="changed-build")):
            armed = status_for(capture())
            mutate(armed)
            client = FakeClient([disabled(), armed])
            with self.assertRaises((collector.CollectionError, collector.analyzer.InputError)):
                collector.collect_capture(client, DEVICE_ID, duration_ms=2000)
            self.assertEqual(len(client.calls), 2)

    def test_reboot_or_replacement_capture_after_arm_is_rejected(self):
        initial = status_for(capture())
        initial["captureId"] = 5
        # Capture IDs are monotonic in one boot; returning to 1 is not an
        # acknowledgement of the same software session after the preflight.
        client = FakeClient([initial, status_for(capture())])
        with self.assertRaisesRegex(collector.CollectionError, "new matching capture"):
            collector.collect_capture(client, DEVICE_ID, duration_ms=2000)
        armed = status_for(capture())
        armed["status"].update(active=True, frozen=False, recordCount=0, freezeReason="none", stoppedUs=0)
        frozen = status_for(capture())
        frozen["status"]["startUs"] += 1
        frozen["status"]["deadlineUs"] += 1
        client = FakeClient([disabled(), armed, frozen])
        with self.assertRaisesRegex(collector.CollectionError, "timing or storage binding changed"):
            collector.collect_capture(client, DEVICE_ID, duration_ms=2000, sleep=lambda _: None)
        self.assertEqual(len(client.calls), 3)

    def test_export_rejects_session_status_identity_cursor_and_missing_page(self):
        data = capture()
        for mutate in (lambda page: page.update(captureId=2),
                       lambda page: page["status"].update(stoppedUs=21_000),
                       lambda page: page["deviceBinding"].update(firmwareVersion="changed-build"),
                       lambda page: page.update(offset=1),
                       lambda page: page.update(nextOffset=0),
                       lambda page: page.update(records=[], nextOffset=0)):
            page = pages_for(data)[0]
            mutate(page)
            client = FakeClient([status_for(data), page])
            with self.assertRaises((collector.CollectionError, collector.analyzer.InputError)):
                collector.collect_capture(client, DEVICE_ID, capture_id=1)
            self.assertEqual(len(client.calls), 2)

    def test_explicit_capture_id_and_duration_are_validated_before_network(self):
        for kwargs in ({}, {"capture_id": 1, "duration_ms": 2000}, {"capture_id": 0},
                       {"capture_id": True}, {"duration_ms": 0}, {"duration_ms": 2001}):
            client = FakeClient([])
            with self.assertRaises((collector.CollectionError, collector.analyzer.InputError)):
                collector.collect_capture(client, DEVICE_ID, **kwargs)
            self.assertFalse(client.calls)
        client = FakeClient([status_for(capture())])
        with self.assertRaisesRegex(collector.CollectionError, "not frozen"):
            collector.collect_capture(client, DEVICE_ID, capture_id=2)
        self.assertEqual(len(client.calls), 1)

    def test_empty_frozen_capture_exports_one_empty_page(self):
        data = capture([])
        client = FakeClient([status_for(data), *pages_for(data)])
        pages, report = collector.collect_capture(client, DEVICE_ID, capture_id=1)
        self.assertEqual(len(pages), 1)
        self.assertEqual(report["summary"]["records"], 0)

    def test_url_credentials_paths_queries_and_control_characters_are_rejected(self):
        for url in ("file:///etc/passwd", "http://user:secret@miner.local", "http://miner.local/api",
                    "http://miner.local?token=secret", "http://miner.local/#secret", "http://miner.local\n",
                    "http://miner.local:99999", "http:///miner.local"):
            with self.subTest(url=url), self.assertRaises(collector.CollectionError):
                collector.CaptureClient(url)
        self.assertEqual(collector.base_url("https://miner.local:443/"), "https://miner.local:443")
        self.assertEqual(collector.base_url("http://[::1]:8080"), "http://[::1]:8080")
        for session in ("", "bad\nsession", "bad session", "x" * 2049):
            with self.assertRaises(collector.CollectionError):
                collector.CaptureClient("http://miner.local", session)

    def test_http_client_uses_only_capture_endpoint_no_proxy_or_redirect(self):
        client = collector.CaptureClient("http://miner.local", "private-session-fixture")
        opener = mock.Mock()
        opener.open.return_value = FakeResponse(b'{"fixture":true}')
        client.opener = opener
        self.assertEqual(client.request(), {"fixture": True})
        request = opener.open.call_args.args[0]
        self.assertEqual(request.full_url, "http://miner.local/api/5tratum/asic-capture")
        self.assertEqual(request.get_header("X-otp-session"), "private-session-fixture")
        self.assertEqual(request.get_method(), "GET")
        self.assertEqual(opener.open.call_args.kwargs, {"timeout": 10})
        self.assertIsNone(collector.NoRedirects().redirect_request(request, None, 302, "", {}, "http://other.local"))
        for body in ({"action": "freeze", "captureId": 1}, {"action": "arm", "durationMs": 2001},
                     {"action": "arm", "durationMs": True}, {"action": "arm", "durationMs": 2000, "register": 1}):
            with self.assertRaises(collector.CollectionError):
                client.request(body=body)
        self.assertEqual(opener.open.call_count, 1)

    def test_http_errors_bad_content_duplicate_keys_and_oversized_json_are_bounded(self):
        client = collector.CaptureClient("http://miner.local", "private-session-fixture")
        for response in (FakeResponse(b'{}', "text/html"), FakeResponse(b'{}', status=204),
                         FakeResponse(b'{"duplicate":1,"duplicate":2}'), FakeResponse(b'{"x":NaN}'),
                         FakeResponse(b" " * (collector.MAX_RESPONSE_BYTES + 1))):
            client.opener = mock.Mock()
            client.opener.open.return_value = response
            with self.assertRaises(collector.CollectionError) as raised:
                client.request()
            self.assertNotIn("private-session-fixture", str(raised.exception))
            self.assertEqual(client.opener.open.call_count, 1)
        for code in (302, 401, 403, 404, 409, 503):
            client.opener = mock.Mock()
            client.opener.open.side_effect = urllib.error.HTTPError("http://secret.invalid", code, "secret", {}, None)
            with self.assertRaises(collector.CollectionError) as raised:
                client.request(body={"action": "arm", "durationMs": 2000})
            self.assertNotIn("secret", str(raised.exception))
            self.assertEqual(client.opener.open.call_count, 1)

    def test_private_output_roundtrips_and_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.json"
            pages = pages_for(capture())
            collector.write_private(path, pages)
            data, digest = collector.analyzer.read_capture(path)
            self.assertEqual(data["records"], capture()["records"])
            self.assertEqual(len(digest), 64)
            self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o600)
            original = path.read_bytes()
            with self.assertRaises(FileExistsError):
                collector.write_private(path, [])
            self.assertEqual(path.read_bytes(), original)

    def test_existing_output_prevents_any_network_or_arm(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.json"
            path.write_text("original")
            with mock.patch.object(collector, "CaptureClient") as client, mock.patch("sys.stderr", new_callable=io.StringIO):
                self.assertEqual(collector.main(["http://miner.local", "--device-id", DEVICE_ID, "--arm", "--output", str(path)]), 2)
                client.assert_not_called()
            self.assertEqual(path.read_text(), "original")


if __name__ == "__main__":
    unittest.main(verbosity=2)
