#pragma once
#include <ArduinoJson.h>
#include "esp_http_server.h"
namespace FiveTratumAutotune {
// Power owner only, recursive hardware mutex held. HTTP reads never poll hardware.
void tick();
bool ownsSettings();
// Short nonlocking guard for a power-owner PLL/PMBus transition.
bool transitionAllowed();
// Power owner only; strict acquisition outside the one-frame write gate.
bool transitionHealthy();
// Immediate revocation, including while the HTTP OTA owner holds the power lock.
void externalHandoff(const char *reason);
struct FirmwareUpdateGuard { FirmwareUpdateGuard(); ~FirmwareUpdateGuard(); };
bool report(JsonDocument &document);
}
esp_err_t GET_five_tratum_autotune(httpd_req_t *req);
esp_err_t POST_five_tratum_autotune(httpd_req_t *req);
