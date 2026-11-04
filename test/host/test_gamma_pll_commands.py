#!/usr/bin/env python3
"""Production BM1370 command and PLL resolver, with a captured wire boundary."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def method(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'


class GammaPllCommandTests(unittest.TestCase):
    def test_checked_solver_grid_holes_and_real_driver_never_sends_invalid_tuple(self):
        with tempfile.TemporaryDirectory(prefix='5fw-gamma-pll-') as temporary:
            directory = Path(temporary)
            (directory / 'esp_log.h').write_text(
                '#include <stdio.h>\nvoid host_log(void);\n#define ESP_LOGI(tag,...) do {(void)(tag); host_log(); if(0) printf(__VA_ARGS__);} while(0)\n'
                '#define ESP_LOGE ESP_LOGI\n')
            source = r'''
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "pll.h"
#include "esp_log.h"
#define TYPE_CMD 0x40
#define GROUP_ALL 0x10
#define CMD_WRITE 0x01
#define BM1370_SERIALTX_DEBUG false
static const char *TAG="host";
static unsigned frames;
static unsigned log_calls;
void host_log(void) {++log_calls;}
static uint8_t payload[6];
static void _send_BM1370(uint8_t type,const uint8_t *bytes,uint8_t size,bool debug) {
    (void)debug;assert(type==0x51 && size==6);
    assert(bytes[0]==0 && bytes[1]==8 && (bytes[2]==0x40||bytes[2]==0x50));
    assert(bytes[3]>=160 && bytes[3]<=239 && bytes[4]>=1 && bytes[4]<=2);
    unsigned p1=(bytes[5]>>4)+1,p2=(bytes[5]&15)+1;
    assert(p1>p2 && p1<=7 && p2>=1);memcpy(payload,bytes,6);++frames;
}
''' + method((ROOT / 'components/asic/bm1370.c').read_text(), 'float BM1370_send_hash_frequency(') + r'''
int main(void) {
    uint8_t fb,ref,p1,p2;float actual;
    for(unsigned i=64;i<=192;++i) {
        float requested=i*6.25f;unsigned before=frames,logs=log_calls;
        assert(pll_get_parameters_checked(requested,160,239,&fb,&ref,&p1,&p2,&actual));
        assert(log_calls==logs);
        assert(fabsf(actual-requested)<=2);
        assert(BM1370_send_hash_frequency(requested)==actual && frames==before+1);
        float encoded=25.0f*payload[3]/(payload[4]*((payload[5]>>4)+1)*((payload[5]&15)+1));
        assert(fabsf(encoded-actual)<.0001f);
    }
    /* Check every quarter-MHz origin too; unsupported holes are denied before
     * any frame rather than relaxing the power owner's existing 2 MHz check. */
    unsigned holes=0;
    for(unsigned i=1600;i<=4800;++i) {
        float requested=i*.25f;unsigned before=frames,logs=log_calls;
        bool supported=pll_get_parameters_checked(requested,160,239,&fb,&ref,&p1,&p2,&actual);
        assert(log_calls==logs);
        float sent=BM1370_send_hash_frequency(requested);
        assert(supported ? sent>0 && frames==before+1 : sent==0 && frames==before);
        if(!supported) {++holes;assert(fb==0&&ref==0&&p1==0&&p2==0&&actual==0);}
    }
    assert(holes==48);
    const float preserved[]={50,625.125f,690,750,1200};
    for(unsigned i=0;i<sizeof(preserved)/sizeof(preserved[0]);++i) {
        unsigned before=frames;float result=BM1370_send_hash_frequency(preserved[i]);
        assert(result>0&&fabsf(result-preserved[i])<=2&&frames==before+1);
    }
    const float invalid[]={0,-1,1,NAN,INFINITY,-INFINITY,FLT_MAX,3000,65535,1197.5f};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        unsigned before=frames;assert(BM1370_send_hash_frequency(invalid[i])==0&&frames==before);
    }
    assert(!pll_get_parameters_checked(750,239,160,&fb,&ref,&p1,&p2,&actual));
    assert(!pll_get_parameters_checked(750,0,239,&fb,&ref,&p1,&p2,&actual));
    assert(!pll_get_parameters_checked(750,160,256,&fb,&ref,&p1,&p2,&actual));
    assert(!pll_get_parameters_checked(750,160,239,NULL,&ref,&p1,&p2,&actual));
    unsigned logs=log_calls;
    pll_get_parameters(450,60,200,&fb,&ref,&p1,&p2,&actual);
    assert(actual==450&&fb==72&&ref==2&&p1==2&&p2==1&&log_calls==logs+1);
    puts("PASS");return 0;
}
'''
            (directory / 'commands.c').write_text(source)
            binary = directory / 'commands'
            command = shlex.split(os.environ.get('CC', 'cc')) + [
                '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                '-fno-omit-frame-pointer', '-I', str(directory), '-I', str(ROOT / 'components/asic/include'),
                str(ROOT / 'components/asic/pll.c'), str(directory / 'commands.c'), '-lm', '-o', str(binary)]
            subprocess.run(command, check=True, capture_output=True, text=True)
            environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
            result = subprocess.run([str(binary)], capture_output=True, text=True, env=environment)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout.strip(), 'PASS')


if __name__ == '__main__':
    unittest.main()
