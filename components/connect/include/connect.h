#ifndef CONNECT_H_
#define CONNECT_H_

#include "lwip/sys.h"
#include <arpa/inet.h>
#include <lwip/netdb.h>
#include <stdbool.h>

#include "esp_err.h"
#include "esp_wifi_types.h"
#include "esp_netif.h"

typedef struct GlobalState GlobalState;

// Structure to hold WiFi scan results
typedef struct {
    char ssid[33];  // 32 chars + null terminator
    int8_t rssi;
    wifi_auth_mode_t authmode;
} wifi_ap_record_simple_t;

void toggle_wifi_softap(void);
void wifi_init(GlobalState * GLOBAL_STATE);
esp_err_t wifi_apply_hostname(const char *hostname);
esp_err_t wifi_scan(wifi_ap_record_simple_t *ap_records, uint16_t *ap_count);
esp_err_t get_wifi_current_rssi(int8_t *rssi);
bool wifi_is_connected(void);
// IPv4 readiness of either configured uplink, independent of Wi-Fi association.
bool network_is_connected(void);
esp_netif_t *network_active_netif(void);
bool network_ethernet_supported(const GlobalState *state);
// Capture saved opt-in before peripherals/self-test; performs no hardware I/O.
void network_prepare_boot(const GlobalState *state);
bool network_ethernet_pins_available(const GlobalState *state);
bool network_ethernet_uses_accessory_port(const GlobalState *state);
typedef struct {
    bool ethernetEnabled;
    bool ethernetDriverEnabled;
    const char *ethernetStatus;
    char ethernetIP[16];
    const char *activeNetwork;
} network_info_t;
void network_get_info(network_info_t *info);
esp_err_t update_mdns_hostname(const char *new_hostname, GlobalState *GLOBAL_STATE);

#endif /* CONNECT_H_ */
