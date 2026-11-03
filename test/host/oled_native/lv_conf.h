#include "../../../main/lv_conf.h"
/* Desktop harness: retain the target renderer, widgets, fonts and I1 format. */
#undef LV_USE_OS
#define LV_USE_OS LV_OS_NONE
#undef LV_MEM_POOL_INCLUDE
#undef LV_MEM_POOL_ALLOC
/* A regression must fail immediately instead of hanging in LVGL's default
 * assertion loop when the real shared target-sized pool is exhausted. */
#ifndef __ASSEMBLER__
#include <stdlib.h>
#endif
#define LV_ASSERT_HANDLER abort();
