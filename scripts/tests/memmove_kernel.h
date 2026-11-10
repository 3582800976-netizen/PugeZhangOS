/* Included only by temporary QEMU test kernels, after their module headers. */
static void memmove_probe(void)
{
    uint8 before[24], actual[24], expected[24];
    int cases = 0;
    memmove(NULL, NULL, 0);
    for (uint32 seed = 0; seed < 2; seed++) {
        for (uint32 src = 0; src <= 16; src++) {
            for (uint32 dst = 0; dst <= 16; dst++) {
                uint32 limit = 16 - (src > dst ? src : dst);
                for (uint32 n = 0; n <= limit; n++) {
                    for (uint32 i = 0; i < sizeof(before); i++) {
                        before[i] = (uint8)(i * 37 + seed * 91);
                        actual[i] = expected[i] = before[i];
                    }
                    for (uint32 i = 0; i < n; i++)
                        expected[4 + dst + i] = before[4 + src + i];
                    memmove(actual + 4 + dst, actual + 4 + src, n);
                    for (uint32 i = 0; i < sizeof(actual); i++)
                        assert(actual[i] == expected[i], "memmove snapshot mismatch");
                    cases++;
                }
            }
        }
    }
    printf("[memmove] RV64 PASS cases=%d\n", cases);
}
