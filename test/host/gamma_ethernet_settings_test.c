/* Actual NVS implementation + extracted production PATCH/identity/GET code.
 * Only SDK storage/queue/lock and network status boundaries are simulated. */
#define main existing_gamma_boot_main
#define nvs_open fixture_original_nvs_open
#define nvs_close fixture_original_nvs_close
#define nvs_set_u16 fixture_original_nvs_set_u16
#define nvs_commit fixture_original_nvs_commit
#define xSemaphoreTake fixture_original_take
#define xSemaphoreGive fixture_original_give
#define xQueueSend fixture_original_queue_send
#include "gamma_boot_test.c"
#undef main
#undef nvs_open
#undef nvs_close
#undef nvs_set_u16
#undef nvs_commit
#undef xSemaphoreTake
#undef xSemaphoreGive
#undef xQueueSend
#include <strings.h>
#include "cJSON.h"
#include "display.h"
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static bool locked, writing_flag, prior_flag, staged;
static uint16_t staged_value;
static unsigned queue_sends, hostname_updates;
static GlobalState fixture_state;
static GlobalState *GLOBAL_STATE = &fixture_state;
static const char *TAG = "ethernet-settings-fixture";

BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, unsigned timeout)
{
    assert(mutex && timeout == portMAX_DELAY && !locked);
    locked = true;
    return pdPASS;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex)
{
    assert(mutex && locked);
    locked = false;
    return pdPASS;
}
static void assert_old_cache(void)
{
    if (writing_flag) {
        assert(locked);
        assert(nvs_config_get_settings(NVS_CONFIG_ETHERNET_ENABLED)->value[0].b == prior_flag);
    }
}
esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *out)
{
    assert_old_cache();
    return fixture_original_nvs_open(name, mode, out);
}
esp_err_t nvs_set_u16(nvs_handle_t writer, const char *key, uint16_t value)
{
    assert(writer == 1 && !strcmp(key, "5fw_eth_en") && value <= 1);
    assert_old_cache();
    ++writes;
    if (set_result != ESP_OK) return set_result;
    staged = true;
    staged_value = value;
    return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t writer)
{
    assert(writer == 1 && staged);
    assert_old_cache();
    ++commits;
    if (commit_result != ESP_OK) return commit_result;
    Entry *entry = find("5fw_eth_en");
    if (!entry) { put("5fw_eth_en", STORE_U16, NULL, staged_value); }
    else { entry->type = STORE_U16; entry->number = staged_value; }
    staged = false;
    return ESP_OK;
}
void nvs_close(nvs_handle_t writer)
{
    assert_old_cache();
    staged = false; /* Uncommitted fake changes are discarded on close. */
    fixture_original_nvs_close(writer);
}
BaseType_t xQueueSend(QueueHandle_t queue, const void *item, unsigned timeout)
{
    assert(queue && item && timeout == portMAX_DELAY);
    ++queue_sends;
    return pdPASS;
}
#define strlcpy fixture_strlcpy
static size_t fixture_strlcpy(char *to, const char *from, size_t count)
{
    size_t length = strlen(from);
    if (count) snprintf(to, count, "%s", from);
    return length;
}
static void normalize_hostname(char *name, size_t size) { (void)name; (void)size; }
static void update_mdns_hostname(const char *name, GlobalState *state)
{ assert(name && state == GLOBAL_STATE); ++hostname_updates; }
enum { STRATUM_PROTOCOL_UNKNOWN, STRATUM_PROTOCOL_V1, SV2_CHANNEL_UNKNOWN = 0 };
static int stratum_protocol_from_string(const char *value)
{ return !strcmp(value,"SV1") || !strcmp(value,"SV2") ? STRATUM_PROTOCOL_V1 : STRATUM_PROTOCOL_UNKNOWN; }
static int sv2_channel_type_from_string(const char *value)
{ return !strcmp(value,"extended") || !strcmp(value,"standard") ? 1 : SV2_CHANNEL_UNKNOWN; }
#define CONFIG_GPIO_BUTTON_BOOT 0
#define CONFIG_GPIO_ASIC_RESET 1
#define CONFIG_GPIO_ASIC_ENABLE 2
#define CONFIG_GPIO_PLUG_SENSE 3
#include "ethernet_settings_production.inc"

typedef struct {
    bool ethernetEnabled, ethernetDriverEnabled;
    const char *ethernetStatus;
    char ethernetIP[16];
    const char *activeNetwork;
} network_info_t;
static network_info_t network_snapshot;
static unsigned status_reads;
static void network_get_info(network_info_t *out) { ++status_reads; *out = network_snapshot; }
#include "ethernet_system_json.inc"

static bool patch(const char *text)
{
    cJSON *json = cJSON_Parse(text);
    assert(json);
    prior_flag = nvs_config_get_bool(NVS_CONFIG_ETHERNET_ENABLED);
    writing_flag = true;
    bool result = check_settings_and_update(json, NULL);
    writing_flag = false;
    assert(!locked && !staged);
    cJSON_Delete(json);
    return result;
}
static void reject(const char *text)
{
    unsigned before_writes = writes, before_commits = commits, before_queue = queue_sends, before_open = open_calls;
    bool old = nvs_config_get_bool(NVS_CONFIG_ETHERNET_ENABLED);
    assert(!patch(text));
    assert(writes == before_writes && commits == before_commits && queue_sends == before_queue && open_calls == before_open);
    assert(nvs_config_get_bool(NVS_CONFIG_ETHERNET_ENABLED) == old && !hostname_updates);
}
static void untouched_non_ethernet(void)
{
    assert_string(NVS_CONFIG_WIFI_SSID,"fixture-network");
    assert_string(NVS_CONFIG_WIFI_PASS,"fixture-passphrase");
    assert_string(NVS_CONFIG_STRATUM_URL,"pool.fixture.invalid");
    assert_string(NVS_CONFIG_STRATUM_USER,"fixture.worker");
    assert_string(NVS_CONFIG_HOSTNAME,"fixture-gamma");
    assert(nvs_config_get_float(NVS_CONFIG_ASIC_FREQUENCY) == 625.125f);
    assert(nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE) == 1173);
    assert(nvs_config_get_u16(NVS_CONFIG_MANUAL_FAN_SPEED) == 77);
    assert(nvs_config_get_u16(NVS_CONFIG_MIN_FAN_SPEED) == 31);
    assert(!queue_sends && !hostname_updates && !erase_calls);
}
int main(int argc, char **argv)
{
    assert(argc == 3);
    const char *scenario = argv[1];
    put("boardversion",STORE_STRING,argv[2],0);
    put("hostname",STORE_STRING,"fixture-gamma",0);
    put("wifissid",STORE_STRING,"fixture-network",0);
    put("wifipass",STORE_STRING,"fixture-passphrase",0);
    put("stratumurl",STORE_STRING,"pool.fixture.invalid",0);
    put("stratumuser",STORE_STRING,"fixture.worker",0);
    put("asicfrequency_f",STORE_STRING,"625.125",0);
    put("asicvoltage",STORE_U16,NULL,1173);
    put("manualfanspeed",STORE_U16,NULL,77);
    put("minfanspeed",STORE_U16,NULL,31);
    if (!strncmp(scenario,"raw-",4)) {
        if (!strcmp(scenario,"raw-string")) put("5fw_eth_en",STORE_STRING,"true",0);
        else put("5fw_eth_en",STORE_U16,NULL,strtoul(scenario+4,NULL,10));
    }
    assert(nvs_config_set_ethernet_enabled(true) == ESP_ERR_INVALID_STATE);
    assert(!writes && !open_calls);
    assert(nvs_config_init() == ESP_OK && device_config_init(GLOBAL_STATE) == ESP_OK);
    assert(!writes && !commits);
    Settings *setting = nvs_config_get_settings(NVS_CONFIG_ETHERNET_ENABLED);
    assert(setting->type == TYPE_BOOL && !strcmp(setting->nvs_key_name,"5fw_eth_en") && !strcmp(setting->rest_name,"ethernetEnabled") && !setting->default_value.b);
    if (!strncmp(scenario,"raw-",4)) {
        assert(nvs_config_get_bool(NVS_CONFIG_ETHERNET_ENABLED) == !strcmp(scenario,"raw-1"));
        untouched_non_ethernet();
    } else if (!strcmp(scenario,"success")) {
        unsigned before = writes, before_commits = commits;
        assert(patch("{\"ethernetEnabled\":true}"));
        assert(nvs_config_get_bool(NVS_CONFIG_ETHERNET_ENABLED) && nvs_config_has_key(NVS_CONFIG_ETHERNET_ENABLED));
        assert(writes == before+1 && commits == before_commits+1 && find("5fw_eth_en")->number == 1);
        untouched_non_ethernet();
        assert(nvs_config_init() == ESP_OK && nvs_config_get_bool(NVS_CONFIG_ETHERNET_ENABLED));
        assert(patch("{\"ethernetEnabled\":false}"));
        assert(!nvs_config_get_bool(NVS_CONFIG_ETHERNET_ENABLED) && find("5fw_eth_en")->number == 0);
        assert(writes == before+2 && commits == before_commits+2);
        assert(nvs_config_init() == ESP_OK && !nvs_config_get_bool(NVS_CONFIG_ETHERNET_ENABLED));
        untouched_non_ethernet();
    } else if (!strcmp(scenario,"reject")) {
        const char *invalid[] = {"null","[]","true","{\"ethernetEnabled\":1}","{\"ethernetEnabled\":0}","{\"ethernetEnabled\":\"true\"}","{\"ethernetEnabled\":null}","{\"ethernetEnabled\":{}}","{\"EthernetEnabled\":true}","{\"ethernetenabled\":true}","{\"ethernetEnabled\":true,\"ethernetEnabled\":false}","{\"ethernetEnabled\":false,\"ETHERNETENABLED\":false}","{\"ethernetEnabled\":true,\"hostname\":\"\"}","{\"hostname\":\"valid-host\",\"frequency\":0,\"ethernetEnabled\":true}","{\"ethernetEnabled\":true,\"display\":\"unsupported\"}","{\"ethernetEnabled\":true,\"pools\":[]}","{\"ethernetEnabled\":true,\"primaryPoolIndex\":0}"};
        for (unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);++i) reject(invalid[i]);
        const char *boards[] = {NULL,"600","603","601x","601\n","NerdQAxe++","NerdOctAxe"};
        for (unsigned i=0;i<sizeof(boards)/sizeof(*boards);++i) { GLOBAL_STATE->DEVICE_CONFIG.board_version=boards[i]; reject("{\"ethernetEnabled\":true}"); reject("{\"ethernetEnabled\":false}"); }
        GLOBAL_STATE->DEVICE_CONFIG.board_version=argv[2];
        GLOBAL_STATE->DEVICE_CONFIG.family.asic.name="BM1368"; reject("{\"ethernetEnabled\":true}");
        GLOBAL_STATE->DEVICE_CONFIG.family.asic.name="BM1370";
        I2cPins conflict={.sda=39,.scl=22}; GLOBAL_STATE->DEVICE_CONFIG.pins.i2c=&conflict;
        reject("{\"ethernetEnabled\":true}");
        assert(patch("{\"ethernetEnabled\":false}")); /* A user can disable a pin conflict. */
        untouched_non_ethernet();
    } else if (!strcmp(scenario,"failure")) {
        for (unsigned old=0;old<2;++old) {
            assert(patch(old ? "{\"ethernetEnabled\":true}" : "{\"ethernetEnabled\":false}"));
            for (unsigned failure=0;failure<3;++failure) {
                unsigned old_writes=writes, old_commits=commits, old_queue=queue_sends;
                write_open_result = failure==0 ? ESP_FAIL : ESP_OK;
                set_result = failure==1 ? ESP_FAIL : ESP_OK;
                commit_result = failure==2 ? ESP_FAIL : ESP_OK;
                assert(!patch(old ? "{\"ethernetEnabled\":false,\"hostname\":\"changed\",\"frequency\":525}" : "{\"ethernetEnabled\":true,\"hostname\":\"changed\",\"frequency\":525}"));
                assert(nvs_config_get_bool(NVS_CONFIG_ETHERNET_ENABLED) == (bool)old);
                assert(writes == old_writes+(failure!=0) && commits == old_commits+(failure==2) && queue_sends==old_queue);
                assert(find("5fw_eth_en")->number==old);
                write_open_result=set_result=commit_result=ESP_OK;
                assert(nvs_config_init()==ESP_OK && nvs_config_get_bool(NVS_CONFIG_ETHERNET_ENABLED)==(bool)old);
                untouched_non_ethernet();
            }
        }
    } else if (!strcmp(scenario,"get")) {
        network_snapshot=(network_info_t){.ethernetEnabled=true,.ethernetDriverEnabled=false,.ethernetStatus="restart-required",.ethernetIP="192.0.2.42",.activeNetwork="wifi"};
        cJSON *json=cJSON_CreateObject(); system_ethernet_json(json,GLOBAL_STATE);
        assert(cJSON_GetArraySize(json)==5 && status_reads==1);
        assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json,"ethernetEnabled")));
        assert(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(json,"ethernetDriverEnabled")));
        assert(!strcmp(cJSON_GetObjectItemCaseSensitive(json,"ethernetStatus")->valuestring,"restart-required"));
        assert(!strcmp(cJSON_GetObjectItemCaseSensitive(json,"ethernetIP")->valuestring,"192.0.2.42"));
        assert(!strcmp(cJSON_GetObjectItemCaseSensitive(json,"activeNetwork")->valuestring,"wifi"));
        cJSON_Delete(json); GLOBAL_STATE->DEVICE_CONFIG.board_version="603";
        json=cJSON_CreateObject();system_ethernet_json(json,GLOBAL_STATE);assert(cJSON_GetArraySize(json)==0 && status_reads==1);cJSON_Delete(json);
        untouched_non_ethernet(); assert(!writes && !commits);
    } else assert(!"unknown test scenario");
    puts("PASS");
    return 0;
}
