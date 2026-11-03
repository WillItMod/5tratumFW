#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "oled_display_layout.h"
#include "tasks/hashrate_display.h"

static uint8_t framebuffer[128*32];
static uint8_t draw_buffer[128*32/8+8];
static int32_t text_width(const char *text)
{
    lv_point_t size;
    lv_text_get_size(&size,text,&lv_font_portfolio_6x8,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    return size.x;
}
static const char *tuning_row(FiveTratumOledView *view,unsigned row)
{
    static char line[128];const char *text=lv_label_get_text(view->tuning_body);
    for(unsigned i=0;i<row;++i) {text=strchr(text,'\n');assert(text);++text;}
    const char *end=strchr(text,'\n');size_t length=end ? (size_t)(end-text) : strlen(text);
    assert(length<sizeof(line));memcpy(line,text,length);line[length]=0;return line;
}
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *map)
{
    /* LVGL I1 palettes precede the row-aligned 1-bit pixels. */
    unsigned stride = (unsigned)(area->x2-area->x1+1+7)/8;
    const uint8_t *pixels = map+8;
    for (int y=area->y1; y<=area->y2; ++y)
        for (int x=area->x1; x<=area->x2; ++x)
            framebuffer[y*128+x] = (pixels[(y-area->y1)*stride+(x-area->x1)/8] >> (7-(x-area->x1)%8)) & 1;
    lv_display_flush_ready(display);
}
static void save(const char *directory, const char *name, FiveTratumOledView *view, const FiveTratumOledData *data)
{
    oled_render_view(view, data);
    lv_screen_load(view->screen);
    lv_obj_update_layout(view->screen);
    lv_refr_now(NULL);
    assert(lv_obj_get_width(view->screen)==128 && lv_obj_get_height(view->screen)==32);
    if (view->page==OLED_CANDIDATE) {
        assert(lv_image_get_src(view->donut)==&oled_donut_variants[data->candidate_variant%OLED_DONUT_VARIANTS]);
        assert(lv_obj_get_x(view->donut)>=0 && lv_obj_get_y(view->donut)>=0);
        assert(lv_obj_get_x(view->donut)+lv_obj_get_width(view->donut)<=32);
        assert(lv_obj_get_y(view->donut)+lv_obj_get_height(view->donut)<=32);
        for (unsigned i=0;i<4;++i) {
            lv_obj_t *label=i ? view->rows[i-1] : view->title;
            const char *text=lv_label_get_text(label);
            assert(text_width(text)<=96);
            assert(lv_obj_get_y(label)+lv_obj_get_height(label)<=32);
        }
    } else if (view->page==OLED_TUNING) {
        assert(text_width(lv_label_get_text(view->title))<=128);
        for(unsigned i=0;i<3;++i) {
            assert(text_width(tuning_row(view,i))<=128);
            assert(lv_obj_get_y(view->tuning_body)+lv_obj_get_height(view->tuning_body)<=32);
        }
    }
    unsigned lit=0;
    for (unsigned i=0; i<sizeof(framebuffer); ++i) lit+=framebuffer[i];
    assert(lit>50 && lit<3000);
    char path[1024];
    snprintf(path,sizeof(path),"%s/%s.pgm",directory,name);
    FILE *file=fopen(path,"wb"); assert(file);
    fprintf(file,"P5\n128 32\n255\n");
    for (unsigned i=0; i<sizeof(framebuffer); ++i) fputc(framebuffer[i]?255:0,file);
    fclose(file);
}
static void data_tests(void)
{
    FiveTratumOledCandidate candidate={0};
    oled_candidate_update(&candidate,false,0,1,UINT32_MAX);
    assert(!candidate.has_variant && !candidate.active);
    for (unsigned i=0;i<100;++i) {
        uint8_t previous=candidate.variant;
        const uint64_t started=UINT64_C(10000000)+i*UINT64_C(1000000);
        oled_candidate_update(&candidate,true,i,started,UINT32_MAX-i);
        assert(candidate.variant<OLED_DONUT_VARIANTS && (!i || candidate.variant!=previous));
        const uint8_t chosen=candidate.variant;
        assert(oled_candidate_frame(&candidate,started)==1);
        assert(oled_candidate_frame(&candidate,started+OLED_CANDIDATE_ANIMATION_US-1)>0);
        assert(oled_candidate_frame(&candidate,started+OLED_CANDIDATE_ANIMATION_US)==0);
        assert(oled_candidate_frame(&candidate,started-1)==0);
        oled_candidate_update(&candidate,true,i,started+1,0);
        assert(candidate.variant==chosen && candidate.started_us==started);
        oled_candidate_update(&candidate,false,i,started+2,0);
        assert(!candidate.active && oled_candidate_frame(&candidate,started+2)==0);
    }
    for (unsigned i=0;i<OLED_DONUT_VARIANTS;++i) {
        assert(oled_donut_variants[i].header.w==26 && oled_donut_variants[i].header.h==26);
        assert(oled_donut_variants[i].header.cf==LV_COLOR_FORMAT_I1 && oled_donut_variants[i].header.stride==4);
        assert(oled_donut_variants[i].data_size==112);
        assert(text_width(oled_candidate_heading(i))<=96);
    }
    FiveTratumOledData d={.initialized=true,.runtime_ready=true,.rate_fresh=true,.rate_gh=1200,.power_w=18.4};
    assert(oled_rate_available(&d));
    d.rate_gh=0; assert(oled_rate_available(&d));
    d.rate_gh=NAN; assert(!oled_rate_available(&d));
    d.rate_gh=1200; d.requested_paused=true;
    assert(!oled_rate_available(&d) && !strcmp(oled_mining_state(&d),"PAUSING"));
    d.applied_paused=true; assert(!strcmp(oled_mining_state(&d),"PAUSED"));
    d.requested_paused=false; assert(!strcmp(oled_mining_state(&d),"RESUMING"));
    d.applied_paused=false; d.runtime_ready=false; assert(!strcmp(oled_mining_state(&d),"STARTING"));
    HashrateDisplaySnapshot sample=hashrate_display_sample(1200,1000000,5999999);
    assert(sample.fresh);
    sample=hashrate_display_sample(1200,1000000,6000000);
    assert(!sample.fresh); /* Exactly five seconds expires a positive rate. */
    d.runtime_ready=true; d.rate_fresh=sample.fresh;
    assert(!oled_rate_available(&d) && !strcmp(oled_mining_state(&d),"NO DATA"));
    assert(!hashrate_display_sample(1200,0,6000000).fresh); /* No computed sample. */
    assert(!hashrate_display_sample(1200,6000001,6000000).fresh); /* Future sample. */
    sample=hashrate_display_sample(0,6000000,6000000);
    assert(sample.fresh); d.rate_gh=sample.rate_gh; d.rate_fresh=sample.fresh;
    assert(oled_rate_available(&d) && !strcmp(oled_mining_state(&d),"MINING"));
    char text[64];
    oled_number(text,sizeof(text),0,"C",1); assert(!strcmp(text,"--C"));
    oled_number(text,sizeof(text),NAN,"W",1); assert(!strcmp(text,"--W"));
    oled_count(text,sizeof(text),UINT64_C(4294967296)); assert(!strcmp(text,"4295.0M"));
    oled_count(text,sizeof(text),UINT64_MAX); assert(strstr(text,"E"));
    d.mux_connected=true; d.mux_acknowledged=true;
    oled_route(text,sizeof(text),&d); assert(!strcmp(text,"P1 5tratMUX"));
    d.fallback=true; oled_route(text,sizeof(text),&d); assert(!strcmp(text,"P2 SV1/TCP"));
    d.mux_fallback=true; d.mux_acknowledged=false; d.mux_expired=true;
    oled_route(text,sizeof(text),&d); assert(!strcmp(text,"P2 5tratMUX stale"));
    d.sv2=true; oled_route(text,sizeof(text),&d); assert(!strcmp(text,"P2 SV2"));
    d.sv2=false; d.mux_connected=false; oled_route(text,sizeof(text),&d); assert(!strstr(text,"MUX"));
    assert(oled_priority_page(true,true,true,true,true)==OLED_OVERHEAT);
    assert(oled_priority_page(true,false,true,false,true)==OLED_FAULT);
    assert(oled_priority_page(true,false,false,false,true)==OLED_SELF_TEST);
    assert(oled_priority_page(true,false,false,true,true)==OLED_SELF_TEST);
    assert(oled_safety_active(false,false,false,true));
    puts("PASS: measured/missing data, 64-bit counters, requested/applied transitions, MUX attribution and safety priority");
}
int main(int argc,char **argv)
{
    assert(argc==2);
    data_tests();
    lv_init();
    lv_display_t *display=lv_display_create(128,32); assert(display);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_I1);
    lv_display_set_buffers(display,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display,flush);
    /* Create every runtime page together to catch the real shared LVGL pool budget. */
    FiveTratumOledView views[OLED_CREDITS+1];
    for (unsigned i=0;i<=OLED_CREDITS;++i) views[i]=oled_create_view(i);
    FiveTratumOledData d={
        .rate_gh=1200,.temperature_c=58.2,.vr_temperature_c=47.1,.power_w=18.4,.fan_percent=80,.fan_rpm=4200,
        .configured_mhz=600,.configured_mv=1130,.core_mv=1128,.input_mv=5200,
        .accepted=4294967296ULL,.rejected=12,.uptime_seconds=187920,.rssi=-58,.block_height=920000,
        .initialized=true,.runtime_ready=true,.rate_fresh=true,.wifi_connected=true,
        .best_session="2.3G",.best_ever="14.8G",.host="pool.example.invalid",.ip="192.0.2.60",
        .ssid="Simulated Wi-Fi",.ap_ssid="5FW_setup",.network_difficulty="120.5T",.scriptsig="/simulated job/",
        .message="Fan tachometer fault",.result="PASS",.finished="Restart to mine",
        .filename="esp-miner.bin",.update_status="Writing 42%",.board_name="Gamma",.board_version="601",
    };
    const char *names[]={"rate","health","shares","route","operating","wifi","job","candidate","fault","overheat","self-test","update","setup","connection","boot","credits"};
    d.hardware_fault=true;
    for (unsigned i=0;i<=OLED_CREDITS;++i) save(argv[1],names[i],&views[i],&d);
    FiveTratumOledView tuning=oled_create_view(OLED_TUNING);
    d.tuning=(GammaAutotuneDisplaySnapshot){.available=true,.supported=true,.telemetryFresh=true,
        .currentAvailable=true,.priority=FWT_EFFICIENCY,.current={625.125,1173},
        .search={.minimumSamples=8,.roundExperimentLimit=24}};
    save(argv[1],"tuning-disabled",&tuning,&d);
    assert(!strcmp(lv_label_get_text(tuning.title),"TUNING OFF"));
    assert(!strcmp(tuning_row(&tuning,0),"EFFICIENCY DISABLED"));
    assert(!strcmp(tuning_row(&tuning,1),"Set 625.125MHz 1173mV"));
    assert(!strcmp(tuning_row(&tuning,2),"Read only - use web"));
    for(unsigned priority=FWT_EFFICIENCY;priority<=FWT_PERFORMANCE;++priority)
        for(unsigned state=FWT_DISABLED;state<=FWT_APPLYING;++state) {
            d.tuning.priority=priority;d.tuning.state=state;
            oled_render_view(&tuning,&d);
            assert(text_width(tuning_row(&tuning,0))<=128);
        }
    d.tuning.priority=FWT_BALANCED;d.tuning.state=FWT_MEASURING;
    d.tuning.active=d.tuning.ownsSettings=true;
    d.tuning.search.timed=true;d.tuning.search.phaseElapsedSeconds=74.9;d.tuning.search.minimumSeconds=75;
    d.tuning.search.samples=7;d.tuning.search.roundExperiments=3;d.tuning.search.rejectedWindows=2;
    save(argv[1],"tuning-measuring",&tuning,&d);
    assert(!strcmp(tuning_row(&tuning,2),"Min 74/75s S7/8"));
    assert(!strcmp(lv_label_get_text(tuning.title),"TUNING ACTIVE"));
    d.uptime_seconds=4;d.tuning.hashRateGHs=1200;d.tuning.powerW=18.4;
    d.tuning.hasDecision=true;d.tuning.latestDecision="Point applied";
    save(argv[1],"tuning-efficiency",&tuning,&d);
    assert(!strcmp(tuning_row(&tuning,1),"1.20TH/s 15.3J/TH"));
    assert(!strcmp(tuning_row(&tuning,2),"Point applied"));
    const double invalid[]={0,-1,NAN,INFINITY};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        d.tuning.hashRateGHs=invalid[i];oled_render_view(&tuning,&d);
        assert(!strstr(tuning_row(&tuning,1),"J/TH"));
        d.tuning.hashRateGHs=1200;d.tuning.powerW=invalid[i];oled_render_view(&tuning,&d);
        assert(!strstr(tuning_row(&tuning,1),"J/TH"));
        d.tuning.powerW=18.4;
    }
    d.tuning.state=FWT_OBSERVING;save(argv[1],"tuning-monitor",&tuning,&d);
    assert(!strcmp(lv_label_get_text(tuning.title),"TUNING MONITOR"));
    d.uptime_seconds=0;d.tuning.hasDecision=false;
    d.tuning.priority=FWT_PERFORMANCE;d.tuning.state=FWT_SETTLING;
    d.tuning.search.phaseElapsedSeconds=12;d.tuning.search.minimumSeconds=30;d.tuning.search.samples=0;
    save(argv[1],"tuning-settling",&tuning,&d);
    d.tuning.state=FWT_HOLDING;d.tuning.active=false;d.tuning.search.timed=false;
    save(argv[1],"tuning-holding",&tuning,&d);
    assert(!strcmp(tuning_row(&tuning,2),"Trial 3/24 Rej2"));
    d.tuning.restoring=true;
    save(argv[1],"tuning-restoring",&tuning,&d);
    assert(!strcmp(tuning_row(&tuning,2),"Read only - use web"));
    d.tuning.fault=true;
    oled_render_view(&tuning,&d);
    assert(strstr(tuning_row(&tuning,0),"BLOCKED")); /* A queued restore cannot hide a protection fault. */
    d.tuning.restoring=false;d.tuning.fault=true;
    save(argv[1],"tuning-blocked",&tuning,&d);
    d.tuning.fault=false;d.tuning.telemetryFresh=false;d.tuning.active=true;
    save(argv[1],"tuning-stale",&tuning,&d);
    assert(!strcmp(tuning_row(&tuning,1),"Set --MHz --mV"));
    d.tuning.available=false;
    save(argv[1],"tuning-unavailable",&tuning,&d);
    assert(!strcmp(tuning_row(&tuning,0),"Tuning unavailable"));
    assert(strstr(lv_label_get_text(views[OLED_CANDIDATE].rows[0]),"CANDIDATE"));
    assert(!strstr(lv_label_get_text(views[OLED_CANDIDATE].rows[2]),"accepted"));
    for (unsigned i=0;i<OLED_DONUT_VARIANTS;++i) {
        char name[32]; snprintf(name,sizeof(name),"candidate-%02u",i+1);
        d.candidate_variant=i; d.candidate_frame=0;
        save(argv[1],name,&views[OLED_CANDIDATE],&d);
    }
    d.candidate_variant=0;
    d.candidate_frame=2; save(argv[1],"candidate-motion-2",&views[OLED_CANDIDATE],&d);
    d.candidate_frame=3; save(argv[1],"candidate-motion-3",&views[OLED_CANDIDATE],&d);
    d.candidate_frame=0;
    FiveTratumOledView identify=oled_create_view(OLED_IDENTIFY);
    save(argv[1],"identify",&identify,&d);
    assert(identify.donut==NULL && !strcmp(lv_label_get_text(identify.title),"IDENTIFY"));
    const char *recovery="Hold BOOT button for 2 seconds to cancel self-test, or press RESET to run self-test again.";
    d.message="ASIC chain detection failed"; d.result="SELF-TEST FAIL!"; d.finished=recovery;
    save(argv[1],"self-test-asic-failure",&views[OLED_SELF_TEST],&d);
    assert(!strcmp(lv_label_get_text(views[OLED_SELF_TEST].rows[2]),recovery));
    d.hardware_fault=false;
    d.mux_connected=true; d.mux_acknowledged=true;
    save(argv[1],"mux-connected",&views[OLED_ROUTE],&d);
    d.mux_acknowledged=false; d.mux_expired=true;
    save(argv[1],"mux-stale",&views[OLED_ROUTE],&d);
    d.fallback=true;
    save(argv[1],"fallback-mux-mismatch",&views[OLED_ROUTE],&d);
    assert(!strstr(lv_label_get_text(views[OLED_ROUTE].rows[0]),"MUX"));
    d.requested_paused=true;
    save(argv[1],"pausing",&views[OLED_RATE],&d);
    assert(!strcmp(lv_label_get_text(views[OLED_RATE].rate),"--"));
    d.applied_paused=true;
    save(argv[1],"paused",&views[OLED_RATE],&d);
    d.requested_paused=false;
    save(argv[1],"resuming",&views[OLED_RATE],&d);
    d.applied_paused=false; d.temperature_c=NAN; d.power_w=0; d.rate_gh=NAN;
    save(argv[1],"unavailable",&views[OLED_RATE],&d);
    assert(!strcmp(lv_label_get_text(views[OLED_RATE].temperature),"--C"));
    save(argv[1],"efficiency-unavailable",&views[OLED_HEALTH],&d);
    assert(!strcmp(lv_label_get_text(views[OLED_HEALTH].rows[2]),"Eff -- J/TH"));
    d.rate_gh=1200; d.temperature_c=58.2; d.power_w=18.4; d.rate_fresh=false;
    save(argv[1],"stale-positive-rate",&views[OLED_RATE],&d);
    assert(!strcmp(lv_label_get_text(views[OLED_RATE].rate),"--"));
    assert(!strcmp(lv_label_get_text(views[OLED_RATE].title),"NO DATA"));
    save(argv[1],"stale-positive-efficiency",&views[OLED_HEALTH],&d);
    assert(!strcmp(lv_label_get_text(views[OLED_HEALTH].rows[2]),"Eff -- J/TH"));
    printf("PASS: 46 actual LVGL9 I1 framebuffer views including ten read-only tuning states, six donuts (%u bitmap bytes), candidate bounds/no repeats/bounded motion, separate identify, self-test recovery and expired positive rate\n",(unsigned)(sizeof(oled_donut_variants)/sizeof(oled_donut_variants[0])*oled_donut_variants[0].data_size));
    lv_mem_monitor_t memory;lv_mem_monitor(&memory);
    printf("LVGL_MEMORY renderer total=%zu peak_used=%zu free=%zu biggest=%zu\n",memory.total_size,memory.max_used,memory.free_size,memory.free_biggest_size);
    lv_deinit();
    return 0;
}
