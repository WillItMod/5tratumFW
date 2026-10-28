#!/usr/bin/env python3
"""RC1 security changes must reconnect through actual StratumConfig reload/copy."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
class Sv2ReloadTests(unittest.TestCase):
    def test_auth_authority_and_channel_changes_reload_only_their_pool(self):
        with tempfile.TemporaryDirectory(prefix='sv2-security-reload-') as directory:
            temp=Path(directory)
            for name in ('stratum_config.h','stratum_config.cpp'):
                shutil.copy(ROOT/'main/stratum'/name,temp/name)
            (temp/'esp_log.h').write_text('#pragma once\n')
            (temp/'psram_allocator.h').write_text('#pragma once\n#include <cstdlib>\n#include <cstring>\n#define safe_free(p) do { free(p); p=nullptr; } while(0)\n')
            (temp/'nvs_config.h').write_text('''#pragma once
#include <cstdint>
#include <cstring>
namespace Config {
inline bool required[2]={false,false}; inline uint16_t channel[2]={0,0};
inline const char *authority[2]={"",""};
inline char *getStratumURL(){return strdup("primary.test");} inline char *getStratumFallbackURL(){return strdup("fallback.test");}
inline char *getStratumUser(){return strdup("user");} inline char *getStratumFallbackUser(){return strdup("fallback");}
inline char *getStratumPass(){return strdup("secret");} inline char *getStratumFallbackPass(){return strdup("other");}
inline int getStratumPortNumber(){return 3333;} inline int getStratumFallbackPortNumber(){return 4444;}
inline bool isStratumEnonceSubscribe(){return false;} inline bool isStratumFallbackEnonceSubscribe(){return false;}
inline bool isStratumTLS(){return true;} inline bool isStratumFallbackTLS(){return true;}
inline int getStratumProtocol(){return 1;} inline int getFallbackStratumProtocol(){return 1;}
inline char *getSV2AuthorityPubkey(){return strdup(authority[0]);} inline char *getFallbackSV2AuthorityPubkey(){return strdup(authority[1]);}
inline uint16_t getSV2ChannelType(){return channel[0];} inline uint16_t getFallbackSV2ChannelType(){return channel[1];}
inline bool isSV2RequireAuth(){return required[0];} inline bool isFallbackSV2RequireAuth(){return required[1];}
}
''')
            (temp/'harness.cpp').write_text('''#include "stratum_config.h"
#include <cassert>
int main(){
 StratumConfig primary(0),fallback(1),copy(1);assert(!primary.reload()&&!fallback.reload());
 Config::required[0]=true;assert(primary.reload()&&!primary.reload()&&!fallback.reload());
 Config::authority[0]="primary-authority";assert(primary.reload()&&!fallback.reload());
 Config::channel[0]=1;assert(primary.reload()&&!fallback.reload());
 primary.copyInto(&copy);assert(copy.isPrimary()&&!copy.reload());
 Config::required[1]=true;Config::authority[1]="fallback-authority";assert(fallback.reload()&&!primary.reload());
 Config::required[0]=false;assert(primary.reload()&&copy.reload()&&!fallback.reload());
}
''')
            binary=temp/'test'
            subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-unused-variable','-Wno-pragma-once-outside-header','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(temp),'-I',str(ROOT/'components/arduinojson'),str(temp/'stratum_config.cpp'),str(temp/'harness.cpp'),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
if __name__=='__main__':unittest.main(verbosity=2)
