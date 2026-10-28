/* Original 5tratumFW W5500 MACRAW protocol engine. GPL-3.0-or-later. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GAMMA_W5500_RING_BYTES 16384u
#define GAMMA_W5500_FRAME_BYTES 1518u

typedef enum {
    GAMMA_W5500_OK = 0,
    GAMMA_W5500_AGAIN,
    GAMMA_W5500_NOT_FOUND,
    GAMMA_W5500_IO,
    GAMMA_W5500_TIMEOUT,
    GAMMA_W5500_INVALID,
    GAMMA_W5500_BAD_FRAME,
    GAMMA_W5500_LINK_DOWN
} gamma_w5500_result_t;

/* A transfer is one complete W5500 VDM SPI frame. Callers serialize the engine.
 * transfer returns zero on success; all callbacks, including SPI, must be bounded.
 * No network-stack callback is made while this protocol engine runs. */
typedef struct {
    void *context;
    int (*transfer)(void *context, uint8_t block, uint16_t address, bool write, void *data, size_t length);
    uint64_t (*milliseconds)(void *context);
    void (*sleep_ms)(void *context, unsigned milliseconds);
} gamma_w5500_io_t;

typedef struct {
    gamma_w5500_io_t io;
    bool ready;
} gamma_w5500_t;

gamma_w5500_result_t gamma_w5500_initialize(gamma_w5500_t *chip, const gamma_w5500_io_t *io, const uint8_t mac[6]);
gamma_w5500_result_t gamma_w5500_close(gamma_w5500_t *chip);
gamma_w5500_result_t gamma_w5500_link(gamma_w5500_t *chip, bool *up);
gamma_w5500_result_t gamma_w5500_transmit(gamma_w5500_t *chip, const void *frame, size_t length);
/* Copies one frame and releases its hardware ring space. AGAIN means no complete
 * frame; BAD_FRAME means the socket was safely reopened after invalid framing. */
gamma_w5500_result_t gamma_w5500_receive(gamma_w5500_t *chip, void *frame, size_t capacity, size_t *length);
