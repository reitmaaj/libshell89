#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "shell89_capture.h"
#include "shell89_fixtures.h"
#include "shell89_support.h"

static unsigned char pattern(unsigned long offset, unsigned long seed)
{
    unsigned long v;

    v = offset * 131UL + seed * 17UL + (offset >> 8);
    return (unsigned char)(v & 0xffUL);
}

static int open_count(void)
{
    int i;
    int n;

    n = 0;
    for (i = 0; i < 256; i++)
    {
        if (fcntl(i, F_GETFD) >= 0)
        {
            n++;
        }
    }
    return n;
}

struct concurrent_job
{
    const char *path;
    int ok;
};

static void *concurrent_run(void *arg)
{
    struct concurrent_job *job;
    shell89 *sh;
    struct shell89_fixture_context ctx;
    shell89_result *res;
    unsigned char *out;
    unsigned char *err;
    size_t out_len;
    size_t err_len;

    job = (struct concurrent_job *)arg;
    ctx.counter = 0;
    sh = shell89_create();
    if (sh == NULL)
    {
        return NULL;
    }
    shell89_fixture_register_native(sh, &ctx);
    shell89_fixture_register_process(sh, job->path);
    if (shell89_capture_run(sh,
                            "fn emit --size=\"4096\" --seed=\"1\" | "
                            "proc copy | fn count",
                            "", 0, &out, &out_len, &err, &err_len,
                            &res) == SHELL89_OK)
    {
        job->ok = shell89_status(res) == 0 && out_len == 5 &&
                  memcmp(out, "4096\n", 5) == 0;
        shell89_result_destroy(res);
        shell89_capture_free(out);
        shell89_capture_free(err);
    }
    shell89_destroy(sh);
    return NULL;
}

int main(void)
{
    shell89 *sh;
    struct shell89_fixture_context ctx;
    shell89_result *res;
    unsigned char *out;
    unsigned char *err;
    size_t out_len;
    size_t err_len;
    const char *path;
    char script[8192];
    char *p;
    size_t i;
    int before;
    int after;
    struct shell89_run_io null_io;
    struct concurrent_job jobs[16];
    pthread_t threads[16];

    path = getenv("SHELL89_PROC_FIXTURE");
    if (path == NULL)
    {
        path = "./build/fixture_proc";
    }
    alarm(60);
    sh = shell89_create();
    ctx.counter = 0;
    shell89_fixture_register_native(sh, &ctx);
    shell89_fixture_register_process(sh, path);

    CHECK_EQ(
        shell89_capture_run(
            sh,
            "fn emit --size=\"67108864\" --seed=\"1\" | proc copy | fn count",
            "", 0, &out, &out_len, &err, &err_len, &res),
        SHELL89_OK);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(out_len, 9);
    CHECK_EQ(memcmp(out, "67108864\n", 9), 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    p = script;
    strcpy(p, "fn emit --size=\"1\" --seed=\"1\"");
    p += strlen(p);
    for (i = 0; i < 14; i++)
    {
        strcpy(p, i == 0 ? " | proc copy" : " | fn copy");
        p += strlen(p);
    }
    strcpy(p, " | fn count");
    CHECK_EQ(shell89_capture_run(sh, script, "", 0, &out, &out_len, &err,
                                 &err_len, &res),
             SHELL89_OK);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(out_len, 2);
    CHECK_EQ(memcmp(out, "1\n", 2), 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    p = script;
    for (i = 0; i < 64; i++)
    {
        strcpy(p, i == 0 ? "fn state" : "; fn state");
        p += strlen(p);
    }
    CHECK_EQ(shell89_capture_run(sh, script, "", 0, &out, &out_len, &err,
                                 &err_len, &res),
             SHELL89_OK);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(ctx.counter, 64);
    CHECK_EQ(out_len, 64 * 6);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    null_io.stdin_fd = -1;
    null_io.stdout_fd = -1;
    null_io.stderr_fd = -1;
    before = open_count();
    for (i = 0; i < 10000; i++)
    {
        CHECK_EQ(shell89_run(sh, "", &null_io, &res), SHELL89_OK);
        CHECK_EQ(shell89_status(res), 0);
        shell89_result_destroy(res);
    }
    after = open_count();
    CHECK_EQ(before, after);

    CHECK_EQ(shell89_capture_run(sh,
                                 "fn emit --size=\"1048576\" --seed=\"1\" | "
                                 "proc slow --chunk=\"4096\" --delay=\"1000\"",
                                 "", 0, &out, &out_len, &err, &err_len, &res),
             SHELL89_OK);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(out_len, 1048576);
    if (out_len == 1048576)
    {
        for (i = 0; i < out_len; i++)
        {
            if (out[i] != pattern(i, 1))
            {
                CHECK(0);
                break;
            }
        }
    }
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    for (i = 0; i < 100; i++)
    {
        CHECK_EQ(
            shell89_capture_run(sh,
                                "proc emit --size=\"1048576\" --seed=\"1\" | "
                                "fn partial --size=\"64\"",
                                "", 0, &out, &out_len, &err, &err_len, &res),
            SHELL89_OK);
        shell89_result_destroy(res);
        shell89_capture_free(out);
        shell89_capture_free(err);
    }

    for (i = 0; i < 16; i++)
    {
        jobs[i].path = path;
        jobs[i].ok = 0;
        CHECK_EQ(pthread_create(&threads[i], NULL, concurrent_run, &jobs[i]),
                 0);
    }
    for (i = 0; i < 16; i++)
    {
        CHECK_EQ(pthread_join(threads[i], NULL), 0);
        CHECK_EQ(jobs[i].ok, 1);
    }

    shell89_destroy(sh);
    alarm(0);
    return shell89_test_report("stress");
}
