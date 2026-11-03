#!/usr/bin/env python3
"""Actual buck initialization and recovery-scope guard; fake PMBus only."""
from pathlib import Path
import os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
def method(source,name):
 a=source.index(name);b=source.index('{',a);depth=1;e=b+1
 while depth:depth+=(source[e]=='{')-(source[e]=='}');e+=1
 return source[a:e]
runtime=(ROOT/'main/autotune/runtime.cpp').read_text()
source='''#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>
#include "autotune/recovery_guard.h"
#include "boards/drivers/pmbus_commands.h"
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
namespace FiveTratumAutotune {std::atomic_uint recoveryStartups{0};
'''
for name in ['bool preserveRecoveryFaults()','RecoveryStartupGuard::RecoveryStartupGuard()','RecoveryStartupGuard::~RecoveryStartupGuard()']:source+=method(runtime,name)+'\n'
source+='''}
static unsigned clears=0;static bool latched=false;
class TPS53647 {
public:
 static constexpr uint16_t DEVICE_CODE=0x1f0;uint16_t deviceCode=DEVICE_CODE;
 bool m_initialized=false;uint8_t m_initOnOffConfig=0x17;float m_initOtWarnLimit=95,m_initOtFaultLimit=125;
 uint16_t get_device_code(){return deviceCode;}
 void write_command(uint8_t command){if(command==PMBUS_CLEAR_FAULTS){++clears;latched=false;}}
 void write_byte(uint8_t,uint8_t){}void write_word(uint8_t,uint16_t){}void set_phases(int){}
 uint16_t float_to_slinear11(float value){return static_cast<uint16_t>(value);}
 uint8_t volt_to_vid(float){return 1;}void show_voltage_settings(){}
 bool init(int,int,float);void clear_faults();
};
class TPS53667:public TPS53647 {
public:
 static constexpr uint16_t DEVICE_CODE=0x1f1;float m_initVOutMax=1.4f;
 TPS53667(){deviceCode=DEVICE_CODE;}bool init(int,int,float);
};
'''
source+=method((ROOT/'main/boards/drivers/TPS53647.cpp').read_text(),'bool TPS53647::init(')+'\n'
source+=method((ROOT/'main/boards/drivers/TPS53647.cpp').read_text(),'void TPS53647::clear_faults(')+'\n'
source+=method((ROOT/'main/boards/drivers/TPS53667.cpp').read_text(),'bool TPS53667::init(')+'\n'
source+='''constexpr int ESP_OK=0;
static int smb_write_command(uint8_t command){assert(command==PMBUS_CLEAR_FAULTS);++clears;latched=false;return ESP_OK;}
'''
source+=method((ROOT/'main/boards/drivers/rev7/TPS546.cpp').read_text(),'void TPS546_clear_faults(void)')+'\n'
source+='''int main(){
 TPS53647 four;TPS53667 eight;assert(four.init(4,90,95)&&eight.init(6,180,185)&&clears==2);
 {FiveTratumAutotune::RecoveryStartupGuard startup;
  assert(FiveTratumAutotune::preserveRecoveryFaults());
  latched=true;const auto before=clears;
  assert(four.init(4,90,95)&&eight.init(6,180,185));four.clear_faults();TPS546_clear_faults();
  assert(clears==before&&latched); // A fault arriving after off-state preflight is not cleared by startup.
  {FiveTratumAutotune::RecoveryStartupGuard nested;assert(FiveTratumAutotune::preserveRecoveryFaults());}
  assert(FiveTratumAutotune::preserveRecoveryFaults());
 }
 assert(!FiveTratumAutotune::preserveRecoveryFaults());four.clear_faults();assert(!latched&&clears==3);
 std::cout<<"Actual TPS53647/67 init and TPS546 clear paths preserve faults for the entire recovery startup scope; normal behavior unchanged\\n";
}
'''
with tempfile.TemporaryDirectory(prefix='5tfw-recovery-faults-') as folder:
 p=Path(folder);(p/'test.cpp').write_text(source);exe=p/'test'
 subprocess.run([os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++'),'-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I',str(ROOT/'main'),str(p/'test.cpp'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
