#include "memory.h"

void *memset(void *dst, int value, uint64 size)
{
    uint8 *p = dst;
    for (uint64 i = 0; i < size; i++) p[i] = (uint8)value;
    return dst;
}

void *memcpy(void *dst, const void *src, uint64 size)
{
    uint8 *d = dst;
    const uint8 *s = src;
    for (uint64 i = 0; i < size; i++) d[i] = s[i];
    return dst;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (uint8)*a - (uint8)*b;
}

uint64 strlen(const char *s)
{
    uint64 n = 0;
    while (s[n]) n++;
    return n;
}
