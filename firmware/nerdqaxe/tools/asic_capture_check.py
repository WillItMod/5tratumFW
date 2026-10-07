#!/usr/bin/env python3
"""Bounded, offline BM1370 capture checks. No serial/network/device access.

Interpretations come from this repository's Asic::send/processWork, BM1370 ID
mapping, construct_bm_job and test_nonce_value. RX CRC is checked against the
pinned Mujina captured BM1370 frame format, not a qualification of this QAxe.
This tool never verifies independent work assignment or physical isolation.
See docs/asic-capture-checker.md for the closed input schema and limits.
"""
from __future__ import annotations

import argparse
from collections import defaultdict
from decimal import Decimal, InvalidOperation, localcontext
import hashlib
import json
from pathlib import Path
import re
import sys

MAX_FILE_BYTES = 2 * 1024 * 1024
MAX_EVENTS = 4096
MAX_WORKS = 16
MAX_MAPPING_PREVIEW = 8
DIFF1_TARGET = 0xFFFF << 208
RX_CRC_STATUS = "captured-bm1370-format-checked-per-event"
RX_CRC_VALID = "verified-captured-bm1370-format"
RX_CRC_FAILED = "failed-captured-bm1370-format"
RX_CRC_EVIDENCE = {
    "id": "mujina-bm1370-crc5-v1", "revision": "980128574d8bb8b9619b0081b26e18eb4d0c5e31",
    "scope": "captured 11-byte BM1370 frame format; not physical QAxe qualification",
    "reference": "https://github.com/256foundation/mujina/blob/980128574d8bb8b9619b0081b26e18eb4d0c5e31/mujina-miner/src/asic/bm13xx/REFERENCE.md#L333-L352",
    "codec": "https://github.com/256foundation/mujina/blob/980128574d8bb8b9619b0081b26e18eb4d0c5e31/mujina-miner/src/asic/bm13xx/codec.rs#L111-L118",
    "physicalDeviceQualification": False,
}


class InputError(ValueError):
    pass


def closed(value, required, optional=()):
    if type(value) is not dict:
        raise InputError("expected object")
    if set(value) - set(required) - set(optional) or set(required) - set(value):
        raise InputError("missing or unknown object keys")


def integer(value, low, high):
    if type(value) is not int or not low <= value <= high:
        raise InputError("integer out of bounds")
    return value


def label(value, maximum=64):
    if type(value) is not str or not 1 <= len(value) <= maximum:
        raise InputError("label length out of bounds")
    if any(ord(c) < 32 or ord(c) > 126 for c in value):
        raise InputError("label must be printable ASCII")
    return value


def hex_bytes(value, length=None, maximum=255):
    if type(value) is not str or len(value) % 2 or not re.fullmatch(r"[0-9a-f]*", value):
        raise InputError("hex must use complete lowercase bytes")
    size = len(value) // 2
    if length is not None and size != length or not 1 <= size <= maximum:
        raise InputError("hex byte length out of bounds")
    return bytes.fromhex(value)


def difficulty(value):
    # Exact decimal strings avoid a JSON binary-float threshold silently changing.
    if type(value) is not str or len(value) > 64 or not re.fullmatch(r"[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?", value):
        raise InputError("ticketDifficulty must be a positive bounded decimal string")
    try:
        result = Decimal(value)
    except InvalidOperation as exc:
        raise InputError("invalid ticket difficulty") from exc
    if not result.is_finite() or result <= 0 or not -64 <= result.adjusted() <= 64:
        raise InputError("ticket difficulty out of bounds")
    return result


def no_duplicates(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise InputError("duplicate JSON key")
        result[key] = value
    return result


def read_capture(path):
    with Path(path).open("rb") as stream:
        raw = stream.read(MAX_FILE_BYTES + 1)
    if len(raw) > MAX_FILE_BYTES:
        raise InputError("capture exceeds 2 MiB")
    try:
        data = json.loads(raw.decode("utf-8"), object_pairs_hook=no_duplicates,
                          parse_constant=lambda _: (_ for _ in ()).throw(InputError("nonfinite JSON number")))
    except InputError:
        raise
    except (UnicodeError, ValueError, RecursionError) as exc:
        raise InputError("invalid capture JSON") from exc
    return data, hashlib.sha256(raw).hexdigest()


def validate_capture(data):
    closed(data, ("captureVersion", "provenance", "asicModel", "attribution", "works", "events"),
           ("deviceBinding", "negativeControl"))
    if type(data["captureVersion"]) is not int or data["captureVersion"] != 1 or data["asicModel"] != "BM1370":
        raise InputError("only captureVersion 1 / BM1370 is supported")
    if data["provenance"] not in ("observed", "simulated"):
        raise InputError("provenance must be observed or simulated")
    attribution = data["attribution"]
    if attribution is not None:
        closed(attribution, ("mode", "addresses", "source"))
        if attribution["mode"] != "nerd-bm1370-v1" or attribution["source"] not in (
                "observed-enumeration", "source-inferred", "simulated"):
            raise InputError("unsupported attribution mode/source")
        addresses = attribution["addresses"]
        if type(addresses) is not list or not 1 <= len(addresses) <= 8:
            raise InputError("expected 1..8 declared chip addresses")
        for address in addresses:
            integer(address, 0, 255)
        interval = 256 // (1 << (len(addresses) - 1).bit_length())
        if addresses != [i * interval for i in range(len(addresses))]:
            raise InputError("addresses differ from the declared source enumeration")
        if data["provenance"] == "simulated" and attribution["source"] == "observed-enumeration":
            raise InputError("simulated data cannot claim observed enumeration")
    binding = data.get("deviceBinding")
    if binding is not None:
        closed(binding, ("firmwareSha256", "sourceRevision", "boardProfile", "pcbRevision"),
               ("configurationBefore", "configurationAfter"))
        for key in ("sourceRevision", "boardProfile", "pcbRevision"):
            if binding[key] is not None:
                label(binding[key])
        if binding["firmwareSha256"] is not None:
            hex_bytes(binding["firmwareSha256"], 32)
        for key in ("configurationBefore", "configurationAfter"):
            if key in binding:
                closed(binding[key], ("frequencyMHz", "coreVoltageMilliVolts", "vrFrequencyHz"))
                for field in binding[key]:
                    integer(binding[key][field], 1, 0xFFFFFFFF)
    if type(data["works"]) is not list or len(data["works"]) > MAX_WORKS:
        raise InputError("at most 16 work templates")
    works = {}
    for work in data["works"]:
        closed(work, ("id", "headerHex", "versionMask", "ticketDifficulty"),
               ("startingNonce", "requiredVersionMask", "requiredVersionValue"))
        work_id = label(work["id"])
        if work_id in works:
            raise InputError("duplicate work id")
        header = hex_bytes(work["headerHex"], 80)
        mask = integer(work["versionMask"], 0, 0xFFFFFFFF)
        start = integer(work.get("startingNonce", 0), 0, 0xFFFFFFFF)
        required_mask = integer(work.get("requiredVersionMask", 0), 0, 0xFFFFFFFF)
        required_value = integer(work.get("requiredVersionValue", 0), 0, 0xFFFFFFFF)
        if required_value & ~required_mask:
            raise InputError("required version value exceeds its mask")
        works[work_id] = {"header": header, "versionMask": mask, "startingNonce": start,
                          "requiredVersionMask": required_mask, "requiredVersionValue": required_value,
                          "difficulty": difficulty(work["ticketDifficulty"])}
    if type(data["events"]) is not list or not 1 <= len(data["events"]) <= MAX_EVENTS:
        raise InputError("expected 1..4096 events")
    previous = -1
    for event in data["events"]:
        if type(event) is not dict or event.get("type") not in ("tx", "rx"):
            raise InputError("event type must be tx or rx")
        optional = ("work", "generation", "logicalJobCounter") if event["type"] == "tx" else ("candidateHeaders",)
        closed(event, ("type", "atUs", "packetHex"), optional)
        at = integer(event["atUs"], 0, (1 << 63) - 1)
        if at < previous:
            raise InputError("event timestamps must be nondecreasing")
        previous = at
        hex_bytes(event["packetHex"], maximum=255)
        if event["type"] == "tx":
            if "work" in event:
                if label(event["work"]) not in works or "generation" not in event:
                    raise InputError("bound TX requires a known work and generation")
                integer(event["generation"], 1, (1 << 63) - 1)
            elif "generation" in event or "logicalJobCounter" in event:
                raise InputError("generation/counter requires a work binding")
            if "logicalJobCounter" in event:
                integer(event["logicalJobCounter"], 0, 0xFFFFFFFF)
        else:
            candidates = event.get("candidateHeaders", [])
            if type(candidates) is not list or len(candidates) > MAX_WORKS:
                raise InputError("at most 16 candidate headers per response")
            for candidate in candidates:
                closed(candidate, ("work", "headerHex"))
                if label(candidate["work"]) not in works:
                    raise InputError("candidate references unknown work")
                hex_bytes(candidate["headerHex"], 80)
    control = data.get("negativeControl")
    if control is not None:
        closed(control, ("firstWork", "secondWork", "secondTxEventIndex", "windowStartUs", "windowEndUs", "minPerAsic", "drainBasis"))
        first_id, second_id = label(control["firstWork"]), label(control["secondWork"])
        if first_id not in works or second_id not in works or first_id == second_id:
            raise InputError("negative control requires two distinct known works")
        integer(control["secondTxEventIndex"], 0, len(data["events"]) - 1)
        integer(control["windowStartUs"], 0, (1 << 63) - 1)
        integer(control["windowEndUs"], control["windowStartUs"], (1 << 63) - 1)
        integer(control["minPerAsic"], 1, 4096)
        if control["drainBasis"] is not None:
            label(control["drainBasis"], 256)
    return works


def crc5_source(data):
    """Exact source shift recurrence for the non-overflowing input domain."""
    if not 1 <= len(data) <= 31:
        raise ValueError("source CRC5 length counter overflows beyond 31 bytes")
    registers = [1] * 5
    for byte in data:
        for bit in range(7, -1, -1):
            feedback = registers[4] ^ ((byte >> bit) & 1)
            registers = [feedback, registers[0], registers[1] ^ feedback, registers[2], registers[3]]
    return sum(value << bit for bit, value in enumerate(registers))


def crc16_false_source(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ (0x1021 if crc & 0x8000 else 0)) & 0xFFFF
    return crc


def reverse_words(data):
    """Reverse eight 4-byte words, preserving bytes within each word."""
    return b"".join(data[i:i + 4] for i in range(len(data) - 4, -1, -4))


def decode_tx(packet):
    result = {"checksPassed": False, "errors": [], "warnings": []}
    errors, warnings = result["errors"], result["warnings"]
    if len(packet) < 5:
        errors.append("tx-frame-too-short")
        return result
    if packet[:2] != b"\x55\xaa":
        errors.append("tx-preamble")
    header = packet[2]
    is_job = bool(header & 0x20)  # Asic::send classification, not a protocol guess.
    result.update(header=f"{header:02x}", packetKind="job" if is_job else "command", lengthField=packet[3])
    if packet[3] != len(packet) - 2:
        errors.append("tx-length-field")
    if is_job and len(packet) < 6:
        errors.append("tx-job-too-short")
        return result
    payload = packet[4:-2 if is_job else -1]
    result["payloadBytes"] = len(payload)
    if is_job:
        computed = crc16_false_source(packet[2:-2])
        supplied = int.from_bytes(packet[-2:], "big")
        result["crc"] = {"algorithm": "source-crc16-ccitt-false", "computed": computed, "supplied": supplied, "verified": computed == supplied}
    elif len(packet[2:-1]) <= 31:
        computed = crc5_source(packet[2:-1])
        supplied = packet[-1]
        result["crc"] = {"algorithm": "source-crc5", "computed": computed, "supplied": supplied, "verified": computed == supplied}
    else:
        result["crc"] = {"algorithm": "source-crc5", "verified": False, "status": "unverified-source-length-counter-overflow"}
        warnings.append("command-crc-input-exceeds-source-domain")
    if "computed" in result["crc"] and not result["crc"]["verified"]:
        errors.append("tx-crc-mismatch")
    if is_job:
        result["jobSemantics"] = "source-established-0x21" if header == 0x21 else "unknown-header-no-targeting-claim"
        if header != 0x21:
            warnings.append("unknown-job-header-semantics")
        elif len(payload) != 82:
            errors.append("source-job-payload-must-be-82-bytes")
        else:
            result["job"] = {
                "wireJobId": payload[0], "numMidstates": payload[1],
                "startingNonce": int.from_bytes(payload[2:6], "little"),
                "nBits": int.from_bytes(payload[6:10], "little"),
                "nTime": int.from_bytes(payload[10:14], "little"),
                "merkleWireHex": payload[14:46].hex(), "prevWireHex": payload[46:78].hex(),
                "baseVersion": int.from_bytes(payload[78:82], "little"),
            }
            if payload[0] not in range(0, 128, 8):
                errors.append("wire-job-id-outside-source-mapping")
            if payload[1] != 1:
                errors.append("num-midstates-differs-from-source-sender")
    else:
        # A single-address register write is not evidence of addressed work.
        names = {0x40: "set-address", 0x41: "register-write-single", 0x51: "register-write-all",
                 0x52: "register-read-all", 0x53: "chain-inactive"}
        result["commandSemantics"] = names.get(header, "unknown-header")
        if header in (0x40, 0x41) and payload:
            result["addressField"] = payload[0]
        if header in (0x41, 0x51) and len(payload) == 6:
            result.update(register=payload[1], registerValue=int.from_bytes(payload[2:], "big"))
        if header not in names:
            warnings.append("unknown-command-header-semantics")
    result["checksPassed"] = not errors and result["crc"]["verified"]
    return result


def bind_tx(decoded, event, works):
    if "work" not in event:
        return
    if not decoded["checksPassed"] or "job" not in decoded:
        decoded["errors"].append("cannot-bind-work-to-invalid-or-unknown-job")
        decoded["checksPassed"] = False
        return
    job = decoded["job"]
    work = works[event["work"]]
    template = work["header"]
    expected = {
        "startingNonce": work["startingNonce"], "nBits": int.from_bytes(template[72:76], "little"),
        "nTime": int.from_bytes(template[68:72], "little"), "merkleWireHex": reverse_words(template[36:68]).hex(),
        "prevWireHex": reverse_words(template[4:36]).hex(), "baseVersion": int.from_bytes(template[:4], "little"),
    }
    for field, value in expected.items():
        if job[field] != value:
            decoded["errors"].append(f"work-template-mismatch:{field}")
    if "logicalJobCounter" in event and job["wireJobId"] != ((event["logicalJobCounter"] * 24) & 0x7F):
        decoded["errors"].append("logical-counter-wire-id-mismatch")
    decoded["checksPassed"] = not decoded["errors"]
    if decoded["checksPassed"]:
        decoded["workBinding"] = {"work": event["work"], "generation": event["generation"]}


def decode_rx(packet, attribution):
    result = {"checksPassed": False, "responseCrc": "not-checked-invalid-framing", "errors": [], "warnings": []}
    if len(packet) != 11:
        result["errors"].append("rx-frame-must-be-11-bytes")
        return result
    if packet[:2] != b"\xaa\x55":
        result["errors"].append("rx-preamble")
        return result
    result["flagsCrcByte"] = packet[10]
    # Captured BM1370 frames pack three type bits and five CRC bits together.
    # Validate all 9 bytes after the preamble, INCLUDING that entire trailer.
    residue = crc5_source(packet[2:])
    result["crc"] = {"algorithm": "crc5-poly05-init1f-no-reflection-no-xor", "residue": residue,
                     "verified": residue == 0, "typeBits": packet[10] >> 5, "checksumBits": packet[10] & 0x1F,
                     "evidenceId": RX_CRC_EVIDENCE["id"]}
    result["responseCrc"] = RX_CRC_VALID if residue == 0 else RX_CRC_FAILED
    if residue:
        result["errors"].append("rx-crc-mismatch")
        return result
    is_nonce = bool(packet[10] & 0x80)
    result["packetKind"] = "nonce" if is_nonce else "register"
    if is_nonce:
        address = (int.from_bytes(packet[2:6], "big") >> 17) & 0xFF
        result.update(nonce=int.from_bytes(packet[2:6], "little"), rawJobByte=packet[7],
                      wireJobId=(packet[7] & 0xF0) >> 1, discardedJobBits=packet[7] & 0x0F,
                      midstateByte=packet[6], rolledVersionBits=int.from_bytes(packet[8:10], "big") << 13,
                      nonceAddressBits=address)
    else:
        address = packet[6]
        result.update(register=packet[7], data=int.from_bytes(packet[2:6], "big"), responseAddressByte=address)
    if attribution is None:
        result.update(sourceAsicIndex=None, enumeratedAddress=None)
        result["warnings"].append("no-declared-enumeration")
    else:
        interval = 256 // (1 << (len(attribution["addresses"]) - 1).bit_length())
        index = address // interval
        result["sourceAsicIndex"] = index
        result["enumeratedAddress"] = attribution["addresses"][index] if index < len(attribution["addresses"]) else None
        if result["enumeratedAddress"] is None:
            result["warnings"].append("source-index-outside-declared-enumeration")
    # Framing/captured-format CRC/source decode, not physical qualification.
    result["checksPassed"] = True
    return result


def check_candidate(candidate, response, works, mapped_works):
    work_id = candidate["work"]
    work = works[work_id]
    header = bytes.fromhex(candidate["headerHex"])
    digest = hashlib.sha256(hashlib.sha256(header).digest()).digest()
    hash_integer = int.from_bytes(digest, "little")
    version = int.from_bytes(header[:4], "little")
    base_version = int.from_bytes(work["header"][:4], "little")
    failures = []
    if response.get("packetKind") != "nonce" or not response["checksPassed"]:
        failures.append("response-is-not-a-source-decoded-nonce")
    else:
        if int.from_bytes(header[76:], "little") != response["nonce"]:
            failures.append("candidate-nonce-mismatch")
        if version != base_version | response["rolledVersionBits"]:
            failures.append("candidate-version-mismatch")
    if header[4:76] != work["header"][4:76]:
        failures.append("candidate-immutable-header-mismatch")
    if (version ^ base_version) & (~work["versionMask"] & 0xFFFFFFFF):
        failures.append("version-change-outside-declared-mask")
    if version & work["requiredVersionMask"] != work["requiredVersionValue"]:
        failures.append("required-version-bits-mismatch")
    if work_id not in mapped_works:
        failures.append("candidate-work-not-bound-to-prior-tx-id")
    with localcontext() as context:
        context.prec = 160
        meets_ticket = Decimal(hash_integer) * work["difficulty"] <= Decimal(DIFF1_TARGET)
        bdiff = "infinity" if not hash_integer else str(Decimal(DIFF1_TARGET) / Decimal(hash_integer))
    return {"work": work_id, "hashHex": digest[::-1].hex(), "hashInteger": str(hash_integer),
            "difficultyUnit": "Bitcoin-reference-bdiff", "computedDifficulty": bdiff,
            "ticketDifficulty": str(work["difficulty"]), "meetsDeclaredTicket": meets_ticket,
            "bindingErrors": failures, "matchesResponseContext": not failures,
            "hashAndBindingPass": meets_ticket and not failures}


def negative_control_report(control, results, attribution, provenance):
    count = len(attribution["addresses"]) if attribution else 0
    counts, hash_counts = [0] * count, [0] * count
    issues = []
    cutover = control["secondTxEventIndex"]
    second = results[cutover]
    if second.get("type") != "tx" or second.get("workBinding", {}).get("work") != control["secondWork"] or not second["checksPassed"]:
        issues.append("cutover-is-not-a-validated-second-work-tx")
    if control["windowStartUs"] < second["atUs"]:
        issues.append("measurement-window-precedes-cutover")
    first = [item for item in results[:cutover] if item.get("type") == "tx" and
             item.get("workBinding", {}).get("work") == control["firstWork"] and item["checksPassed"]]
    if not first:
        issues.append("no-validated-first-work-before-cutover")
    if first and "job" in second and any(item["job"]["wireJobId"] == second["job"]["wireJobId"] for item in first):
        issues.append("negative-control-wire-id-reused")
    if not attribution:
        issues.append("no-declared-chip-enumeration")
    other_work, ambiguous, unbound = 0, 0, 0
    for item in results[cutover + 1:]:
        if item["type"] != "rx" or item.get("packetKind") != "nonce" or not item["checksPassed"]:
            continue
        if not control["windowStartUs"] <= item["atUs"] <= control["windowEndUs"]:
            continue
        binding = item["workMapping"]
        if binding["status"] == "ambiguous-id-reuse":
            ambiguous += 1
        elif binding["status"] == "unbound":
            unbound += 1
        elif binding["work"] != control["secondWork"]:
            other_work += 1
        else:
            index = item["sourceAsicIndex"]
            if index is not None and 0 <= index < count:
                counts[index] += 1
                if any(candidate["work"] == control["secondWork"] and candidate["hashAndBindingPass"] for candidate in item["candidateChecks"]):
                    hash_counts[index] += 1
    complete = bool(count) and not issues and all(value >= control["minPerAsic"] for value in counts)
    return {"observation": "consistent-with-chain-broadcast" if complete else "inconclusive",
            "evidenceKind": "simulated-fixture" if provenance == "simulated" else "self-declared-observation",
            "sourceDecodedSecondWorkByAsic": counts, "hashCheckedSecondWorkByAsic": hash_counts,
            "otherWorkResponsesInWindow": other_work, "ambiguousResponsesInWindow": ambiguous,
            "unboundResponsesInWindow": unbound, "issues": issues, "drainBasis": control["drainBasis"],
            "drainVerified": False, "responseCrc": RX_CRC_STATUS, "isolationVerified": False,
            "limitations": ["enumeration/provenance are supplied by the capture author",
                            "silence is not work exclusion; bounded samples cannot prove isolation",
                            "captured-format CRC does not qualify this physical device; drain semantics remain unverified"]}


def analyze_capture(data, capture_sha256=None):
    works = validate_capture(data)
    known_ids = defaultdict(set)
    mapping_previews = defaultdict(list)
    mapped_works = defaultdict(set)
    results = []
    for index, event in enumerate(data["events"]):
        packet = bytes.fromhex(event["packetHex"])
        decoded = decode_tx(packet) if event["type"] == "tx" else decode_rx(packet, data["attribution"])
        decoded.update(eventIndex=index, type=event["type"], atUs=event["atUs"])
        if event["type"] == "tx":
            bind_tx(decoded, event, works)
            if "workBinding" in decoded:
                binding = decoded["workBinding"]
                wire_id = decoded["job"]["wireJobId"]
                entry = (binding["work"], binding["generation"])
                if entry not in known_ids[wire_id]:
                    known_ids[wire_id].add(entry)
                    if len(mapping_previews[wire_id]) < MAX_MAPPING_PREVIEW:
                        mapping_previews[wire_id].append(entry)
                mapped_works[wire_id].add(binding["work"])
        else:
            mappings = known_ids.get(decoded.get("wireJobId"), set())
            if decoded.get("packetKind") == "nonce":
                if len(mappings) == 1:
                    work_id, generation = next(iter(mappings))
                    decoded["workMapping"] = {"status": "unique-prior-tx-binding", "work": work_id, "generation": generation}
                    decoded["fullVersion"] = int.from_bytes(works[work_id]["header"][:4], "little") | decoded["rolledVersionBits"]
                else:
                    decoded["workMapping"] = {"status": "ambiguous-id-reuse" if mappings else "unbound",
                                              "candidateCount": len(mappings),
                                              "candidatesTruncated": len(mappings) > MAX_MAPPING_PREVIEW,
                                              "candidates": [{"work": work_id, "generation": generation}
                                                             for work_id, generation in mapping_previews.get(decoded["wireJobId"], ())]}
                    decoded["warnings"].append(decoded["workMapping"]["status"])
            decoded["candidateChecks"] = [check_candidate(candidate, decoded, works, mapped_works.get(decoded.get("wireJobId"), set()))
                                          for candidate in event.get("candidateHeaders", [])]
            if not event.get("candidateHeaders") and decoded.get("packetKind") == "nonce":
                decoded["hashCheck"] = "skipped-no-explicit-80-byte-candidate"
        results.append(decoded)
    return {"reportVersion": 2, "tool": "asic_capture_check", "captureSha256": capture_sha256,
            "provenance": data["provenance"], "provenanceAttested": False,
            "deviceBinding": data.get("deviceBinding"), "declaredAttribution": data["attribution"],
            "verification": {"txCrc": "source-checked-per-event", "responseCrc": RX_CRC_STATUS,
                             "responseCrcEvidence": RX_CRC_EVIDENCE,
                             "independentWorkAssignment": False, "isolationVerified": False,
                             "physicalDeviceQualification": False},
            "summary": {"txEvents": sum(item["type"] == "tx" for item in results),
                        "rxEvents": sum(item["type"] == "rx" for item in results),
                        "unverifiedTxEvents": sum(item["type"] == "tx" and not item["checksPassed"] for item in results),
                        "crcCheckedRxEvents": sum(item["type"] == "rx" and "crc" in item for item in results),
                        "rxCrcFailures": sum(item.get("responseCrc") == RX_CRC_FAILED for item in results),
                        "eventsWithErrors": sum(bool(item["errors"]) for item in results),
                        "eventsWithWarnings": sum(bool(item["warnings"]) for item in results),
                        "failedCandidateChecks": sum(not candidate["hashAndBindingPass"] for item in results for candidate in item.get("candidateChecks", []))},
            "events": results,
            "negativeControl": negative_control_report(data["negativeControl"], results, data["attribution"], data["provenance"]) if data.get("negativeControl") is not None else None}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", help="closed-schema capture JSON file; no device access")
    args = parser.parse_args(argv)
    try:
        data, capture_hash = read_capture(args.capture)
        report = analyze_capture(data, capture_hash)
    except (InputError, OSError, RecursionError) as exc:
        print(json.dumps({"error": str(exc), "isolationVerified": False}), file=sys.stderr)
        return 2
    print(json.dumps(report, indent=2, allow_nan=False))
    return 1 if report["summary"]["eventsWithErrors"] or report["summary"]["failedCandidateChecks"] or report["summary"]["unverifiedTxEvents"] else 0


if __name__ == "__main__":
    sys.exit(main())
