#include <stdlib.h>
#include <string.h>

#include "shell89_capture.h"
#include "shell89_fixtures.h"
#include "shell89_support.h"

static unsigned char pattern(unsigned long offset, unsigned long seed)
{
    unsigned long v;

    v = offset * 131UL + seed * 17UL + (offset >> 8);
    return (unsigned char)(v & 0xffUL);
}

static void fill_pattern(unsigned char *p, size_t n, unsigned long seed)
{
    size_t i;

    for (i = 0; i < n; i++)
    {
        p[i] = pattern((unsigned long)i, seed);
    }
}

static void run_compare(shell89 *sh, const char *source, size_t size)
{
    shell89_result *res;
    unsigned char *out;
    unsigned char *err;
    unsigned char *want;
    size_t out_len;
    size_t err_len;

    CHECK_EQ(shell89_capture_run(sh, source, "", 0, &out, &out_len, &err,
                                 &err_len, &res),
             SHELL89_OK);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(out_len, size);
    want = (unsigned char *)malloc(size == 0 ? 1 : size);
    CHECK(want != NULL);
    fill_pattern(want, size, 1);
    if (out_len == size)
    {
        CHECK_EQ(memcmp(out, want, size), 0);
    }
    free(want);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);
}

static void run_count(shell89 *sh, const char *source, size_t size)
{
    shell89_result *res;
    unsigned char *out;
    unsigned char *err;
    size_t out_len;
    size_t err_len;
    char want[64];
    int n;

    CHECK_EQ(shell89_capture_run(sh, source, "", 0, &out, &out_len, &err,
                                 &err_len, &res),
             SHELL89_OK);
    CHECK_EQ(shell89_status(res), 0);
    n = sprintf(want, "%lu\n", (unsigned long)size);
    CHECK_EQ(out_len, (size_t)n);
    CHECK_EQ(memcmp(out, want, (size_t)n), 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);
}

int main(void)
{
    shell89 *sh;
    struct shell89_fixture_context ctx;
    const char *path;
    static const char *const groups[] = {"fn", "proc", "fn", "proc"};
    size_t i;
    size_t j;
    size_t k;
    shell89_result *res;
    unsigned char *out;
    unsigned char *err;
    size_t out_len;
    size_t err_len;

    path = getenv("SHELL89_PROC_FIXTURE");
    if (path == NULL)
    {
        path = "./build/fixture_proc";
    }
    sh = shell89_create();
    ctx.counter = 0;
    shell89_fixture_register_native(sh, &ctx);
    shell89_fixture_register_process(sh, path);

    for (i = 0; i < 4; i++)
    {
        char script[256];
        char consumer[32];
        strcpy(consumer, groups[i]);
        strcat(consumer, " copy");
        sprintf(script, "%s emit --size=\"65536\" --seed=\"1\" | %s",
                groups[i / 2], consumer);
        run_compare(sh, script, 65536);
    }

    for (i = 0; i < 8; i++)
    {
        char script[512];
        const char *a;
        const char *b;
        const char *c;

        a = groups[(i >> 2) & 1];
        b = groups[(i >> 1) & 1];
        c = groups[i & 1];
        sprintf(script,
                "%s emit --size=\"100000\" --seed=\"1\" | %s copy | %s count",
                a, b, c);
        run_count(sh, script, 100000);
    }

    run_compare(sh, "proc emit --size=\"1048576\" --seed=\"1\" | fn copy",
                1048576);
    run_compare(sh, "fn emit --size=\"1048576\" --seed=\"1\" | proc copy",
                1048576);
    run_count(sh,
              "fn emit --size=\"1048576\" --seed=\"1\" | proc copy | fn count",
              1048576);

    for (j = 0; j < 2; j++)
    {
        for (k = 0; k < 2; k++)
        {
            char script[256];
            sprintf(script,
                    "%s emit --size=\"1048576\" --seed=\"1\" | %s partial "
                    "--size=\"1024\"",
                    groups[j], groups[k]);
            CHECK_EQ(shell89_capture_run(sh, script, "", 0, &out, &out_len,
                                         &err, &err_len, &res),
                     SHELL89_OK);
            CHECK(shell89_status(res) == 0 || shell89_status(res) == 1 ||
                  shell89_status(res) == 141);
            CHECK_EQ(shell89_stage_status(res, 0, 1), 0);
            shell89_result_destroy(res);
            shell89_capture_free(out);
            shell89_capture_free(err);
        }
    }

    CHECK_EQ(shell89_capture_run(sh,
                                 "fn emit --size=\"32\" --seed=\"1\" |\n"
                                 "    proc copy\n"
                                 "    | fn count",
                                 "", 0, &out, &out_len, &err, &err_len, &res),
             SHELL89_OK);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(out_len, 3);
    CHECK_EQ(memcmp(out, "32\n", 3), 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    shell89_destroy(sh);
    return shell89_test_report("integration_pipelines");
}
