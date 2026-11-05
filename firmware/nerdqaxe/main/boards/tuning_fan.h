#pragma once

#include <stdint.h>

// One power-owner acquisition. A false result never authorizes an adjustment.
struct TuningFanSample {
    bool valid = false;
    uint16_t rpm = 0;
    const char *reason = "Fan reading is unavailable";
};
