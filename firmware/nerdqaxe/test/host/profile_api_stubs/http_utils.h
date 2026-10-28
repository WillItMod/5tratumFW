#pragma once
#include <ArduinoJson.h>
#include "esp_http_server.h"
#include "sdkconfig.h"
#include "nvs_config.h"
extern httpd_handle_t http_server;
struct Board {enum class Error {NONE,TEMP_FAULT};int getAbsMaxAsicFrequency(){return 800;}int getAbsMinAsicVoltageMillis(){return 1005;}int getAbsMaxAsicVoltageMillis(){return 1400;}int getDefaultAsicFrequency(){return 500;}int getAsicFrequency(){return Config::getAsicFrequency(500);}int getAsicVoltageMillis(){return Config::getAsicVoltage(1130);}const char *getDeviceModel(){return "NerdQAxe++";}const char *getAsicModel(){return "BM1370";}int getAsicCount(){return 4;}void loadSettings(){++loads;}int loads=0;bool initialized=true,buckInitialized=true,shutdown=false;bool isInitialized(){return initialized;}bool isBuckInitialized(){return buckInitialized;}bool isShutdown(){return shutdown;}};
struct System {Board board;Board::Error error=Board::Error::NONE;Board *getBoard(){return &board;}Board::Error getBoardError(){return error;}};extern System SYSTEM_MODULE;
struct StratumManager {int loads=0;void loadSettings(){++loads;}};extern StratumManager *STRATUM_MANAGER;
struct ConGuard {ConGuard(int,httpd_req_t *){}};
inline int getJsonData(httpd_req_t *r,JsonDocument &d){return deserializeJson(d,r->body)?ESP_FAIL:ESP_OK;}
inline int sendJsonResponse(httpd_req_t *r,JsonDocument &d){r->response.clear();serializeJson(d,r->response);return ESP_OK;}
inline int validateOTP(httpd_req_t *r){return r->otp?ESP_OK:httpd_resp_send_err(r,401,"OTP required");}
