#ifndef WORK_SOURCE_STAMP_H
#define WORK_SOURCE_STAMP_H
#include <stdint.h>
/* Internal queued-work metadata, never a Stratum wire field. */
typedef struct {
    uint64_t session_epoch;
    uint64_t job_epoch;
    uint64_t received_at_us;
    uint8_t pool_index;
} work_source_stamp;
#endif
