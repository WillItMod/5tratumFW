#pragma once
/* Public API doubles for the production network policy; no peripheral access. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <arpa/inet.h>
typedef int esp_err_t;
enum { ESP_OK=0, ESP_FAIL=-1, ESP_ERR_INVALID_STATE=1, ESP_ERR_NOT_FOUND=2, ESP_ERR_NO_MEM=3 };
#include "device_pins.h"
typedef struct { uint32_t addr; } esp_ip4_addr_t;
typedef struct { uint32_t addr[4]; } esp_ip6_addr_t;
typedef struct { esp_ip4_addr_t ip, netmask, gw; } esp_netif_ip_info_t;
typedef struct { bool up, ip6; int index; esp_netif_ip_info_t ip; } esp_netif_t;
typedef int wifi_auth_mode_t;
typedef struct { int8_t rssi; } wifi_ap_record_t;
typedef const char *esp_event_base_t;
typedef struct { int reason, ssid_len, rssi; char ssid[33]; } wifi_event_sta_disconnected_t;
typedef struct { esp_netif_t *esp_netif; esp_netif_ip_info_t ip_info; } ip_event_got_ip_t;
typedef struct { esp_netif_t *esp_netif; struct { esp_ip6_addr_t ip; } ip6_info; } ip_event_got_ip6_t;
typedef struct GlobalState {
    struct { const char *board_version; struct { struct { const char *name; } asic; } family;
        DevicePins pins; bool asic_enable, plug_sense; } DEVICE_CONFIG;
    struct { const char *ssid; bool is_connected, ap_enabled; char ip_addr_str[16], ipv6_addr_str[64], wifi_status[256]; } SYSTEM_MODULE;
    unsigned frequency, voltage, fan;
    const char *pool;
} GlobalState;
typedef uint32_t TickType_t;
typedef void *TaskHandle_t;
typedef void *TimerHandle_t;
typedef int BaseType_t;
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portTICK_PERIOD_MS 1
#define WIFI_EVENT ((esp_event_base_t)1)
#define IP_EVENT ((esp_event_base_t)2)
enum { WIFI_EVENT_SCAN_DONE=1, WIFI_EVENT_STA_START, WIFI_EVENT_STA_CONNECTED, WIFI_EVENT_STA_DISCONNECTED,
       WIFI_EVENT_AP_START, WIFI_EVENT_AP_STOP, WIFI_EVENT_AP_STACONNECTED, WIFI_EVENT_AP_STADISCONNECTED,
       IP_EVENT_STA_LOST_IP, IP_EVENT_STA_GOT_IP, IP_EVENT_GOT_IP6, WIFI_REASON_ROAMING=200 };
enum { NVS_CONFIG_ETHERNET_ENABLED=1, NVS_CONFIG_HOSTNAME=2 };
typedef unsigned mdns_event_actions_t;
enum { MDNS_EVENT_ENABLE_IP4=1<<1, MDNS_EVENT_ENABLE_IP6=1<<2,
       MDNS_EVENT_ANNOUNCE_IP4=1<<3,
       MDNS_EVENT_DISABLE_IP4=1<<5, MDNS_EVENT_DISABLE_IP6=1<<6 };
#define MAX_AP_COUNT 20
#define IP4ADDR_STRLEN_MAX 16
#define IPSTR "%u.%u.%u.%u"
#define IP2STR(ip) ((const uint8_t*)&(ip)->addr)[0],((const uint8_t*)&(ip)->addr)[1],((const uint8_t*)&(ip)->addr)[2],((const uint8_t*)&(ip)->addr)[3]
#define ESP_LOGI(tag,...) ((void)(tag))
#define ESP_LOGW ESP_LOGI
#define ESP_LOGE ESP_LOGI
#define ESP_ERROR_CHECK(value) assert((value)==ESP_OK)
#ifndef CONFIG_GPIO_BUTTON_BOOT
#define CONFIG_GPIO_BUTTON_BOOT 0
#endif
#ifndef CONFIG_GPIO_ASIC_RESET
#define CONFIG_GPIO_ASIC_RESET 1
#endif
#ifndef CONFIG_GPIO_ASIC_ENABLE
#define CONFIG_GPIO_ASIC_ENABLE 10
#endif
#ifndef CONFIG_GPIO_PLUG_SENSE
#define CONFIG_GPIO_PLUG_SENSE 12
#endif
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *);
bool esp_netif_is_netif_up(esp_netif_t *);
esp_err_t esp_netif_get_ip_info(esp_netif_t *,esp_netif_ip_info_t *);
esp_err_t esp_netif_set_default_netif(esp_netif_t *);
esp_err_t esp_netif_create_ip6_linklocal(esp_netif_t *);
esp_err_t esp_netif_get_ip6_linklocal(esp_netif_t *,esp_ip6_addr_t *);
int esp_netif_get_netif_impl_index(esp_netif_t *);
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *);
esp_err_t esp_wifi_connect(void);
esp_err_t esp_wifi_disconnect(void);
esp_err_t esp_wifi_scan_get_ap_num(uint16_t *);
esp_err_t esp_wifi_scan_get_ap_records(uint16_t *,wifi_ap_record_t *);
TickType_t xTaskGetTickCount(void);
BaseType_t xTaskCreate(void (*)(void *),const char *,unsigned,void *,unsigned,TaskHandle_t *);
void xTaskNotifyGive(TaskHandle_t);
unsigned ulTaskNotifyTake(int,TickType_t);
void vTaskDelay(TickType_t);
TimerHandle_t xTimerCreate(const char *,TickType_t,int,void *,void (*)(TimerHandle_t));
void *pvTimerGetTimerID(TimerHandle_t);
int xTimerStart(TimerHandle_t,unsigned);
int xTimerStop(TimerHandle_t,unsigned);
bool nvs_config_get_bool(int);
char *nvs_config_get_string(int);
const char *esp_err_to_name(esp_err_t);
esp_err_t mdns_register_netif(esp_netif_t *);
esp_err_t mdns_netif_action(esp_netif_t *,mdns_event_actions_t);
void protocol_coordinator_request_network_reconnect(void);
size_t strlcpy(char *,const char *,size_t);
