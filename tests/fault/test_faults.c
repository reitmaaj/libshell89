#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "shell89_capture.h"
#include "shell89_fault.h"
#include "shell89_fixtures.h"
#include "shell89_support.h"

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

static void assert_no_children(void)
{
    int status;
    pid_t p;

    p = waitpid(-1, &status, WNOHANG);
    CHECK_EQ(p, -1);
    CHECK_EQ(errno, ECHILD);
}

static int run_script(shell89 *sh, const char *script, int use_null_io,
                      shell89_result **res)
{
    unsigned char *out;
    unsigned char *err;
    size_t out_len;
    size_t err_len;
    struct shell89_run_io io;
    int rc;

    if (use_null_io != 0)
    {
        io.stdin_fd = -1;
        io.stdout_fd = -1;
        io.stderr_fd = -1;
        return shell89_run(sh, script, &io, res);
    }
    rc = shell89_capture_run(sh, script, "abc", 3, &out, &out_len, &err,
                             &err_len, res);
    shell89_capture_free(out);
    shell89_capture_free(err);
    return rc;
}

static void exercise(shell89 *sh, int site, unsigned long nth,
                     const char *script, int use_null_io)
{
    shell89_result *res;
    int before;
    int after;
    int rc;

    res = NULL;
    before = open_count();
    shell89_test_fault_reset();
    shell89_test_fault_arm(site, nth);
    alarm(10);
    rc = run_script(sh, script, use_null_io, &res);
    alarm(0);
    shell89_test_fault_disarm();
    CHECK(rc == SHELL89_OK || rc == SHELL89_ENOMEM || rc == SHELL89_EINTERNAL);
    if (rc == SHELL89_OK)
    {
        CHECK(res != NULL);
        if (res != NULL)
        {
            CHECK(shell89_status(res) <= 255);
            shell89_result_destroy(res);
        }
    }
    else
    {
        CHECK(res == NULL);
    }
    after = open_count();
    CHECK_EQ(before, after);
    assert_no_children();
}

static void enumerate(shell89 *sh, int site, const char *script,
                      int use_null_io)
{
    unsigned long calls;
    unsigned long i;
    shell89_result *res;
    int rc;

    shell89_test_fault_reset();
    rc = run_script(sh, script, use_null_io, &res);
    CHECK_EQ(rc, SHELL89_OK);
    if (res != NULL)
    {
        shell89_result_destroy(res);
    }
    calls = shell89_test_fault_calls(site);
    if (calls > 256)
    {
        calls = 256;
    }
    for (i = 1; i <= calls; i++)
    {
        exercise(sh, site, i, script, use_null_io);
    }
}

int main(void)
{
    shell89 *sh;
    struct shell89_fixture_context ctx;
    const char *path;
    static const int sites[] = {SHELL89_TEST_MALLOC,
                                SHELL89_TEST_REALLOC,
                                SHELL89_TEST_PIPE,
                                SHELL89_TEST_FCNTL,
                                SHELL89_TEST_DUP,
                                SHELL89_TEST_POSIX_SPAWN,
                                SHELL89_TEST_PTHREAD_CREATE,
                                SHELL89_TEST_PTHREAD_JOIN,
                                SHELL89_TEST_WAITPID,
                                SHELL89_TEST_READ,
                                SHELL89_TEST_WRITE,
                                SHELL89_TEST_OPEN};
    size_t i;

    path = getenv("SHELL89_PROC_FIXTURE");
    if (path == NULL)
    {
        path = "./build/fixture_proc";
    }
    sh = shell89_create();
    ctx.counter = 0;
    shell89_fixture_register_native(sh, &ctx);
    shell89_fixture_register_process(sh, path);

    for (i = 0; i < sizeof sites / sizeof sites[0]; i++)
    {
        int site;

        site = sites[i];
        if (site == SHELL89_TEST_POSIX_SPAWN)
        {
            enumerate(sh, site, "proc copy", 0);
        }
        else if (site == SHELL89_TEST_OPEN)
        {
            enumerate(sh, site, "fn copy", 1);
        }
        else
        {
            enumerate(sh, site, "fn copy", 0);
        }
    }

    /* Process spawned, then native thread creation fails. */
    exercise(sh, SHELL89_TEST_PTHREAD_CREATE, 1, "proc copy | fn copy", 0);
    exercise(sh, SHELL89_TEST_PTHREAD_CREATE, 1, "fn copy | proc copy", 0);

    /* A native write can fail after a partial pipeline. */
    exercise(sh, SHELL89_TEST_WRITE, 1,
             "proc emit --size=\"1048576\" --seed=\"1\" | fn copy", 0);

    shell89_destroy(sh);
    return shell89_test_report("fault");
}
