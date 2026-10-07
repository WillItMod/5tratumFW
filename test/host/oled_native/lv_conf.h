#include "../../../main/lv_conf.h"
/* Desktop harness: retain the target renderer, widgets, fonts and I1 format. */
#undef LV_USE_OS
#define LV_USE_OS LV_OS_NONE
#undef LV_MEM_POOL_INCLUDE
#undef LV_MEM_POOL_ALLOC
