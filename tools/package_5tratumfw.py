#!/usr/bin/env python3
"""Package paired OTA images and their source without producing a factory image."""
from datetime import datetime, timezone
import hashlib
import json
import re
from pathlib import Path
import shutil
import struct
import subprocess
import zipfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    root = Path(__file__).resolve().parents[1]
    version = (root / 'version.txt').read_text().strip()
    if not re.fullmatch(r'5tratumFW-\d+\.\d+\.\d+-beta\.\d+', version):
        raise SystemExit('This development package must use a named BETA version')
    description = json.loads((root / 'build/project_description.json').read_text())
    if description['project_version'] != version:
        raise SystemExit('Application build version does not match version.txt')
    if (root / 'main/http_server/axe-os/dist/axe-os/version.txt').read_text().strip() != version:
        raise SystemExit('Web bundle and application versions do not match')
    web_provenance = json.loads((root / 'main/http_server/axe-os/dist/axe-os/build-info.json').read_text())
    if web_provenance['version'] != version or web_provenance['nodeVersion'] != '24.14.0':
        raise SystemExit('Unexpected web build version or Node provenance')
    config = json.loads((root / 'build/config/sdkconfig.json').read_text())
    if config['IDF_TARGET'] != 'esp32s3':
        raise SystemExit('Expected an ESP32-S3 build')
    budget = json.loads((root / 'artifacts/web-budget.json').read_text())
    if not budget['passed']:
        raise SystemExit('Web payload exceeded its size budget')
    firmware = root / 'build/esp-miner.bin'
    website = root / 'build/www.bin'
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
    if app_version != version or idf_version != 'v5.5.3':
        raise SystemExit('Unexpected application version or IDF provenance')
    # Recreate SPIFFS with the exact build configuration and compare bytes. This
    # verifies the actual WWW image contains the current UI/version/provenance,
    # including detecting a stale image with the same development version.
    verify = root / 'artifacts/www-verify.bin'
    command = ['docker', 'run', '--rm', '--mount', f'type=bind,source={root},target=/project',
               '-w', '/project',
               'espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805',
               'python', '/opt/esp/idf/components/spiffs/spiffsgen.py',
               str(budget['www_partition_bytes']), '/project/main/http_server/axe-os/dist/axe-os',
               '/project/artifacts/www-verify.bin',
               '--page-size=' + str(config['SPIFFS_PAGE_SIZE']),
               '--obj-name-len=' + str(config['SPIFFS_OBJ_NAME_LEN']),
               '--meta-len=' + str(config['SPIFFS_META_LENGTH'])]
    if config['SPIFFS_USE_MAGIC']:
        command.append('--use-magic')
    if config['SPIFFS_USE_MAGIC_LENGTH']:
        command.append('--use-magic-len')
    subprocess.run(command, check=True, capture_output=True)
    if digest(verify) != digest(website):
        raise SystemExit('WWW image is stale or differs from the current web bundle')
    output = root / 'artifacts' / version
    output.mkdir(parents=True, exist_ok=True)
    for source in (firmware, website):
        shutil.copy2(source, output / source.name)
    shutil.copy2(root / 'artifacts/web-budget.json', output / 'web-budget.json')
    source_zip = output / (version + '-source.zip')
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
    source_files.update(root / name for name in ('sdkconfig', 'dependencies.lock') if (root / name).is_file())
    with zipfile.ZipFile(source_zip, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(source_files):
            archive.write(path, arcname=str(path.relative_to(root)))
        archive.writestr('SOURCE_FILES.json', json.dumps({
            'files': {str(p.relative_to(root)): digest(p) for p in sorted(source_files)}
        }, indent=2) + '\n')
    manifest = {
        'product': '5tratumFW', 'version': version,
        'release_channel': 'BETA', 'github_prerelease': True,
        'repository': 'https://github.com/WillItMod/5tratumFW',
        'source_archive_commit': source_commit,
        'firmware_build_commit': (root / 'build/build-source-commit.txt').read_text().strip()
            if (root / 'build/build-source-commit.txt').is_file() else None,
        'built_at_utc': datetime.now(timezone.utc).isoformat(),
        'upstream': {'repository': 'https://github.com/bitaxeorg/ESP-Miner', 'tag': 'v2.14.2',
                     'commit': '64680f8a4da0b9a3b532051f0aa18429fcf04e82'},
        'target': 'esp32s3', 'supported_board_versions': ['601', '602'],
        'idf_version': idf_version, 'node_version': web_provenance['nodeVersion'],
        'image_type': 'Paired application and WWW OTA images; not a factory image',
        'hardware_tested': False,
        'implemented': ['5tratumFW device interface', '601/602 boot identity guard', 'NVS-preserving startup',
                        'Gamma OLED graphics with sample freshness',
                        'Strict informational MUX peer status with 90-second TTL',
                        'Coinbase decoding off by default; explicit saved settings preserved',
                        'Miner-side 5tratMux connection setup and explicit restart', 'Operating-settings JSON export',
                        'ASIC power-saving pause/resume with requested and applied state',
                        'Persistent weekly pause windows with independent network time and temporary manual override'],
        'not_implemented': ['Automatic tuning', 'Dedicated per-chip multi-coin mining', 'Native authenticated MUX control'],
        'files': {p.name: {'bytes': p.stat().st_size, 'sha256': digest(p)} for p in
                  (output / 'esp-miner.bin', output / 'www.bin', source_zip, output / 'web-budget.json')},
    }
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (output / 'SHA256SUMS').write_text(''.join(f"{row['sha256']}  {name}\n" for name, row in manifest['files'].items()))
    print(json.dumps({'package': str(output), 'version': version, 'files': manifest['files']}, indent=2))


if __name__ == '__main__':
    main()
