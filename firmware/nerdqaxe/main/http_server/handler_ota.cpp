#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "global_state.h"

#include "http_cors.h"
#include "http_utils.h"
#include "nvs_config.h"
#include "displays/display_profile.h"
#include "display_ota_compatibility.h"

static const char *TAG = "http_ota";

extern bool enter_recovery;

static bool requires_display_profile_support()
{
    char *saved = Config::getDisplayProfile();
    const bool required = FiveTratumDisplayOta::requiresProfileSupport(
        saved, FiveTratumDisplay::displayProfileRuntime.active().id);
    free(saved);
    return required;
}


esp_err_t POST_WWW_update(httpd_req_t *req)
{
    // close connection when out of scope
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    // disable OTP when in recovery mode
    if (!enter_recovery && validateOTP(req) != ESP_OK) {
        return ESP_FAIL;
    }

    int remaining = req->content_len;

    const esp_partition_t *www_partition =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "www");
    if (www_partition == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "WWW partition not found");
        return ESP_FAIL;
    }

    // Don't attempt to write more than what can be stored in the partition
    if (remaining > www_partition->size) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File provided is too large for device");
        return ESP_FAIL;
    }

    // Erase the entire www partition before writing
    {
        // lock the power management module
        LockGuard g(POWER_MANAGEMENT_MODULE);
        ESP_LOGI(TAG, "erasing www partition ...");
        ESP_ERROR_CHECK(esp_partition_erase_range(www_partition, 0, www_partition->size));
        ESP_LOGI(TAG, "erasing done");
    }

    // don't put it on the stack
    char *buf = (char*) malloc(2048);
    uint32_t offset = 0;

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, min(remaining, 2048));

        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        } else if (recv_len <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
            free(buf);
            return ESP_FAIL;
        }

        {
            // lock the power management module
            LockGuard g(POWER_MANAGEMENT_MODULE);

            // print each 64kb
            if (!(offset & 0xffff)) {
                ESP_LOGI(TAG, "flashing to %08lx", offset);
            }
            if (esp_partition_write(www_partition, offset, (const void *) buf, recv_len) != ESP_OK) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write Error");
                free(buf);
                return ESP_FAIL;
            }
        }
        remaining -= recv_len;
        offset += recv_len;

        // switch task to not stall the PID
        taskYIELD();
    }

    free(buf);

    httpd_resp_sendstr(req, "WWW update complete\n");
    return ESP_OK;
}

/*
 * Handle OTA file upload
 */
esp_err_t POST_OTA_update(httpd_req_t *req)
{
    // close connection when out of scope
    ConGuard g(http_server, req);

    if (is_network_allowed(req) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    }

    if (validateOTP(req) != ESP_OK) {
        return ESP_FAIL;
    }

    esp_ota_handle_t ota_handle;
    int remaining = req->content_len;

    uint8_t prefix[FiveTratumDisplayOta::ImagePrefixBytes];
    std::size_t prefix_size = 0;
    if (requires_display_profile_support()) {
        if (remaining < static_cast<int>(sizeof(prefix))) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Application header is incomplete");
        }
        const int64_t prefix_started_us = esp_timer_get_time();
        while (prefix_size < sizeof(prefix)) {
            if (FiveTratumDisplayOta::prefixReadDeadlineExpired(prefix_started_us, esp_timer_get_time())) {
                return httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Application header timed out");
            }
            const int received = httpd_req_recv(req, reinterpret_cast<char *>(prefix) + prefix_size,
                                               sizeof(prefix) - prefix_size);
            if (FiveTratumDisplayOta::prefixReadDeadlineExpired(prefix_started_us, esp_timer_get_time())) {
                return httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Application header timed out");
            }
            if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;
            if (received <= 0) {
                return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Application header is incomplete");
            }
            prefix_size += static_cast<std::size_t>(received);
        }
        // Refuse legacy or unrelated images while the current firmware is
        // still running normally: neither flash nor ASIC power changes first.
        if (!FiveTratumDisplayOta::compatibleImagePrefix(prefix, prefix_size)) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                "480x320 requires a profile-aware QAxe application (5tratumFW QA beta.7 or newer)");
        }
        remaining -= static_cast<int>(prefix_size);
    }

    // lock the power management module
    LockGuard lg(POWER_MANAGEMENT_MODULE);

    // Shut down buck converter before starting OTA.
    // During OTA, I2C conflicts prevent the PID from working correctly.
    // Disabling hardware avoids any risk of overheating.
    // The device will reboot after the update anyway.
    POWER_MANAGEMENT_MODULE.shutdown();

    const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
    ESP_ERROR_CHECK(esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle));

    // don't put it on the stack
    char *buf = (char*) malloc(2048);
    uint32_t offset = 0;

    if (prefix_size) {
        if (esp_ota_write(ota_handle, prefix, prefix_size) != ESP_OK) {
            esp_ota_abort(ota_handle);
            free(buf);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Error");
        }
        offset = static_cast<uint32_t>(prefix_size);
    }

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, min(remaining, 2048));

        // Timeout Error: Just retry
        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;

            // Serious Error: Abort OTA
        } else if (recv_len <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
            free(buf);
            return ESP_FAIL;
        }

        // Successful Upload: Flash firmware chunk
        // print each 64kb
        if (!(offset & 0xffff)) {
            ESP_LOGI(TAG, "flashing to %08lx", offset);
        }
        if (esp_ota_write(ota_handle, (const void *) buf, recv_len) != ESP_OK) {
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Error");
            free(buf);
            return ESP_FAIL;
        }

        remaining -= recv_len;
        offset += recv_len;

        // give the scheduler a chance for other tasks
        taskYIELD();
    }

    free(buf);

    // Validate and switch to new OTA image and reboot
    if (esp_ota_end(ota_handle) != ESP_OK || esp_ota_set_boot_partition(ota_partition) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error");
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, "Firmware update complete, rebooting now!\n");
    ESP_LOGI(TAG, "Restarting System because of Firmware update complete");
    vTaskDelay(pdMS_TO_TICKS(1000));
    POWER_MANAGEMENT_MODULE.restart();

    // unreachable
    return ESP_OK;
}
