/* Original 5tratumFW optional Ethernet interface. GPL-3.0-or-later. */
#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool started;
    bool available;
    bool link_up;
    bool has_ip;
    esp_ip4_addr_t ip;
    esp_err_t last_error;
} gamma_ethernet_status_t;

/* No initialization or GPIO access occurs before start. The caller must first
 * establish the supported Gamma identity, enable Ethernet explicitly, exclude
 * the BAP UART sharing GPIO39/40, and initialize esp_netif/the default event loop.
 * Start/stop must be serialized by the management task. */
esp_err_t gamma_ethernet_start(const char *hostname);
esp_err_t gamma_ethernet_stop(void);
/* Concurrent-safe snapshot; a lost cable immediately invalidates cached IP. */
void gamma_ethernet_get_status(gamma_ethernet_status_t *status);
/* Valid until the next serialized stop; no sticky default-route override. */
esp_netif_t *gamma_ethernet_netif(void);

#ifdef __cplusplus
}
#endif
