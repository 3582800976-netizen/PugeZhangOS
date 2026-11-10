#include <stddef.h>
#include <stdio.h>

/* Link this symbol to the course's actual utils.c, not a host implementation. */
void course_memmove(void *dst, const void *src, unsigned int n);

enum { MAX_CAPACITY = 8192, GUARD = 8, TOTAL = MAX_CAPACITY + 2 * GUARD };
static unsigned char before[TOTAL], actual[TOTAL], expected[TOTAL];
static unsigned long cases;

static int check_case(unsigned int capacity, unsigned int src,
                      unsigned int dst, unsigned int count, unsigned int seed)
{
    unsigned int total = capacity + 2 * GUARD;
    for (unsigned int i = 0; i < total; i++) {
        before[i] = (unsigned char)(i * 37 + (i >> 8) * 19 + seed * 91);
        actual[i] = expected[i] = before[i];
    }
    /* Build the expected result from an immutable snapshot of the source. */
    for (unsigned int i = 0; i < count; i++)
        expected[GUARD + dst + i] = before[GUARD + src + i];
    course_memmove(actual + GUARD + dst, actual + GUARD + src, count);
    for (unsigned int i = 0; i < total; i++) {
        if (actual[i] != expected[i]) {
            fprintf(stderr, "FAIL src=%u dst=%u n=%u byte=%u actual=%u expected=%u\n",
                    src, dst, count, i, actual[i], expected[i]);
            return 1;
        }
    }
    cases++;
    return 0;
}

int main(void)
{
    unsigned char byte = 0xa5;
    course_memmove(NULL, NULL, 0);
    course_memmove(NULL, &byte, 0);
    course_memmove(&byte, NULL, 0);
    if (byte != 0xa5)
        return 1;

    /* Both overlap directions, self-copy, adjacency, and zero-length copies. */
    for (unsigned int seed = 0; seed < 2; seed++) {
        for (unsigned int src = 0; src <= 64; src++) {
            for (unsigned int dst = 0; dst <= 64; dst++) {
                unsigned int limit = 64 - (src > dst ? src : dst);
                for (unsigned int n = 0; n <= limit; n++)
                    if (check_case(64, src, dst, n, seed))
                        return 1;
            }
        }
    }
    if (check_case(MAX_CAPACITY, 0, 1, 4097, 0) ||
        check_case(MAX_CAPACITY, 1, 0, 4097, 1) ||
        check_case(MAX_CAPACITY, 0, 4096, 4096, 0) ||
        check_case(MAX_CAPACITY, 4096, 0, 4096, 1))
        return 1;

    /* Separate objects also exercise both address orders. */
    unsigned char left[32], right[32];
    for (unsigned int i = 0; i < sizeof(left); i++) {
        left[i] = (unsigned char)(i + 128);
        right[i] = 0;
    }
    course_memmove(right, left, sizeof(left));
    for (unsigned int i = 0; i < sizeof(left); i++)
        if (right[i] != (unsigned char)(i + 128))
            return 1;
    for (unsigned int i = 0; i < sizeof(right); i++)
        right[i] = (unsigned char)(255 - i);
    course_memmove(left, right, sizeof(right));
    for (unsigned int i = 0; i < sizeof(left); i++)
        if (left[i] != (unsigned char)(255 - i))
            return 1;
    printf("memmove: PASS (%lu snapshot cases, null/zero-length and separate objects)\n",
           cases);
    return 0;
}
