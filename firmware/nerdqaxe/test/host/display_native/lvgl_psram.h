#pragma once
#include <stdlib.h>
static inline void *lv_psram_alloc(size_t size) { return size ? malloc(size) : NULL; }
static inline void lv_psram_free(void *ptr) { free(ptr); }
static inline void *lv_psram_realloc(void *ptr, size_t size) { return realloc(ptr, size); }
