#ifndef FIVE_TRATUM_MUX_PEER_STATUS_H
#define FIVE_TRATUM_MUX_PEER_STATUS_H

#include "five_tratum_mux_status.h"

uint64_t mux_peer_status_begin(bool fallback);
void mux_peer_status_disconnect(void);
void mux_peer_status_receive(uint64_t generation, bool valid, int64_t now_us);
FiveTratumMuxSnapshot mux_peer_status_snapshot(void);

#endif
