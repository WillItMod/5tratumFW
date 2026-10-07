#include "esp_event.h"
#include "esp_log.h"
#include "esp_psram.h"

#include "asic_result_task.h"
#include "create_jobs_task.h"
#include "hashrate_monitor_task.h"
#include "fan_controller_task.h"
#include "statistics_task.h"
#include "system.h"
#include "http_server.h"
#include "serial.h"
#include "protocol_coordinator.h"
#include "operating_profiles.h"
#include "pool_schedule.h"
#include "i2c_bitaxe.h"
#include "adc.h"
#include "nvs_config.h"
#include "self_test.h"
#include "asic.h"
#include "bap/bap.h"
#include "device_config.h"
#include "connect.h"
#include "asic_reset.h"
#include "asic_init.h"
#include "task_monitor.h"
#include "filesystem.h"
#include "input.h"
#include "log_buffer.h"
#include "5tratumfw.h"
#include "mining_schedule.h"

static GlobalState GLOBAL_STATE;

static const char * TAG = FIVE_STRATUM_FW_NAME;

void app_main(void)
{
    if (esp_psram_is_initialized()) {
        GLOBAL_STATE.psram_is_available = true;
        log_buffer_init();
    }

    ESP_LOGI(TAG, "Welcome to " FIVE_STRATUM_FW_NAME " - based on Bitaxe/ESP-Miner");

    if (xTaskCreate(cpu_monitor_task, "cpu_monitor", 4096, (void *)&GLOBAL_STATE, 1, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creating cpu monitor task");
    }
#ifdef CONFIG_ENABLE_TASK_MONITOR
    if (xTaskCreate(task_monitor_task, "task_monitor", 8192, NULL, 1, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creating task monitor task");
    }
#endif
  
    if (!esp_psram_is_initialized()) {
        ESP_LOGE(TAG, "No PSRAM available on ESP32 device!");
    }

    // Validate persisted board identity and load settings before touching hardware.
    if (nvs_config_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init NVS");
        return;
    }

    // Ensure SSID is initialized before any screen/self-test uses it.
    GLOBAL_STATE.SYSTEM_MODULE.ssid = nvs_config_get_string(NVS_CONFIG_WIFI_SSID);
    if (GLOBAL_STATE.SYSTEM_MODULE.ssid == NULL) {
        ESP_LOGW(TAG, "No SSID configured in NVS, using empty string");
        GLOBAL_STATE.SYSTEM_MODULE.ssid = strdup("");
        if (GLOBAL_STATE.SYSTEM_MODULE.ssid == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for SSID");
            return;
        }
    }

    if (device_config_init(&GLOBAL_STATE) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init device config");
        return;
    }

    // Only validated Gamma boards reach peripheral or ASIC initialization.
    ESP_ERROR_CHECK(i2c_bitaxe_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    // Hold the ASIC in reset as soon as its board profile has been validated.
    ESP_ERROR_CHECK(asic_hold_reset_low());
    ESP_LOGI(TAG, "RST pin initialized to low");

    vTaskDelay(100 / portTICK_PERIOD_MS);
    ADC_init();

    if (self_test_init(&GLOBAL_STATE) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init self test");
        return;
    }

    SYSTEM_init_system(&GLOBAL_STATE);
    operating_profiles_init(&GLOBAL_STATE);
    if (!GLOBAL_STATE.SELF_TEST_MODULE.is_active && pool_schedule_init(&GLOBAL_STATE) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize pool schedule; keeping ASIC reset asserted");
        return;
    }
    if (!GLOBAL_STATE.SELF_TEST_MODULE.is_active && mining_schedule_init(&GLOBAL_STATE) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize mining schedule; keeping ASIC reset asserted");
        return;
    }
    if (scoreboard_init(&GLOBAL_STATE.SYSTEM_MODULE.scoreboard) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init scoreboard");
    }

    if (!GLOBAL_STATE.SELF_TEST_MODULE.is_active) {
        wifi_init(&GLOBAL_STATE);
        mining_schedule_start_time_sync();
    }

    esp_err_t system_init_ret = SYSTEM_init_peripherals(&GLOBAL_STATE);
    
    if (system_init_ret == ESP_OK) {
        if (xTaskCreate(POWER_MANAGEMENT_task, "power management", 8192, (void *) &GLOBAL_STATE, 10, NULL) != pdPASS) {
            ESP_LOGE(TAG, "Error creating power management task");
            POWER_MANAGEMENT_stop_for_fault(&GLOBAL_STATE, "Power management task could not start");
            system_init_ret = ESP_FAIL;
        }
        if (!GLOBAL_STATE.SELF_TEST_MODULE.is_active) {
            if (xTaskCreate(FAN_CONTROLLER_task, "fan_controller", 8192, (void *) &GLOBAL_STATE, 5, NULL) != pdPASS) {
                ESP_LOGE(TAG, "Error creating fan controller task");
                POWER_MANAGEMENT_stop_for_fault(&GLOBAL_STATE, "Fan controller task could not start");
                system_init_ret = ESP_FAIL;
            }
        }
    } else {
        POWER_MANAGEMENT_stop_for_fault(&GLOBAL_STATE, "Peripheral initialization failed");
        ESP_LOGE(TAG, "Critical peripheral initialization failure (%s). Entering degraded mode.", esp_err_to_name(GLOBAL_STATE.SELF_TEST_MODULE.system_init_ret));
    }
    
    if (!GLOBAL_STATE.SELF_TEST_MODULE.is_active) {
        // start the API for AxeOS
        if (start_rest_server((void *) &GLOBAL_STATE) != ESP_OK) {
            POWER_MANAGEMENT_stop_for_fault(&GLOBAL_STATE, "HTTP server could not start");
            system_init_ret = ESP_FAIL;
        }
    }

    // After mounting SPIFFS
    SYSTEM_init_versions(&GLOBAL_STATE);

    // Initialize BAP interface
    esp_err_t bap_ret = BAP_init(&GLOBAL_STATE);
    if (bap_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BAP interface: %d", bap_ret);
        // Continue anyway, as BAP is not critical for core functionality
    }

    while (!GLOBAL_STATE.SYSTEM_MODULE.is_connected) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    queue_init(&GLOBAL_STATE.stratum_queue);

    if (system_init_ret == ESP_OK) {
        if (GLOBAL_STATE.SELF_TEST_MODULE.is_active &&
            asic_initialize(&GLOBAL_STATE, ASIC_INIT_COLD_BOOT, 0) == 0) {
            self_test_show_message(&GLOBAL_STATE, GLOBAL_STATE.SYSTEM_MODULE.asic_status);
            system_init_ret = ESP_FAIL;
        } else {
            if (xTaskCreate(create_jobs_task, "stratum miner", 8192, (void *) &GLOBAL_STATE, 20, NULL) != pdPASS) {
                ESP_LOGE(TAG, "Error creating stratum miner task");
                POWER_MANAGEMENT_stop_for_fault(&GLOBAL_STATE, "Mining job task could not start");
                system_init_ret = ESP_FAIL;
            }
            if (xTaskCreate(ASIC_result_task, "asic result", 8192, (void *) &GLOBAL_STATE, 15, NULL) != pdPASS) {
                ESP_LOGE(TAG, "Error creating asic result task");
                POWER_MANAGEMENT_stop_for_fault(&GLOBAL_STATE, "ASIC result task could not start");
                system_init_ret = ESP_FAIL;
            }

            if (xTaskCreateWithCaps(hashrate_monitor_task, "hashrate monitor", 8192, (void *) &GLOBAL_STATE, 5, NULL, MALLOC_CAP_SPIRAM) != pdPASS) {
                ESP_LOGE(TAG, "Error creating hashrate monitor task");
                POWER_MANAGEMENT_stop_for_fault(&GLOBAL_STATE, "Hashrate monitor task could not start");
                system_init_ret = ESP_FAIL;
            }
            if (xTaskCreateWithCaps(statistics_task, "statistics", 8192, (void *) &GLOBAL_STATE, 3, NULL, MALLOC_CAP_SPIRAM) != pdPASS) {
                ESP_LOGE(TAG, "Error creating statistics task");
                POWER_MANAGEMENT_stop_for_fault(&GLOBAL_STATE, "Statistics task could not start");
                system_init_ret = ESP_FAIL;
            }
        }
    }

    protocol_coordinator_init(&GLOBAL_STATE);
    if (!GLOBAL_STATE.SELF_TEST_MODULE.is_active) pool_schedule_start();
    if (xTaskCreate(protocol_coordinator_task, "protocol coord", 8192, (void *) &GLOBAL_STATE, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creating protocol coordinator task");
        POWER_MANAGEMENT_stop_for_fault(&GLOBAL_STATE, "Protocol coordinator task could not start");
        system_init_ret = ESP_FAIL;
    }

    GLOBAL_STATE.SYSTEM_MODULE.mining_runtime_ready = system_init_ret == ESP_OK;

    if (GLOBAL_STATE.SELF_TEST_MODULE.is_active) {
        GLOBAL_STATE.SELF_TEST_MODULE.system_init_ret = system_init_ret;
        if (xTaskCreate(self_test_task, "self_test", 8192, (void *) &GLOBAL_STATE, 10, NULL) != pdPASS) {
            ESP_LOGE(TAG, "Error creating self test task");
        }
    }
}
