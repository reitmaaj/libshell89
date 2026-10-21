#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "shell89_capture.h"
#include "shell89_fixtures.h"
#include "shell89_support.h"

static const char *proc_path(void)
{
    const char *p;

    p = getenv("SHELL89_PROC_FIXTURE");
    return p != NULL ? p : "./build/fixture_proc";
}

static void make_executable_file(char *path, size_t cap)
{
    int fd;

    strncpy(path, "/tmp/shell89_noexec_XXXXXX", cap - 1);
    path[cap - 1] = '\0';
    fd = mkstemp(path);
    CHECK(fd >= 0);
    if (fd >= 0)
    {
        close(fd);
        chmod(path, 0644);
    }
}

int main(void)
{
    shell89 *sh;
    shell89_result *res;
    unsigned char *out;
    unsigned char *err;
    size_t out_len;
    size_t err_len;
    struct shell89_process p;
    const char *env_argv[] = {"env", NULL};
    const char *env[] = {"LC_ALL=C", NULL};
    const char *noenv[] = {NULL};
    char noexec[64];
    int rc;

    make_executable_file(noexec, sizeof noexec);
    sh = shell89_create();
    shell89_fixture_register_process(sh, proc_path());

    p.path = proc_path();
    p.fixed_argv = env_argv;
    p.envp = env;
    CHECK_EQ(shell89_register_process(sh, "p", "envset", NULL, &p), SHELL89_OK);
    p.envp = noenv;
    CHECK_EQ(shell89_register_process(sh, "p", "envnull", NULL, &p),
             SHELL89_OK);
    p.path = "./build/definitely_missing_proc";
    p.envp = env;
    CHECK_EQ(shell89_register_process(sh, "p", "missing", NULL, &p),
             SHELL89_OK);
    p.path = noexec;
    CHECK_EQ(shell89_register_process(sh, "p", "noexec", NULL, &p), SHELL89_OK);

    rc = shell89_capture_run(
        sh, "proc args --gamma=\"g\" --beta --alpha=\"a b;|$`\"", "", 0, &out,
        &out_len, &err, &err_len, &res);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(out_len, strlen("alpha=a b;|$`\nbeta=1\ngamma=g\n"));
    CHECK_EQ(memcmp(out, "alpha=a b;|$`\nbeta=1\ngamma=g\n", out_len), 0);
    CHECK_EQ(shell89_stage_kind(res, 0, 0), SHELL89_STAGE_PROCESS);
    CHECK_EQ(shell89_stage_termination(res, 0, 0), SHELL89_TERM_EXIT);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    rc = shell89_capture_run(sh, "p envset", "", 0, &out, &out_len, &err,
                             &err_len, &res);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(out_len, strlen("LC_ALL=C\nPATH=\n"));
    CHECK_EQ(memcmp(out, "LC_ALL=C\nPATH=\n", out_len), 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    rc = shell89_capture_run(sh, "p envnull", "", 0, &out, &out_len, &err,
                             &err_len, &res);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK_EQ(out_len, strlen("LC_ALL=\nPATH=\n"));
    CHECK_EQ(memcmp(out, "LC_ALL=\nPATH=\n", out_len), 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    rc = shell89_capture_run(sh, "proc fail --status=\"7\"", "", 0, &out,
                             &out_len, &err, &err_len, &res);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK_EQ(shell89_status(res), 7);
    CHECK_EQ(shell89_stage_termination(res, 0, 0), SHELL89_TERM_EXIT);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    rc = shell89_capture_run(sh, "proc fail --status=\"127\"", "", 0, &out,
                             &out_len, &err, &err_len, &res);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK_EQ(shell89_status(res), 127);
    CHECK_EQ(shell89_stage_termination(res, 0, 0), SHELL89_TERM_EXIT);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    rc = shell89_capture_run(sh, "proc signal --sig=\"15\"", "", 0, &out,
                             &out_len, &err, &err_len, &res);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK_EQ(shell89_status(res), 128 + 15);
    CHECK_EQ(shell89_stage_termination(res, 0, 0), SHELL89_TERM_SIGNAL);
    CHECK_EQ(shell89_stage_signal(res, 0, 0), 15);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    rc = shell89_capture_run(sh, "p missing", "", 0, &out, &out_len, &err,
                             &err_len, &res);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK_EQ(shell89_status(res), 127);
    CHECK_EQ(shell89_stage_termination(res, 0, 0), SHELL89_TERM_SHELL);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    rc = shell89_capture_run(sh, "p noexec", "", 0, &out, &out_len, &err,
                             &err_len, &res);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK_EQ(shell89_status(res), 126);
    CHECK_EQ(shell89_stage_termination(res, 0, 0), SHELL89_TERM_SHELL);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    rc = shell89_capture_run(sh,
                             "proc emit --size=\"1048576\" --seed=\"1\" | proc "
                             "partial --size=\"1024\"",
                             "", 0, &out, &out_len, &err, &err_len, &res);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK(shell89_status(res) == 0 || shell89_status(res) == 141);
    CHECK_EQ(shell89_pipeline_count(res), 1);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    shell89_destroy(sh);
    unlink(noexec);
    return shell89_test_report("integration_process");
}
