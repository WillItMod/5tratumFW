#ifndef WORK_DIFFICULTY_API_H
#define WORK_DIFFICULTY_API_H
#include "global_state.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "work_difficulty.h"
cJSON *work_difficulty_report(const WorkDifficultySnapshot *snapshot);
esp_err_t work_difficulty_register(httpd_handle_t server,GlobalState *global);
#endif
