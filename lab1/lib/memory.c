#include "memory.h"

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
