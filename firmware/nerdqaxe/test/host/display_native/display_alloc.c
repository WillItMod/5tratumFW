// Host accounting for the unchanged firmware custom-allocation policy. Counts
// requested bytes, not desktop allocator headers or physical ESP heap capacity.
#include "lvgl_psram.h"
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
typedef union {max_align_t alignment;size_t bytes;} Header;
static size_t live_bytes,peak_bytes;
static void account(size_t old,size_t current) {
    assert(live_bytes>=old);live_bytes=live_bytes-old+current;
    if(live_bytes>peak_bytes)peak_bytes=live_bytes;
}
void *display_host_alloc(size_t size) {
    if(!size || size>SIZE_MAX-sizeof(Header))return NULL;
    Header *p=malloc(sizeof(Header)+size);if(!p)return NULL;
    p->bytes=size;account(0,size);return p+1;
}
void display_host_free(void *ptr) {
    if(!ptr)return;
    Header *p=(Header *)ptr-1;
    account(p->bytes,0);free(p);
}
void *display_host_realloc(void *ptr,size_t size) {
    if(!ptr)return display_host_alloc(size);
    if(!size){display_host_free(ptr);return NULL;}
    if(size>SIZE_MAX-sizeof(Header))return NULL;
    Header *old=(Header *)ptr-1;const size_t prior=old->bytes;
    Header *p=realloc(old,sizeof(Header)+size);if(!p)return NULL;
    p->bytes=size;account(prior,size);return p+1;
}
size_t display_host_live_bytes(void){return live_bytes;}
size_t display_host_peak_bytes(void){return peak_bytes;}
