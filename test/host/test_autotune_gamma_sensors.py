#!/usr/bin/env python3
"""Compile exact strict sensor/actuator bodies with failing I2C boundaries."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]

def method(source,signature):
    start=source.index(signature); brace=source.index('{',start); end=brace+1; depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]+'\n'

COMMON=r'''
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG -2
#define ESP_LOGI(tag,...) ((void)(tag))
#define ESP_LOGE(tag,...) ((void)(tag))
#define ESP_RETURN_ON_ERROR(call,tag,...) do {int e=(call);(void)(tag);if(e!=ESP_OK)return e;}while(0)
static const char *TAG="strict-fixture";
static unsigned reads,writes,fail_at;
static bool fail_read(void) {return ++reads==fail_at;}
'''

class GammaStrictSensorTests(unittest.TestCase):
    def compile_run(self,name,source):
        with tempfile.TemporaryDirectory(prefix='5fw-strict-gamma-') as temp:
            directory=Path(temp);(directory/'esp_err.h').write_text('#pragma once\n')
            (directory/'i2c_bitaxe.h').write_text('#pragma once\n')
            path=directory/(name+'.c');path.write_text(COMMON+source)
            binary=directory/name
            command=shlex.split(os.environ.get('CC','cc'))+['-std=gnu11','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(directory),
                '-I',str(ROOT/'main'),str(path),'-lm','-o',str(binary)]
            result=subprocess.run(command,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run([str(binary)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr or result.stdout)
            self.assertIn('PASS',result.stdout)
    def test_actual_tps546_transactions_modes_faults_and_voltage_commands(self):
        actual=(ROOT/'main/power/TPS546.c').read_text()
        harness=r'''
#include "power/TPS546.h"
#include "pmbus_commands.h"
#define TPS546_POWER_GOOD_GRACE_MS 250
#define pdMS_TO_TICKS(ms) (ms)
static unsigned xTaskGetTickCount(void) {return 100;}
static unsigned tps546_power_good_grace_until;
static TPS546_CONFIG tps546_config={.TPS546_INIT_VOUT_MIN=1,.TPS546_INIT_VOUT_MAX=1.25,.TPS546_INIT_VOUT_COMMAND=1.15};
static uint8_t mode=0x97,operation=OPERATION_ON;
static uint16_t raw_vin=5,raw_vout=600,raw_iout=10,raw_temp=55,command_value,register_values[256];
static esp_err_t smb_read_byte(uint8_t reg,uint8_t *out) {
    if(fail_read())return ESP_FAIL;
    *out=reg==PMBUS_VOUT_MODE?mode:reg==PMBUS_OPERATION?operation:0;return ESP_OK;
}
static esp_err_t smb_read_word(uint8_t reg,uint16_t *out) {
    if(fail_read())return ESP_FAIL;
    *out=reg==PMBUS_READ_VIN?raw_vin:reg==PMBUS_READ_VOUT?raw_vout:
        reg==PMBUS_READ_IOUT?raw_iout:reg==PMBUS_READ_TEMPERATURE_1?raw_temp:0;return ESP_OK;
}
static esp_err_t smb_write_byte(uint8_t reg,uint8_t value) {(void)reg;(void)value;++writes;return ESP_OK;}
static esp_err_t smb_write_word(uint8_t reg,uint16_t value) {++writes;register_values[reg]=value;if(reg==PMBUS_VOUT_COMMAND)command_value=value;return ESP_OK;}
'''
        for signature in ('static float slinear11_2_float(', 'static bool decode_vout_mode(',
                          'static float ulinear16_2_float(', 'static esp_err_t encode_voltage_in_mode(', 'static esp_err_t encode_voltage_checked(',
                          'static esp_err_t TPS546_write_vout_limit_ratios(', 'static esp_err_t TPS546_write_initial_vout_settings(', 'esp_err_t TPS546_set_vout(',
                          'esp_err_t TPS546_read_strict('):
            harness+=method(actual,signature)
        harness+=r'''
int main(void) {
    TPS546_StrictTelemetry sample;
    reads=0;assert(TPS546_read_strict(&sample)==ESP_OK);unsigned total_reads=reads;assert(total_reads==11);
    assert(fabs(sample.vout-600.0/512)<.00001&&sample.vin==5&&sample.iout==10&&sample.temperature_c==55);
    for(unsigned bad=1;bad<=total_reads;++bad) {
        reads=0;fail_at=bad;memset(&sample,0xA5,sizeof(sample));TPS546_StrictTelemetry before=sample;
        assert(TPS546_read_strict(&sample)!=ESP_OK&&!memcmp(&sample,&before,sizeof(sample)));
    }
    fail_at=0;
    for(unsigned rel=0;rel<2;++rel) {
        mode=rel?0x97:0x17;reads=0;
        assert(TPS546_read_strict(&sample)==ESP_OK&&fabs(sample.vout-600.0/512)<.00001);
        assert(fabs(ulinear16_2_float(600)-600.0/512)<.00001);
        reads=writes=0;assert(TPS546_set_vout(1.15f)==ESP_OK&&command_value==588&&writes==8);
        const uint8_t regs[]={PMBUS_VOUT_OV_FAULT_LIMIT,PMBUS_VOUT_OV_WARN_LIMIT,PMBUS_VOUT_MARGIN_HIGH,PMBUS_VOUT_MARGIN_LOW,PMBUS_VOUT_UV_WARN_LIMIT,PMBUS_VOUT_UV_FAULT_LIMIT};
        const float ratios[]={TPS546_INIT_VOUT_OV_FAULT_LIMIT,TPS546_INIT_VOUT_OV_WARN_LIMIT,TPS546_INIT_VOUT_MARGIN_HIGH,TPS546_INIT_VOUT_MARGIN_LOW,TPS546_INIT_VOUT_UV_WARN_LIMIT,TPS546_INIT_VOUT_UV_FAULT_LIMIT};
        for(unsigned i=0;i<6;++i) {
            const float value=rel?ratios[i]:ratios[i]*1.15f;
            assert(register_values[regs[i]]==(uint16_t)((double)value*512));
        }
        reads=writes=0;assert(TPS546_write_initial_vout_settings()==ESP_OK&&writes==9);
        assert(register_values[PMBUS_VOUT_COMMAND]==588&&register_values[PMBUS_VOUT_MIN]==512&&register_values[PMBUS_VOUT_MAX]==640);
    }
    for(unsigned bad=0;bad<256;++bad) {
        mode=bad;int exp=(int)(mode&31);if(exp&16)exp-=32;
        const bool expected=!(mode&0x60)&&exp>=-12&&exp<=-4;
        uint16_t encoded=123;assert((encode_voltage_in_mode(1.15f,mode,&encoded)==ESP_OK)==expected);
    }
    mode=0x97;reads=writes=0;fail_at=1;assert(TPS546_write_initial_vout_settings()!=ESP_OK&&writes==0);
    fail_at=0;mode=0xB7;reads=writes=0;assert(TPS546_write_initial_vout_settings()!=ESP_OK&&writes==0);
    reads=0;mode=0x37;assert(TPS546_read_strict(&sample)!=ESP_OK);
    mode=0x9c;assert(TPS546_read_strict(&sample)!=ESP_OK); /* Valid hardware -4, too coarse for adaptive5mV. */
    mode=0;assert(TPS546_read_strict(&sample)!=ESP_OK); /* Unsupported exponent. */
    mode=0x17;raw_iout=0x7ff;assert(TPS546_read_strict(&sample)!=ESP_OK);
    raw_iout=10;raw_vout=0;assert(TPS546_read_strict(&sample)!=ESP_OK);raw_vout=600;
    raw_temp=151;assert(TPS546_read_strict(&sample)!=ESP_OK);raw_temp=55;
    reads=writes=0;fail_at=1;assert(TPS546_set_vout(1.15f)!=ESP_OK&&writes==0);
    reads=writes=0;fail_at=0;mode=0x37;assert(TPS546_set_vout(1.15f)!=ESP_OK&&writes==0);
    mode=0x17;assert(TPS546_set_vout(NAN)!=ESP_OK&&writes==0);
    assert(TPS546_set_vout(1.5f)!=ESP_OK&&writes==0);
    reads=writes=0;assert(TPS546_set_vout(1.15f)==ESP_OK&&command_value==588&&writes==8);
    operation=OPERATION_OFF;reads=writes=0;assert(TPS546_set_vout(1.15f)!=ESP_OK);
    reads=writes=0;assert(TPS546_set_vout(0)==ESP_OK&&writes==1&&tps546_power_good_grace_until==0);
    puts("PASS: TPS546 0x97 REL and 0x17 absolute command/init/all6limits, all256mode bytes, strict acquisition failure and encoding guards");
}
'''
        self.compile_run('tps546',harness)
    def test_actual_emc2101_rejects_torn_samples_open_short_and_dead_fan(self):
        actual=(ROOT/'main/thermal/EMC2101.c').read_text()
        harness=r'''
typedef int i2c_master_dev_handle_t;
#include "thermal/EMC2101.h"
static i2c_master_dev_handle_t emc2101_dev_handle;
static int temp_offset;
static uint16_t raw_temp=50*8,raw_tach=1800;
static uint8_t status;
static bool torn;
static esp_err_t i2c_bitaxe_register_read(int device,uint8_t reg,uint8_t *out,unsigned size) {
    (void)device;assert(size==1);if(fail_read())return ESP_FAIL;
    if(reg==EMC2101_STATUS)*out=status;
    else if(reg==EMC2101_EXTERNAL_TEMP_MSB)*out=(raw_temp>>3)+(torn&&reads==4?1:0);
    else if(reg==EMC2101_EXTERNAL_TEMP_LSB)*out=(raw_temp&7)<<5;
    else if(reg==EMC2101_TACH_LSB)*out=raw_tach&255;
    else if(reg==EMC2101_TACH_MSB)*out=raw_tach>>8;
    else assert(false);return ESP_OK;
}
'''
        harness+=method(actual,'esp_err_t EMC2101_read_strict(')
        harness+=r'''
int main(void) {
    float temperature=99;uint16_t rpm=999;
    assert(EMC2101_read_strict(&temperature,&rpm)==ESP_OK&&temperature==50&&rpm==3000);
    for(unsigned bad=1;bad<=6;++bad) {
        reads=0;fail_at=bad;temperature=99;rpm=999;
        assert(EMC2101_read_strict(&temperature,&rpm)!=ESP_OK&&temperature==99&&rpm==999);
    }
    fail_at=0;reads=0;status=4;assert(EMC2101_read_strict(&temperature,&rpm)!=ESP_OK);status=0;
    reads=0;raw_temp=EMC2101_TEMP_FAULT_OPEN_CIRCUIT;assert(EMC2101_read_strict(&temperature,&rpm)!=ESP_OK);
    reads=0;raw_temp=EMC2101_TEMP_FAULT_SHORT;assert(EMC2101_read_strict(&temperature,&rpm)!=ESP_OK);
    raw_temp=50*8;reads=0;torn=true;assert(EMC2101_read_strict(&temperature,&rpm)!=ESP_OK);torn=false;
    reads=0;raw_tach=0;assert(EMC2101_read_strict(&temperature,&rpm)==ESP_OK&&rpm==0);
    reads=0;raw_tach=UINT16_MAX;assert(EMC2101_read_strict(&temperature,&rpm)==ESP_OK&&rpm==0);
    assert(EMC2101_read_strict(NULL,&rpm)!=ESP_OK);
    (void)writes;(void)TAG;
    puts("PASS: exact EMC2101 strict body rejects failed/torn/open/short acquisition and distinguishes stoppedfan");
}
'''
        self.compile_run('emc2101',harness)
if __name__=='__main__': unittest.main()
