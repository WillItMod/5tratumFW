#include "gamma_network_runtime_stubs.h"
#include "connect.h"
#include "gamma_ethernet.h"
static const char *TAG="network-test";
static void wifi_softap_on(void), wifi_softap_off(void), spawn_mdns_init_if_needed(GlobalState *);
static void mdns_init_task(void *arg) { (void)arg; assert(!"Init task execution is outside this host fixture"); }
static const char *get_wifi_reason_string(int);
/* State, event handler, readiness/route/mDNS policy and startup are unmodified
 * production source. The runner extracts complete functions, never rewrites them. */
#include "network_policy.inc"

static GlobalState *owner;
static esp_netif_t sta, eth, ap;
static gamma_ethernet_status_t hardware;
static bool associated, saved, fail_task, have_eth, fail_mdns;
static bool fail_mdns_task, reenter_mdns;
static esp_err_t start_error;
static TickType_t now;
static unsigned starts, stops, tasks, notifications, delays, connects, disconnects;
static unsigned ap_on, ap_off, reconnects, mdns_spawns, mdns_registers, mdns_actions[5];
static esp_netif_t *default_netif;
static const BapPins bap={.tx=39,.rx=40};
static const I2cPins i2c={.sda=47,.scl=48};
static void wifi_softap_on(void) { ++ap_on; owner->SYSTEM_MODULE.ap_enabled=true; }
static void wifi_softap_off(void) { ++ap_off; owner->SYSTEM_MODULE.ap_enabled=false; }
static const char *get_wifi_reason_string(int reason) { (void)reason; return "Fixture disconnect"; }
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key)
{ if(!strcmp(key,"WIFI_STA_DEF"))return &sta; assert(!strcmp(key,"WIFI_AP_DEF"));return &ap; }
bool esp_netif_is_netif_up(esp_netif_t *n) { return n->up; }
esp_err_t esp_netif_get_ip_info(esp_netif_t *n,esp_netif_ip_info_t *v) { *v=n->ip;return ESP_OK; }
esp_err_t esp_netif_set_default_netif(esp_netif_t *n) { default_netif=n;return ESP_OK; }
esp_err_t esp_netif_create_ip6_linklocal(esp_netif_t *n) { assert(n);return ESP_OK; }
esp_err_t esp_netif_get_ip6_linklocal(esp_netif_t *n,esp_ip6_addr_t *a) { memset(a,0,sizeof(*a));return n->ip6?ESP_OK:ESP_FAIL; }
int esp_netif_get_netif_impl_index(esp_netif_t *n) { return n->index; }
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *r) { r->rssi=-58;return associated?ESP_OK:ESP_FAIL; }
esp_err_t esp_wifi_connect(void) { ++connects;return ESP_OK; }
esp_err_t esp_wifi_disconnect(void) { ++disconnects;associated=false;sta.up=false;sta.ip.ip.addr=0;return ESP_OK; }
esp_err_t esp_wifi_scan_get_ap_num(uint16_t *n) { *n=0;return ESP_OK; }
esp_err_t esp_wifi_scan_get_ap_records(uint16_t *n,wifi_ap_record_t *r) { (void)n;(void)r;return ESP_OK; }
TickType_t xTaskGetTickCount(void) { return now; }
BaseType_t xTaskCreate(void (*fn)(void *),const char *name,unsigned stack,void *arg,unsigned priority,TaskHandle_t *out)
{
    assert(stack>=4096 && arg==owner && priority);
    if(!strcmp(name,"mdns_init")) {
        assert(fn==mdns_init_task && !out);++mdns_spawns;
        if(reenter_mdns) { reenter_mdns=false;spawn_mdns_init_if_needed(owner); }
        return fail_mdns_task?0:pdPASS;
    }
    assert(fn==network_monitor_task && !strcmp(name,"network_status"));++tasks;
    if (fail_task) return 0;
    *out = (void *)1;
    return pdPASS;
}
void xTaskNotifyGive(TaskHandle_t t) { assert(t==(void*)1);++notifications; }
unsigned ulTaskNotifyTake(int clear,TickType_t wait) { (void)clear;(void)wait;assert(!"Infinite task must not run in host fixture");return 0; }
void vTaskDelay(TickType_t ticks) { delays+=ticks; }
TimerHandle_t xTimerCreate(const char *name,TickType_t wait,int repeat,void *arg,void (*fn)(TimerHandle_t))
{ assert(name && wait==30000 && !repeat && arg==owner && fn==ip_timeout_callback);return owner; }
void *pvTimerGetTimerID(TimerHandle_t t) { return t; }
int xTimerStart(TimerHandle_t t,unsigned wait) { (void)t;(void)wait;return 1; }
int xTimerStop(TimerHandle_t t,unsigned wait) { (void)t;(void)wait;return 1; }
bool nvs_config_get_bool(int key) { assert(key==NVS_CONFIG_ETHERNET_ENABLED);return saved; }
char *nvs_config_get_string(int key) { assert(key==NVS_CONFIG_HOSTNAME);return strdup("fixture-miner"); }
const char *esp_err_to_name(esp_err_t e) { (void)e;return "fixture-error"; }
esp_err_t gamma_ethernet_start(const char *hostname) { assert(!strcmp(hostname,"fixture-miner"));++starts;return start_error; }
esp_err_t gamma_ethernet_stop(void) { ++stops;memset(&hardware,0,sizeof(hardware));have_eth=false;return ESP_OK; }
void gamma_ethernet_get_status(gamma_ethernet_status_t *v) { *v=hardware; }
esp_netif_t *gamma_ethernet_netif(void) { return have_eth?&eth:NULL; }
esp_err_t mdns_register_netif(esp_netif_t *n) { assert(n==&eth);++mdns_registers;return ESP_OK; }
esp_err_t mdns_netif_action(esp_netif_t *n,mdns_event_actions_t action)
{ assert(n==&eth && (action==MDNS_EVENT_ENABLE_IP4 || action==MDNS_EVENT_ENABLE_IP6 || action==MDNS_EVENT_DISABLE_IP4 || action==MDNS_EVENT_DISABLE_IP6 || action==MDNS_EVENT_ANNOUNCE_IP4));unsigned i=action==MDNS_EVENT_ENABLE_IP4?0:action==MDNS_EVENT_ENABLE_IP6?1:action==MDNS_EVENT_DISABLE_IP4?2:action==MDNS_EVENT_DISABLE_IP6?3:4;++mdns_actions[i];if(fail_mdns){fail_mdns=false;return ESP_FAIL;}return ESP_OK; }
void protocol_coordinator_request_network_reconnect(void) { ++reconnects; }
#if !defined(__APPLE__)
size_t strlcpy(char *out,const char *in,size_t cap) { size_t n=strlen(in);if(cap){size_t size=n<cap-1?n:cap-1;memcpy(out,in,size);out[size]=0;}return n; }
#endif
static void wifi_ready(void) { associated=true;sta.up=true;sta.ip.ip.addr=inet_addr("192.0.2.10");s_wifi_has_ip=true; }
static void eth_ready(void)
{ have_eth=true;eth.up=true;eth.ip.ip.addr=inet_addr("192.0.2.20");hardware=(gamma_ethernet_status_t){.started=true,.available=true,.link_up=true,.has_ip=true,.ip=eth.ip.ip}; }
static void managed(void) { s_ethernet_enabled_at_boot=true;s_network_managed=true;s_network_task=(void*)1; }
static void got_wifi_ip(void)
{ ip_event_got_ip_t e={.esp_netif=&sta,.ip_info=sta.ip};event_handler(owner,IP_EVENT,IP_EVENT_STA_GOT_IP,&e); }
static void disconnected(int reason)
{ wifi_event_sta_disconnected_t e={.reason=reason};event_handler(owner,WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,&e); }
static void unchanged(void)
{ assert(owner->frequency==500 && owner->voltage==1130 && owner->fan==80 && !strcmp(owner->pool,"fixture.invalid")); }

int main(int argc,char **argv)
{
    assert(argc==2);
    GlobalState state={.DEVICE_CONFIG={.board_version="601",.family={.asic={.name="BM1370"}},.pins={.bap=&bap,.i2c=&i2c},.asic_enable=true,.plug_sense=true},
        .SYSTEM_MODULE={.ssid="Saved network",.ap_enabled=true},.frequency=500,.voltage=1130,.fan=80,.pool="fixture.invalid"};
    owner=&state;sta.index=1;eth.index=3;ap.index=2;ap.up=true;now=10000;
    const char *scenario=argv[1];
    if(!strcmp(scenario,"off")) {
        network_start_ethernet(owner);assert(starts==0 && tasks==0 && stops==0 && !network_ethernet_uses_accessory_port(owner));
        saved=true;network_info_t info;network_get_info(&info);
        assert(info.ethernetEnabled && !info.ethernetDriverEnabled && !strcmp(info.ethernetStatus,"disabled"));
        wifi_ready();got_wifi_ip();assert(owner->SYSTEM_MODULE.is_connected && network_is_connected() && ap_off==1);
    } else if(!strcmp(scenario,"self-test-reservation")) {
        // Self-test skips wifi_init; read-only boot capture must still reserve
        // the physically connected adapter's shared pins without starting it.
        network_prepare_boot(owner);assert(!network_ethernet_uses_accessory_port(owner));
        saved=true;network_prepare_boot(owner);assert(network_ethernet_uses_accessory_port(owner));
        assert(starts==0 && stops==0 && tasks==0 && !s_network_managed);
        saved=false;assert(network_ethernet_uses_accessory_port(owner)); // Saved changes wait for next boot.
        owner->DEVICE_CONFIG.board_version="603";saved=true;network_prepare_boot(owner);
        assert(!network_ethernet_uses_accessory_port(owner) && !starts && !tasks);
    } else if(!strcmp(scenario,"pins")) {
        assert(network_ethernet_supported(owner));owner->DEVICE_CONFIG.board_version="602";assert(network_ethernet_supported(owner));
        owner->DEVICE_CONFIG.board_version="603";assert(!network_ethernet_supported(owner));owner->DEVICE_CONFIG.board_version="601";
        bool reserved=CONFIG_GPIO_BUTTON_BOOT==41 || CONFIG_GPIO_ASIC_RESET==41 || CONFIG_GPIO_ASIC_ENABLE==41 || CONFIG_GPIO_PLUG_SENSE==41;
        assert(network_ethernet_pins_available(owner)==!reserved);
        I2cPins bad={.sda=39,.scl=48};owner->DEVICE_CONFIG.pins.i2c=&bad;assert(!network_ethernet_pins_available(owner));owner->DEVICE_CONFIG.pins.i2c=&i2c;
        I80Pins lcd={0};owner->DEVICE_CONFIG.pins.i80=&lcd;assert(!network_ethernet_pins_available(owner));owner->DEVICE_CONFIG.pins.i80=NULL;
        managed();assert(network_ethernet_uses_accessory_port(owner));
    } else if(!strcmp(scenario,"absent")) {
        managed();start_error=ESP_ERR_NOT_FOUND;network_start_ethernet(owner);assert(starts==1 && tasks==1);
        wifi_ready();got_wifi_ip();assert(!owner->SYSTEM_MODULE.is_connected && notifications==1 && ap_off==0);
        network_refresh(owner);assert(owner->SYSTEM_MODULE.is_connected && default_netif==&sta && !owner->SYSTEM_MODULE.ap_enabled);
        network_info_t info;network_get_info(&info);assert(!strcmp(info.ethernetStatus,"not-detected") && !strcmp(info.activeNetwork,"wifi"));
    } else if(!strcmp(scenario,"empty-ssid")) {
        managed();owner->SYSTEM_MODULE.ssid="";eth_ready();network_refresh(owner);
        assert(owner->SYSTEM_MODULE.is_connected && !owner->SYSTEM_MODULE.ap_enabled && default_netif==&eth && connects==0 && reconnects==0);
        assert(!strcmp(owner->SYSTEM_MODULE.ip_addr_str,"192.0.2.20") && !strcmp(owner->SYSTEM_MODULE.wifi_status,"Ethernet connected"));
        assert(network_is_connected() && network_active_netif()==&eth && !wifi_is_connected());
    } else if(!strcmp(scenario,"retire-wifi")) {
        managed();wifi_ready();network_refresh(owner);assert(default_netif==&sta && reconnects==0);
        eth_ready();network_refresh(owner);assert(default_netif==&eth && reconnects==1 && disconnects==1 && !associated && !sta.up);
        unsigned modes=ap_on+ap_off;disconnected(1);assert(notifications==1 && delays==0 && ap_on+ap_off==modes);
        network_refresh(owner);assert(reconnects==1 && network_active_netif()==&eth && !owner->SYSTEM_MODULE.ap_enabled && ap_on+ap_off==modes);
    } else if(!strcmp(scenario,"recovery")) {
        managed();eth_ready();network_refresh(owner);hardware.link_up=hardware.has_ip=false;eth.up=false;
        network_refresh(owner);assert(!owner->SYSTEM_MODULE.is_connected && owner->SYSTEM_MODULE.ap_enabled && reconnects==1 && connects==1);
        assert(!strcmp(owner->SYSTEM_MODULE.wifi_status,"Waiting for network"));
        wifi_ready();got_wifi_ip();assert(!owner->SYSTEM_MODULE.is_connected);network_refresh(owner);
        assert(owner->SYSTEM_MODULE.is_connected && default_netif==&sta && !owner->SYSTEM_MODULE.ap_enabled && reconnects==1);
    } else if(!strcmp(scenario,"readdress")) {
        managed();eth_ready();mdns_initialized=true;network_refresh(owner);
        strcpy(owner->SYSTEM_MODULE.ipv6_addr_str,"fe80::1234%3");
        eth.ip.ip.addr=inet_addr("192.0.2.21");hardware.ip=eth.ip.ip;
        network_refresh(owner);assert(reconnects==1 && !strcmp(owner->SYSTEM_MODULE.ip_addr_str,"192.0.2.21"));
        assert(!strcmp(owner->SYSTEM_MODULE.ipv6_addr_str,"fe80::1234%3"));
        assert(mdns_actions[0]==1 && mdns_actions[4]==1);
        network_refresh(owner);assert(reconnects==1 && mdns_actions[4]==1);
        eth.ip.ip.addr=inet_addr("192.0.2.22");hardware.ip=eth.ip.ip;fail_mdns=true;
        network_refresh(owner);assert(reconnects==2 && mdns_actions[4]==2);
        network_refresh(owner);assert(reconnects==2 && mdns_actions[4]==3);
        network_refresh(owner);assert(reconnects==2 && mdns_actions[4]==3);
    } else if(!strcmp(scenario,"readiness")) {
        managed();wifi_ready();sta.ip.ip.addr=0;assert(!network_is_connected());sta.ip.ip.addr=inet_addr("192.0.2.10");sta.up=false;assert(!network_is_connected());
        sta.up=true;associated=false;assert(!network_is_connected());associated=true;assert(network_active_netif()==&sta);
    } else if(!strcmp(scenario,"stale-eth-status")) {
        managed();wifi_ready();eth_ready();eth.up=false;network_info_t info;network_get_info(&info);
        assert(network_active_netif()==&sta && !strcmp(info.activeNetwork,"wifi"));
    } else if(!strcmp(scenario,"task-failure")) {
        managed();s_network_task=NULL;wifi_ready();got_wifi_ip();assert(!owner->SYSTEM_MODULE.is_connected);
        have_eth=true;fail_task=true;network_start_ethernet(owner);
        assert(stops==1 && !s_network_managed && owner->SYSTEM_MODULE.is_connected && !owner->SYSTEM_MODULE.ap_enabled);
        assert(network_ethernet_uses_accessory_port(owner)); // Boot reservation survives failed task/SPI uncertainty.
        associated=false;sta.up=false;sta.ip.ip.addr=0;disconnected(1);
        assert(owner->SYSTEM_MODULE.ap_enabled && delays==5000 && connects==1 && !owner->SYSTEM_MODULE.is_connected);
    } else if(!strcmp(scenario,"ap-client")) {
        managed();clients_connected_to_ap=1;network_refresh(owner);assert(connects==0 && !strcmp(owner->SYSTEM_MODULE.wifi_status,"Config AP connected!"));
        clients_connected_to_ap=0;network_refresh(owner);assert(connects==1);now+=100;network_refresh(owner);assert(connects==1);
    } else if(!strcmp(scenario,"ap-stop-client")) {
        managed();event_handler(owner,WIFI_EVENT,WIFI_EVENT_AP_STACONNECTED,NULL);assert(clients_connected_to_ap==1);
        event_handler(owner,WIFI_EVENT,WIFI_EVENT_AP_STOP,NULL);assert(clients_connected_to_ap==0);
        event_handler(owner,WIFI_EVENT,WIFI_EVENT_AP_STADISCONNECTED,NULL);assert(clients_connected_to_ap==0);
        network_refresh(owner);assert(connects==1);
    } else if(!strcmp(scenario,"scan-ap-stop")) {
        managed();event_handler(owner,WIFI_EVENT,WIFI_EVENT_AP_STACONNECTED,NULL);
        assert(clients_connected_to_ap==1 && owner->SYSTEM_MODULE.ap_enabled);
        is_scanning=true;
        event_handler(owner,WIFI_EVENT,WIFI_EVENT_AP_STOP,NULL);
        assert(clients_connected_to_ap==0 && !owner->SYSTEM_MODULE.ap_enabled && is_scanning);
        // A late client event during the same scan cannot resurrect the count.
        event_handler(owner,WIFI_EVENT,WIFI_EVENT_AP_STADISCONNECTED,NULL);
        assert(clients_connected_to_ap==0);
        network_refresh(owner);assert(connects==1);
    } else if(!strcmp(scenario,"roaming")) {
        wifi_ready();owner->SYSTEM_MODULE.is_connected=true;disconnected(WIFI_REASON_ROAMING);
        assert(s_wifi_has_ip && owner->SYSTEM_MODULE.is_connected && !notifications && !delays && !ap_on && !connects);
        managed();disconnected(WIFI_REASON_ROAMING);assert(s_wifi_has_ip && !notifications && !delays);
    } else if(!strcmp(scenario,"events")) {
        managed();eth_ready();network_refresh(owner);unsigned modes=ap_off;
        disconnected(1);assert(!delays && notifications==1 && ap_off==modes);
        ip_event_got_ip6_t e={.esp_netif=&sta};strcpy(owner->SYSTEM_MODULE.ipv6_addr_str,"retained-active");
        event_handler(owner,IP_EVENT,IP_EVENT_GOT_IP6,&e);assert(!strcmp(owner->SYSTEM_MODULE.ipv6_addr_str,"retained-active"));
        event_handler(owner,IP_EVENT,IP_EVENT_STA_LOST_IP,NULL);assert(notifications==2 && owner->SYSTEM_MODULE.is_connected);
    } else if(!strcmp(scenario,"mdns")) {
        managed();eth_ready();network_refresh(owner);assert(mdns_registers==0 && mdns_spawns==1);
        mdns_initialized=true;fail_mdns=true;network_refresh(owner);assert(mdns_registers==1 && mdns_actions[0]==1 && !s_mdns_ethernet_ip4);
        network_refresh(owner);assert(mdns_registers==1 && mdns_actions[0]==2 && s_mdns_ethernet_ip4);
        eth.ip6=true;network_refresh(owner);assert(mdns_actions[1]==1);
        hardware.link_up=hardware.has_ip=false;eth.up=false;network_refresh(owner);assert(mdns_actions[2]==1 && mdns_actions[3]==1);
        eth_ready();eth.ip6=true;network_refresh(owner);assert(mdns_registers==1 && mdns_actions[0]==3 && mdns_actions[1]==2);
    } else if(!strcmp(scenario,"mdns-start-guard")) {
        fail_mdns_task=true;spawn_mdns_init_if_needed(owner);
        assert(mdns_spawns==1 && !mdns_init_in_progress);
        fail_mdns_task=false;reenter_mdns=true;spawn_mdns_init_if_needed(owner);
        assert(mdns_spawns==2 && mdns_init_in_progress && !reenter_mdns);
        spawn_mdns_init_if_needed(owner);assert(mdns_spawns==2);
        mdns_initialized=true;mdns_init_in_progress=false;
        spawn_mdns_init_if_needed(owner);assert(mdns_spawns==2 && !mdns_init_in_progress);
    } else if(!strcmp(scenario,"saved-and-diagnostic")) {
        managed();eth_ready();hardware.last_error=ESP_ERR_NO_MEM;network_info_t info;network_get_info(&info);
        assert(!info.ethernetEnabled && info.ethernetDriverEnabled && !strcmp(info.ethernetStatus,"connected") && !strcmp(info.activeNetwork,"ethernet"));
        assert(network_is_connected() && network_ethernet_uses_accessory_port(owner));
        hardware.started=false;hardware.has_ip=false;network_get_info(&info);assert(!strcmp(info.ethernetStatus,"error"));
    } else assert(!"Unknown scenario");
    unchanged();puts("PASS");return 0;
}
