#pragma once

#include <stdint.h>

// Metadata about the exact forwarded mining.notify. It describes the pool's
// candidate block, never a separate explorer/node tip or an ASIC assignment.
struct MuxWorkContext {
    bool available = false;
    char jobId[65] = {};
    char nBits[9] = {};
    bool heightAvailable = false;
    uint32_t height = 0;
    bool difficultyAvailable = false;
    double networkDifficulty = 0; // Bitcoin-reference bdiff, not coin-node normalization.
    uint32_t ageSeconds = 0;
};
