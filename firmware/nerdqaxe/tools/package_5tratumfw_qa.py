#!/usr/bin/env python3
"""Verify and package the NerdQAxe++ paired OTA build. Never contacts a miner."""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from nerd_build_identity import BuildIdentity, QA, OCT, checked_identity

WEB = ROOT / "main/http_server/axe-os/dist/axe-os"
WEB_VERSION = ROOT / "main/http_server/axe-os/src/app/firmware-web-version.ts"
IDF_IMAGE = "espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805"
BOARD = "NERDQAXEPLUS2"
APP_BYTES = 4 * 1024 * 1024
WWW_BYTES = 3 * 1024 * 1024
PRIVATE_PARTS = {".git", ".cache", "local-devices", "node_modules", "managed_components", "release", "artifacts", "__pycache__", ".venv", "venv"}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(*args: str, cwd: Path = ROOT) -> str:
    return subprocess.check_output(args, cwd=cwd, text=True).strip()


def safe_relative(name: str) -> Path:
    path = Path(name)
    if not name or path.is_absolute() or ".." in path.parts or any(part in PRIVATE_PARTS for part in path.parts):
        raise ValueError("Source inventory contains an unsafe or private path")
    if any(part == "build" or (part.startswith("build-") and part != "build-aux") for part in path.parts[:-1]):
        raise ValueError("Source inventory contains generated build files")
    if path.name.startswith(".env") or path.suffix in {".bin", ".log", ".pyc", ".backup"}:
        raise ValueError("Source inventory contains a generated or private file")
    return path


def source_receipt(identity: BuildIdentity = QA) -> tuple[str, dict[str, Path], dict[str, str], str]:
    """Use reviewed tracked files only, or verify a corresponding source inventory."""
    checked_identity(identity)
    probe = subprocess.run(["git", "rev-parse", "--show-toplevel"], cwd=ROOT, capture_output=True, text=True)
    files: dict[str, Path] = {}
    submodules: dict[str, str] = {}
    if probe.returncode == 0:
        subprocess.run(["git", "diff", "--quiet", "--ignore-submodules=none", "HEAD", "--", "."], cwd=ROOT, check=True)
        commit = run("git", "rev-parse", "HEAD")
        prefix = run("git", "rev-parse", "--show-prefix").rstrip("/") or "."
        raw = subprocess.check_output(["git", "ls-files", "--cached", "-z", "--", "."], cwd=ROOT)
        for name in filter(None, raw.decode().split("\0")):
            relative = safe_relative(name)
            path = ROOT / relative
            if path.is_symlink():
                raise ValueError("Source symlinks are not packaged")
            if path.is_file():
                files[name] = path
            elif path.is_dir():
                staged = run("git", "ls-files", "--stage", "--", name)
                fields = staged.split(maxsplit=3)
                if len(fields) != 4 or fields[0] != "160000":
                    raise ValueError("Unexpected tracked source directory")
                pinned = fields[1]
                actual = run("git", "rev-parse", "HEAD", cwd=path)
                if actual != pinned:
                    raise ValueError("Submodule checkout differs from its pinned commit")
                subprocess.run(["git", "diff", "--quiet", "HEAD"], cwd=path, check=True)
                submodules[name] = pinned
                nested = subprocess.check_output(["git", "ls-files", "--cached", "-z"], cwd=path)
                for child in filter(None, nested.decode().split("\0")):
                    archive_name = str(relative / safe_relative(child))
                    candidate = path / child
                    if not candidate.is_file() or candidate.is_symlink():
                        raise ValueError("Submodule source is missing or is a symlink")
                    files[archive_name] = candidate
    else:
        inventory = json.loads((ROOT / "SOURCE_FILES.json").read_text())
        commit = inventory.get("sourceCommit", "")
        prefix = inventory.get("sourceRootInRepository", ".")
        submodules = inventory.get("submodules", {})
        for name, expected in inventory.get("files", {}).items():
            path = ROOT / safe_relative(name)
            if not path.is_file() or path.is_symlink() or sha256(path) != expected:
                raise ValueError("Corresponding source inventory verification failed")
            files[name] = path
    required = {"LICENSE", identity.version_file, identity.build_helper, identity.package_helper,
                "tools/package_5tratumfw_qa.py", "tools/nerd_build_identity.py",
                identity.web_version_file, identity.board_source}
    if identity == OCT:
        required.add("main/boards/five_tratum_model_labels.h")
    if not re.fullmatch(r"[0-9a-f]{40}", commit) or not required.issubset(files):
        raise ValueError("A clean reviewed source commit, model build helpers and GPL license are required")
    if not isinstance(submodules, dict) or any(not re.fullmatch(r"[0-9a-f]{40}", str(v)) for v in submodules.values()):
        raise ValueError("Invalid pinned submodule provenance")
    return commit, files, submodules, prefix


def version(identity: BuildIdentity = QA) -> str:
    checked_identity(identity)
    match = re.search(r"export\s+const\s+FIRMWARE_WEB_VERSION\s*=\s*'([^']+)'\s*;", (ROOT / identity.web_version_file).read_text())
    value = match.group(1) if match else ""
    if (ROOT / identity.version_file).read_text().strip() != value:
        raise ValueError(f"{identity.version_file} and its FIRMWARE_WEB_VERSION must match")
    if not re.fullmatch(rf"5tratumFW-{identity.family}-\d+\.\d+\.\d+-beta\.\d+", value) or len(value.encode("ascii")) > 31:
        raise ValueError("Expected the exact model-family BETA version fitting the ESP app descriptor")
    return value


def web_provenance(commit: str, release_version: str, identity: BuildIdentity = QA) -> dict:
    checked_identity(identity)
    entry = WEB / "index.html.gz"
    if not entry.is_file():
        raise ValueError("Build the compressed production web interface first")
    scripts = list(WEB.glob("*.js.gz"))
    if not scripts or not any(release_version.encode() in gzip.decompress(p.read_bytes()) for p in scripts):
        raise ValueError("Actual compressed JavaScript does not contain the paired firmware web version")
    node_version = run("node", "--version")
    if node_version != "v24.14.0":
        raise ValueError("Use pinned Node v24.14.0")
    return {"product": "5tratumFW", "version": release_version, "boardProfile": identity.board,
            "nodeVersion": node_version[1:], "sourceCommit": commit}


def cache_values(path: Path) -> dict[str, str]:
    values = {}
    for line in path.read_text().splitlines():
        match = re.match(r"([^:#]+):[^=]+=(.*)$", line)
        if match:
            values[match.group(1)] = match.group(2)
    return values


def package(build: Path, output: Path, identity: BuildIdentity = QA) -> dict:
    checked_identity(identity)
    commit, sources, submodules, prefix = source_receipt(identity)
    release_version = version(identity)
    description = json.loads((build / "project_description.json").read_text())
    config = json.loads((build / "config/sdkconfig.json").read_text())
    receipt = json.loads((build / "build-source-receipt.json").read_text())
    expected_receipt = {"sourceCommit": commit, "boardProfile": identity.board, "version": release_version, "idfImage": IDF_IMAGE}
    if receipt != expected_receipt or description.get("project_version") != release_version:
        raise ValueError("Firmware build receipt/version differs from the reviewed source")
    if config.get("IDF_TARGET") != "esp32s3" or description.get("target") != "esp32s3":
        raise ValueError("Expected the ESP32-S3 target")
    cache = cache_values(build / "CMakeCache.txt")
    if cache.get("FIVETRATUM_RELEASE_PROFILE") != identity.board:
        raise ValueError("Target build lacks the exact public release profile")
    for flag in ("FIVETRATUM_ASIC_CAPTURE_LOGS", "FIVETRATUM_BM1370_CAPTURE", "FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER"):
        if cache.get(flag) != "OFF":
            raise ValueError("Public builds require diagnostic driver, passive capture and serial capture logs OFF")
    commands = json.loads((build / "compile_commands.json").read_text())
    main_command = next((c.get("command", "") for c in commands if c.get("file", "").endswith("/main/main.cpp")), "")
    board_flags = set(re.findall(r"(?:^|\s)-D((?:NERD|Q13)[A-Z0-9_]+)(?:\s|$)", main_command))
    if board_flags != {identity.board}:
        raise ValueError(f"Application compiler did not select only {identity.board}")
    if config.get("PARTITION_TABLE_CUSTOM_FILENAME") != "partitions.csv":
        raise ValueError("Unexpected partition layout")
    # Verify the board contract from the reviewed implementation, not a guessed name.
    board_source = (ROOT / identity.board_source).read_text()
    model_matches = bool(re.search(rf'm_deviceModel\s*=\s*"{re.escape(identity.model)}"\s*;', board_source))
    if identity == OCT:
        labels = (ROOT / "main/boards/five_tratum_model_labels.h").read_text()
        # The real API model is UTF-8; its subscribe agent is the separately
        # reviewed ASCII constant. Do not relax the QA literal model contract.
        model_matches = bool(
            re.search(r'#include\s+"five_tratum_model_labels\.h"', board_source)
            and re.search(r'm_deviceModel\s*=\s*FiveTratumModels::OctaxeGamma\s*;', board_source)
            and re.search(r'm_miningAgent\s*=\s*FiveTratumModels::OctaxeGammaMiningAgent\s*;', board_source)
            and re.search(r'namespace\s+FiveTratumModels\s*\{', labels)
            and re.search(r'constexpr\s+char\s+OctaxeGamma\[\]\s*=\s*"NerdOCTAXE-\\xCE\\xB3"\s*;', labels)
            and re.search(r'constexpr\s+char\s+OctaxeGammaMiningAgent\[\]\s*=\s*"NerdOCTAXE-Gamma"\s*;', labels)
        )
    if (not re.search(r'm_asicModel\s*=\s*"BM1370"\s*;', board_source)
        or not re.search(rf"m_asicCount\s*=\s*{identity.asic_count}\s*;", board_source)
        or not model_matches):
        raise ValueError("Selected board source no longer matches its exact model/BM1370/count contract")
    partition_text = (ROOT / "partitions.csv").read_text()
    expected_partitions = {"www": ("0x410000", "3M"), "ota_0": ("0x710000", "4M"), "ota_1": ("0xb10000", "4M")}
    rows = [list(map(str.strip, line.split(","))) for line in partition_text.splitlines() if line.strip() and not line.lstrip().startswith("#")]
    for name, (offset, size) in expected_partitions.items():
        row = next((r for r in rows if r[0] == name), [])
        if len(row) < 5 or row[3].lower() != offset or row[4] != size:
            raise ValueError("The reviewed OTA/WWW partition geometry changed")
    firmware = build / "esp-miner.bin"
    website = build / "www.bin"
    image = firmware.read_bytes()
    if not 176 <= len(image) < APP_BYTES or image[0] != 0xE9 or struct.unpack_from("<I", image, 32)[0] != 0xABCD5432:
        raise ValueError("Expected an ESP-IDF application image fitting a 4 MiB OTA slot")
    if struct.unpack_from("<H", image, 12)[0] != 9:
        raise ValueError("Application image is not an ESP32-S3 image")
    descriptor_version = image[48:80].split(b"\0", 1)[0].decode("ascii")
    idf_version = image[144:176].split(b"\0", 1)[0].decode("ascii")
    if descriptor_version != release_version or idf_version != "v5.5.3":
        raise ValueError("Application descriptor version/toolchain does not match")
    if website.stat().st_size != WWW_BYTES:
        raise ValueError("Expected a complete 3 MiB WWW partition image")
    expected_web = web_provenance(commit, release_version, identity)
    actual_web = json.loads(gzip.decompress((WEB / "build-info.json.gz").read_bytes()))
    if actual_web != expected_web:
        raise ValueError("Compressed web build provenance differs from the paired application")
    payload = [p for p in WEB.rglob("*") if p.is_file()]
    if any(p.is_symlink() for p in WEB.rglob("*")):
        raise ValueError("Web payload must not contain symlinks")
    payload_bytes = sum(p.stat().st_size for p in payload)
    if payload_bytes >= WWW_BYTES:
        raise ValueError("Web payload exceeds the WWW partition before filesystem metadata")
    verify = build / "www-verify.bin"
    relative_verify = verify.relative_to(ROOT)
    command = ["docker", "run", "--rm", "--mount", f"type=bind,source={ROOT},target=/project", "-w", "/project", IDF_IMAGE,
               "python", "/opt/esp/idf/components/spiffs/spiffsgen.py", str(WWW_BYTES), "/project/main/http_server/axe-os/dist/axe-os",
               "/project/" + str(relative_verify), "--page-size=" + str(config["SPIFFS_PAGE_SIZE"]),
               "--obj-name-len=" + str(config["SPIFFS_OBJ_NAME_LEN"]), "--meta-len=" + str(config["SPIFFS_META_LENGTH"])]
    if config.get("SPIFFS_USE_MAGIC"):
        command.append("--use-magic")
    if config.get("SPIFFS_USE_MAGIC_LENGTH"):
        command.append("--use-magic-len")
    subprocess.run(command, check=True, capture_output=True)
    if sha256(verify) != sha256(website):
        raise ValueError("WWW image is stale or does not contain the current verified web payload")
    archive_files = dict(sources)
    sdkconfig = Path(description["config_file"])
    if str(sdkconfig).startswith("/project/"):
        sdkconfig = ROOT / sdkconfig.relative_to("/project")
    if sdkconfig.resolve() != (build / "sdkconfig").resolve():
        raise ValueError("Build configuration must be isolated inside the selected build directory")
    archive_files["BUILD_CONFIG/sdkconfig"] = sdkconfig
    lock = ROOT / "dependencies.lock"
    if lock.is_file():
        archive_files["dependencies.lock"] = lock
    inventory = {"sourceCommit": commit, "sourceRootInRepository": prefix, "submodules": submodules,
                 "files": {name: sha256(path) for name, path in sorted(archive_files.items())}}
    # Validate every input before creating a reviewable output. Refuse reuse of a release directory.
    if output.exists():
        raise ValueError("Output already exists; never overwrite a previously packaged release")
    output.mkdir(parents=True)
    app_name = identity.app_name
    shutil.copy2(firmware, output / app_name)
    shutil.copy2(website, output / "www.bin")
    archive_name = f"5tratumFW-{identity.asset_model}-{release_version}-source.zip"
    with zipfile.ZipFile(output / archive_name, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, path in sorted(archive_files.items()):
            archive.write(path, arcname=name)
        archive.writestr("SOURCE_FILES.json", json.dumps(inventory, sort_keys=True, indent=2) + "\n")
    web_report = {"payloadBytes": payload_bytes, "gzipBytes": sum(p.stat().st_size for p in payload if p.suffix == ".gz"),
                  "fileCount": len(payload), "wwwPartitionBytes": WWW_BYTES, "recreatedWwwMatches": True,
                  "files": {str(p.relative_to(WEB)): {"bytes": p.stat().st_size, "sha256": sha256(p)} for p in sorted(payload)}}
    (output / "web-budget.json").write_text(json.dumps(web_report, indent=2) + "\n")
    manifest = {"schema": 1, "product": "5tratumFW", "version": release_version, "releaseChannel": "BETA", "githubPrerelease": True,
                "repository": "https://github.com/WillItMod/5tratumFW", "sourceRootInRepository": prefix,
                "firmwareBuildCommit": commit, "sourceArchiveCommit": commit, "boardProfile": identity.board, "deviceModel": identity.model,
                "asicModel": "BM1370", "asicCount": identity.asic_count, "physicalPcbRevision": "unidentified", "target": "esp32s3",
                "releaseTag": identity.tag_prefix + release_version.removeprefix(f"5tratumFW-{identity.family}-"),
                "partitionLayout": {"source": "partitions.csv", "flashBytesAssumed": 16 * 1024 * 1024,
                    "wwwOffset": "0x410000", "wwwBytes": WWW_BYTES, "otaOffsets": ["0x710000", "0xb10000"],
                    "otaSlotBytes": APP_BYTES, "physicalLayoutVerified": False},
                "imageType": "Paired application and WWW OTA images; no factory or NVS image",
                "idfVersion": idf_version[1:], "idfImage": IDF_IMAGE, "nodeVersion": "24.14.0", "pinnedSubmodules": submodules,
                "upstream": {"repository": "https://github.com/shufps/ESP-Miner-NerdQAxePlus", "tag": "v1.1.0-rc1-test1",
                             "commit": "8b45522a6695c6bbccc3d032370fe08a29856d80"},
                "independentWorkAssignment": False, "workTargeting": "chain-broadcast", "nativePoolStreamAgentVersion": 1,
                "diagnosticDriver": False, "passiveCapture": False, "captureLogs": False,
                "automaticTrialRollback": bool(config.get("BOOTLOADER_APP_ROLLBACK_ENABLE", False)),
                "hardwareTested": False, "qualification": "Build/package checks only; consult this version's separate physical validation record",
                "unsupportedModels": ["Bitaxe Gamma", "NerdQAxe+", "NerdOctAxe", "other Nerd board profiles",
                    OCT.model if identity == QA else QA.model],
                "images": {app_name: {"partitionBytes": APP_BYTES}, "www.bin": {"partitionBytes": WWW_BYTES}}}
    names = [app_name, "www.bin", archive_name, "web-budget.json"]
    manifest["files"] = {name: {"bytes": (output / name).stat().st_size, "sha256": sha256(output / name)} for name in names}
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    names.append("manifest.json")
    (output / "SHA256SUMS").write_text("".join(f"{sha256(output / name)}  {name}\n" for name in names))
    return {"package": str(output), "version": release_version, "boardProfile": identity.board, "webPayloadBytes": payload_bytes, "files": len(names)}


def main(identity: BuildIdentity = QA) -> None:
    checked_identity(identity)
    parser = argparse.ArgumentParser(description=f"Verify {identity.model} paired OTA only; never contacts a miner.")
    parser.add_argument("--record-web-build", action="store_true", help="Stamp the actual compressed web build before target compilation")
    parser.add_argument("--build-dir", default=identity.build_directory)
    parser.add_argument("--output", help="New model-specific output directory; defaults under ignored release/")
    args = parser.parse_args()
    try:
        if args.record_web_build:
            commit, _, _, _ = source_receipt(identity)
            proof = web_provenance(commit, version(identity), identity)
            (WEB / "build-info.json.gz").write_bytes(gzip.compress((json.dumps(proof, sort_keys=True) + "\n").encode(), mtime=0))
            print(json.dumps({"webVersion": proof["version"], "sourceCommit": commit, "nodeVersion": proof["nodeVersion"]}))
            return
        build = (ROOT / args.build_dir).resolve()
        build.relative_to(ROOT / "build")
        output = (ROOT / (args.output or f"release/{identity.asset_model}/{version(identity)}")).resolve()
        output.relative_to(ROOT / "release")
        print(json.dumps(package(build, output, identity), indent=2))
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError, StopIteration) as error:
        raise SystemExit(f"{identity.model} package verification failed: {error}") from error


if __name__ == "__main__":
    main()
