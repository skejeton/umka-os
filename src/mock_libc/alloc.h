#pragma once
#include <stddef.h>

void allocatorInit(void *start);
void *allocatorRealloc(void *ptr, size_t size);
void *allocatorAlloc(size_t size);
void allocatorFree(void *ptr);