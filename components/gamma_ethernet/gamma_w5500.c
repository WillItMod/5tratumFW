/* Original implementation from WIZnet W5500 v1.1.0 register/protocol facts.
 * No W5500 driver implementation is incorporated. GPL-3.0-or-later. */
#include "gamma_w5500.h"
#include <string.h>

enum {
    COMMON = 0, SOCKET0 = 1, TX0 = 2, RX0 = 3,
    MODE = 0x0000, SOURCE_MAC = 0x0009, INTERRUPT_MASK = 0x0016,
    SOCKET_INTERRUPT_MASK = 0x0018, PHY = 0x002e, VERSION = 0x0039,
    S_MODE = 0x0000, S_COMMAND = 0x0001, S_INTERRUPT = 0x0002, S_STATUS = 0x0003,
    S_RX_SIZE = 0x001e, S_TX_SIZE = 0x001f, S_TX_FREE = 0x0020,
    S_TX_WRITE = 0x0024, S_RX_COUNT = 0x0026, S_RX_READ = 0x0028, S_MASK = 0x002c,
    OPEN = 0x01, CLOSE = 0x10, SEND = 0x20, RECEIVE = 0x40,
    MACRAW = 0x04, MACRAW_STATUS = 0x42, SEND_OK = 0x10, SEND_TIMEOUT = 0x08,
    COMMAND_MS = 20, SEND_MS = 100, WAIT_LIMIT = 128, STABLE_LIMIT = 8
};

static gamma_w5500_result_t access_bytes(gamma_w5500_t *chip, uint8_t block, uint16_t address,
                                        bool write, void *data, size_t length)
{
    if (!chip || !chip->io.transfer || !data || !length || length > GAMMA_W5500_FRAME_BYTES)
        return GAMMA_W5500_INVALID;
    if (chip->io.transfer(chip->io.context, block, address, write, data, length) != 0) {
        chip->ready = false;
        return GAMMA_W5500_IO;
    }
    return GAMMA_W5500_OK;
}

static gamma_w5500_result_t read_byte(gamma_w5500_t *chip, uint8_t block, uint16_t address, uint8_t *value)
{
    return access_bytes(chip, block, address, false, value, 1);
}

static gamma_w5500_result_t write_byte(gamma_w5500_t *chip, uint8_t block, uint16_t address, uint8_t value)
{
    return access_bytes(chip, block, address, true, &value, 1);
}

static gamma_w5500_result_t read_word(gamma_w5500_t *chip, uint16_t address, uint16_t *value)
{
    uint8_t bytes[2];
    gamma_w5500_result_t result = access_bytes(chip, SOCKET0, address, false, bytes, sizeof(bytes));
    if (result == GAMMA_W5500_OK) *value = (uint16_t)((uint16_t)bytes[0] << 8 | bytes[1]);
    return result;
}

static gamma_w5500_result_t stable_word(gamma_w5500_t *chip, uint16_t address, uint16_t *value)
{
    uint16_t previous, current;
    gamma_w5500_result_t result = read_word(chip, address, &previous);
    if (result != GAMMA_W5500_OK) return result;
    for (unsigned attempt = 0; attempt < STABLE_LIMIT; ++attempt) {
        result = read_word(chip, address, &current);
        if (result != GAMMA_W5500_OK) return result;
        if (previous == current) { *value = current; return GAMMA_W5500_OK; }
        previous = current;
    }
    return GAMMA_W5500_AGAIN;
}

static gamma_w5500_result_t write_word(gamma_w5500_t *chip, uint16_t address, uint16_t value)
{
    uint8_t bytes[] = {(uint8_t)(value >> 8), (uint8_t)value};
    return access_bytes(chip, SOCKET0, address, true, bytes, sizeof(bytes));
}

static gamma_w5500_result_t wait_byte(gamma_w5500_t *chip, uint8_t block, uint16_t address,
                                    uint8_t mask, uint8_t expected, unsigned timeout_ms)
{
    const uint64_t began = chip->io.milliseconds(chip->io.context);
    for (unsigned attempt = 0; attempt < WAIT_LIMIT; ++attempt) {
        uint8_t value;
        gamma_w5500_result_t result = read_byte(chip, block, address, &value);
        if (result != GAMMA_W5500_OK) return result;
        if ((value & mask) == expected) return GAMMA_W5500_OK;
        if (chip->io.milliseconds(chip->io.context) - began >= timeout_ms) break;
        chip->io.sleep_ms(chip->io.context, 1);
    }
    chip->ready = false;
    return GAMMA_W5500_TIMEOUT;
}

static gamma_w5500_result_t command(gamma_w5500_t *chip, uint8_t block, uint8_t operation)
{
    gamma_w5500_result_t result = write_byte(chip, block, S_COMMAND, operation);
    return result == GAMMA_W5500_OK ? wait_byte(chip, block, S_COMMAND, 0xff, 0, COMMAND_MS) : result;
}

static gamma_w5500_result_t open_macraw(gamma_w5500_t *chip)
{
    gamma_w5500_result_t result = command(chip, SOCKET0, CLOSE);
    if (result != GAMMA_W5500_OK) return result;
    result = wait_byte(chip, SOCKET0, S_STATUS, 0xff, 0, COMMAND_MS);
    if (result != GAMMA_W5500_OK) return result;
    /* MAC filtering remains disabled: the host stack needs multicast for mDNS
     * and IPv6. The hardware broadcast/multicast/IPv6 blocking bits stay clear. */
    if ((result = write_byte(chip, SOCKET0, S_MODE, MACRAW)) != GAMMA_W5500_OK) return result;
    if ((result = write_byte(chip, SOCKET0, S_MASK, 0x1f)) != GAMMA_W5500_OK) return result;
    if ((result = write_byte(chip, SOCKET0, S_INTERRUPT, 0x1f)) != GAMMA_W5500_OK) return result;
    if ((result = command(chip, SOCKET0, OPEN)) != GAMMA_W5500_OK) return result;
    result = wait_byte(chip, SOCKET0, S_STATUS, 0xff, MACRAW_STATUS, COMMAND_MS);
    chip->ready = result == GAMMA_W5500_OK;
    return result;
}

gamma_w5500_result_t gamma_w5500_initialize(gamma_w5500_t *chip, const gamma_w5500_io_t *io, const uint8_t mac[6])
{
    if (!chip || !io || !io->transfer || !io->milliseconds || !io->sleep_ms || !mac || (mac[0] & 1))
        return GAMMA_W5500_INVALID;
    bool nonzero = false;
    for (unsigned i = 0; i < 6; ++i) nonzero |= mac[i] != 0;
    if (!nonzero) return GAMMA_W5500_INVALID;
    memset(chip, 0, sizeof(*chip));
    chip->io = *io;
    uint8_t version;
    gamma_w5500_result_t result = read_byte(chip, COMMON, VERSION, &version);
    if (result != GAMMA_W5500_OK) return result;
    if (version != 0x04) return GAMMA_W5500_NOT_FOUND;
    if ((result = write_byte(chip, COMMON, MODE, 0x80)) != GAMMA_W5500_OK) return result;
    if ((result = wait_byte(chip, COMMON, MODE, 0x80, 0, COMMAND_MS)) != GAMMA_W5500_OK) return result;
    if ((result = write_byte(chip, COMMON, INTERRUPT_MASK, 0)) != GAMMA_W5500_OK) return result;
    if ((result = write_byte(chip, COMMON, SOCKET_INTERRUPT_MASK, 0)) != GAMMA_W5500_OK) return result;
    if ((result = access_bytes(chip, COMMON, SOURCE_MAC, true, (void *)mac, 6)) != GAMMA_W5500_OK) return result;
    /* Disable unused banks before allocating the complete 16 KiB to socket 0. */
    for (unsigned socket = 0; socket < 8; ++socket) {
        uint8_t block = (uint8_t)(1 + 4 * socket);
        if ((result = command(chip, block, CLOSE)) != GAMMA_W5500_OK) return result;
        if ((result = write_byte(chip, block, S_RX_SIZE, 0)) != GAMMA_W5500_OK) return result;
        if ((result = write_byte(chip, block, S_TX_SIZE, 0)) != GAMMA_W5500_OK) return result;
    }
    if ((result = write_byte(chip, SOCKET0, S_RX_SIZE, 16)) != GAMMA_W5500_OK) return result;
    if ((result = write_byte(chip, SOCKET0, S_TX_SIZE, 16)) != GAMMA_W5500_OK) return result;
    return open_macraw(chip);
}

gamma_w5500_result_t gamma_w5500_close(gamma_w5500_t *chip)
{
    if (!chip || !chip->io.transfer) return GAMMA_W5500_INVALID;
    gamma_w5500_result_t result = command(chip, SOCKET0, CLOSE);
    chip->ready = false;
    return result;
}

gamma_w5500_result_t gamma_w5500_link(gamma_w5500_t *chip, bool *up)
{
    if (!chip || !chip->ready || !up) return GAMMA_W5500_INVALID;
    uint8_t version, value;
    gamma_w5500_result_t result = read_byte(chip, COMMON, VERSION, &version);
    if (result != GAMMA_W5500_OK) return result;
    if (version != 4) { chip->ready = false; return GAMMA_W5500_NOT_FOUND; }
    result = read_byte(chip, COMMON, PHY, &value);
    if (result == GAMMA_W5500_OK) *up = (value & 1) != 0;
    return result;
}

static gamma_w5500_result_t ring(gamma_w5500_t *chip, uint8_t block, uint16_t pointer,
                                bool write, void *buffer, size_t length)
{
    size_t offset = pointer & (GAMMA_W5500_RING_BYTES - 1u);
    size_t first = GAMMA_W5500_RING_BYTES - offset;
    if (first > length) first = length;
    gamma_w5500_result_t result = access_bytes(chip, block, (uint16_t)offset, write, buffer, first);
    if (result != GAMMA_W5500_OK || first == length) return result;
    return access_bytes(chip, block, 0, write, (uint8_t *)buffer + first, length - first);
}

gamma_w5500_result_t gamma_w5500_transmit(gamma_w5500_t *chip, const void *frame, size_t length)
{
    if (!chip || !chip->ready || !frame || length < 14 || length > GAMMA_W5500_FRAME_BYTES)
        return GAMMA_W5500_INVALID;
    bool link;
    gamma_w5500_result_t result = gamma_w5500_link(chip, &link);
    if (result != GAMMA_W5500_OK) return result;
    if (!link) return GAMMA_W5500_LINK_DOWN;
    uint16_t free_bytes, pointer;
    const size_t wire_length = length < 60 ? 60 : length;
    if ((result = stable_word(chip, S_TX_FREE, &free_bytes)) != GAMMA_W5500_OK) return result;
    if (free_bytes > GAMMA_W5500_RING_BYTES) { chip->ready = false; return GAMMA_W5500_IO; }
    if (free_bytes < wire_length) return GAMMA_W5500_AGAIN;
    if ((result = read_word(chip, S_TX_WRITE, &pointer)) != GAMMA_W5500_OK) return result;
    if ((result = write_byte(chip, SOCKET0, S_INTERRUPT, SEND_OK | SEND_TIMEOUT)) != GAMMA_W5500_OK) return result;
    if ((result = ring(chip, TX0, pointer, true, (void *)frame, length)) != GAMMA_W5500_OK) return result;
    if (wire_length > length) {
        uint8_t padding[60] = {0};
        if ((result = ring(chip, TX0, (uint16_t)(pointer + length), true, padding, wire_length - length)) != GAMMA_W5500_OK)
            return result;
    }
    if ((result = write_word(chip, S_TX_WRITE, (uint16_t)(pointer + wire_length))) != GAMMA_W5500_OK) return result;
    if ((result = command(chip, SOCKET0, SEND)) != GAMMA_W5500_OK) return result;
    const uint64_t began = chip->io.milliseconds(chip->io.context);
    for (unsigned attempt = 0; attempt < WAIT_LIMIT; ++attempt) {
        uint8_t flags;
        if ((result = read_byte(chip, SOCKET0, S_INTERRUPT, &flags)) != GAMMA_W5500_OK) return result;
        if (flags & SEND_TIMEOUT) break;
        if (flags & SEND_OK) return write_byte(chip, SOCKET0, S_INTERRUPT, SEND_OK);
        if (chip->io.milliseconds(chip->io.context) - began >= SEND_MS) break;
        chip->io.sleep_ms(chip->io.context, 1);
    }
    /* An uncertain send is never replayed here. Reopen the socket so a later
     * frame cannot accidentally include bytes from the timed-out transaction. */
    result = open_macraw(chip);
    return result == GAMMA_W5500_OK ? GAMMA_W5500_TIMEOUT : result;
}

gamma_w5500_result_t gamma_w5500_receive(gamma_w5500_t *chip, void *frame, size_t capacity, size_t *length)
{
    if (length) *length = 0;
    if (!chip || !chip->ready || !frame || !length || capacity < 14) return GAMMA_W5500_INVALID;
    uint16_t available, pointer;
    gamma_w5500_result_t result = stable_word(chip, S_RX_COUNT, &available);
    if (result != GAMMA_W5500_OK) return result;
    if (available < 2) return GAMMA_W5500_AGAIN;
    if (available > GAMMA_W5500_RING_BYTES) {
        result = open_macraw(chip);
        return result == GAMMA_W5500_OK ? GAMMA_W5500_BAD_FRAME : result;
    }
    if ((result = read_word(chip, S_RX_READ, &pointer)) != GAMMA_W5500_OK) return result;
    uint8_t header[2];
    if ((result = ring(chip, RX0, pointer, false, header, sizeof(header))) != GAMMA_W5500_OK) return result;
    const size_t total = (size_t)header[0] << 8 | header[1];
    /* MACRAW prepends a big-endian count including these two header bytes.
     * Never subtract until bounded, never pass the header to the host stack. */
    if (total < 16 || total > GAMMA_W5500_FRAME_BYTES + 2u || total > available || total - 2u > capacity) {
        result = open_macraw(chip);
        return result == GAMMA_W5500_OK ? GAMMA_W5500_BAD_FRAME : result;
    }
    if ((result = ring(chip, RX0, (uint16_t)(pointer + 2u), false, frame, total - 2u)) != GAMMA_W5500_OK) return result;
    if ((result = write_word(chip, S_RX_READ, (uint16_t)(pointer + total))) != GAMMA_W5500_OK) return result;
    if ((result = command(chip, SOCKET0, RECEIVE)) != GAMMA_W5500_OK) return result;
    *length = total - 2u;
    return GAMMA_W5500_OK;
}
