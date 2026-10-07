#include "mux_peer_status.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static FiveTratumMuxPeer peer;

uint64_t mux_peer_status_begin(bool fallback)
{
    taskENTER_CRITICAL(&mux);
    uint64_t generation = five_tratum_mux_begin(&peer, fallback);
    taskEXIT_CRITICAL(&mux);
    return generation;
}

void mux_peer_status_disconnect(void)
{
    taskENTER_CRITICAL(&mux);
    five_tratum_mux_disconnect(&peer);
    taskEXIT_CRITICAL(&mux);
}

void mux_peer_status_receive(uint64_t generation, bool valid, int64_t now_us)
{
    taskENTER_CRITICAL(&mux);
    five_tratum_mux_receive(&peer, generation, valid, now_us);
    taskEXIT_CRITICAL(&mux);
}

FiveTratumMuxSnapshot mux_peer_status_snapshot(void)
{
    taskENTER_CRITICAL(&mux);
    int64_t now_us = esp_timer_get_time();
    FiveTratumMuxSnapshot result = five_tratum_mux_snapshot(&peer, now_us);
    taskEXIT_CRITICAL(&mux);
    return result;
}
