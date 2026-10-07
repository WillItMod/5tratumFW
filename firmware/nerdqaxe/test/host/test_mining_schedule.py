#!/usr/bin/env python3
"""Compile the production schedule parser, engine and NVS runtime with sanitizers."""
from pathlib import Path
import json
import shutil
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]

class MiningScheduleTests(unittest.TestCase):
    def test_actual_policy_schema_and_persistence_runtime(self):
        with tempfile.TemporaryDirectory(prefix='5tfw-mining-schedule-') as directory:
            temp = Path(directory)
            shutil.copy(ROOT/'test/host/profile_api_stubs/nvs.h', temp/'nvs.h')
            shutil.copy(ROOT/'test/host/profile_api_stubs/esp_http_server.h', temp/'esp_http_server.h')
            (temp/'sntp.h').write_text('#pragma once\nstruct SNTP {bool synchronized=false; bool isTimeSynced(){return synchronized;}};\n')
            # Local include resolution otherwise selects the production SNTP header.
            shutil.copy(ROOT/'main/tasks/mining_schedule.cpp', temp/'mining_schedule.cpp')
            binary = temp/'test'
            subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                '-I', str(temp), '-I', str(ROOT/'components/arduinojson'), '-I', str(ROOT/'main/tasks'),
                str(temp/'mining_schedule.cpp'), str(ROOT/'main/tasks/mining_schedule_schema.cpp'),
                '-x', 'c++', str(ROOT/'main/tasks/mining_schedule_policy.c'),
                str(ROOT/'test/host/mining_schedule_harness.cpp'), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

    def test_actual_http_routes_and_authorization(self):
        with tempfile.TemporaryDirectory(prefix='5tfw-mining-http-') as directory:
            temp = Path(directory)
            for stub in (ROOT/'test/host/profile_api_stubs').iterdir(): shutil.copy(stub, temp/stub.name)
            utils = temp/'http_utils.h'
            utils.write_text(utils.read_text().replace('struct Board {', 'class Board {public:'))
            (temp/'sdkconfig.h').write_text('#pragma once\n')
            (temp/'sntp.h').write_text('#pragma once\nstruct SNTP {bool isTimeSynced(){return false;}};\n')
            (temp/'nvs_config.h').write_text('#pragma once\nnamespace Config {inline int getAsicFrequency(int d){return d;} inline int getAsicVoltage(int d){return d;}}\n')
            shutil.copy(ROOT/'main/tasks/mining_schedule.cpp', temp/'mining_schedule.cpp')
            shutil.copy(ROOT/'main/http_server/handler_mining.cpp', temp/'handler_mining.cpp')
            binary = temp/'test'
            subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                '-I', str(temp), '-I', str(ROOT/'components/arduinojson'), '-I', str(ROOT/'main'),
                '-I', str(ROOT/'main/tasks'), '-I', str(ROOT/'main/http_server'),
                str(temp/'mining_schedule.cpp'), str(temp/'handler_mining.cpp'),
                str(ROOT/'main/tasks/mining_schedule_schema.cpp'),
                '-x', 'c++', str(ROOT/'main/tasks/mining_schedule_policy.c'),
                str(ROOT/'test/host/mining_http_harness.cpp'), '-o', str(binary)], check=True)
            result = subprocess.run([str(binary)], check=True, text=True, capture_output=True)
            lines = [line[len('POWER_SNAPSHOT '):] for line in result.stdout.splitlines() if line.startswith('POWER_SNAPSHOT ')]
            self.assertEqual(len(lines), 1)
            report = json.loads(lines[0])
            self.assertEqual(set(report), {'schemaVersion', 'identity', 'hardware', 'firmware', 'supported', 'schedule', 'status', 'limits'})
            self.assertEqual(report['schemaVersion'], 1)
            self.assertIs(type(report['schemaVersion']), int)
            self.assertEqual(report['schedule'], {'enabled': True, 'timezone': 'Europe/London',
                                                 'windows': [{'days': 127, 'start': '22:00', 'end': '06:00'}]})
            self.assertEqual(report['limits'], {'maxWindows': 8, 'timezones': ['UTC', 'Europe/London', 'Europe/Berlin',
                'America/New_York', 'America/Chicago', 'America/Denver', 'America/Los_Angeles', 'Asia/Tokyo', 'Asia/Shanghai', 'Australia/Sydney']})
            self.assertEqual(set(report['status']), {'clockValid', 'localTime', 'scheduledPause', 'manualOverride',
                                                    'requestedPaused', 'appliedPaused', 'transitionPending', 'error'})
            self.assertEqual(set(report['identity']), {'deviceId'})
            self.assertEqual(set(report['hardware']), {'boardModel', 'asicModel', 'asicCount'})
            self.assertEqual(set(report['firmware']), {'product', 'version'})
            self.assertIsNone(report['status']['localTime'])
            self.assertTrue(report['status']['requestedPaused'])
            self.assertTrue(report['status']['appliedPaused'])

if __name__ == '__main__': unittest.main(verbosity=2)
