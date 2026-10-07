#!/usr/bin/env python3
"""Analyze frozen, bounded BM1370 software-recorder exports entirely offline.

UART reads are software observations, not electrical timestamps or a proof of
chip identity. SHA256d identifies matching *captured* header contexts; neither
ID reuse, a reset marker, nor a successful hash establishes pipeline drain or
independent work assignment. No network, serial, or device APIs are used.
"""
from __future__ import annotations

import argparse
from collections import defaultdict
from decimal import Decimal, localcontext
import hashlib
import json
from pathlib import Path
import re
import sys

import asic_capture_check as source

MAX_FILE_BYTES = 2 * 1024 * 1024
MAX_RECORDS = 384
MAX_RAW_BYTES = 88
MAX_DURATION_US = 2_000_000
MAX_PAGE_RECORDS = 8
MAX_CANDIDATE_PREVIEW = 8
MAX_U32 = (1 << 32) - 1
MAX_U64 = (1 << 64) - 1
MAX_I64 = (1 << 63) - 1
KNOWN_FLAGS = 0x3FF
KINDS = {"tx-job", "tx-bytes", "rx-chunk", "transport", "retirement"}
FREEZE_REASONS = {"none", "manual", "time-limit", "capacity", "invalid-input", "invalid-clock"}
TRANSPORT = {1: "rx-error", 2: "tx-error", 3: "rx-timeout", 4: "buffer-flush",
             5: "baud-changed", 6: "observed-uart-loss", 7: "partial-discard", 8: "caller-gap"}
RETIREMENT = {1: "generation-change", 2: "power-reset", 3: "parser-reset", 4: "job-slot-retired"}
COMMON_RECORD_FIELDS = {"sequence", "timestampUs", "generation", "kind", "marker", "requestedLength",
                        "transportReturned", "capturedLength", "packetHex", "flags", "auxiliaryValue"}
JOB_FIELDS = {"headerHex", "logicalCounter", "versionMask", "ticketDifficulty", "poolIndex"}
STATUS_FIELDS = {"active", "frozen", "recordCount", "capacity", "incompleteFlags", "freezeReason", "droppedBusy",
                 "startUs", "deadlineUs", "stoppedUs", "physicalWireComplete", "physicalChipIdentity",
                 "independentWorkAssignment"}
BINDING_FIELDS = {"firmwareVersion", "boardModel", "asicModel", "asicCount", "deviceId",
                  "diagnosticDriver", "captureBuild"}
HEAP_FIELDS = {"captureStorageBytes", "freeInternal", "largestInternal", "minimumInternal", "freePsram", "largestPsram"}


class InputError(ValueError):
    pass


def integer(value, low, high):
    if type(value) is not int or not low <= value <= high:
        raise InputError("integer outside declared bounds")
    return value


def closed(value, required, optional=()):
    if type(value) is not dict or set(value) - set(required) - set(optional) or set(required) - set(value):
        raise InputError("missing or unknown object fields")


def boolean(value):
    if type(value) is not bool:
        raise InputError("expected boolean")
    return value


def text_label(value):
    if type(value) is not str or not 1 <= len(value) <= 64 or any(not 32 <= ord(c) <= 126 for c in value):
        raise InputError("expected bounded printable label")
    return value


def packet_bytes(value, length=None):
    if type(value) is not str or len(value) % 2 or not re.fullmatch(r"[0-9a-f]*", value):
        raise InputError("expected complete lowercase hexadecimal bytes")
    size = len(value) // 2
    if size > MAX_RAW_BYTES or length is not None and size != length:
        raise InputError("packet length outside declared bounds")
    return bytes.fromhex(value)


def ticket(value):
    # The recorder stores the actual integer ASIC mask threshold, not a guessed
    # pool/network difficulty or a floating JSON number.
    if type(value) is not str or not re.fullmatch(r"[1-9][0-9]{0,9}", value):
        raise InputError("ticketDifficulty must be a positive uint32 decimal string")
    return integer(int(value), 1, MAX_U32)


def validate_envelope(data):
    closed(data, {"schemaVersion", "captureId", "status", "deviceBinding", "records"}, {"offset", "nextOffset", "heap"})
    if integer(data["schemaVersion"], 1, 1) != 1:
        raise InputError("unsupported schema")
    integer(data["captureId"], 1, MAX_U32)
    status = data["status"]
    closed(status, STATUS_FIELDS)
    for key in ("active", "frozen", "physicalWireComplete", "physicalChipIdentity", "independentWorkAssignment"):
        boolean(status[key])
    if status["active"] or not status["frozen"]:
        raise InputError("only frozen inactive captures can be analyzed")
    if any(status[key] for key in ("physicalWireComplete", "physicalChipIdentity", "independentWorkAssignment")):
        raise InputError("recorder cannot attest physical completeness, identity or isolation")
    capacity = integer(status["capacity"], 1, MAX_RECORDS)
    integer(status["recordCount"], 0, capacity)
    integer(status["incompleteFlags"], 0, KNOWN_FLAGS)
    integer(status["droppedBusy"], 0, MAX_U64)
    for key in ("startUs", "deadlineUs", "stoppedUs"):
        integer(status[key], 0, MAX_I64)
    if not 1 <= status["deadlineUs"] - status["startUs"] <= MAX_DURATION_US:
        raise InputError("capture duration outside 1..2000000 microseconds")
    if type(status["freezeReason"]) is not str or status["freezeReason"] not in FREEZE_REASONS:
        raise InputError("unknown freeze reason")
    binding = data["deviceBinding"]
    closed(binding, BINDING_FIELDS)
    for key in ("firmwareVersion", "boardModel", "asicModel"):
        text_label(binding[key])
    if binding["asicModel"] != "BM1370":
        raise InputError("only BM1370 captures are supported")
    integer(binding["asicCount"], 1, 128)
    if type(binding["deviceId"]) is not str or not re.fullmatch(r"5tfw:[0-9a-f]{32}", binding["deviceId"]) or binding["deviceId"] == "5tfw:" + "0" * 32:
        raise InputError("expected nonplaceholder opaque device identity")
    if not boolean(binding["diagnosticDriver"]) or not boolean(binding["captureBuild"]):
        raise InputError("export does not declare the diagnostic capture build")
    if type(data["records"]) is not list or len(data["records"]) > MAX_RECORDS:
        raise InputError("at most 384 records")
    if ("offset" in data) != ("nextOffset" in data):
        raise InputError("both page cursors are required")
    if "heap" in data:
        closed(data["heap"], HEAP_FIELDS)
        for value in data["heap"].values():
            integer(value, 0, MAX_U32)
    return status


def assemble_pages(pages):
    """Join at most 48 immutable eight-record pages from one frozen session."""
    if type(pages) is not list or not 1 <= len(pages) <= (MAX_RECORDS + MAX_PAGE_RECORDS - 1) // MAX_PAGE_RECORDS:
        raise InputError("expected 1..48 frozen pages")
    first, expected, records = None, 0, []
    for index, page in enumerate(pages):
        status = validate_envelope(page)
        if "offset" not in page or len(page["records"]) > MAX_PAGE_RECORDS:
            raise InputError("expected bounded export page")
        integer(page["offset"], 0, status["recordCount"])
        if page["offset"] != expected:
            raise InputError("page gap, overlap or out-of-order cursor")
        identity = {key: page[key] for key in ("schemaVersion", "captureId", "status", "deviceBinding")}
        if first is None:
            first = identity
        elif identity != first:
            raise InputError("page session, status or device binding changed")
        expected += len(page["records"])
        if expected > status["recordCount"] or not page["records"] and status["recordCount"]:
            raise InputError("page length contradicts snapshot")
        next_offset = page["nextOffset"]
        if expected < status["recordCount"]:
            if integer(next_offset, 0, MAX_RECORDS) != expected or index == len(pages) - 1:
                raise InputError("missing subsequent page")
        elif (next_offset is not None and integer(next_offset, 0, MAX_RECORDS) != expected) or index != len(pages) - 1:
            raise InputError("final page cursor or trailing page invalid")
        records.extend(page["records"])
    if expected != first["status"]["recordCount"]:
        raise InputError("incomplete frozen export")
    result = dict(first, records=records)
    if "heap" in pages[0]:
        result["heap"] = pages[0]["heap"]
    validate_capture(result)
    return result


def validate_capture(data):
    status = validate_envelope(data)
    if len(data["records"]) != status["recordCount"]:
        raise InputError("recordCount differs from supplied records")
    if "offset" in data and (integer(data["offset"], 0, MAX_RECORDS) != 0 or
            data["nextOffset"] is not None and integer(data["nextOffset"], 0, MAX_RECORDS) != status["recordCount"]):
        raise InputError("single page does not contain complete capture")
    for record in data["records"]:
        if type(record) is not dict or type(record.get("kind")) is not str or record["kind"] not in KINDS:
            raise InputError("unknown record kind")
        closed(record, COMMON_RECORD_FIELDS | (JOB_FIELDS if record["kind"] == "tx-job" else set()))
        integer(record["sequence"], 1, MAX_U64)
        integer(record["timestampUs"], 0, MAX_I64)
        integer(record["generation"], 1 if record["kind"] == "tx-job" else 0, MAX_U64)
        integer(record["requestedLength"], 0, MAX_U32)
        integer(record["transportReturned"], -(1 << 31), (1 << 31) - 1)
        integer(record["capturedLength"], 0, MAX_RAW_BYTES)
        packet_bytes(record["packetHex"], record["capturedLength"])
        integer(record["flags"], 0, KNOWN_FLAGS)
        integer(record["auxiliaryValue"], 0, MAX_U32)
        marker = integer(record["marker"], 0, 255)
        if record["kind"] in ("tx-job", "tx-bytes", "rx-chunk") and marker:
            raise InputError("byte records cannot carry a marker")
        if record["kind"] == "transport" and marker not in TRANSPORT or record["kind"] == "retirement" and marker not in RETIREMENT:
            raise InputError("unknown marker")
        if record["kind"] in ("transport", "retirement") and (record["capturedLength"] or record["requestedLength"]):
            raise InputError("marker records cannot contain transport bytes")
        if record["kind"] == "tx-job":
            packet_bytes(record["headerHex"], 80)
            integer(record["logicalCounter"], 0, MAX_U32)
            integer(record["versionMask"], 0, MAX_U32)
            ticket(record["ticketDifficulty"])
            if integer(record["poolIndex"], 0, 255) not in (0, 1, 255):
                raise InputError("poolIndex must be primary, secondary or unknown")
    return data


def read_capture(path):
    with Path(path).open("rb") as stream:
        raw = stream.read(MAX_FILE_BYTES + 1)
    if len(raw) > MAX_FILE_BYTES:
        raise InputError("capture exceeds 2 MiB")
    try:
        data = json.loads(raw.decode("utf-8"), object_pairs_hook=source.no_duplicates,
                          parse_constant=lambda _: (_ for _ in ()).throw(InputError("nonfinite JSON number")))
    except (UnicodeError, ValueError, RecursionError) as exc:
        raise InputError("invalid or duplicate-key capture JSON") from exc
    if type(data) is list:
        data = assemble_pages(data)
    validate_capture(data)
    return data, hashlib.sha256(raw).hexdigest()


def candidate_check(context, response):
    header = bytearray(context["header"])
    base_version = int.from_bytes(header[:4], "little")
    version = base_version | response["rolledVersionBits"]
    header[:4] = version.to_bytes(4, "little")
    header[76:] = response["nonce"].to_bytes(4, "little")
    digest = hashlib.sha256(hashlib.sha256(header).digest()).digest()
    hash_integer = int.from_bytes(digest, "little")
    errors = []
    if (version ^ base_version) & (~context["versionMask"] & MAX_U32):
        errors.append("version-change-outside-captured-mask")
    meets_ticket = hash_integer * context["ticket"] <= source.DIFF1_TARGET
    with localcontext() as arithmetic:
        arithmetic.prec = 100
        bdiff = "infinity" if not hash_integer else str(Decimal(source.DIFF1_TARGET) / Decimal(hash_integer))
    return {"txSequence": context["sequence"], "generation": context["generation"], "poolIndex": context["poolIndex"],
            "softwareRetired": context["retired"], "hashHex": digest[::-1].hex(), "fullVersion": version,
            "computedDifficulty": bdiff, "difficultyUnit": "Bitcoin-reference-bdiff",
            "ticketDifficulty": str(context["ticket"]), "meetsCapturedTicket": meets_ticket,
            "bindingErrors": errors, "hashAndBindingPass": meets_ticket and not errors}


def analyze_capture(data, capture_sha256=None):
    validate_capture(data)
    status = data["status"]
    issues, warnings, tx_reports, marker_reports, frames = [], [], [], [], []
    contexts = defaultdict(list)
    buffer = []  # At most eleven (byte, source sequence, observation timestamp) tuples.
    discarded = crc_failures = unsupported = rx_bytes = 0
    pool_matches = {"primary": 0, "secondary": 0, "unknown": 0}
    previous_time = status["startUs"]
    if status["incompleteFlags"]:
        issues.append("capture-incomplete-flags")
    if status["droppedBusy"]:
        issues.append("recorder-dropped-busy-observations")
    if status["stoppedUs"] < status["startUs"] or status["freezeReason"] == "none":
        issues.append("invalid-freeze-boundary")
    if status["freezeReason"] in ("capacity", "invalid-input", "invalid-clock"):
        issues.append("capture-frozen-with-incomplete-reason")
    if status["freezeReason"] == "capacity" and (status["recordCount"] != status["capacity"] or status["incompleteFlags"] & 3 != 3):
        issues.append("capacity-freeze-metadata-mismatch")
    if status["recordCount"] == status["capacity"] and status["freezeReason"] != "capacity":
        issues.append("full-buffer-without-capacity-freeze")
    if status["freezeReason"] == "time-limit" and status["stoppedUs"] < status["deadlineUs"]:
        issues.append("time-limit-freeze-before-deadline")

    def consume_frame():
        nonlocal discarded, crc_failures, unsupported, buffer
        packet = bytes(item[0] for item in buffer)
        frame = {"firstRecordSequence": buffer[0][1], "lastRecordSequence": buffer[-1][1],
                 "observedAtUs": buffer[-1][2], "packetHex": packet.hex()}
        residue = source.crc5_source(packet[2:])
        frame["crcResidue"] = residue
        if residue:
            crc_failures += 1
            frame.update(checksPassed=False, errors=["rx-crc-mismatch"])
            # Preserve overlapping AA55 or trailing AA, exactly as the bounded
            # diagnostic stream parser does; no serial flush or guessed length.
            keep = next((i for i in range(1, 10) if packet[i:i + 2] == b"\xaa\x55"), 11)
            if keep == 11 and packet[-1] == 0xAA:
                keep = 10
            discarded += keep
            buffer = buffer[keep:]
        elif packet[10] >> 5 not in (0, 4):
            unsupported += 1
            frame.update(checksPassed=False, errors=["unsupported-response-type"])
            discarded += 11
            buffer = []
        else:
            response = source.decode_rx(packet, None)
            frame.update(checksPassed=True, errors=[], packetKind=response["packetKind"])
            buffer = []
            if response["packetKind"] == "nonce":
                frame.update(nonce=response["nonce"], rawJobByte=response["rawJobByte"], wireJobId=response["wireJobId"],
                             rolledVersionBits=response["rolledVersionBits"])
                candidates = [candidate_check(context, response) for context in contexts[response["wireJobId"]]]
                matches = [candidate for candidate in candidates if candidate["hashAndBindingPass"]]
                frame.update(candidateCount=len(candidates), matchingContextCount=len(matches),
                             candidatesTruncated=len(candidates) > MAX_CANDIDATE_PREVIEW,
                             candidates=candidates[:MAX_CANDIDATE_PREVIEW],
                             matchingContextsTruncated=len(matches) > MAX_CANDIDATE_PREVIEW,
                             matchingContexts=matches[:MAX_CANDIDATE_PREVIEW], candidateSetComplete=False,
                             preArmWorkUnknown=True, wireIdReused=len(candidates) > 1)
                if len(matches) == 1:
                    frame["mapping"] = "unique-match-within-captured-contexts"
                    pool = matches[0]["poolIndex"]
                    pool_matches[{0: "primary", 1: "secondary", 255: "unknown"}[pool]] += 1
                elif matches:
                    frame["mapping"] = "ambiguous-matching-contexts"
                else:
                    frame["mapping"] = "unbound" if not candidates else "no-captured-context-meets-ticket"
            else:
                frame.update(register=response["register"], registerValue=response["data"], responseAddressByte=packet[6])
        frames.append(frame)

    for expected, record in enumerate(data["records"], 1):
        ordered = record["sequence"] == expected
        if not ordered:
            issues.append("record-sequence-not-contiguous")
        timestamp = record["timestampUs"]
        if timestamp < previous_time or timestamp > status["stoppedUs"] or timestamp >= status["deadlineUs"]:
            issues.append("record-timestamp-outside-monotonic-capture")
            ordered = False
        previous_time = timestamp
        if record["flags"]:
            issues.append("record-incomplete-flags")
        kind = record["kind"]
        packet = bytes.fromhex(record["packetHex"])
        if kind in ("tx-job", "tx-bytes"):
            decoded = source.decode_tx(packet)
            errors = list(decoded["errors"])
            complete_write = record["requestedLength"] == len(packet) == record["transportReturned"] and len(packet) > 0
            if not complete_write:
                errors.append("tx-not-fully-accepted-by-uart-driver")
                issues.append("short-or-failed-tx")
            if not ordered:
                errors.append("tx-observation-order-invalid")
            if kind == "tx-job":
                header = bytes.fromhex(record["headerHex"])
                work = {"header": header, "startingNonce": int.from_bytes(header[76:], "little")}
                if decoded["checksPassed"] and "job" in decoded:
                    # Reuse the source-exact header transform, not guessed
                    # network coinbase, current pool job or node-tip metadata.
                    source.bind_tx(decoded, {"work": "captured", "generation": record["generation"],
                                           "logicalJobCounter": record["logicalCounter"]}, {"captured": work})
                    errors.extend(error for error in decoded["errors"] if error not in errors)
                else:
                    errors.append("tx-job-is-not-a-valid-established-job")
                if complete_write and not errors:
                    contexts[decoded["job"]["wireJobId"]].append({"sequence": record["sequence"],
                        "generation": record["generation"], "poolIndex": record["poolIndex"], "header": header,
                        "versionMask": record["versionMask"], "ticket": ticket(record["ticketDifficulty"]), "retired": False})
            elif decoded.get("packetKind") == "job":
                warnings.append("transmitted-job-without-captured-context")
            tx_reports.append({"sequence": record["sequence"], "kind": kind, "errors": errors,
                               "checksPassed": not errors and decoded["checksPassed"],
                               "uartDriverAcceptedCompleteWrite": complete_write,
                               "physicalDeliveryVerified": False,
                               "wireJobId": decoded.get("job", {}).get("wireJobId")})
        elif kind == "rx-chunk":
            length_matches = record["transportReturned"] == len(packet) and 0 < len(packet) <= record["requestedLength"]
            if not length_matches or not ordered:
                if not length_matches:
                    issues.append("rx-chunk-transport-length-mismatch")
                discarded += len(buffer)
                buffer = []
                continue
            rx_bytes += len(packet)
            for byte in packet:
                item = (byte, record["sequence"], timestamp)
                if not buffer:
                    if byte == 0xAA:
                        buffer.append(item)
                    else:
                        discarded += 1
                elif len(buffer) == 1 and byte != 0x55:
                    if byte == 0xAA:
                        discarded += 1
                        buffer = [item]
                    else:
                        discarded += 2
                        buffer = []
                else:
                    buffer.append(item)
                    if len(buffer) == 11:
                        consume_frame()
        else:
            marker_name = (TRANSPORT if kind == "transport" else RETIREMENT)[record["marker"]]
            marker_reports.append({"sequence": record["sequence"], "kind": kind, "marker": marker_name,
                                   "generation": record["generation"], "auxiliaryValue": record["auxiliaryValue"]})
            if kind == "transport" and record["marker"] in (1, 2, 6, 8):
                issues.append("transport-error-or-observed-gap")
            if kind == "transport" and record["marker"] in (1, 4, 5, 6, 7, 8):
                discarded += len(buffer)
                buffer = []
                issues.append("transport-stream-discontinuity")
            if kind == "retirement" and record["marker"] in (1, 2):
                # Software retirement is observable; silicon pipeline drain
                # is not. Keep every prior same-ID context for hash comparison.
                for history in contexts.values():
                    for context in history:
                        context["retired"] = True

    if buffer:
        warnings.append("capture-ends-with-partial-rx-frame")
    if not frames:
        warnings.append("no-complete-rx-frame-observed")
    if not contexts:
        warnings.append("no-valid-captured-job-context")
    issues = sorted(set(issues))
    return {"reportVersion": 1, "tool": "bm1370_capture_analyze", "captureId": data["captureId"],
            "captureSha256": capture_sha256, "declaredDeviceBinding": data["deviceBinding"], "captureStatus": status,
            "exportHeapObservation": data.get("heap"),
            "verification": {"softwareObservationComplete": not issues, "physicalWireComplete": False,
                             "physicalChipIdentity": False, "independentWorkAssignment": False, "isolationVerified": False,
                             "provenanceAttested": False, "preArmWorkUnknown": True, "pipelineDrainVerified": False,
                             "timestampMeaning": "post-UART software observation; not electrical byte timing"},
            "issues": issues, "warnings": sorted(set(warnings)),
            "summary": {"records": len(data["records"]), "jobContexts": sum(map(len, contexts.values())),
                        "txRecords": len(tx_reports), "txCheckFailures": sum(not item["checksPassed"] for item in tx_reports),
                        "rxBytes": rx_bytes, "rxFrameCandidates": len(frames), "rxCrcFailures": crc_failures,
                        "unsupportedResponseTypes": unsupported, "discardedRxBytes": discarded,
                        "partialRxBytesAtFreeze": len(buffer),
                        "nonceFrames": sum(item.get("packetKind") == "nonce" for item in frames),
                        "registerFrames": sum(item.get("packetKind") == "register" for item in frames),
                        "uniqueHashMatchesByCapturedPool": pool_matches},
            "tx": tx_reports, "markers": marker_reports, "rx": frames}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", help="private merged frozen export or JSON array of complete frozen pages")
    args = parser.parse_args(argv)
    try:
        data, digest = read_capture(args.capture)
        report = analyze_capture(data, digest)
    except (InputError, OSError, RecursionError) as exc:
        print(json.dumps({"error": str(exc), "isolationVerified": False}), file=sys.stderr)
        return 2
    print(json.dumps(report, indent=2, allow_nan=False))
    return 1 if report["issues"] or report["summary"]["txCheckFailures"] or report["summary"]["rxCrcFailures"] or report["summary"]["unsupportedResponseTypes"] else 0


if __name__ == "__main__":
    sys.exit(main())
