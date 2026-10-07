#define FIVE_TRATUM_OLED_HOST_TEST
#include "oled_screen_runtime_stubs.h"
#include <assert.h>
int64_t host_now=10000000;
bool host_applied, host_display_on;
int host_lock_depth;
int host_display_timeout=-1;
FiveTratumMuxSnapshot host_mux;
float host_rate_gh=1200;
uint64_t host_rate_sample_us=9000000;
/* The legacy image declarations link, but compact runtime uses established new mark. */
const lv_img_dsc_t osmu_logo={0}, identify_text={0};
#include "../../../main/screen.c"
static uint8_t buffer[128*32/8+8];
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels)
{
    (void)area; (void)pixels; lv_display_flush_ready(display);
}
static void update(void)
{
    assert(host_lock_depth==0);
    lvgl_port_lock(0);
    screen_update_cb(NULL);
    lvgl_port_unlock();
    assert(host_lock_depth==0);
}
int main(void)
{
    lv_init();
    lv_display_t *display=lv_display_create(128,32);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_I1);
    lv_display_set_buffers(display,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display,flush);
    GlobalState state={
        .SYSTEM_MODULE={.ssid="Simulated Wi-Fi",.pool_url="pool.example.invalid",.fallback_pool_url="fallback.example.invalid",
                        .ap_ssid="5FW_setup",.ip_addr_str="192.0.2.60",.wifi_status="Connected",
                        .best_session_diff_string="2.3G",.best_diff_string="14.8G",.current_hashrate=1200,
                        .shares_accepted=4294967296ULL,.shares_rejected=12,.is_connected=true,
                        .is_screen_active=true,.mining_runtime_ready=true},
        .POWER_MANAGEMENT_MODULE={.chip_temp_avg=58.2,.vr_temp=47.1,.power=18.4,.fan_perc=80,.fan_rpm=4200,.core_voltage=1128,.voltage=5200},
        .DEVICE_CONFIG={.family={.name="Gamma"},.board_version="601"},
        .ASIC_initalized=true,.stratum_protocol=STRATUM_PROTOCOL_V1,
    };
    assert(screen_start(&state)==ESP_OK && host_lock_depth==0);
    update(); assert(get_current_screen()==SCR_BITAXE_LOGO);
    screen_button_press(); assert(get_current_screen()==SCR_OSMU_LOGO);
    screen_button_press(); assert(get_current_screen()==SCR_URLS); /* Never cycles into a false candidate. */
    screen_button_press(); assert(get_current_screen()==SCR_STATS);
    screen_button_press(); assert(get_current_screen()==SCR_WIFI); /* Decode off skips job metadata. */
    state.SYSTEM_MODULE.pool_decode_coinbase_tx=true;
    screen_show(SCR_STATS); screen_button_press(); assert(get_current_screen()==SCR_MINING);
    state.SYSTEM_MODULE.identify_mode_time_ms=5000;
    host_display_timeout=0;
    lv_tick_inc(100000);
    update(); assert(!lv_obj_has_flag(identify_image,LV_OBJ_FLAG_HIDDEN));
    assert(host_display_on); /* Explicit identify wakes a configured-off display. */
    state.SYSTEM_MODULE.hardware_fault=true;
    strcpy(state.SYSTEM_MODULE.hardware_fault_msg,"ASIC power-off failed");
    state.SYSTEM_MODULE.is_firmware_update=true;
    update(); assert(get_current_screen()==SCR_ASIC_STATUS && host_display_on);
    assert(!strcmp(lv_label_get_text(compact_views[SCR_ASIC_STATUS].rows[1]),"Stop requested"));
    assert(lv_obj_has_flag(identify_image,LV_OBJ_FLAG_HIDDEN));
    screen_button_press(); assert(get_current_screen()==SCR_ASIC_STATUS);
    state.SYSTEM_MODULE.overheat_mode=true;
    update(); assert(get_current_screen()==SCR_OVERHEAT);
    state.SYSTEM_MODULE.overheat_mode=false;
    state.SYSTEM_MODULE.hardware_fault=false;
    state.SYSTEM_MODULE.is_firmware_update=false;
    state.SYSTEM_MODULE.identify_mode_time_ms=0;
    const char *recovery="Hold BOOT button for 2 seconds to cancel self-test, or press RESET to run self-test again.";
    state.SELF_TEST_MODULE=(SelfTestModule){.is_active=true,.is_finished=true,
        .message="ASIC chain detection failed",.result="SELF-TEST FAIL!",.finished=recovery};
    state.SYSTEM_MODULE.asic_status="ASIC chain detection failed";
    update(); assert(get_current_screen()==SCR_SELF_TEST);
    assert(!strcmp(lv_label_get_text(compact_views[SCR_SELF_TEST].rows[0]),state.SYSTEM_MODULE.asic_status));
    assert(!strcmp(lv_label_get_text(compact_views[SCR_SELF_TEST].rows[1]),"SELF-TEST FAIL!"));
    assert(!strcmp(lv_label_get_text(compact_views[SCR_SELF_TEST].rows[2]),recovery));
    screen_button_press(); assert(get_current_screen()==SCR_SELF_TEST);
    state.SYSTEM_MODULE.hardware_fault=true;
    update(); assert(get_current_screen()==SCR_ASIC_STATUS); /* Real power fault still preempts. */
    state.SYSTEM_MODULE.overheat_mode=true;
    update(); assert(get_current_screen()==SCR_OVERHEAT);
    state.SYSTEM_MODULE.overheat_mode=false; state.SYSTEM_MODULE.hardware_fault=false;
    update(); assert(get_current_screen()==SCR_SELF_TEST);
    state.SELF_TEST_MODULE.is_active=false; state.SYSTEM_MODULE.asic_status=NULL;
    state.SYSTEM_MODULE.show_new_block=true;
    update(); assert(get_current_screen()==SCR_CANDIDATE);
    assert(strstr(lv_label_get_text(compact_views[SCR_CANDIDATE].rows[0]),"CANDIDATE"));
    state.SYSTEM_MODULE.hardware_fault=true;
    update(); assert(get_current_screen()==SCR_ASIC_STATUS);
    state.SYSTEM_MODULE.hardware_fault=false;
    update(); assert(get_current_screen()==SCR_CANDIDATE); /* Clearing fault retains candidate. */
    state.SYSTEM_MODULE.show_new_block=false;
    state.SYSTEM_MODULE.mining_paused=true;
    update(); assert(!strcmp(lv_label_get_text(compact_views[SCR_STATS].title),"PAUSING"));
    assert(!strcmp(lv_label_get_text(compact_views[SCR_STATS].rate),"--"));
    host_applied=true;
    update(); assert(!strcmp(lv_label_get_text(compact_views[SCR_STATS].title),"PAUSED"));
    state.SYSTEM_MODULE.mining_paused=false;
    update(); assert(!strcmp(lv_label_get_text(compact_views[SCR_STATS].title),"RESUMING"));
    host_applied=false;
    host_mux=(FiveTratumMuxSnapshot){.connected=true,.acknowledged=true};
    update(); assert(!strcmp(lv_label_get_text(compact_views[SCR_URLS].rows[0]),"P1 5tratMUX"));
    state.SYSTEM_MODULE.is_using_fallback=true;
    update(); assert(!strcmp(lv_label_get_text(compact_views[SCR_URLS].rows[0]),"P2 SV1/TCP"));
    state.POWER_MANAGEMENT_MODULE.power=0;
    state.POWER_MANAGEMENT_MODULE.chip_temp_avg=0;
    update();
    assert(!strcmp(lv_label_get_text(compact_views[SCR_STATS].power),"--W"));
    assert(!strcmp(lv_label_get_text(compact_views[SCR_STATS].temperature),"--C"));
    assert(!strcmp(lv_label_get_text(compact_views[SCR_HEALTH].rows[2]),"Eff -- J/TH"));
    state.POWER_MANAGEMENT_MODULE.power=18.4;
    state.POWER_MANAGEMENT_MODULE.chip_temp_avg=58.2;
    host_now=(int64_t)(host_rate_sample_us+HASHRATE_DISPLAY_TTL_US-1);
    update(); assert(!strcmp(lv_label_get_text(compact_views[SCR_STATS].title),"MINING"));
    assert(!strcmp(lv_label_get_text(compact_views[SCR_STATS].rate),"1.20"));
    host_now++;
    update(); assert(!strcmp(lv_label_get_text(compact_views[SCR_STATS].title),"NO DATA"));
    assert(!strcmp(lv_label_get_text(compact_views[SCR_STATS].rate),"--"));
    assert(!strcmp(lv_label_get_text(compact_views[SCR_HEALTH].rows[2]),"Eff -- J/TH"));
    host_rate_sample_us=(uint64_t)host_now+1;
    update(); assert(!strcmp(lv_label_get_text(compact_views[SCR_STATS].title),"NO DATA"));
    host_rate_sample_us=0;
    update(); assert(!strcmp(lv_label_get_text(compact_views[SCR_STATS].rate),"--"));
    host_rate_sample_us=(uint64_t)host_now; host_rate_gh=0;
    update(); assert(!strcmp(lv_label_get_text(compact_views[SCR_STATS].rate),"0"));
    assert(!strcmp(lv_label_get_text(compact_views[SCR_STATS].title),"MINING"));
    state.SYSTEM_MODULE.mining_paused=true; host_applied=true; host_rate_gh=1200;
    update(); assert(!strcmp(lv_label_get_text(compact_views[SCR_STATS].title),"PAUSED"));
    assert(!strcmp(lv_label_get_text(compact_views[SCR_STATS].rate),"--"));
    host_rate_sample_us=0; state.SYSTEM_MODULE.mining_paused=false; host_applied=false;
    update(); assert(!strcmp(lv_label_get_text(compact_views[SCR_STATS].title),"NO DATA"));
    assert(host_lock_depth==0);
    puts("PASS: actual screen.c startup/carousel, self-test recovery, safety priority, pause transitions, stale rates and MUX fallback");
    lv_deinit();
    return 0;
}
