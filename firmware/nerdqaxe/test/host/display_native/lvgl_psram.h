#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void *display_host_alloc(size_t size);
void display_host_free(void *ptr);
void *display_host_realloc(void *ptr,size_t size);
size_t display_host_live_bytes(void);
size_t display_host_peak_bytes(void);
#ifdef __cplusplus
}
#endif
static inline void *lv_psram_alloc(size_t size) { return display_host_alloc(size); }
static inline void lv_psram_free(void *ptr) { display_host_free(ptr); }
static inline void *lv_psram_realloc(void *ptr, size_t size) { return display_host_realloc(ptr,size); }
