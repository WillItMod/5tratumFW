#!/usr/bin/env python3
r"""Export one bounded QAxe BM1370 diagnostic capture and check it offline.

The default operation exports an explicitly selected frozen capture. --arm
starts the existing RAM-only recorder for at most two seconds; it does not
send ASIC work, write registers, change settings, submit shares, or flash.
The diagnostic capture firmware must already be installed. A normal release
without the capture endpoint is rejected, never upgraded by this tool.

Example (write private observations beneath the ignored local-devices/):
  python3 tools/bm1370_capture_collect.py http://miner.local \
    --device-id 5tfw:<32-lowercase-hex-digits> --arm \
    --output local-devices/bm1370-capture.json

An existing OTP session may be supplied through FIVETRATUM_OTP_SESSION.
Credentials are never accepted in the URL, command line, output, or errors.
Collected software observations cannot prove physical ASIC isolation.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

import bm1370_capture_analyze as analyzer

ENDPOINT = "/api/5tratum/asic-capture"
MAX_RESPONSE_BYTES = 64 * 1024
MAX_PAGES = (analyzer.MAX_RECORDS + analyzer.MAX_PAGE_RECORDS - 1) // analyzer.MAX_PAGE_RECORDS


class CollectionError(ValueError):
    pass


def device_identity(value):
    if type(value) is not str or not re.fullmatch(r"5tfw:[0-9a-f]{32}", value) or value == "5tfw:" + "0" * 32:
        raise CollectionError("expected a nonplaceholder 5tfw device identity")
    return value


def base_url(value):
    try:
        parsed = urllib.parse.urlsplit(value)
        valid_port = parsed.port is None or 1 <= parsed.port <= 65535
    except ValueError as exc:
        raise CollectionError("invalid miner URL") from exc
    if (parsed.scheme not in ("http", "https") or not parsed.hostname or not valid_port
            or parsed.username is not None or parsed.password is not None
            or parsed.path not in ("", "/") or parsed.query or parsed.fragment
            or any(ord(char) < 33 or ord(char) > 126 for char in value)):
        raise CollectionError("miner URL must contain only http(s), hostname and optional port")
    return urllib.parse.urlunsplit((parsed.scheme, parsed.netloc, "", "", ""))


class NoRedirects(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, newurl):
        # Never forward an OTP session to a redirected origin or endpoint.
        return None


class CaptureClient:
    def __init__(self, url, session=None):
        self.url = base_url(url)
        if session is not None and (not 1 <= len(session) <= 2048 or any(not 33 <= ord(char) <= 126 for char in session)):
            raise CollectionError("invalid OTP session environment value")
        self.session = session
        # A local miner request must not inherit an ambient HTTP proxy carrying
        # the session elsewhere. TLS retains the standard certificate checks.
        self.opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirects())

    def request(self, query="", body=None):
        if query and not re.fullmatch(r"\?captureId=[1-9][0-9]{0,9}&offset=[0-9]{1,3}", query):
            raise CollectionError("invalid capture page request")
        if body is not None and (set(body) != {"action", "durationMs"} or body["action"] != "arm"
                or type(body["durationMs"]) is not int or not 1 <= body["durationMs"] <= 2000):
            raise CollectionError("only a bounded recorder arm request is supported")
        payload = None if body is None else json.dumps(body, separators=(",", ":")).encode("ascii")
        headers = {"Accept": "application/json", "Cache-Control": "no-store"}
        if payload is not None:
            headers["Content-Type"] = "application/json"
        if self.session:
            headers["X-OTP-Session"] = self.session
        request = urllib.request.Request(self.url + ENDPOINT + query, data=payload, headers=headers)
        try:
            # No network operation, including the mutating arm, is retried.
            with self.opener.open(request, timeout=10) as response:
                if response.status != 200 or response.headers.get_content_type() != "application/json":
                    raise CollectionError("capture endpoint did not return a JSON success response")
                raw = response.read(MAX_RESPONSE_BYTES + 1)
        except urllib.error.HTTPError as exc:
            exc.close()
            if exc.code == 404:
                raise CollectionError("capture endpoint unavailable; diagnostic capture firmware is required") from None
            if exc.code in (401, 403):
                raise CollectionError("capture access denied; provide a current OTP session if enabled") from None
            raise CollectionError(f"capture request rejected (HTTP {exc.code}); no request was retried") from None
        except (urllib.error.URLError, TimeoutError, OSError) as exc:
            raise CollectionError("capture transport failed; an arm request may have succeeded and was not retried") from None
        if len(raw) > MAX_RESPONSE_BYTES:
            raise CollectionError("capture response exceeds 64 KiB")
        try:
            return json.loads(raw.decode("utf-8"), object_pairs_hook=analyzer.source.no_duplicates,
                              parse_constant=lambda _: (_ for _ in ()).throw(ValueError()))
        except (ValueError, UnicodeError, RecursionError):
            raise CollectionError("capture endpoint returned invalid or duplicate-key JSON") from None


def snapshot(data, expected_device_id):
    """Check the live status envelope before any arm or export request."""
    analyzer.closed(data, {"schemaVersion", "captureId", "status", "deviceBinding", "records"}, {"heap"})
    analyzer.integer(data["schemaVersion"], 1, 1)
    capture_id = analyzer.integer(data["captureId"], 0, analyzer.MAX_U32)
    binding = data["deviceBinding"]
    analyzer.closed(binding, analyzer.BINDING_FIELDS)
    analyzer.text_label(binding["firmwareVersion"])
    if (binding["boardModel"] != "NerdQAxe++" or binding["asicModel"] != "BM1370"
            or type(binding["asicCount"]) is not int or binding["asicCount"] != 4
            or binding["deviceId"] != expected_device_id
            or binding["diagnosticDriver"] is not True or binding["captureBuild"] is not True):
        raise CollectionError("capture device identity, model or diagnostic build does not match")
    status = data["status"]
    analyzer.closed(status, analyzer.STATUS_FIELDS)
    for key in ("active", "frozen", "physicalWireComplete", "physicalChipIdentity", "independentWorkAssignment"):
        analyzer.boolean(status[key])
    if status["active"] and status["frozen"]:
        raise CollectionError("capture cannot be both active and frozen")
    if any(status[key] for key in ("physicalWireComplete", "physicalChipIdentity", "independentWorkAssignment")):
        raise CollectionError("software recorder cannot attest physical wire completeness, chip identity or isolation")
    capacity = analyzer.integer(status["capacity"], 0, analyzer.MAX_RECORDS)
    analyzer.integer(status["recordCount"], 0, capacity)
    analyzer.integer(status["incompleteFlags"], 0, analyzer.KNOWN_FLAGS)
    analyzer.integer(status["droppedBusy"], 0, analyzer.MAX_U64)
    for key in ("startUs", "deadlineUs", "stoppedUs"):
        analyzer.integer(status[key], 0, analyzer.MAX_I64)
    if type(status["freezeReason"]) is not str or status["freezeReason"] not in analyzer.FREEZE_REASONS:
        raise CollectionError("unknown capture freeze reason")
    if status["active"] or status["frozen"]:
        if not capture_id or not capacity or not 1 <= status["deadlineUs"] - status["startUs"] <= analyzer.MAX_DURATION_US:
            raise CollectionError("invalid capture identity, capacity or duration")
    elif capture_id or capacity or status["recordCount"] or status["startUs"] or status["deadlineUs"] or status["stoppedUs"]:
        raise CollectionError("invalid disabled recorder state")
    if data["records"] != []:
        raise CollectionError("status response unexpectedly contains capture records")
    if "heap" in data:
        analyzer.closed(data["heap"], analyzer.HEAP_FIELDS)
        for value in data["heap"].values():
            analyzer.integer(value, 0, analyzer.MAX_U32)
    return data


def collect_capture(client, expected_device_id, *, capture_id=None, duration_ms=None, sleep=time.sleep):
    device_identity(expected_device_id)
    if (capture_id is None) == (duration_ms is None):
        raise CollectionError("select an existing capture ID or a new bounded capture")
    if capture_id is not None:
        analyzer.integer(capture_id, 1, analyzer.MAX_U32)
    if duration_ms is not None:
        analyzer.integer(duration_ms, 1, 2000)
    initial = snapshot(client.request(), expected_device_id)
    if initial["status"]["active"]:
        raise CollectionError("another capture is active; it will not be replaced or frozen")
    if duration_ms is not None:
        armed = snapshot(client.request(body={"action": "arm", "durationMs": duration_ms}), expected_device_id)
        if armed["deviceBinding"] != initial["deviceBinding"] or armed["captureId"] <= initial["captureId"]:
            raise CollectionError("arm acknowledgement does not identify one new matching capture")
        if not armed["status"]["active"] and not armed["status"]["frozen"]:
            raise CollectionError("arm acknowledgement did not confirm a recording or frozen capture")
        capture_id = armed["captureId"]
        if armed["status"]["deadlineUs"] - armed["status"]["startUs"] != duration_ms * 1000:
            raise CollectionError("arm acknowledgement has a different duration")
        if armed["status"]["active"]:
            sleep(duration_ms / 1000 + 0.05)
        frozen = snapshot(client.request(), expected_device_id)
        if frozen["deviceBinding"] != initial["deviceBinding"]:
            raise CollectionError("device binding changed during capture")
        if any(frozen["status"][key] != armed["status"][key] for key in ("startUs", "deadlineUs", "capacity")):
            raise CollectionError("capture timing or storage binding changed after arming")
    else:
        frozen = initial
    if frozen["captureId"] != capture_id or frozen["status"]["active"] or not frozen["status"]["frozen"]:
        raise CollectionError("requested capture is not frozen; no additional arm or freeze was attempted")
    # Validate immutable status before fetching any raw pages. A snapshot has no
    # records even when recordCount > 0; the envelope validator permits this.
    analyzer.validate_envelope(frozen)
    pages, offset = [], 0
    for _ in range(MAX_PAGES):
        page = client.request(query=f"?captureId={capture_id}&offset={offset}")
        analyzer.validate_envelope(page)
        if any(page[key] != frozen[key] for key in ("schemaVersion", "captureId", "status", "deviceBinding")):
            raise CollectionError("capture session, frozen status or device binding changed during export")
        if page.get("offset") != offset or len(page["records"]) > analyzer.MAX_PAGE_RECORDS:
            raise CollectionError("capture page has an unexpected cursor or length")
        offset += len(page["records"])
        if offset > frozen["status"]["recordCount"] or page.get("nextOffset") != offset:
            raise CollectionError("capture page does not advance to the exact next cursor")
        if not page["records"] and offset != frozen["status"]["recordCount"]:
            raise CollectionError("capture export stalled before all records were read")
        pages.append(page)
        if offset == frozen["status"]["recordCount"]:
            assembled = analyzer.assemble_pages(pages)
            return pages, analyzer.analyze_capture(assembled)
    raise CollectionError("capture exceeds the bounded page count")


def write_private(path, pages):
    raw = (json.dumps(pages, indent=2, allow_nan=False) + "\n").encode("utf-8")
    if len(raw) > analyzer.MAX_FILE_BYTES:
        raise CollectionError("merged capture exceeds the offline analyzer's 2 MiB limit")
    # Exclusive creation preserves an earlier observation; private mode avoids
    # exposing the opaque device identity and raw work on shared machines.
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(raw)
    except BaseException:
        Path(path).unlink(missing_ok=True)
        raise


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("miner", help="http(s) origin of the already-installed diagnostic firmware")
    parser.add_argument("--device-id", required=True, help="expected opaque physical parent ID")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--capture-id", type=int, help="export this existing frozen capture without arming")
    mode.add_argument("--arm", action="store_true", help="start one RAM-only capture; never retried")
    parser.add_argument("--duration-ms", type=int, default=2000, help="new capture duration, 1..2000 (default 2000)")
    parser.add_argument("--output", type=Path, required=True, help="new private JSON file; will not overwrite")
    args = parser.parse_args(argv)
    try:
        if args.output.exists() or not args.output.parent.is_dir():
            raise CollectionError("output must be a new file in an existing private directory")
        client = CaptureClient(args.miner, os.environ.get("FIVETRATUM_OTP_SESSION"))
        pages, report = collect_capture(client, args.device_id, capture_id=args.capture_id,
                                        duration_ms=args.duration_ms if args.arm else None)
        write_private(args.output, pages)
    except (CollectionError, analyzer.InputError, OSError, RecursionError) as exc:
        # Local filesystem errors may contain a private path; HTTP errors and
        # raw response bodies are deliberately absent from diagnostics.
        message = str(exc) if isinstance(exc, (CollectionError, analyzer.InputError)) else "private capture file could not be written"
        print(json.dumps({"error": message, "independentWorkAssignment": False}), file=sys.stderr)
        return 2
    print(json.dumps({"captureId": report["captureId"], "pages": len(pages), "summary": report["summary"],
                      "issues": report["issues"], "independentWorkAssignment": False,
                      "physicalChipIdentity": False, "isolationVerified": False}, indent=2, allow_nan=False))
    return 1 if (report["issues"] or report["summary"]["txCheckFailures"] or report["summary"]["rxCrcFailures"]
                 or report["summary"]["unsupportedResponseTypes"]) else 0


if __name__ == "__main__":
    sys.exit(main())
