#include <stdio.h>
#include "esp_log.h"
#include "esp_check.h"

#include "i2c_bitaxe.h"
#include "EMC2101.h"

static const char * TAG = "EMC2101";

static i2c_master_dev_handle_t emc2101_dev_handle;

static int temp_offset;

/**
 * @brief Initialize the EMC2101 sensor.
 *
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t EMC2101_init(int temp_offset_param)
{
    ESP_LOGI(TAG, "Initializing EMC2101 (Temperature offset: %d° C)", temp_offset_param);

    temp_offset = temp_offset_param;

    if (i2c_bitaxe_add_device(EMC2101_I2CADDR_DEFAULT, &emc2101_dev_handle, TAG) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device");
        return ESP_FAIL;
    }

    // set the TACH input
    ESP_RETURN_ON_ERROR(i2c_bitaxe_register_write_byte(emc2101_dev_handle, EMC2101_REG_CONFIG, 0x04), TAG, "Failed to set TACH input");


    //! deprecated bit was never set successfully
    // --- Configure Fan Settings (Register 0x4A) ---
    // Start with a base configuration:
    // Bit 7 (MASK) = 0: Tach Alert Enabled
    // Bit 6 (EN)   = 1: *** FAN DRIVER ENABLED ***
    // Bit 5 (LEVEL)= 1: Direct Setting Mode (use PWM Duty Cycle Reg 0x4C)
    // Bit 4 (POL)  = 0: Normal Polarity (placeholder, set below)
    // Bit 3:2(EDGES)=00: 2 Tach pulses per revolution (standard)
    // Bit 1:0(RANGE)=00: PWM Frequency ~22.5 kHz (common setting)
    // //if (invertPolarity) {
        ESP_RETURN_ON_ERROR(i2c_bitaxe_register_write_byte(emc2101_dev_handle, EMC2101_FAN_CONFIG, 0b00100011), TAG, "Failed to configure fan settings");
    // //}
    



    // We're using default filtering and conversion, no need to set them again.
    // //set filtering
    // ESP_ERROR_CHECK(i2c_bitaxe_register_write_byte(emc2101_dev_handle, EMC2101_TEMP_FILTER, EMC2101_DEFAULT_FILTER));

    // //set conversion rate
    // ESP_ERROR_CHECK(i2c_bitaxe_register_write_byte(emc2101_dev_handle, EMC2101_REG_DATA_RATE, EMC2101_DEFAULT_DATARATE));

    return ESP_OK;
}

esp_err_t EMC2101_set_ideality_factor(uint8_t ideality){
    //set Ideality Factor
    ESP_RETURN_ON_ERROR(i2c_bitaxe_register_write_byte(emc2101_dev_handle, EMC2101_IDEALITY_FACTOR, ideality), TAG, "Failed to set ideality factor");
    return ESP_OK;
}

esp_err_t EMC2101_set_beta_compensation(uint8_t beta){
    //set Beta Compensation
    ESP_RETURN_ON_ERROR(i2c_bitaxe_register_write_byte(emc2101_dev_handle, EMC2101_BETA_COMPENSATION, beta), TAG, "Failed to set beta compensation");
    return ESP_OK;
}

// takes a fan speed percent
esp_err_t EMC2101_set_fan_speed(float percent)
{
    uint8_t speed;

    speed = (uint8_t) (63.0 * percent);
    ESP_RETURN_ON_ERROR(i2c_bitaxe_register_write_byte(emc2101_dev_handle, EMC2101_REG_FAN_SETTING, speed), TAG, "Failed to set fan speed");
    return ESP_OK;
}

// RPM = 5400000/reading
uint16_t EMC2101_get_fan_speed(void)
{
    uint8_t tach_lsb = 0, tach_msb = 0;
    uint16_t reading;
    uint16_t RPM;
    esp_err_t err;

    err = i2c_bitaxe_register_read(emc2101_dev_handle, EMC2101_TACH_LSB, &tach_lsb, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read fan speed LSB: %s", esp_err_to_name(err));
        return 0;
    }
    
    err = i2c_bitaxe_register_read(emc2101_dev_handle, EMC2101_TACH_MSB, &tach_msb, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read fan speed MSB: %s", esp_err_to_name(err));
        return 0;
    }

    // ESP_LOGI(TAG, "Raw Fan Speed = %02X %02X", tach_msb, tach_lsb);

    reading = tach_lsb | (tach_msb << 8);
    if (reading == 0 || reading == UINT16_MAX) return 0;
    RPM = 5400000 / reading;

    // ESP_LOGI(TAG, "Fan Speed = %d RPM", RPM);
    if (RPM == 82) {
        return 0;
    }
    return RPM;
}

float EMC2101_get_external_temp(void)
{
    uint8_t temp_msb = 0, temp_lsb = 0;
    uint16_t reading;
    esp_err_t err;

    err = i2c_bitaxe_register_read(emc2101_dev_handle, EMC2101_EXTERNAL_TEMP_MSB, &temp_msb, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read external temperature MSB: %s", esp_err_to_name(err));
        return -1;
    }
    
    err = i2c_bitaxe_register_read(emc2101_dev_handle, EMC2101_EXTERNAL_TEMP_LSB, &temp_lsb, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read external temperature LSB: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Combine MSB and LSB, and then right shift to get 11 bits
    reading = (temp_msb << 8) | temp_lsb;
    reading >>= 5;  // Now, `reading` contains an 11-bit signed value

    // Cast `reading` to a signed 16-bit integer
    int16_t signed_reading = (int16_t)reading;

    // If the 11th bit (sign bit in 11-bit data) is set, extend the sign
    if (signed_reading & 0x0400) {
        signed_reading |= 0xF800;  // Set upper bits to extend the sign
    }

    if (signed_reading == EMC2101_TEMP_FAULT_OPEN_CIRCUIT) {
        ESP_LOGE(TAG, "EMC2101 TEMP_FAULT_OPEN_CIRCUIT: %04X", signed_reading);
    }
    if (signed_reading == EMC2101_TEMP_FAULT_SHORT) {
        ESP_LOGE(TAG, "EMC2101 TEMP_FAULT_SHORT: %04X", signed_reading);
    }

    // Convert the signed reading to temperature in Celsius
    float result = (float)signed_reading / 8.0;

    return result + temp_offset;
}

float EMC2101_get_internal_temp(void)
{
    uint8_t temp = 0;
    esp_err_t err;
    
    err = i2c_bitaxe_register_read(emc2101_dev_handle, EMC2101_INTERNAL_TEMP, &temp, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read internal temperature: %s", esp_err_to_name(err));
        return -1;
    }
    return (float) temp + temp_offset;
}

esp_err_t EMC2101_read_strict(float *external_c, uint16_t *fan_rpm)
{
    if (!external_c || !fan_rpm) return ESP_ERR_INVALID_ARG;
    uint8_t status = 0, msb = 0, lsb = 0, check_msb = 0, tach_lo = 0, tach_hi = 0;
    if (i2c_bitaxe_register_read(emc2101_dev_handle, EMC2101_STATUS, &status, 1) != ESP_OK ||
        (status & 0x04) ||
        i2c_bitaxe_register_read(emc2101_dev_handle, EMC2101_EXTERNAL_TEMP_MSB, &msb, 1) != ESP_OK ||
        i2c_bitaxe_register_read(emc2101_dev_handle, EMC2101_EXTERNAL_TEMP_LSB, &lsb, 1) != ESP_OK ||
        i2c_bitaxe_register_read(emc2101_dev_handle, EMC2101_EXTERNAL_TEMP_MSB, &check_msb, 1) != ESP_OK ||
        msb != check_msb ||
        i2c_bitaxe_register_read(emc2101_dev_handle, EMC2101_TACH_LSB, &tach_lo, 1) != ESP_OK ||
        i2c_bitaxe_register_read(emc2101_dev_handle, EMC2101_TACH_MSB, &tach_hi, 1) != ESP_OK) return ESP_FAIL;
    int16_t raw = (int16_t)(((uint16_t)msb << 8 | lsb) >> 5);
    if (raw == EMC2101_TEMP_FAULT_OPEN_CIRCUIT || raw == EMC2101_TEMP_FAULT_SHORT) return ESP_FAIL;
    if (raw & 0x0400) raw |= (int16_t)0xF800;
    const uint16_t tach = (uint16_t)tach_lo | ((uint16_t)tach_hi << 8);
    const uint32_t rpm = tach && tach != UINT16_MAX ? 5400000u / tach : 0;
    if (rpm > UINT16_MAX) return ESP_FAIL;
    *external_c = raw / 8.0f + temp_offset;
    *fan_rpm = rpm == 82 ? 0 : (uint16_t)rpm;
    return ESP_OK;
}
esp_err_t EMC2101_read_fan_strict(uint16_t *fan_rpm)
{
    if (!fan_rpm) return ESP_ERR_INVALID_ARG;
    uint8_t lo=0,hi=0;
    if (i2c_bitaxe_register_read(emc2101_dev_handle,EMC2101_TACH_LSB,&lo,1)!=ESP_OK ||
        i2c_bitaxe_register_read(emc2101_dev_handle,EMC2101_TACH_MSB,&hi,1)!=ESP_OK) return ESP_FAIL;
    const uint16_t tach=(uint16_t)lo|((uint16_t)hi<<8);
    const uint32_t rpm=tach && tach!=UINT16_MAX?5400000u/tach:0;
    if (rpm>UINT16_MAX) return ESP_FAIL;
    *fan_rpm=rpm==82?0:(uint16_t)rpm;return ESP_OK;
}
