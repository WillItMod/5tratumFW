#!/usr/bin/env python3
"""Actual candidate MUX writer -> firmware parser; synthetic jobs, no device I/O.

Run with MUX_SOURCE pointing at the candidate source checkout and that checkout's
Python dependencies available. Missing source is an explicit optional-test skip.
The MUX source is imported without modifying it or starting its server.
"""
import importlib
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ProductionMuxWireTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = os.environ.get("MUX_SOURCE")
        if not source or not (Path(source) / "app.py").is_file():
            raise unittest.SkipTest("Set MUX_SOURCE to the actual MUX candidate source checkout")
        cls.source = Path(source).resolve()
        sys.path.insert(0, str(cls.source))
        cls.app = importlib.import_module("app")
        if Path(cls.app.__file__).resolve() != cls.source / "app.py":
            raise RuntimeError("A different app module was already imported")
        cls.tmp = tempfile.TemporaryDirectory(prefix="5tratum-production-wire-")
        cls.binary = Path(cls.tmp.name) / "work-wire"
        subprocess.run([
            shutil.which("clang++") or shutil.which("g++"), "-std=c++17",
            "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
            "-fno-omit-frame-pointer", "-I", str(ROOT / "components/arduinojson"),
            "-I", str(ROOT / "main"), "-I", str(ROOT / "main/http_server"),
            str(ROOT / "main/http_server/status_report.cpp"),
            str(ROOT / "test/host/work_context_wire_harness.cpp"), "-o", str(cls.binary),
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def frames(self, label, *, direct=False, native_index=None, height=1000000,
               bits="1B0404CB"):
        # A synthetic BIP34 coinbase prefix; no production wallets/coinbase.
        number = height.to_bytes(max(1, (height.bit_length() + 7) // 8), "little")
        if number[-1] & 0x80:
            number += b"\0"
        prefix = (b"\x01\x00\x00\x00\x01" + b"\0" * 32 + b"\xff" * 4
                  + b"\x14" + bytes([len(number)]) + number).hex()
        upstream = {"id": None, "method": "mining.notify", "params": [
            "synthetic-upstream-id", "00" * 32, prefix, "", [],
            "20000000", bits, "65000000", True]}
        client, server = socket.socketpair()
        try:
            session = self.app.ProxySession(server, ("192.0.2.20", 1234))
            session.agent = "5tratumFW/NerdQAxe++/BM1370/host/work-context-v2"
            if native_index is not None:
                import native_pool_identity
                session.agent = ("5tratumFW/NerdQAxe++/BM1370/host/"
                                 f"native-pool-v1:{'34' * 16}:{native_index}/work-context-v2")
                session.native_pool_identity = native_pool_identity.parse_agent(
                    session.agent, enabled=True)
                self.assertEqual(session.native_pool_identity["poolIndex"], native_index)
                self.assertFalse(session.native_pool_identity["independentWorkAssignment"])
            session.mux_status_started = True
            session.short_job_ids = True
            session.short_job_seed = 0x12345670 + (native_index or 0) * 0x1000
            session.stable_extranonce_mux = False
            target = {"coinLabel": label}
            lane = SimpleNamespace(target_id="synthetic-route", target_name="synthetic",
                                   target=target, extranonce1="00", extranonce2_size=8,
                                   difficulty=1, version_rolling_mask="", connection_generation=1)
            if direct:
                downstream = session._short_passthrough_notify(upstream, lane.target_id)
            else:
                downstream = session._mux_job(lane, upstream, force_clean=True)
            session._send_client_message(downstream, coin_target=target)
            client.settimeout(1)
            wire = b""
            while wire.count(b"\n") < 2:
                wire += client.recv(16384)
            frames = [json.loads(line) for line in wire.splitlines()]
            self.assertEqual(len(frames), 2)
            self.assertEqual(frames[0]["method"], "mining.notify")
            context = frames[1]["params"][0]["workContext"]
            self.assertNotEqual(frames[0]["params"][0], upstream["params"][0])
            self.assertEqual(context["jobId"], frames[0]["params"][0])
            self.assertEqual(context["nBits"], frames[0]["params"][6].lower())
            return frames
        finally:
            client.close()
            server.close()

    def receive(self, frames, *, valid=True, secondary=None, mode="fresh"):
        fixture = Path(self.tmp.name) / "synthetic.jsonl"
        fixture.write_text("".join(json.dumps(frame, separators=(",", ":")) + "\n"
                                   for frame in frames))
        args = [str(self.binary), str(fixture)]
        if secondary is not None:
            other = Path(self.tmp.name) / "synthetic-secondary.jsonl"
            other.write_text("".join(json.dumps(frame, separators=(",", ":")) + "\n"
                                    for frame in secondary))
            args.extend([str(other), mode])
        result = subprocess.run(args, capture_output=True, text=True)
        if not valid:
            self.assertNotEqual(result.returncode, 0)
            return None
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads(result.stdout)

    def test_every_actual_canonical_coin_survives_both_downstream_job_rewriters(self):
        for label, coin in self.app.MUX_ROUTE_COIN_CONTEXTS.items():
            for direct in (False, True):
                with self.subTest(coin=label, direct=direct):
                    report = self.receive(self.frames(label, direct=direct))
                    self.assertEqual({key: report["coin"][key] for key in ("id", "ticker", "name")}, coin)
                    self.assertEqual(report["coin"]["source"], "5tratMUX-route-advertisement")
                    self.assertEqual(report["workContext"]["nBits"], "1b0404cb")
                    self.assertAlmostEqual(report["workContext"]["networkDifficulty"],
                                           16307.420938523983, delta=16307.420938523983 * 1e-9)
                    expected_height = 1000000 if coin["id"] in self.app.MUX_BIP34_COINS else None
                    self.assertEqual(report["workContext"]["height"], expected_height)
                    self.assertFalse(report["work"]["independentAssignment"])

    def test_5trat_does_not_relax_observed_job_id_or_nbits_binding(self):
        for field, value in (("jobId", "a-different-job"), ("nBits", "1d00ffff")):
            with self.subTest(field=field):
                frames = self.frames("5TRAT")
                frames[1]["params"][0]["workContext"][field] = value
                self.receive(frames, valid=False)

    def test_malformed_coin_ids_still_invalidate_reserved_frames(self):
        for value in ("-5trat", "_5trat", "5TRAT", "5trat!", "5trat coin", "5trat\0x", "é"):
            with self.subTest(coin_id=value):
                frames = self.frames("5TRAT")
                frames[1]["params"][0]["coin"]["id"] = value
                self.receive(frames, valid=False)

    def native_frames(self):
        return [self.frames(label, native_index=index, height=height, bits=bits)
                for index, label, height, bits in ((0, "5TRAT", 23763, "1921275e"),
                                                   (1, "BTC", 970381, "17021ef0"))]

    def test_native_dual_routes_keep_distinct_actual_job_context_through_status_and_display(self):
        first, second = self.native_frames()
        self.assertNotEqual(first[0]["params"][0], second[0]["params"][0])
        report = self.receive(first, secondary=second)
        self.assertEqual(report["work"]["poolMode"], "dual")
        self.assertIsNone(report["work"]["activePool"])
        self.assertIsNone(report["coin"]["id"])
        self.assertIsNone(report["workContext"])
        self.assertFalse(report["work"]["independentAssignment"])
        for index, frames in enumerate((first, second)):
            pool = report["mux"]["pools"][index]
            self.assertEqual(pool["index"], index)
            self.assertTrue(pool["connected"])
            context = pool["workContext"]
            self.assertEqual(context["jobId"], frames[0]["params"][0])
            self.assertEqual(context["nBits"], frames[0]["params"][6])
            self.assertEqual(context["height"], (23763, 970381)[index])
            self.assertEqual(context["source"], "forwarded-stratum-job")
            self.assertAlmostEqual(context["networkDifficulty"],
                self.app._mux_nbits_difficulty(context["nBits"]),
                delta=context["networkDifficulty"] * 1e-9)
            rendered = report["hostDisplayProof"]["routes"][index]
            self.assertEqual(rendered["coin"], ("5TRAT", "BTC")[index])
            self.assertIn(str(context["height"]), rendered["block"])
            self.assertIn(context["nBits"], rendered["nBits"])

    def test_native_route_stale_disconnect_reconnect_or_new_job_does_not_leak_or_erase_other_pool(self):
        first, second = self.native_frames()
        for mode in ("primary-expired", "primary-disconnected", "primary-reconnected",
                     "primary-new-notify"):
            with self.subTest(mode=mode):
                report = self.receive(first, secondary=second, mode=mode)
                primary, other = report["mux"]["pools"]
                self.assertIsNone(primary["coin"]["id"])
                self.assertIsNone(primary["workContext"])
                self.assertEqual(other["coin"]["id"], "bitcoin")
                self.assertEqual(other["workContext"]["height"], 970381)
                self.assertEqual(other["workContext"]["jobId"], second[0]["params"][0])
                self.assertTrue(other["connected"])
                self.assertIsNone(report["workContext"])
                self.assertIsNone(report["coin"]["id"])
                self.assertEqual(report["hostDisplayProof"]["routes"][0]["coin"], "unknown")
                self.assertEqual(report["hostDisplayProof"]["routes"][1]["coin"], "BTC")

    def test_other_native_session_advertisement_cannot_identify_this_sessions_work(self):
        first, second = self.native_frames()
        self.assertNotEqual(first[0]["params"][0], second[0]["params"][0])
        self.receive([first[0], second[1]], valid=False)
        self.receive([second[0], first[1]], valid=False)


if __name__ == "__main__":
    unittest.main()
