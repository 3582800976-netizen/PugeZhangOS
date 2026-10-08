/* 所有自检放在这里；正常的打印实现不承担验收流程。 */
#include "console.h"
#include "memory.h"
#include "selftest.h"

static int test_format(void)
{
    char buffer[160];
    const char *expected = "-2147483648 0 4294967295 ffffffff 0x80200000 A ok %";
    int n = console_snprintf(buffer, sizeof(buffer), "%d %u %u %x %p %c %s %%",
                             (-2147483647 - 1), 0u, 4294967295u, 0xffffffffu,
                             (void *)0x80200000UL, 'A', "ok");
    int failures = strcmp(buffer, expected) != 0 || n != (int)strlen(expected);
    console_snprintf(buffer, sizeof(buffer), "tail%");
    failures += strcmp(buffer, "tail%") != 0;
    console_snprintf(buffer, sizeof(buffer), "%ld %lu", (-9223372036854775807L - 1), ~0UL);
    failures += strcmp(buffer, "-9223372036854775808 18446744073709551615") != 0;
    console_snprintf(buffer, sizeof(buffer), "%s %q", (char *)0);
    failures += strcmp(buffer, "(null) %q") != 0;
    char tiny[4];
    n = console_snprintf(tiny, sizeof(tiny), "abcdef");
    failures += n != 6 || strcmp(tiny, "abc") != 0;
    failures += console_snprintf(0, 0, "abcdef") != 6;
    console_printf("[test] format %s\n", failures ? "FAIL" : "PASS");
    return failures;
}

void selftest_hart(uint64 id, int primary)
{
    (void)primary; /* 本阶段所有核心执行相同的并发打印检验。 */
    for (unsigned seq = 0; seq < 4; seq++)
        console_printf("[uart-test] hart=%lu seq=%u\n", id, seq);
}

int selftest_run(void)
{
    int failures = test_format();
    console_printf("[test] summary failures=%d\n", failures);
    return failures;
}
