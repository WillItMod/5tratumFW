#!/usr/bin/env python3
"""Exercise the production display OTA parser and pre-mutation handler guards.

No device, network or firmware image is written. Version admission is a
compatibility contract; ESP-IDF still validates the complete uploaded image.
"""
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HTTP = ROOT / 'main/http_server'


def image_prefix(version='5tratumFW-qa-0.1.0-beta.7', project='esp-miner'):
    result = bytearray(288)
    result[0:2] = b'\xe9\x05'
    struct.pack_into('<H', result, 12, 9)  # ESP32-S3, not another ESP target.
    result[23] = 1
    struct.pack_into('<I', result, 28, 256)
    struct.pack_into('<I', result, 32, 0xabcd5432)
    for offset, value in ((48, version), (80, project)):
        encoded = value.encode('utf8')
        if len(encoded) > 32:
            raise ValueError('Test descriptor field is oversized')
        result[offset:offset + len(encoded)] = encoded
    return result


def function_body(source, signature):
    start = source.index('{', source.index(signature))
    depth = 1
    end = start + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class DisplayOtaCompatibilityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix='display-ota-')
        cls.directory = Path(cls.temporary.name)
        cls.binary = cls.directory / 'parser'
        compiler = shutil.which('c++')
        if not compiler:
            raise RuntimeError('Display OTA compatibility checks require a C++ compiler')
        subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        '-I', str(HTTP), str(ROOT / 'test/host/display_ota_compatibility_harness.cpp'),
                        '-o', str(cls.binary)], check=True, capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def parse(self, image=None, saved='480x320-rgb', active='320x170'):
        path = self.directory / 'prefix.bin'
        path.write_bytes(image_prefix() if image is None else image)
        result = subprocess.run([str(self.binary), str(path), saved, active],
                                check=True, capture_output=True, text=True)
        required, admitted, null_profiles, null_data = map(int, result.stdout.split())
        self.assertEqual((null_profiles, null_data), (0, 0))
        return bool(required), bool(admitted)

    def test_saved_or_active_wide_profile_requires_support(self):
        for saved, active, expected in [
            ('320x170', '320x170', False), ('480x320-rgb', '320x170', True),
            ('480x320-bgr', '320x170', True), ('320x170', '480x320-rgb', True),
            ('320x170', '480x320-bgr', True), ('480x320-rgb', '480x320-bgr', True),
            ('', '', False), ('480x320', '320x170', False),
        ]:
            with self.subTest(saved=saved, active=active):
                self.assertEqual(self.parse(saved=saved, active=active), (expected, True))

    def test_first_aware_version_same_version_and_future_qa_versions(self):
        for version in ['5tratumFW-qa-0.1.0-beta.7', '5tratumFW-qa-0.1.0-beta.8',
                        '5tratumFW-qa-0.1.0-rc.1', '5tratumFW-qa-0.1.0',
                        '5tratumFW-qa-0.1.1-alpha.0', '5tratumFW-qa-0.2.0',
                        '5tratumFW-qa-1.0.0-beta.0', '5tratumFW-qa-2.0.0']:
            with self.subTest(version=version):
                self.assertEqual(self.parse(image_prefix(version)), (True, True))

    def test_older_and_other_family_versions_are_refused(self):
        for version in ['5tratumFW-qa-0.1.0-beta.6', '5tratumFW-qa-0.1.0-beta.0',
                        '5tratumFW-qa-0.1.0-alpha.9', '5tratumFW-qa-0.0.9',
                        '5tratumFW-oct-0.1.0-beta.7', '5tratumFW-0.1.0-beta.9',
                        'v1.1.0.1', '5tratumFW-qa-mux-a10', '']:
            with self.subTest(version=version):
                self.assertEqual(self.parse(image_prefix(version)), (True, False))

    def test_malformed_semantic_versions_are_refused(self):
        for suffix in ['0.1.0-beta.07', '0.1.0-beta.-1', '0.1.0-beta.', '0.1.0-beta.7x',
                       '00.1.0-beta.7', '0.01.0-beta.7', '0.1.00-beta.7', '0.1.0-dev.7',
                       '0.1.0-beta.7\n', '0.1.0-beta.7+test', '0.1.0-rc.01', '0.1.0.7']:
            with self.subTest(suffix=suffix):
                self.assertEqual(self.parse(image_prefix('5tratumFW-qa-' + suffix)), (True, False))

    def test_truncated_header_or_descriptor_is_refused(self):
        image = image_prefix()
        for length in [0, 1, 23, 24, 31, 32, 47, 48, 79, 80, 111, 175, 287]:
            with self.subTest(length=length):
                self.assertEqual(self.parse(image[:length]), (True, False))
        self.assertEqual(self.parse(image + bytes(4096)), (True, True))

    def test_invalid_target_image_descriptor_and_project_are_refused(self):
        for offset, value in [(0, 0), (1, 0), (1, 17), (12, 0), (12, 5), (13, 1),
                              (23, 2), (28, 0), (29, 0), (32, 0), (33, 0), (80, ord('x'))]:
            image = image_prefix()
            image[offset] = value
            if offset == 28:
                image[29:32] = bytes(3)  # First segment cannot contain its descriptor.
            with self.subTest(offset=offset, value=value):
                self.assertEqual(self.parse(image), (True, False))
        self.assertEqual(self.parse(image_prefix(project='esp-miner-other')), (True, False))

    def test_nonterminated_version_and_project_are_refused(self):
        for offset in [48, 80]:
            image = image_prefix()
            image[offset:offset + 32] = b'x' * 32
            with self.subTest(offset=offset):
                self.assertEqual(self.parse(image), (True, False))

    def test_raw_ota_validates_before_power_or_flash_and_writes_buffered_prefix(self):
        source = (HTTP / 'handler_ota.cpp').read_text()
        body = function_body(source, 'esp_err_t POST_OTA_update(')
        for mutation in ['POWER_MANAGEMENT_MODULE.shutdown()', 'esp_ota_begin(', 'esp_ota_write(']:
            self.assertLess(body.index('compatibleImagePrefix('), body.index(mutation))
        self.assertLess(body.index('requires_display_profile_support()'), body.index('httpd_req_recv('))
        self.assertIn('remaining -= static_cast<int>(prefix_size)', body)
        self.assertIn('esp_ota_write(ota_handle, prefix, prefix_size)', body)
        self.assertLess(body.index('esp_ota_write(ota_handle, prefix, prefix_size)'), body.index('while (remaining > 0)'))
        self.assertIn('displayProfileRuntime.active().id', source)
        self.assertIn('Config::getDisplayProfile()', source)

    def test_prefix_deadline_boundaries_and_clock_anomalies(self):
        result = subprocess.run([str(self.binary), '--deadline'],
                                check=True, capture_output=True, text=True)
        self.assertEqual(list(map(int, result.stdout.split())), [0, 0, 1, 1, 1, 1, 1])

    def test_stalled_prefix_reads_recheck_a_fixed_deadline_before_and_after_receive(self):
        body = function_body((HTTP / 'handler_ota.cpp').read_text(), 'esp_err_t POST_OTA_update(')
        loop = function_body(body, 'while (prefix_size < sizeof(prefix))')
        self.assertEqual(body.count('const int64_t prefix_started_us = esp_timer_get_time();'), 1)
        self.assertLess(body.index('const int64_t prefix_started_us'),
                        body.index('while (prefix_size < sizeof(prefix))'))
        self.assertEqual(loop.count('prefixReadDeadlineExpired('), 2)
        self.assertLess(loop.index('prefixReadDeadlineExpired('), loop.index('httpd_req_recv('))
        self.assertLess(loop.index('httpd_req_recv('), loop.rindex('prefixReadDeadlineExpired('))
        self.assertLess(loop.rindex('prefixReadDeadlineExpired('), loop.index('HTTPD_SOCK_ERR_TIMEOUT'))
        self.assertIn('HTTPD_408_REQ_TIMEOUT', loop)

    def test_factory_request_and_worker_refuse_before_download_or_mutation(self):
        source = (HTTP / 'handler_ota_factory.cpp').read_text()
        request = function_body(source, 'esp_err_t POST_OTA_update_from_url(')
        worker = function_body(source, 'esp_err_t FactoryOTAUpdate::ota_update_from_factory(')
        self.assertLess(request.index('requires_display_profile_support()'),
                        request.index('FACTORY_OTA_UPDATER.trigger('))
        for action in ['MALLOC(', 'follow_redirect(', 'POWER_MANAGEMENT_MODULE.shutdown()',
                       'do_firmware_update(', 'erase_nvs_partition()']:
            self.assertLess(worker.index('requires_display_profile_support()'), worker.index(action))
        self.assertEqual(worker.count('requires_display_profile_support()'), 2)
        self.assertIn('displayProfileRuntime.active().id', source)
        self.assertIn('Config::getDisplayProfile()', source)


if __name__ == '__main__':
    unittest.main()
