#pragma once
#include <ArduinoJson.h>
struct PSRAMAllocator:ArduinoJson::Allocator {void *allocate(size_t n) override{return malloc(n);}void deallocate(void *p)override{free(p);}void *reallocate(void *p,size_t n)override{return realloc(p,n);}};
