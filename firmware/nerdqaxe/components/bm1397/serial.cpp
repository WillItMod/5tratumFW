#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "driver/uart.h"

#include "esp_log.h"
#include "soc/uart_struct.h"

#include "asic.h"
#include "serial.h"
#include "mining_utils.h"
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
#include "bm1370_capture_runtime.h"
#endif

#define ECHO_TEST_TXD (17)
#define ECHO_TEST_RXD (18)
#define BUF_SIZE (1024)

static const char *TAG = "serial";

static pthread_mutex_t tx_mute = PTHREAD_MUTEX_INITIALIZER;

void SERIAL_init(void)
{
    ESP_LOGI(TAG, "Initializing serial");
    // Configure UART1 parameters
    uart_config_t uart_config;

    memset(&uart_config, 0, sizeof(uart_config_t));

    uart_config.baud_rate = 115200;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 122;

    // Configure UART1 parameters
    uart_param_config(UART_NUM_1, &uart_config);
    // Set UART1 pins(TX: IO17, RX: I018)
    uart_set_pin(UART_NUM_1, ECHO_TEST_TXD, ECHO_TEST_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Install UART driver (we don't need an event queue here)
    // tx buffer 0 so the tx time doesn't overlap with the job wait time
    //  by returning before the job is written
    uart_driver_install(UART_NUM_1, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0);
}

void SERIAL_set_baud(int baud)
{
    ESP_LOGI(TAG, "Changing UART baud to %i", baud);
    const esp_err_t result = uart_set_baudrate(UART_NUM_1, baud);
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
    FiveTratumCapture::transport(BM1370Capture::Transport::BaudChanged, result, baud);
#else
    (void)result;
#endif
}

bool SERIAL_set_baud_checked(int baud) {
    ESP_LOGI(TAG, "Changing UART baud to %i", baud);
    const esp_err_t result = uart_set_baudrate(UART_NUM_1, baud);
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
    FiveTratumCapture::transport(BM1370Capture::Transport::BaudChanged, result, baud);
#endif
    return result == ESP_OK;
}

static int send_packet(uint8_t *data, int len
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
    , const BM1370Capture::JobMetadata *job, uint64_t generation
#endif
)
{
    // lock the transport
    pthread_mutex_lock(&tx_mute);

#ifdef ASIC_SERIALTX_DEBUG
    ESP_LOG_BUFFER_HEX_LEVEL("serial_tx", data, len, ESP_LOG_INFO);
#endif

    int written = uart_write_bytes(UART_NUM_1, (const char *) data, len);
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
    FiveTratumCapture::tx(data, len, written, job, generation);
#endif
    pthread_mutex_unlock(&tx_mute);

    return written;
}

int SERIAL_send(uint8_t *data, int len) {
    return send_packet(data, len
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
        , nullptr, 0
#endif
    );
}
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
int SERIAL_send_job(uint8_t *data, int len, const BM1370Capture::JobMetadata &job, uint64_t generation) {
    return send_packet(data, len, &job, generation);
}
#endif

/// @brief waits for a serial response from the device
/// @param buf buffer to read data into
/// @param buf number of ms to wait before timing out
/// @return number of bytes read, or -1 on error
int16_t SERIAL_rx(uint8_t *buf, uint16_t size, uint16_t timeout_ms)
{
    int16_t bytes_read = uart_read_bytes(UART_NUM_1, buf, size, pdMS_TO_TICKS(timeout_ms));
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
    if (bytes_read > 0) FiveTratumCapture::rx(buf, bytes_read, size);
    else if (bytes_read < 0) FiveTratumCapture::transport(BM1370Capture::Transport::RxError, bytes_read);
#endif

#ifdef ASIC_SERIALRX_DEBUG
    size_t buff_len = 0;
    if (bytes_read > 0) {
        uart_get_buffered_data_len(UART_NUM_1, &buff_len);
        ESP_LOG_BUFFER_HEX_LEVEL("serial_rx", buf, bytes_read, ESP_LOG_INFO);
    }
#endif

    return bytes_read;
}

void SERIAL_clear_buffer(void)
{
    const esp_err_t result = uart_flush(UART_NUM_1);
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
    FiveTratumCapture::transport(BM1370Capture::Transport::BufferFlush, result);
#else
    (void)result;
#endif
}

bool SERIAL_wait_tx_idle(uint16_t timeout_ms) {
    pthread_mutex_lock(&tx_mute);
    const bool idle = uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(timeout_ms)) == ESP_OK;
    pthread_mutex_unlock(&tx_mute);
    return idle;
}

bool SERIAL_clear_buffer_checked(void) {
    const esp_err_t result = uart_flush(UART_NUM_1);
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
    FiveTratumCapture::transport(BM1370Capture::Transport::BufferFlush, result);
#endif
    return result == ESP_OK;
}
