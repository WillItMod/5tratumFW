#pragma once
#include <ArduinoJson.h>
#include "esp_http_server.h"
#include "display_snapshot.h"
namespace FiveTratumAutotune {
// Once on app_main's internal stack, before either PSRAM power-task start.
// Failure disables tuning storage, never ordinary mining or recovery.
bool preloadStorage();
// RAM-only lifecycle notification after the power owner initializes its fans.
void powerReady();
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
// Passive, nonblocking cache projection; never initializes the runtime.
bool activity(JsonDocument &document);
// Nonblocking RAM-only copy. A busy or uninitialized controller returns no data;
// this never initializes storage, polls hardware or waits for the power owner.
bool tryDisplaySnapshot(DisplaySnapshot &snapshot);
}
esp_err_t GET_five_tratum_autotune(httpd_req_t *req);
esp_err_t GET_five_tratum_autotune_activity(httpd_req_t *req);
esp_err_t POST_five_tratum_autotune(httpd_req_t *req);
