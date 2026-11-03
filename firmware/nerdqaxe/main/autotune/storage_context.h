#pragma once
#include "esp_cpu.h"
#include "esp_memory_utils.h"

namespace FiveTratumAutotune {
// Flash operations can disable the PSRAM cache. A DRAM temporary buffer does
// not make an external task stack safe; reject that caller before any NVS API.
inline bool storageContextAllowed() {
    return esp_ptr_in_dram(reinterpret_cast<const void *>(esp_cpu_get_sp()));
}
}
