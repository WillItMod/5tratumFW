#!/usr/bin/env python3
"""Production profile API + NVS journal, fake storage/HTTP, no miner access."""
import os, shlex, subprocess, tempfile, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
HOST=Path(__file__).resolve().parent
IMAGE='espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805'
class ProfileRuntimeTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.temp=tempfile.TemporaryDirectory(prefix='5fw-profiles-');cls.addClassCleanup(cls.temp.cleanup);directory=Path(cls.temp.name)
  for name in ['cJSON.h','cJSON.c']:
   (directory/name).write_bytes(subprocess.run(['docker','run','--rm','--entrypoint','cat',IMAGE,'/opt/esp/idf/components/json/cJSON/'+name],check=True,capture_output=True).stdout)
  for name in ['esp_err.h','esp_log.h','esp_http_server.h','nvs.h','nvs_flash.h','sv2_protocol.h','freertos/FreeRTOS.h','freertos/queue.h','freertos/task.h','freertos/semphr.h','esp_app_desc.h','esp_mac.h','mbedtls/sha256.h','protocol_coordinator.h']:
   path=directory/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('#include "profiles_runtime_stubs.h"\n')
  cls.binary=directory/'profiles-runtime'
  cmd=shlex.split(os.environ.get('CC','cc'))+['-std=gnu11','-Wall','-Wextra','-Werror','-Wno-sign-compare','-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-misleading-indentation','-I',str(directory),'-I',str(HOST),'-I',str(ROOT/'main'),'-I',str(ROOT/'main/tasks'),'-I',str(ROOT/'main/http_server'),'-include',str(HOST/'profiles_runtime_stubs.h'),str(ROOT/'main/nvs_config.c'),str(ROOT/'main/device_config.c'),str(ROOT/'main/http_server/operating_profiles.c'),str(HOST/'profiles_runtime_test.c'),str(directory/'cJSON.c'),'-lm','-o',str(cls.binary)]
  # Isolate malloc fault injection to the actual production journal unit.
  obj=directory/'nvs-config.o';source=str(ROOT/'main/nvs_config.c')
  base=cmd[:cmd.index(source)]
  compiled=subprocess.run(base+['-Dmalloc=profiles_test_malloc','-c',source,'-o',str(obj)],capture_output=True,text=True)
  if compiled.returncode:raise RuntimeError(compiled.stderr)
  cmd[cmd.index(source)]=str(obj)
  result=subprocess.run(cmd,capture_output=True,text=True)
  if result.returncode:raise RuntimeError(result.stderr)
 def run_case(self,case):
  result=subprocess.run([str(self.binary),case],capture_output=True,text=True);self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(result.stdout.strip(),'PASS')
 def test_exact_ten_slots_redaction_identity_and_read_only_get(self):self.run_case('redaction')
 def test_authorization_before_body_storage_or_mutation(self):self.run_case('auth')
 def test_fractional_point_atomic_commit_and_reboot_replay(self):self.run_case('point')
 def test_bounds_integer_voltage_duplicate_fields_and_unchanged_saved_fraction(self):self.run_case('bounds')
 def test_pool_capture_private_password_apply_preserves_other_route_and_reference_guard(self):self.run_case('pool')
 def test_nvs_set_and_commit_failures_preserve_old_point_across_reboot(self):self.run_case('failure')
 def test_json_allocation_failures_never_commit_partial_profile(self):self.run_case('oom')
 def test_journal_allocation_failures_preserve_complete_previous_snapshot(self):self.run_case('journal-oom')
 def test_private_empty_or_damaged_slot_fields_cannot_leak(self):self.run_case('private-slot')
 def test_corrupt_journal_blocks_hardware_initialization_without_erase(self):self.run_case('corrupt-journal')
if __name__=='__main__':unittest.main(verbosity=2)
