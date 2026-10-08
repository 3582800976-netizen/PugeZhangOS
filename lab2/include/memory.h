#ifndef PG_MEMORY_H
#define PG_MEMORY_H
#include "types.h"
void *memset(void *dst, int value, uint64 size);
void *memcpy(void *dst, const void *src, uint64 size);
int strcmp(const char *a, const char *b);
uint64 strlen(const char *s);
#endif
