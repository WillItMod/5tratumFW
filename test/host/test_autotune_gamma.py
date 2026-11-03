#!/usr/bin/env python3
"""Actual Gamma adaptive adapter/API/NVS with deterministic fake hardware."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
HOST=Path(__file__).resolve().parent
IMAGE='espressif/idf:v5.5.3@sha256:8ccd4d2ce413889c6c2bba57e986c670302094efb91c913c6091152e317a7805'
def method(source,signature):
    start=source.index(signature);brace=source.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]+'\n'
class GammaAutotuneTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory(prefix='5fw-autotune-gamma-')
        cls.addClassCleanup(cls.temp.cleanup)
        directory=Path(cls.temp.name)
        for name in ('cJSON.h','cJSON.c'):
            (directory/name).write_bytes(subprocess.run(['docker','run','--rm','--network','none','--entrypoint','cat',IMAGE,'/opt/esp/idf/components/json/cJSON/'+name],check=True,capture_output=True).stdout)
        for name in ('esp_err.h','esp_log.h','esp_http_server.h','esp_timer.h','nvs.h','nvs_flash.h',
                     'sv2_protocol.h','freertos/FreeRTOS.h','freertos/queue.h','freertos/task.h',
                     'freertos/semphr.h','esp_app_desc.h','esp_mac.h','psa/crypto.h','protocol_coordinator.h','driver/uart.h'):
            path=directory/name;path.parent.mkdir(parents=True,exist_ok=True)
            path.write_text('#include "autotune_gamma_stubs.h"\n')
        (directory/'i2c_bitaxe.h').write_text('#pragma once\n#include "autotune_gamma_stubs.h"\n')
        serial=(ROOT/'components/asic/serial.c').read_text()
        tps=(ROOT/'main/power/TPS546.c').read_text()
        (directory/'gamma_wire_methods.inc').write_text(
            method(serial,'uint32_t SERIAL_tx_failure_count(')+method(serial,'int SERIAL_send(')+
            method(tps,'static esp_err_t smb_write_byte(')+method(tps,'static esp_err_t smb_write_word('))
        cls.binary=directory/'autotune-gamma'
        command=shlex.split(os.environ.get('CC','cc'))+['-std=gnu11','-Wall','-Wextra','-Werror',
            '-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-sign-compare','-Wno-misleading-indentation','-Wno-deprecated-declarations',
            '-fsanitize=address,undefined','-fno-omit-frame-pointer',
            '-I',str(directory),'-I',str(HOST),'-I',str(ROOT/'main'),'-I',str(ROOT/'main/tasks'),
            '-I',str(ROOT/'main/http_server'),'-I',str(ROOT/'components/asic/include'),
            '-include',str(HOST/'autotune_gamma_stubs.h'),str(ROOT/'main/nvs_config.c'),
            str(ROOT/'main/device_config.c'),str(ROOT/'main/http_server/operating_profiles.c'),
            str(HOST/'autotune_gamma_harness.c'),str(directory/'cJSON.c'),'-pthread','-lm','-o',str(cls.binary)]
        result=subprocess.run(command,capture_output=True,text=True)
        if result.returncode: raise RuntimeError(result.stderr)
    def run_case(self,name):
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
        result=subprocess.run([str(self.binary),name],capture_output=True,text=True,env=env)
        self.assertEqual(result.returncode,0,result.stderr or result.stdout)
        self.assertEqual(result.stdout.strip(),'PASS')
    def test_waiver_required_before_backup_or_ownership(self): self.run_case('waiver')
    def test_get_is_read_only_and_authorization_precedes_body(self): self.run_case('read-only')
    def test_cached_regulator_diagnostics_preserve_raw_status_age_and_fault_without_io(self): self.run_case('strict-diagnostic')
    def test_search_report_uses_actual_controller_progress_and_obscures_adapter_override(self): self.run_case('search-presentation')
    def test_small_display_snapshot_never_waits_reuses_stale_point_or_refreshes_observation(self): self.run_case('display-snapshot')
    def test_obsolete_disabled_boot_reason_is_neutral_only_with_fresh_healthy_idle_state(self): self.run_case('disabled-reason')
    def test_backup_is_once_per_device_retained_across_runs_and_reboot(self): self.run_case('backup-once')
    def test_backup_write_or_commit_failure_denies_activation(self): self.run_case('backup-failure')
    def test_backup_binding_corruption_blocks_start_without_erase(self): self.run_case('backup-binding')
    def test_invalid_legacy_snapshot_is_rejected_before_durable_backup_and_can_be_repaired(self): self.run_case('backup-invalid-snapshot')
    def test_cached_backup_mutation_cannot_authorize_a_new_run(self): self.run_case('backup-cached-corruption')
    def test_cached_backup_deletion_does_not_recapture_a_new_original(self): self.run_case('backup-cached-deletion')
    def test_manual_handoff_cancels_without_restore_or_auto_resume(self): self.run_case('manual-handoff')
    def test_physical_transition_requires_fresh_voltage_matching_the_applied_point(self): self.run_case('wire-vout-drift')
    def test_manual_commit_after_owner_snapshot_hands_off_the_latest_atomic_pair(self): self.run_case('manual-pair-interleave')
    def test_calibration_and_regulator_changes_revoke_at_queue_and_commit_but_statistics_do_not(self): self.run_case('hardware-policy-handoff')
    def test_same_saved_manual_point_or_policy_revokes_ram_owner_without_nvs_write(self): self.run_case('same-saved-intent')
    def test_start_cannot_consume_manual_revocation_racing_the_saved_snapshot(self): self.run_case('start-snapshot-interleave')
    def test_restore_storage_failure_freezes_live_point_then_restores_all_fields(self): self.run_case('restore-storage')
    def test_restore_never_clears_fault_or_lowers_cooling_during_fault(self): self.run_case('restore-fault')
    def test_failed_sensor_reads_never_refresh_cached_headroom(self): self.run_case('stale-sensors')
    def test_incomplete_domain_measurements_deny_start(self): self.run_case('domains')
    def test_zero_output_current_cannot_become_valid_mining_power_through_fixed_allowance(self): self.run_case('zero-current')
    def test_lost_apply_completion_faults_and_preserves_backup(self): self.run_case('lost-apply')
    def test_failed_voltage_readback_cannot_confirm_applied_point(self): self.run_case('readback-failure')
    def test_configuration_changes_are_rejected_during_transition(self): self.run_case('configure-busy')
    def test_configuration_preflight_rejects_active_point_exclusion_before_storage(self): self.run_case('configure-preflight')
    def test_explicit_disable_restore_uses_hardware_limits_not_lower_auto_limits(self): self.run_case('restore-low-user-limit')
    def test_coupled_restore_waits_for_two_fresh_rail_readings_without_spurious_fault(self): self.run_case('restore-slow-rail')
    def test_coupled_restore_persistent_rail_mismatch_faults_at_deadline_without_upclock(self): self.run_case('restore-stuck-rail')
    def test_revocation_between_preflight_and_physical_frame_denies_old_generation(self): self.run_case('wire-after-preflight')
    def test_inflight_frame_completes_before_revocation_fence_and_future_frames_fail(self): self.run_case('wire-inflight')
    def test_simultaneous_fault_revocation_preserves_power_off_and_normal_recovery(self): self.run_case('wire-fault')
    def test_short_uart_write_cannot_confirm_applied_point(self): self.run_case('uart-failure')
    def test_unapplied_manual_settings_cannot_become_backup(self): self.run_case('manual-pending')
if __name__=='__main__': unittest.main()
