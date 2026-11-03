#ifndef WORK_DIFFICULTY_H
#define WORK_DIFFICULTY_H
#include <stdbool.h>
#include <stdint.h>
#include "work_source_stamp.h"

typedef struct {
    bool selected, connected, available;
    double share_difficulty, network_difficulty;
    uint64_t received_at_us;
} WorkDifficultyPool;
typedef struct { WorkDifficultyPool pools[2]; uint64_t now_us; } WorkDifficultySnapshot;

/* Metadata only: none of these functions touches a pool, ASIC or saved setting. */
void work_difficulty_select(unsigned pool);
uint64_t work_difficulty_connect(unsigned pool);
void work_difficulty_disconnect(void);
void work_difficulty_invalidate_dispatch(void);
void work_difficulty_forget_job(unsigned pool, uint64_t session);
work_source_stamp work_difficulty_received(unsigned pool, uint64_t session, uint64_t received_at_us);
uint64_t work_difficulty_prepare(const work_source_stamp *stamp);
bool work_difficulty_dispatched(const work_source_stamp *stamp, uint64_t generation,
                               double actual_pool_diff, uint32_t actual_target, bool work_allowed);
void work_difficulty_snapshot(WorkDifficultySnapshot *out, uint64_t now_us, bool work_allowed);
bool work_difficulty_target(uint32_t bits, double *out);
#endif
