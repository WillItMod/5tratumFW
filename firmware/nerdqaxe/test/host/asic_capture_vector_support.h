#pragma once
// Only external UART/RTOS/log boundaries are replaced. This harness creates
// synthetic source vectors; it contains no serial port or device access.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <vector>
#include "asic.h"
#include "bm1370.h"
#include "crc.h"
#define __bswap32 __builtin_bswap32
#define pdMS_TO_TICKS(ms) (ms)
#define ESP_LOGI(tag, ...) do { (void)(tag); } while (0)
#define ESP_LOGE(tag, ...) do { (void)(tag); } while (0)
#define ESP_LOG_BUFFER_HEX(tag, ...) do { (void)(tag); } while (0)
void vTaskDelay(uint32_t);
int SERIAL_send(uint8_t *, int);
int16_t SERIAL_rx(uint8_t *, uint16_t, uint16_t);
void SERIAL_clear_buffer();
unsigned char _reverse_bits(unsigned char);
int _largest_power_of_two(int);
uint8_t hex2val(char);
size_t hex2bin(const char *, uint8_t *, size_t);
void swap_endian_words_bin(uint8_t *, uint8_t *, size_t);
void swap_endian_words(const char *, uint8_t *);
void reverse_bytes(uint8_t *, size_t);
void construct_bm_job(mining_notify *, const char *, uint32_t, bm_job *);
