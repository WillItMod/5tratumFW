#!/usr/bin/env python3
"""Package paired OTA images and their source without producing a factory image."""
from datetime import datetime, timezone
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

IDF_IMAGE = 'espressif/idf:v6.0.2@sha256:0d8c9773d48a327233f9c1d7c654ff0bcf133ae24503ea2e97a57cfe02b8cb67'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def embedded_table_symbol(elf):
    """Locate the compiled ESP32-S3 table without requiring a host cross-toolchain."""
    if len(elf) < 52 or elf[:6] != b'\x7fELF\x01\x01' or struct.unpack_from('<H', elf, 18)[0] != 94:
        raise SystemExit('Expected the ESP32-S3 ELF32 little-endian application')
    offset = struct.unpack_from('<I', elf, 32)[0]
    entry_size, count = struct.unpack_from('<HH', elf, 46)
    if entry_size != 40 or not count or offset + count * entry_size > len(elf):
        raise SystemExit('Invalid application ELF section table')
    sections = [struct.unpack_from('<10I', elf, offset + i * entry_size) for i in range(count)]
    matches = []
    for section in sections:
        if section[1] != 2:  # SHT_SYMTAB
            continue
        if section[9] != 16 or section[5] % 16 or section[4] + section[5] > len(elf) or section[6] >= count:
            raise SystemExit('Invalid application ELF symbol table')
        strings = sections[section[6]]
        if strings[4] + strings[5] > len(elf):
            raise SystemExit('Invalid application ELF string table')
        names = elf[strings[4]:strings[4] + strings[5]]
        for at in range(section[4], section[4] + section[5], 16):
            name, address, size, info, _, index = struct.unpack_from('<IIIBBH', elf, at)
            if name >= len(names):
                raise SystemExit('Invalid application ELF symbol name')
            end = names.find(b'\0', name)
            if end < 0:
                raise SystemExit('Unterminated application ELF symbol name')
            if names[name:end] == b'embedded_files' and info & 15 == 1 and index != 0:
                matches.append((address, size))
    if len(matches) != 1:
        raise SystemExit('Expected one compiled embedded web table in the application ELF')
    return matches[0]


def verify_embedded_web(image, elf_path, directory):
    """Compare every active compiled URL, gzip flag and byte against current dist."""
    elf = elf_path.read_bytes()
    if len(image) < 208 or hashlib.sha256(elf).digest() != image[176:208]:
        raise SystemExit('Application descriptor does not match the selected build ELF')
    address, size = embedded_table_symbol(elf)
    segments = []
    offset = 24
    if not 0 < image[1] <= 16:
        raise SystemExit('Invalid application segment count')
    for _ in range(image[1]):
        if offset + 8 > len(image):
            raise SystemExit('Truncated application segment header')
        start, length = struct.unpack_from('<II', image, offset)
        offset += 8
        if offset + length > len(image) or start + length > 2 ** 32:
            raise SystemExit('Truncated or invalid application segment')
        segments.append((start, image[offset:offset + length]))
        offset += length

    def memory(at, length):
        matches = [data[at - start:at - start + length] for start, data in segments
                   if start <= at and at + length <= start + len(data)]
        if len(matches) != 1:
            raise SystemExit('Embedded web pointer is outside a unique application load segment')
        return matches[0]

    expected = {}
    for path in directory.rglob('*'):
        if path.is_symlink():
            raise SystemExit('Web bundle must not contain symlinks')
        if not path.is_file():
            continue
        relative = path.relative_to(directory).as_posix()
        compressed = relative.endswith('.gz')
        url = '/' + (relative[:-3] if compressed else relative)
        if url in expected:
            raise SystemExit('Web bundle contains duplicate logical URLs')
        expected[url] = (path.read_bytes(), compressed)
    if not expected or size != 16 * len(expected):
        raise SystemExit('Compiled web asset count differs from the current web bundle')
    table = memory(address, size)
    for index, (url, (payload, compressed)) in enumerate(sorted(expected.items())):
        path_at, data_at, length, gzip_flag = struct.unpack_from('<IIIB', table, index * 16)
        if memory(path_at, len(url.encode()) + 1) != url.encode() + b'\0' or gzip_flag != int(compressed):
            raise SystemExit('Compiled web URL or compression flag differs from the current web bundle')
        if length != len(payload) or (length and memory(data_at, length) != payload):
            raise SystemExit('Embedded application web payload is stale or differs from the current web bundle')
    return {'verified': True, 'asset_count': len(expected),
            'payload_bytes': sum(len(payload) for payload, _ in expected.values()), 'elf_sha256': digest(elf_path)}


def selected_sdkconfig(root, build, description):
    remote = Path(description['config_file'])
    try:
        relative = remote.relative_to(description['build_dir'])
        actual = build / relative
    except ValueError:
        try:
            actual = root / remote.relative_to(description['project_path'])
        except ValueError:
            raise SystemExit('Build SDKCONFIG path is outside the recorded project')
    actual = actual.resolve()
    if build != root / 'build' and actual != build / 'sdkconfig':
        raise SystemExit('An isolated build must use its own SDKCONFIG file')
    if not actual.is_file():
        raise SystemExit('The selected build SDKCONFIG file is missing')
    return actual


def verify_www(root, build, website, config, budget, idf_path):
    with tempfile.TemporaryDirectory(prefix='.package-www-', dir=build) as temporary:
        verify = Path(temporary) / 'www.bin'
        options = ['--page-size=' + str(config['SPIFFS_PAGE_SIZE']),
                   '--obj-name-len=' + str(config['SPIFFS_OBJ_NAME_LEN']),
                   '--meta-len=' + str(config['SPIFFS_META_LENGTH'])]
        if config['SPIFFS_USE_MAGIC']:
            options.append('--use-magic')
        if config['SPIFFS_USE_MAGIC_LENGTH']:
            options.append('--use-magic-len')
        if idf_path:
            generator = idf_path / 'components/spiffs/spiffsgen.py'
            if not generator.is_file():
                raise SystemExit('The selected local ESP-IDF source has no SPIFFS generator')
            command = [sys.executable, str(generator), str(budget['www_partition_bytes']),
                       str(root / 'main/http_server/axe-os/dist/axe-os'), str(verify), *options]
            provenance = {'generator_sha256': digest(generator), 'source': str(idf_path)}
        else:
            command = ['docker', 'run', '--rm', '--mount', f'type=bind,source={root},target=/project,readonly',
                       '--mount', f'type=bind,source={temporary},target=/verify', '-w', '/project', IDF_IMAGE,
                       'python', '/opt/esp/idf/components/spiffs/spiffsgen.py', str(budget['www_partition_bytes']),
                       '/project/main/http_server/axe-os/dist/axe-os', '/verify/www.bin', *options]
            provenance = {'container_image': IDF_IMAGE}
        subprocess.run(command, check=True, capture_output=True)
        if digest(verify) != digest(website):
            raise SystemExit('WWW image is stale or differs from the current web bundle')
        return provenance


def package(root, build, idf_path=None):
    version = (root / 'version.txt').read_text().strip()
    if not re.fullmatch(r'5tratumFW-\d+\.\d+\.\d+-beta\.\d+', version):
        raise SystemExit('This development package must use a named BETA version')
    description = json.loads((build / 'project_description.json').read_text())
    if description['project_version'] != version:
        raise SystemExit('Application build version does not match version.txt')
    if (root / 'main/http_server/axe-os/dist/axe-os/version.txt').read_text().strip() != version:
        raise SystemExit('Web bundle and application versions do not match')
    web_provenance = json.loads((root / 'main/http_server/axe-os/dist/axe-os/build-info.json').read_text())
    if web_provenance['version'] != version or web_provenance['nodeVersion'] != '24.14.0':
        raise SystemExit('Unexpected web build version or Node provenance')
    config = json.loads((build / 'config/sdkconfig.json').read_text())
    sdkconfig = selected_sdkconfig(root, build, description)
    if config['IDF_TARGET'] != 'esp32s3':
        raise SystemExit('Expected an ESP32-S3 build')
    budget_report = build / 'web-budget.json'
    if build == root / 'build' and not budget_report.is_file():
        budget_report = root / 'artifacts/web-budget.json'  # Existing default builds.
    budget = json.loads(budget_report.read_text())
    if not budget['passed']:
        raise SystemExit('Web payload exceeded its size budget')
    firmware = build / 'esp-miner.bin'
    website = build / 'www.bin'
    if not firmware.is_file() or not website.is_file():
        raise SystemExit('Both application and WWW builds are required')
    if firmware.stat().st_size >= 4 * 1024 ** 2 or website.stat().st_size != budget['www_partition_bytes']:
        raise SystemExit('Unexpected application or WWW image size')
    # ESP-IDF places esp_app_desc_t at the start of the first segment: the
    # 24-byte image header + 8-byte segment header. Do not match arbitrary text.
    image = firmware.read_bytes()
    if len(image) < 176 or image[0] != 0xE9 or struct.unpack_from('<I', image, 32)[0] != 0xABCD5432:
        raise SystemExit('Expected an ESP-IDF application descriptor')
    app_version = image[48:80].split(b'\0', 1)[0].decode()
    idf_version = image[144:176].split(b'\0', 1)[0].decode()
    if app_version != version or idf_version != 'v6.0.2':
        raise SystemExit('Unexpected application version or IDF provenance')
    if description.get('git_revision') != idf_version:
        raise SystemExit('Build description and application disagree about the SDK version')
    embedded_web = verify_embedded_web(image, build / 'esp-miner.elf', root / 'main/http_server/axe-os/dist/axe-os')
    if embedded_web['payload_bytes'] != budget['payload_bytes'] or embedded_web['asset_count'] != budget['file_count']:
        raise SystemExit('Web budget report does not describe the current embedded payload')
    # Recreate SPIFFS with the exact build configuration and compare bytes. This
    # verifies the actual WWW image contains the current UI/version/provenance,
    # including detecting a stale image with the same development version.
    www_verification = verify_www(root, build, website, config, budget, idf_path)
    source_commit = None
    if (root / '.git').exists():
        subprocess.run(['git', 'diff', '--quiet', 'HEAD'], cwd=root, check=True)
        source_commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root).decode().strip()
        # Arbitrary untracked files, including private observations, never enter
        # the corresponding source archive. Commit reviewed source first.
        tracked = subprocess.check_output(
            ['git', 'ls-files', '--cached', '-z'], cwd=root
        ).decode().split('\0')
    else:
        inventory = root / 'SOURCE_FILES.json'
        if not inventory.is_file():
            raise SystemExit('Package from a clean Git checkout or corresponding source archive')
        tracked = list(json.loads(inventory.read_text())['files'])
    build_receipt = build / 'build-source-commit.txt'
    build_commit = build_receipt.read_text().strip() if build_receipt.is_file() else None
    if source_commit and build_commit != source_commit:
        raise SystemExit('Firmware build source receipt differs from the packaged source commit')
    toolchain_receipt = build / 'build-toolchain.json'
    toolchain = json.loads(toolchain_receipt.read_text()) if toolchain_receipt.is_file() else None
    if toolchain is not None and toolchain.get('idf_version') != idf_version:
        raise SystemExit('SDK execution receipt and application disagree about the SDK version')
    provenance = {
        'firmware_build_commit': build_commit,
        'idf_version': idf_version, 'idf_source_revision': description['git_revision'],
        'compiler': description.get('c_compiler'), 'toolchain_receipt': toolchain,
        'sdkconfig_sha256': digest(sdkconfig),
        'embedded_web': embedded_web, 'www_verification': www_verification,
    }
    source_files = set()
    for name in filter(None, tracked):
        relative = Path(name)
        if relative.is_absolute() or '..' in relative.parts:
            raise SystemExit('Invalid source inventory path')
        path = root / relative
        if path.is_file():
            source_files.add(path)
        elif path.is_dir() and not path.is_symlink():
            # Include the pinned submodule sources, excluding its .git metadata.
            names = subprocess.check_output(['git', 'ls-files', '-z'], cwd=path).decode().split('\0')
            source_files.update(path / name for name in filter(None, names) if (path / name).is_file())
    source_files.update(root / name for name in ('dependencies.lock',) if (root / name).is_file())
    # A previous root SDKCONFIG is unrelated to a fresh isolated build. Archive
    # the selected build's actual configuration under the reproducible root name.
    source_entries = {str(path.relative_to(root)): path for path in source_files}
    source_entries['sdkconfig'] = sdkconfig
    source_entries.pop('BUILD_PROVENANCE.json', None)
    provenance_bytes = (json.dumps(provenance, indent=2) + '\n').encode()
    inventory = {name: digest(path) for name, path in sorted(source_entries.items())}
    inventory['BUILD_PROVENANCE.json'] = hashlib.sha256(provenance_bytes).hexdigest()
    output = root / 'artifacts' / version
    output.mkdir(parents=True, exist_ok=True)
    for source in (firmware, website):
        shutil.copy2(source, output / source.name)
    shutil.copy2(budget_report, output / 'web-budget.json')
    (output / 'build-provenance.json').write_bytes(provenance_bytes)
    source_zip = output / (version + '-source.zip')
    with zipfile.ZipFile(source_zip, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for name, path in sorted(source_entries.items()):
            archive.write(path, arcname=name)
        archive.writestr('BUILD_PROVENANCE.json', provenance_bytes)
        archive.writestr('SOURCE_FILES.json', json.dumps({'files': inventory}, indent=2) + '\n')
    manifest = {
        'product': '5tratumFW', 'version': version,
        'release_channel': 'BETA', 'github_prerelease': True,
        'repository': 'https://github.com/WillItMod/5tratumFW',
        'source_archive_commit': source_commit,
        'firmware_build_commit': build_commit,
        'built_at_utc': datetime.now(timezone.utc).isoformat(),
        'upstream': {'repository': 'https://github.com/bitaxeorg/ESP-Miner', 'tag': 'v2.15.4',
                     'commit': 'd212a0f948cb3ffd90c7ca9572e9b2cf2d865d80'},
        'target': 'esp32s3', 'supported_board_versions': ['601', '602'],
        'idf_version': idf_version, 'node_version': web_provenance['nodeVersion'],
        'build_provenance': provenance,
        'image_type': 'Paired application and WWW OTA images; not a factory image',
        'hardware_tested': False,
        'implemented': ['5tratumFW device interface', '601/602 boot identity guard', 'NVS-preserving startup',
                        'Gamma OLED graphics with sample freshness',
                        'Strict informational MUX peer status with 90-second TTL',
                        'Bound read-only /api/5tratum/status advertisement reporting for OS inventory',
                        'Coinbase decoding off by default; explicit saved settings preserved',
                        'Miner-side 5tratMux connection setup and explicit restart', 'Operating-settings JSON export',
                        'ASIC power-saving pause/resume with requested and applied state',
                        'Persistent weekly pause windows with independent network time and temporary manual override',
                        'Explicit fine frequency/voltage requests with on-device named tuning slots',
                        'Ten on-device named pool slots with credential redaction and explicit Apply',
                        'Persistent weekly pool switch timepoints and coordinated primary hot reload',
                        'Saved tuning profile schedule with protected operating-point transitions',
                        'Embedded matching 5tratumFW web interface (ESP-Miner 2.15.4 base)'],
        'not_implemented': ['Automatic tuning', 'Dedicated per-chip multi-coin mining', 'Native authenticated MUX control'],
        'files': {p.name: {'bytes': p.stat().st_size, 'sha256': digest(p)} for p in
                  (output / 'esp-miner.bin', output / 'www.bin', source_zip,
                   output / 'web-budget.json', output / 'build-provenance.json')},
    }
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (output / 'SHA256SUMS').write_text(''.join(f"{row['sha256']}  {name}\n" for name, row in manifest['files'].items()))
    print(json.dumps({'package': str(output), 'version': version, 'files': manifest['files']}, indent=2))


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=root / 'build',
                        help='Selected application/WWW build directory (default: build)')
    parser.add_argument('--idf-path', type=Path, default=os.environ.get('IDF_PATH'),
                        help='Local ESP-IDF source for SPIFFS verification; otherwise use the pinned SDK container')
    args = parser.parse_args()
    package(root.resolve(), args.build_dir.resolve(), args.idf_path.resolve() if args.idf_path else None)


if __name__ == '__main__':
    main()
