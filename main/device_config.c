#include <string.h>
#include <stdlib.h>
#include "device_config.h"
#include "nvs_config.h"
#include "global_state.h"
#include "esp_log.h"
#include "5tratumfw.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static const char * TAG = "device_config";

esp_err_t device_config_init(void * pvParameters)
{
    GlobalState * GLOBAL_STATE = (GlobalState *) pvParameters;

    // TODO: Read board version from eFuse

    char * board_version = nvs_config_get_string(NVS_CONFIG_BOARD_VERSION);

    if (board_version == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (!five_stratum_fw_board_supported(board_version)) {
        ESP_LOGE(TAG, "This " FIVE_STRATUM_FW_NAME " pilot requires board 601 or 602");
        free(board_version);
        return ESP_ERR_NOT_SUPPORTED;
    }

    for (int i = 0 ; i < ARRAY_SIZE(default_configs); i++) {
        if (strcmp(default_configs[i].board_version, board_version) == 0) {
            GLOBAL_STATE->DEVICE_CONFIG = default_configs[i];

            ESP_LOGI(TAG, "Device Model: %s", GLOBAL_STATE->DEVICE_CONFIG.family.name);
            ESP_LOGI(TAG, "Board Version: %s", GLOBAL_STATE->DEVICE_CONFIG.board_version);
            ESP_LOGI(TAG, "ASIC: %dx %s (%d cores)", GLOBAL_STATE->DEVICE_CONFIG.family.asic_count, GLOBAL_STATE->DEVICE_CONFIG.family.asic.name, GLOBAL_STATE->DEVICE_CONFIG.family.asic.core_count);

            free(board_version);
            return ESP_OK;
        }
    }

    free(board_version);
    return ESP_ERR_NOT_SUPPORTED;
}
