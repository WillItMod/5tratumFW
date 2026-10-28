#!/usr/bin/env python3
"""Production display admission checks before mocked peripheral initialization.

Tests the actual display-config reader/lookup and the initialization prefix.
All pin/LVGL/I2C setup is replaced by one counter; no device access occurs.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def method(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


class GammaDisplayGuardTests(unittest.TestCase):
    def test_patch_rejects_color_before_any_setting_mutation_and_accepts_original_choices(self):
        display = (ROOT / "main/display.c").read_text()
        http = (ROOT / "main/http_server/http_server.c").read_text()
        harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "display.h"
#include "nvs_config.h"
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define ESP_LOGW(tag, ...) ((void)(tag))
static const char *TAG = "display-patch-fixture";
static GlobalState *GLOBAL_STATE;
static unsigned setting_writes, hostname_updates;
static Settings settings[NVS_CONFIG_COUNT] = {
    [NVS_CONFIG_HOSTNAME] = {.rest_name="hostname",.type=TYPE_STR,.min=1,.max=32},
    [NVS_CONFIG_ASIC_FREQUENCY] = {.rest_name="frequency",.type=TYPE_FLOAT,.min=1,.max=1000},
    [NVS_CONFIG_ASIC_VOLTAGE] = {.rest_name="coreVoltage",.type=TYPE_U16,.min=1,.max=1500},
    [NVS_CONFIG_DISPLAY] = {.rest_name="display",.type=TYPE_STR,.min=1,.max=32},
};
// The platform boundary supplies parsed JSON nodes and an instrumented settings
// cache. Validation and the complete validate-then-mutate loops are production.
enum {JSON_OBJECT,JSON_STRING,JSON_NUMBER,JSON_BOOL};
typedef struct cJSON {int type,valueint;double valuedouble;const char *string,*valuestring;struct cJSON *child,*next;} cJSON;
#define cJSON_ArrayForEach(item, root) for ((item)=(root)->child; (item); (item)=(item)->next)
static cJSON *cJSON_GetObjectItem(const cJSON *root,const char *name) {
    for(cJSON *item=root->child;item;item=item->next)if(!strcmp(item->string,name))return item;
    return NULL;
}
static bool cJSON_IsString(const cJSON *item){return item && item->type==JSON_STRING;}
static bool cJSON_IsObject(const cJSON *item){return item && item->type==JSON_OBJECT;}
static cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON *root,const char *name){return cJSON_GetObjectItem(root,name);}
static bool cJSON_IsNumber(const cJSON *item){return item && item->type==JSON_NUMBER;}
static bool cJSON_IsBool(const cJSON *item){return item && item->type==JSON_BOOL;}
static bool cJSON_IsTrue(const cJSON *item){return cJSON_IsBool(item) && item->valueint!=0;}
static bool cJSON_IsFalse(const cJSON *item){return cJSON_IsBool(item) && item->valueint==0;}
static bool network_ethernet_supported(const GlobalState *state){(void)state;return false;}
static bool network_ethernet_pins_available(const GlobalState *state){(void)state;return false;}
esp_err_t nvs_config_set_ethernet_enabled(bool value){(void)value;assert(!"unexpected Ethernet mutation in display test");return ESP_OK;}
Settings *nvs_config_get_settings(NvsConfigKey key){assert(key>=0 && key<NVS_CONFIG_COUNT);return &settings[key];}
void nvs_config_set_string(NvsConfigKey key,const char *value){(void)key;assert(value);++setting_writes;}
void nvs_config_set_u16(NvsConfigKey key,uint16_t value){(void)key;(void)value;++setting_writes;}
void nvs_config_set_i32(NvsConfigKey key,int32_t value){(void)key;(void)value;++setting_writes;}
void nvs_config_set_u64(NvsConfigKey key,uint64_t value){(void)key;(void)value;++setting_writes;}
void nvs_config_set_float(NvsConfigKey key,float value){(void)key;(void)value;++setting_writes;}
void nvs_config_set_bool(NvsConfigKey key,bool value){(void)key;(void)value;++setting_writes;}
#define strlcpy fixture_strlcpy
static size_t fixture_strlcpy(char *dst,const char *src,size_t size){size_t length=strlen(src);if(size)snprintf(dst,size,"%s",src);return length;}
static void normalize_hostname(char *value,size_t size){(void)value;(void)size;}
static void update_mdns_hostname(const char *name,GlobalState *state){(void)name;(void)state;++hostname_updates;}
enum {STRATUM_PROTOCOL_UNKNOWN,STRATUM_PROTOCOL_V1,SV2_CHANNEL_UNKNOWN=0};
static int stratum_protocol_from_string(const char *value){(void)value;return STRATUM_PROTOCOL_V1;}
static int sv2_channel_type_from_string(const char *value){(void)value;return 1;}
'''
        harness += method(display, "const DisplayConfig * get_display_config(")
        harness += method(http, "bool check_settings_and_update(")
        harness += r'''
int main(void) {
    cJSON screen={.type=JSON_STRING,.string="display"};
    cJSON voltage={.type=JSON_NUMBER,.string="coreVoltage",.valueint=1150,.valuedouble=1150,.next=&screen};
    cJSON frequency={.type=JSON_NUMBER,.string="frequency",.valueint=525,.valuedouble=525,.next=&voltage};
    cJSON hostname={.type=JSON_STRING,.string="hostname",.valuestring="gamma-fixture",.next=&frequency};
    cJSON body={.type=JSON_OBJECT,.child=&hostname};
    // The invalid screen is deliberately last in JSON and in settings order:
    // hostname, frequency and voltage must all remain untouched on rejection.
    for(unsigned index=0;index<ARRAY_SIZE(display_configs);++index) {
        screen.valuestring=display_configs[index].name;
        const unsigned before=setting_writes,hosts=hostname_updates;
        const bool accepted=display_configs[index].display!=ST7789_I80;
        char *redirect=(char *)1;
        assert(check_settings_and_update(&body,&redirect)==accepted);
        assert(setting_writes==before+(accepted?4:0));
        assert(hostname_updates==hosts+accepted);
        if(accepted){assert(redirect && strstr(redirect,"gamma-fixture.local"));free(redirect);}
        else assert(redirect==NULL);
    }
    const unsigned before=setting_writes,hosts=hostname_updates;
    screen.valuestring="unknown screen";
    assert(!check_settings_and_update(&body,NULL));
    screen.type=JSON_NUMBER;screen.valueint=1;screen.valuedouble=1;
    assert(!check_settings_and_update(&body,NULL));
    assert(setting_writes==before && hostname_updates==hosts);
    puts("Production Gamma PATCH rejects unsupported color/unknown/nonstring displays before every setting mutation; original OLED/NONE choices remain accepted");
}
'''
        with tempfile.TemporaryDirectory(prefix="5tfw-display-patch-") as directory:
            temporary = Path(directory)
            (temporary / "esp_err.h").write_text("#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n")
            (temporary / "display-patch.c").write_text(harness)
            binary = temporary / "display-patch"
            subprocess.run([shutil.which("clang") or shutil.which("cc"), "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare",
                            "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-I", str(temporary), "-I", str(ROOT / "main"),
                            str(temporary / "display-patch.c"), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

    def test_oled_only_admission_precedes_all_peripheral_work_and_preserves_nvs(self):
        source = (ROOT / "main/display.c").read_text()
        initialization = method(source, "esp_err_t display_init(")
        self.assertLess(initialization.index("read_display_config(GLOBAL_STATE)"), initialization.index("lvgl_port_cfg_t lvgl_cfg"))
        prefix = initialization[:initialization.index("    lvgl_port_cfg_t lvgl_cfg")]
        prefix += "    ++peripheral_initializations; return ESP_OK;\n}\n"
        harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "display.h"
#include "5tratumfw.h"
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define NVS_CONFIG_DISPLAY 1
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGE(tag, ...) ((void)(tag))
#define ESP_RETURN_ON_ERROR(action, tag, ...) do { int err = (action); (void)(tag); if (err != ESP_OK) return err; } while (0)
static const char *TAG = "display-fixture";
static const char *persisted_display = "SSD1306 (128x32)";
static unsigned peripheral_initializations, settings_reads;
struct GlobalState {
    struct { const char *board_version; struct { const void *i80; } pins; } DEVICE_CONFIG;
    DisplayConfig DISPLAY_CONFIG;
};
char *nvs_config_get_string(int key) { assert(key == NVS_CONFIG_DISPLAY); ++settings_reads; return persisted_display ? strdup(persisted_display) : NULL; }
'''
        harness += method(source, "const DisplayConfig * get_display_config(")
        harness += method(source, "static esp_err_t read_display_config(") + prefix
        harness += r'''
int main(void) {
    GlobalState state = {0};
    for (unsigned board = 0; board < 2; ++board) {
        state.DEVICE_CONFIG.board_version = board ? "602" : "601";
        for (unsigned index = 0; index < ARRAY_SIZE(display_configs); ++index) {
            persisted_display = display_configs[index].name;
            state.DISPLAY_CONFIG = display_configs[0];
            const unsigned before = peripheral_initializations;
            const bool color = display_configs[index].display == ST7789_I80;
            assert((display_init(&state) == ESP_OK) == !color);
            assert(peripheral_initializations == before + !color);
            assert(!strcmp(persisted_display, display_configs[index].name));
            assert(state.DISPLAY_CONFIG.display == (color ? display_configs[0].display : display_configs[index].display));
        }
        persisted_display = "SSD1306 (128x32)";
        state.DEVICE_CONFIG.pins.i80 = &state; // A color pin map must never authorize GPIO setup on Gamma.
        const unsigned before = peripheral_initializations, reads = settings_reads;
        assert(display_init(&state) == ESP_ERR_NOT_SUPPORTED);
        assert(peripheral_initializations == before && settings_reads == reads);
        state.DEVICE_CONFIG.pins.i80 = NULL;
    }
    const char *unsupported[] = {NULL, "", "600", "603", "604", "650", "601-extra", "NerdQAxe++"};
    for (unsigned index = 0; index < ARRAY_SIZE(unsupported); ++index) {
        state.DEVICE_CONFIG.board_version = unsupported[index];
        const unsigned before = peripheral_initializations, reads = settings_reads;
        assert(display_init(&state) == ESP_ERR_NOT_SUPPORTED);
        assert(peripheral_initializations == before && settings_reads == reads);
    }
    state.DEVICE_CONFIG.board_version = "601";
    for (unsigned missing = 0; missing < 2; ++missing) {
        persisted_display = missing ? NULL : "unrecognized display";
        const unsigned before = peripheral_initializations;
        assert(display_init(&state) != ESP_OK && peripheral_initializations == before);
    }
    assert(display_init(NULL) == ESP_ERR_NOT_SUPPORTED);
    puts("Production Gamma601/602 OLED admission rejects I80/color/unknown identity before peripheral setup, without changing settings");
}
'''
        with tempfile.TemporaryDirectory(prefix="5tfw-display-guard-") as directory:
            temporary = Path(directory)
            (temporary / "esp_err.h").write_text("#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n#define ESP_ERR_NOT_SUPPORTED 0x106\n")
            (temporary / "display-guard.c").write_text(harness)
            binary = temporary / "display-guard"
            subprocess.run([shutil.which("clang") or shutil.which("cc"), "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-I", str(temporary), "-I", str(ROOT / "main"),
                            str(temporary / "display-guard.c"), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
