/* Original register-level hardware simulator, not a network or device test.
 * GPL-3.0-or-later. */
#include "gamma_w5500.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t common[64], sockets[8][48], tx[16384], rx[16384], sent[1518];
    unsigned calls, writes, sends, opens, recv_commands;
    unsigned fail_after, flap;
    bool missing, stuck_reset, stuck_command, silent_send, frozen_clock;
    uint64_t now;
    size_t sent_length;
    uint16_t rx_previous;
} simulated_chip_t;

static uint16_t word(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] << 8 | p[1]); }
static void put_word(uint8_t *p, uint16_t n) { p[0] = (uint8_t)(n >> 8); p[1] = (uint8_t)n; }

static int transfer(void *context, uint8_t block, uint16_t address, bool write, void *data, size_t length)
{
    simulated_chip_t *s = context;
    ++s->calls;
    if (s->fail_after && s->calls > s->fail_after) return -1;
    assert(data && length > 0 && length <= 1518);
    if (write) ++s->writes;
    if (block == 0) {
        assert(address + length <= sizeof(s->common));
        if (!write) {
            if (s->missing) memset(data, 0xff, length);
            else memcpy(data, s->common + address, length);
        } else if (address == 0 && *(uint8_t *)data == 0x80) {
            memset(s->sockets, 0, sizeof(s->sockets));
            s->common[0] = s->stuck_reset ? 0x80 : 0;
        } else memcpy(s->common + address, data, length);
        return 0;
    }
    if (block == 2 || block == 3) {
        assert(address < 16384 && address + length <= 16384);
        uint8_t *memory = block == 2 ? s->tx : s->rx;
        if (write) memcpy(memory + address, data, length);
        else memcpy(data, memory + address, length);
        return 0;
    }
    assert(block >= 1 && block <= 29 && (block - 1) % 4 == 0);
    unsigned socket = (block - 1) / 4;
    assert(address + length <= sizeof(s->sockets[socket]));
    uint8_t *registers = s->sockets[socket];
    if (!write) {
        if (s->flap && socket == 0 && (address == 0x20 || address == 0x26)) {
            assert(length == 2);
            put_word(data, (uint16_t)(60 + (s->calls & 1)));
        } else memcpy(data, registers + address, length);
    } else if (address == 2 && length == 1) {
        registers[2] &= (uint8_t)~*(uint8_t *)data; /* W1C, not normal RAM. */
    } else if (address == 1 && length == 1) {
        uint8_t command = *(uint8_t *)data;
        registers[1] = s->stuck_command ? command : 0;
        if (s->stuck_command) return 0;
        if (command == 0x10) registers[3] = 0;
        if (command == 1) {
            ++s->opens;
            registers[3] = registers[0] == 4 ? 0x42 : 0;
            put_word(registers + 0x20, (uint16_t)registers[0x1f] * 1024u);
            put_word(registers + 0x22, 0); put_word(registers + 0x24, 0);
            put_word(registers + 0x26, 0); put_word(registers + 0x28, 0);
            s->rx_previous = 0;
        }
        if (command == 0x20) {
            ++s->sends;
            uint16_t begin = word(registers + 0x22), end = word(registers + 0x24);
            s->sent_length = (uint16_t)(end - begin);
            assert(s->sent_length <= sizeof(s->sent));
            for (size_t i = 0; i < s->sent_length; ++i) s->sent[i] = s->tx[(begin + i) & 0x3fff];
            if (!s->silent_send) {
                registers[2] |= 0x10;
                put_word(registers + 0x22, end);
            }
        }
        if (command == 0x40) {
            ++s->recv_commands;
            uint16_t end = word(registers + 0x28), consumed = (uint16_t)(end - s->rx_previous);
            uint16_t available = word(registers + 0x26);
            assert(consumed <= available);
            put_word(registers + 0x26, (uint16_t)(available - consumed));
            s->rx_previous = end;
        }
    } else memcpy(registers + address, data, length);
    return 0;
}

static uint64_t now(void *context) { return ((simulated_chip_t *)context)->now; }
static void sleep_time(void *context, unsigned n)
{
    simulated_chip_t *s = context;
    if (!s->frozen_clock) s->now += n;
}

static gamma_w5500_result_t initialize(simulated_chip_t *s, gamma_w5500_t *chip)
{
    memset(s, 0, sizeof(*s));
    s->common[0x39] = 4;
    s->common[0x2e] = 0xb1;
    const gamma_w5500_io_t io = {s, transfer, now, sleep_time};
    const uint8_t mac[] = {0x02, 0x12, 0x34, 0x56, 0x78, 0x9a};
    return gamma_w5500_initialize(chip, &io, mac);
}

static void add_packet(simulated_chip_t *s, uint16_t pointer, const uint8_t *frame, size_t length)
{
    uint16_t total = (uint16_t)(length + 2);
    s->rx[pointer & 0x3fff] = (uint8_t)(total >> 8);
    s->rx[(pointer + 1) & 0x3fff] = (uint8_t)total;
    for (size_t i = 0; i < length; ++i) s->rx[(pointer + 2 + i) & 0x3fff] = frame[i];
    put_word(s->sockets[0] + 0x28, pointer);
    put_word(s->sockets[0] + 0x26, total);
    s->rx_previous = pointer;
}

int main(void)
{
    simulated_chip_t s;
    gamma_w5500_t chip;
    assert(initialize(&s, &chip) == GAMMA_W5500_OK && chip.ready);
    assert(s.sockets[0][0] == 4 && s.sockets[0][3] == 0x42);
    assert(s.sockets[0][0x1e] == 16 && s.sockets[0][0x1f] == 16);
    assert(s.common[0x18] == 0 && s.sockets[0][0x2c] == 0x1f);
    for (unsigned socket = 1; socket < 8; ++socket)
        assert(s.sockets[socket][0x1e] == 0 && s.sockets[socket][0x1f] == 0);
    const uint8_t mac[] = {0x02, 0x12, 0x34, 0x56, 0x78, 0x9a};
    assert(memcmp(s.common + 9, mac, 6) == 0);

    uint8_t frame[1518], received[1520];
    for (size_t i = 0; i < sizeof(frame); ++i) frame[i] = (uint8_t)(i * 37 + 13);
    const uint16_t positions[] = {0, 0x3ff0, 0x3fff, 0xfffe};
    for (size_t n = 0; n < sizeof(positions) / sizeof(positions[0]); ++n) {
        assert(initialize(&s, &chip) == GAMMA_W5500_OK);
        uint16_t pointer = positions[n];
        put_word(s.sockets[0] + 0x22, pointer); put_word(s.sockets[0] + 0x24, pointer);
        assert(gamma_w5500_transmit(&chip, frame, sizeof(frame)) == GAMMA_W5500_OK);
        assert(s.sends == 1 && s.sent_length == sizeof(frame) && memcmp(frame, s.sent, sizeof(frame)) == 0);
        assert(word(s.sockets[0] + 0x24) == (uint16_t)(pointer + sizeof(frame)));
        add_packet(&s, pointer, frame, sizeof(frame));
        size_t length;
        memset(received, 0xa5, sizeof(received));
        assert(gamma_w5500_receive(&chip, received, 1518, &length) == GAMMA_W5500_OK);
        assert(length == sizeof(frame) && memcmp(frame, received, length) == 0);
        assert(received[1518] == 0xa5 && received[1519] == 0xa5);
        assert(word(s.sockets[0] + 0x28) == (uint16_t)(pointer + length + 2));
        assert(s.recv_commands == 1 && word(s.sockets[0] + 0x26) == 0);
        assert(gamma_w5500_receive(&chip, received, sizeof(received), &length) == GAMMA_W5500_AGAIN && length == 0);
    }
    assert(initialize(&s, &chip) == GAMMA_W5500_OK);
    assert(gamma_w5500_transmit(&chip, frame, 42) == GAMMA_W5500_OK && s.sent_length == 60);
    assert(memcmp(frame, s.sent, 42) == 0);
    for (unsigned i = 42; i < 60; ++i) assert(s.sent[i] == 0);
    assert(gamma_w5500_transmit(&chip, frame, 13) == GAMMA_W5500_INVALID);
    assert(gamma_w5500_transmit(&chip, frame, 1519) == GAMMA_W5500_INVALID);
    s.common[0x2e] &= 0xfe;
    assert(gamma_w5500_transmit(&chip, frame, 60) == GAMMA_W5500_LINK_DOWN && s.sends == 1);

    const uint16_t bad_headers[] = {0, 1, 2, 15, 1521, 0xffff};
    for (unsigned n = 0; n < sizeof(bad_headers) / sizeof(bad_headers[0]); ++n) {
        assert(initialize(&s, &chip) == GAMMA_W5500_OK);
        add_packet(&s, 0x3fff, frame, 60);
        s.rx[0x3fff] = (uint8_t)(bad_headers[n] >> 8); s.rx[0] = (uint8_t)bad_headers[n];
        size_t length = 999;
        memset(received, 0xa5, sizeof(received));
        assert(gamma_w5500_receive(&chip, received, sizeof(received), &length) == GAMMA_W5500_BAD_FRAME);
        assert(chip.ready && length == 0 && received[0] == 0xa5 && s.opens == 2);
    }
    assert(initialize(&s, &chip) == GAMMA_W5500_OK);
    add_packet(&s, 0, frame, 60);
    put_word(s.sockets[0] + 0x26, 20); /* Advertised record exceeds committed bytes. */
    size_t length;
    assert(gamma_w5500_receive(&chip, received, sizeof(received), &length) == GAMMA_W5500_BAD_FRAME);

    assert(initialize(&s, &chip) == GAMMA_W5500_OK);
    unsigned before = s.calls;
    s.flap = 1;
    assert(gamma_w5500_receive(&chip, received, sizeof(received), &length) == GAMMA_W5500_AGAIN);
    assert(s.calls - before <= 9 && chip.ready);
    assert(gamma_w5500_transmit(&chip, frame, 60) == GAMMA_W5500_AGAIN && s.sends == 0);
    s.flap = 0;
    put_word(s.sockets[0] + 0x20, 0);
    assert(gamma_w5500_transmit(&chip, frame, 60) == GAMMA_W5500_AGAIN && s.sends == 0);

    assert(initialize(&s, &chip) == GAMMA_W5500_OK);
    s.silent_send = true;
    assert(gamma_w5500_transmit(&chip, frame, 60) == GAMMA_W5500_TIMEOUT);
    assert(chip.ready && s.sends == 1 && s.opens == 2 && s.now <= 100);
    assert(initialize(&s, &chip) == GAMMA_W5500_OK);
    s.silent_send = s.frozen_clock = true;
    before = s.calls;
    assert(gamma_w5500_transmit(&chip, frame, 60) == GAMMA_W5500_TIMEOUT && s.calls - before < 150);

    assert(initialize(&s, &chip) == GAMMA_W5500_OK);
    s.fail_after = s.calls;
    bool link;
    assert(gamma_w5500_link(&chip, &link) == GAMMA_W5500_IO && !chip.ready);

    const gamma_w5500_io_t io = {&s, transfer, now, sleep_time};
    memset(&s, 0, sizeof(s)); s.missing = true;
    assert(gamma_w5500_initialize(&chip, &io, mac) == GAMMA_W5500_NOT_FOUND && s.writes == 0);
    memset(&s, 0, sizeof(s)); s.common[0x39] = 4; s.stuck_reset = true;
    assert(gamma_w5500_initialize(&chip, &io, mac) == GAMMA_W5500_TIMEOUT && s.now <= 20);
    memset(&s, 0, sizeof(s)); s.common[0x39] = 4; s.stuck_command = true;
    assert(gamma_w5500_initialize(&chip, &io, mac) == GAMMA_W5500_TIMEOUT && s.now <= 20);
    const uint8_t invalid_mac[6] = {1};
    assert(gamma_w5500_initialize(&chip, &io, invalid_mac) == GAMMA_W5500_INVALID);
    puts("W5500 original protocol: initialization, 16KiB/16-bit wrap, framing, bounds, stable reads, padding, link, IO and bounded timeout checks passed");
    return 0;
}
