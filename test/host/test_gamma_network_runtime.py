#!/usr/bin/env python3
"""Production Gamma network policy and event hooks with bounded API doubles."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
HOST=Path(__file__).resolve().parent

def function(source,signature):
    start=source.index(signature); opening=source.index('{',start); depth=1; end=opening+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}'); end+=1
    return source[start:end]

class GammaNetworkRuntimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory(prefix='gamma-network-policy-');cls.addClassCleanup(cls.temp.cleanup)
        temp=Path(cls.temp.name);source=(ROOT/'components/connect/connect.c').read_text()
        state=source[source.index('static TimerHandle_t ip_acquire_timer'):source.index('static const char *get_wifi_reason_string(int reason);')]
        policy=source[source.index('bool network_ethernet_supported('):]
        (temp/'network_policy.inc').write_text(state+'\n'+function(source,'static void spawn_mdns_init_if_needed(')+'\n'+function(source,'bool wifi_is_connected(void)')+'\n'+
            function(source,'static void ip_timeout_callback(')+'\n'+function(source,'static void event_handler(')+'\n'+policy)
        for name in ('esp_err.h','esp_wifi_types.h','esp_netif.h','lwip/sys.h','lwip/netdb.h'):
            path=temp/name;path.parent.mkdir(exist_ok=True,parents=True);path.write_text('#include "gamma_network_runtime_stubs.h"\n')
        cls.binary=temp/'network-test'
        cls.command=shlex.split(os.environ.get('CC','cc'))+['-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-variable',
            '-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','-I',str(temp),'-I',str(HOST),'-I',str(ROOT/'main'),
            '-I',str(ROOT/'components/connect/include'),'-I',str(ROOT/'components/gamma_ethernet/include'),str(HOST/'gamma_network_runtime.c')]
        result=subprocess.run(cls.command+['-o',str(cls.binary)],text=True,capture_output=True)
        if result.returncode: raise RuntimeError(result.stderr)

    def run_case(self,binary,scenario):
        result=subprocess.run([str(binary),scenario],text=True,capture_output=True,timeout=10,
            env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
        self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(result.stdout.strip(),'PASS')

    def test_actual_policy_and_event_timelines(self):
        for scenario in ('off','self-test-reservation','pins','absent','empty-ssid','retire-wifi','recovery','readdress','readiness','stale-eth-status',
                         'task-failure','ap-client','ap-stop-client','roaming','events','mdns','mdns-start-guard','saved-and-diagnostic'):
            with self.subTest(scenario=scenario): self.run_case(self.binary,scenario)

    def test_reserved_hardware_overrides_reject_accessory_spi(self):
        for macro in ('CONFIG_GPIO_BUTTON_BOOT','CONFIG_GPIO_ASIC_RESET','CONFIG_GPIO_ASIC_ENABLE','CONFIG_GPIO_PLUG_SENSE'):
            with self.subTest(macro=macro):
                binary=Path(self.temp.name)/macro
                result=subprocess.run(self.command+['-D'+macro+'=41','-o',str(binary)],text=True,capture_output=True)
                self.assertEqual(result.returncode,0,result.stderr);self.run_case(binary,'pins')

    def test_identity_and_boot_contracts(self):
        config=(ROOT/'main/http_server/system_api_json.c').read_text()
        body=function(config,'static void system_api_add_config(')
        self.assertIn('esp_wifi_get_mac(WIFI_IF_STA, mac);',body)
        self.assertIn('cJSON_AddStringToObject(root, "macAddr", formattedMac);',body)
        self.assertNotIn('ESP_MAC_ETH',body)
        source=(ROOT/'components/connect/connect.c').read_text();boot=function(source,'void wifi_init(')
        capture=function(source,'void network_prepare_boot(')
        self.assertIn('nvs_config_get_bool(NVS_CONFIG_ETHERNET_ENABLED)',capture)
        self.assertNotIn('nvs_config_get_bool(NVS_CONFIG_ETHERNET_ENABLED)',boot)
        main=(ROOT/'main/main.c').read_text()
        self.assertLess(main.index('device_config_init(&GLOBAL_STATE)'),main.index('network_prepare_boot(&GLOBAL_STATE)'))
        self.assertLess(main.index('network_prepare_boot(&GLOBAL_STATE)'),main.index('self_test_init(&GLOBAL_STATE)'))
        self.assertLess(main.index('network_prepare_boot(&GLOBAL_STATE)'),main.index('BAP_init(&GLOBAL_STATE)'))
        self.assertIn('CONFIG_MDNS_MAX_INTERFACES=4',(ROOT/'sdkconfig.defaults').read_text())

if __name__=='__main__': unittest.main(verbosity=2)
