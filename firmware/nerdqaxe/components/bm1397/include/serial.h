#ifndef SERIAL_H_
#define SERIAL_H_

#include <stdint.h>
#include <stdbool.h>

#define CHUNK_SIZE 1024

int SERIAL_send(uint8_t *, int);
#if defined(FIVETRATUM_BM1370_CAPTURE) && FIVETRATUM_BM1370_CAPTURE
#include "bm1370_capture.h"
int SERIAL_send_job(uint8_t *, int, const BM1370Capture::JobMetadata &, uint64_t);
#endif
void SERIAL_init(void);
int16_t SERIAL_rx(uint8_t *, uint16_t, uint16_t);
void SERIAL_clear_buffer(void);
void SERIAL_set_baud(int baud);
bool SERIAL_set_baud_checked(int baud);
bool SERIAL_wait_tx_idle(uint16_t timeout_ms);
bool SERIAL_clear_buffer_checked(void);

#endif /* SERIAL_H_ */
