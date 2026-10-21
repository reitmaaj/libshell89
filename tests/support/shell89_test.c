#include <stdio.h>
#include <string.h>

#include "shell89_support.h"

int shell89_test_failures = 0;

void shell89_test_fail(const char *file, int line, const char *message)
{
    shell89_test_failures++;
    fprintf(stderr, "%s:%d: assertion failed: %s\n", file, line, message);
}

int shell89_test_report(const char *name)
{
    if (shell89_test_failures == 0)
    {
        printf("%s: ok\n", name);
        return 0;
    }
    fprintf(stderr, "%s: %d failure(s)\n", name, shell89_test_failures);
    return 1;
}
