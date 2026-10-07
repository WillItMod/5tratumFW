#!/usr/bin/env python3
"""Run actual profile schema + transaction helpers with ASan/UBSan, no device writes."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
class ProfileTests(unittest.TestCase):
    def test_real_profile_schema_and_atomic_config_patch(self):
        with tempfile.TemporaryDirectory(prefix='5tfw-profiles-') as temp:
            temp=Path(temp)
            header=(ROOT/'main/nvs_config.h').read_text()
            strings={'CONFIG_ESP_WIFI_SSID','CONFIG_ESP_WIFI_PASSWORD','CONFIG_LWIP_LOCAL_HOSTNAME','CONFIG_STRATUM_URL','CONFIG_STRATUM_USER','CONFIG_STRATUM_PW','CONFIG_STRATUM_FALLBACK_URL','CONFIG_STRATUM_FALLBACK_USER','CONFIG_STRATUM_FALLBACK_PW'}
            macros=set(re.findall(r'\bCONFIG_\w+',header)); macros.discard('CONFIG_DONATE_ADDR')
            (temp/'sdkconfig.h').write_text('\n'.join('#define '+m+' '+('""' if m in strings else '0') for m in sorted(macros)))
            executable=temp/'profile-tests'
            subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(temp),'-I',str(ROOT/'components/ArduinoJson'),'-I',str(ROOT/'main/http_server'),str(ROOT/'main/http_server/profile_schema.cpp'),str(ROOT/'test/host/profile_harness.cpp'),'-o',str(executable)],check=True)
            subprocess.run([str(executable)],check=True)
            for stub in (ROOT/'test/host/profile_api_stubs').iterdir(): shutil.copy(stub,temp/stub.name)
            shutil.copy(ROOT/'main/http_server/handler_profiles.cpp',temp/'handler_profiles.cpp')
            api=temp/'profile-api-tests'
            subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(temp),'-I',str(ROOT/'components/ArduinoJson'),'-I',str(ROOT/'main/http_server'),'-I',str(ROOT/'main'),str(temp/'handler_profiles.cpp'),str(ROOT/'main/http_server/profile_store.cpp'),str(ROOT/'main/http_server/profile_schema.cpp'),str(ROOT/'main/http_server/pool_schedule.cpp'),str(ROOT/'test/host/profile_api_harness.cpp'),'-o',str(api)],check=True)
            subprocess.run([str(api)],check=True)
if __name__=='__main__': unittest.main(verbosity=2)
