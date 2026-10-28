#!/usr/bin/env python3
"""Exercise package admission with synthetic ELF/load segments, never hardware."""
from contextlib import redirect_stdout
import gzip
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('gamma_package', ROOT / 'tools/package_5tratumfw.py')
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)
VERSION = '5tratumFW-0.1.0-beta.99'


def compiled_fixture(directory, version=VERSION):
    """Build the real ESP32-S3 table ABI in one synthetic app load segment."""
    base, table_offset = 0x3C000020, 256
    assets = []
    for path in directory.rglob('*'):
        if path.is_file():
            name = path.relative_to(directory).as_posix()
            compressed = name.endswith('.gz')
            assets.append(('/' + (name[:-3] if compressed else name), path.read_bytes(), compressed))
    assets.sort()
    segment = bytearray(table_offset + len(assets) * 16)
    struct.pack_into('<I', segment, 0, 0xABCD5432)
    segment[16:16 + len(version)] = version.encode()
    segment[112:120] = b'v6.0.2\0\0'
    data_offsets = {}
    for index, (url, payload, compressed) in enumerate(assets):
        path_offset = len(segment)
        segment.extend(url.encode() + b'\0')
        data_offset = len(segment)
        data_offsets[url] = data_offset
        segment.extend(payload)
        # Nonzero struct padding is legal; its value has no ABI semantics.
        struct.pack_into('<IIIB3s', segment, table_offset + index * 16,
                         base + path_offset, base + data_offset, len(payload), int(compressed), b'PAD')
    strings = b'\0embedded_files\0'
    strings_offset = 52 + 4 * 40
    symbols_offset = strings_offset + len(strings)
    data_offset = symbols_offset + 32
    elf = bytearray(data_offset)
    elf[:16] = b'\x7fELF\x01\x01\x01' + b'\0' * 9
    struct.pack_into('<HHIIIIIHHHHHH', elf, 16, 2, 94, 1, 0, 0, 52, 0, 52, 0, 0, 40, 4, 0)
    struct.pack_into('<10I', elf, 52 + 40, 0, 3, 0, 0, strings_offset, len(strings), 0, 0, 1, 0)
    struct.pack_into('<10I', elf, 52 + 80, 0, 2, 0, 0, symbols_offset, 32, 1, 1, 4, 16)
    struct.pack_into('<10I', elf, 52 + 120, 0, 1, 2, base, data_offset, len(segment), 0, 0, 4, 0)
    elf[strings_offset:strings_offset + len(strings)] = strings
    struct.pack_into('<IIIBBH', elf, symbols_offset + 16, 1, base + table_offset, len(assets) * 16, 1, 0, 3)
    elf.extend(segment)
    segment[144:176] = hashlib.sha256(elf).digest()
    image = bytearray(24)
    image[0], image[1] = 0xE9, 1
    image.extend(struct.pack('<II', base, len(segment)))
    image.extend(segment)
    return image, bytes(elf), data_offsets


class GammaPackageTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='5tfw-package-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.web = self.root / 'main/http_server/axe-os/dist/axe-os'
        self.web.mkdir(parents=True)
        (self.web / 'index.html.gz').write_bytes(gzip.compress(b'current fixture UI', mtime=0))
        (self.web / 'version.txt').write_text(VERSION + '\n')
        (self.web / 'build-info.json').write_text(json.dumps({'version': VERSION, 'nodeVersion': '24.14.0'}))
        self.build = self.root / 'build/fresh'
        (self.build / 'config').mkdir(parents=True)
        self.image, self.elf, self.offsets = compiled_fixture(self.web)
        (self.build / 'esp-miner.elf').write_bytes(self.elf)

    def verify(self, image=None):
        return PACKAGE.verify_embedded_web(image if image is not None else self.image,
                                           self.build / 'esp-miner.elf', self.web)

    def test_actual_app_table_matches_every_asset_while_ignoring_struct_padding(self):
        report = self.verify()
        self.assertTrue(report['verified'])
        self.assertEqual(report['asset_count'], 3)
        self.assertEqual(report['payload_bytes'], sum(path.stat().st_size for path in self.web.iterdir()))

    def test_same_version_stale_payload_and_wrong_elf_are_rejected(self):
        (self.web / 'index.html.gz').write_bytes(gzip.compress(b'outdated fixture UI', mtime=0))
        with self.assertRaisesRegex(SystemExit, 'payload is stale'):
            self.verify()
        (self.build / 'esp-miner.elf').write_bytes(self.elf + b'other build')
        with self.assertRaisesRegex(SystemExit, 'selected build ELF'):
            self.verify()

    def test_extra_missing_duplicate_and_symlink_assets_are_rejected(self):
        extra = self.web / 'extra'
        extra.write_text('additional current asset')
        with self.assertRaisesRegex(SystemExit, 'asset count'):
            self.verify()
        extra.unlink()
        raw = self.web / 'index.html'
        raw.write_text('duplicate logical URL')
        with self.assertRaisesRegex(SystemExit, 'duplicate logical URLs'):
            self.verify()
        raw.unlink()
        (self.web / 'version.txt').unlink()
        with self.assertRaisesRegex(SystemExit, 'asset count'):
            self.verify()
        (self.web / 'link').symlink_to(self.build / 'esp-miner.elf')
        with self.assertRaisesRegex(SystemExit, 'symlinks'):
            self.verify()

    def test_bad_load_pointers_flags_and_truncated_segments_are_rejected(self):
        for value, offset, message in ((0, 32 + 256, 'pointer'), (2, 32 + 256 + 12, 'compression flag')):
            image = bytearray(self.image)
            if offset == 32 + 256:
                struct.pack_into('<I', image, offset, value)
            else:
                image[offset] = value
            with self.subTest(message=message), self.assertRaisesRegex(SystemExit, message):
                self.verify(image)
        with self.assertRaisesRegex(SystemExit, 'Truncated'):
            self.verify(self.image[:-1])

    def prepare_package(self):
        (self.root / 'version.txt').write_text(VERSION + '\n')
        (self.root / 'sdkconfig').write_text('ROOT_OLD_SDK_CONFIG\n')
        (self.root / '.gitignore').write_text('build/\nartifacts/\nmain/\n')
        subprocess.run(['git', 'init', '-q'], cwd=self.root, check=True)
        subprocess.run(['git', 'add', 'version.txt', 'sdkconfig', '.gitignore'], cwd=self.root, check=True)
        subprocess.run(['git', '-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid',
                        'commit', '-qm', 'package fixture'], cwd=self.root, check=True)
        commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=self.root, text=True).strip()
        (self.build / 'build-source-commit.txt').write_text(commit + '\n')
        (self.root / 'build/build-source-commit.txt').write_text('wrong old build\n')
        (self.build / 'build-toolchain.json').write_text(json.dumps({
            'idf_version': 'v6.0.2', 'container_image': 'fixture older container with mounted SDK6',
            'idf_source': {'repository': 'https://github.com/espressif/esp-idf', 'commit': 'qualified-sdk6-source'},
        }))
        self.description = {'project_version': VERSION, 'git_revision': 'v6.0.2',
                            'build_dir': '/project/build/fresh', 'project_path': '/project',
                            'config_file': '/project/build/fresh/sdkconfig', 'c_compiler': 'actual-sdk6-compiler'}
        (self.build / 'project_description.json').write_text(json.dumps(self.description))
        self.config = {'IDF_TARGET': 'esp32s3', 'SPIFFS_PAGE_SIZE': 256, 'SPIFFS_OBJ_NAME_LEN': 32,
                       'SPIFFS_META_LENGTH': 4, 'SPIFFS_USE_MAGIC': True, 'SPIFFS_USE_MAGIC_LENGTH': True}
        (self.build / 'config/sdkconfig.json').write_text(json.dumps(self.config))
        (self.build / 'sdkconfig').write_text('SELECTED_SDK6_CONFIG\n')
        (self.build / 'esp-miner.bin').write_bytes(self.image)
        (self.build / 'www.bin').write_bytes(b'W' * 4096)
        self.budget = {'passed': True, 'www_partition_bytes': 4096, 'file_count': 3,
                       'payload_bytes': sum(path.stat().st_size for path in self.web.iterdir())}
        (self.build / 'web-budget.json').write_text(json.dumps(self.budget))
        return commit

    def test_package_uses_selected_receipt_and_archives_actual_sdkconfig(self):
        commit = self.prepare_package()
        with patch.object(PACKAGE, 'verify_www', return_value={'fixture_only': True}) as verify, redirect_stdout(io.StringIO()):
            PACKAGE.package(self.root, self.build)
        self.assertEqual(verify.call_args.args[1:3], (self.build, self.build / 'www.bin'))
        output = self.root / 'artifacts' / VERSION
        manifest = json.loads((output / 'manifest.json').read_text())
        self.assertEqual(manifest['firmware_build_commit'], commit)
        self.assertEqual(manifest['source_archive_commit'], commit)
        provenance = manifest['build_provenance']
        self.assertEqual(provenance['toolchain_receipt']['container_image'], 'fixture older container with mounted SDK6')
        self.assertEqual(provenance['idf_version'], 'v6.0.2')
        self.assertEqual(provenance['sdkconfig_sha256'], PACKAGE.digest(self.build / 'sdkconfig'))
        self.assertEqual((self.root / 'sdkconfig').read_text(), 'ROOT_OLD_SDK_CONFIG\n')
        with zipfile.ZipFile(output / (VERSION + '-source.zip')) as archive:
            self.assertEqual(archive.read('sdkconfig'), b'SELECTED_SDK6_CONFIG\n')
            inventory = json.loads(archive.read('SOURCE_FILES.json'))['files']
            self.assertEqual(inventory['sdkconfig'], provenance['sdkconfig_sha256'])
            self.assertEqual(json.loads(archive.read('BUILD_PROVENANCE.json')), provenance)
        self.assertIn('build-provenance.json', manifest['files'])

    def test_wrong_selected_commit_is_not_rescued_by_another_build_receipt(self):
        commit = self.prepare_package()
        (self.root / 'build/build-source-commit.txt').write_text(commit)
        (self.build / 'build-source-commit.txt').write_text('wrong selected commit')
        with patch.object(PACKAGE, 'verify_www', return_value={'fixture_only': True}), self.assertRaisesRegex(SystemExit, 'source receipt'):
            PACKAGE.package(self.root, self.build)
        self.assertFalse((self.root / 'artifacts' / VERSION).exists())

    def test_isolated_sdkconfig_cannot_resolve_to_old_root_config(self):
        self.prepare_package()
        self.description['config_file'] = '/project/sdkconfig'
        with self.assertRaisesRegex(SystemExit, 'its own SDKCONFIG'):
            PACKAGE.selected_sdkconfig(self.root, self.build, self.description)
        self.description['build_dir'] = '/project/build'
        self.assertEqual(PACKAGE.selected_sdkconfig(self.root, self.root / 'build', self.description), self.root / 'sdkconfig')

    def test_native_www_verification_uses_selected_build_and_cleans_temporary_output(self):
        self.prepare_package()
        source = self.root / 'fixture-sdk/components/spiffs'
        source.mkdir(parents=True)
        generator = source / 'spiffsgen.py'
        generator.write_text('import pathlib,sys\npathlib.Path(sys.argv[3]).write_bytes(b"W" * int(sys.argv[1]))\n')
        report = PACKAGE.verify_www(self.root, self.build, self.build / 'www.bin', self.config,
                                    self.budget, self.root / 'fixture-sdk')
        self.assertEqual(report['generator_sha256'], PACKAGE.digest(generator))
        self.assertFalse(list(self.build.glob('.package-www-*')))
        (self.build / 'www.bin').write_bytes(b'Z' * 4096)
        with self.assertRaisesRegex(SystemExit, 'WWW image is stale'):
            PACKAGE.verify_www(self.root, self.build, self.build / 'www.bin', self.config,
                               self.budget, self.root / 'fixture-sdk')

    def test_helper_preserves_old_build_and_root_config_and_rejects_reuse(self):
        tools = self.root / 'tools'
        tools.mkdir()
        helper = tools / 'build_5tratumfw.sh'
        shutil.copy2(ROOT / 'tools/build_5tratumfw.sh', helper)
        (self.root / 'sdkconfig').write_text('DO_NOT_REWRITE\n')
        (self.root / 'build/old-marker').write_text('old SDK build\n')
        binaries = self.root / 'fake-bin'
        binaries.mkdir()
        node = binaries / 'node'
        node.write_text('#!/bin/sh\nprintf x >> "$NODE_MARKER"\nexit 42\n')
        node.chmod(0o755)
        marker = self.root / 'node-marker'
        env = dict(os.environ, PATH=str(binaries) + os.pathsep + os.environ['PATH'], NODE_MARKER=str(marker))
        env.pop('FW_BUILD_DIR', None)
        first = subprocess.run(['bash', str(helper)], env=env, text=True, capture_output=True)
        self.assertEqual(first.returncode, 42, first.stderr)
        self.assertTrue((self.root / 'build/gamma-public').is_dir())
        second = subprocess.run(['bash', str(helper)], env=env, text=True, capture_output=True)
        self.assertNotEqual(second.returncode, 0)
        self.assertIn('new directory', second.stderr)
        self.assertEqual(marker.read_text(), 'x')
        self.assertEqual((self.root / 'sdkconfig').read_text(), 'DO_NOT_REWRITE\n')
        self.assertEqual((self.root / 'build/old-marker').read_text(), 'old SDK build\n')


if __name__ == '__main__':
    unittest.main(verbosity=2)
