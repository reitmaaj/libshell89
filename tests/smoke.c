#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shell89_fixtures.h"
#include "shell89_support.h"

static int run_capture(shell89 *sh, const char *source, const void *input,
                       size_t input_len, char *out, size_t out_cap,
                       char *err, size_t err_cap, shell89_result **result)
{
    FILE *in;
    FILE *output;
    FILE *errors;
    struct shell89_run_io io;
    size_t n;
    int rc;

    in = tmpfile();
    output = tmpfile();
    errors = tmpfile();
    if (in == NULL || output == NULL || errors == NULL)
    {
        return -1;
    }
    if (input_len != 0)
    {
        fwrite(input, 1, input_len, in);
    }
    fflush(in);
    rewind(in);
    io.stdin_fd = fileno(in);
    io.stdout_fd = fileno(output);
    io.stderr_fd = fileno(errors);
    rc = shell89_run(sh, source, &io, result);
    out[0] = '\0';
    err[0] = '\0';
    fflush(output);
    fflush(errors);
    rewind(output);
    rewind(errors);
    n = fread(out, 1, out_cap - 1, output);
    out[n] = '\0';
    n = fread(err, 1, err_cap - 1, errors);
    err[n] = '\0';
    fclose(in);
    fclose(output);
    fclose(errors);
    return rc;
}

int main(void)
{
    shell89 *sh;
    shell89_result *result;
    struct shell89_fixture_context ctx;
    const char *proc_path;
    char out[128];
    char err[128];
    int rc;

    proc_path = getenv("SHELL89_PROC_FIXTURE");
    if (proc_path == NULL)
    {
        proc_path = "./build/fixture_proc";
    }
    ctx.counter = 0;
    sh = shell89_create();
    CHECK(sh != NULL);
    shell89_fixture_register_native(sh, &ctx);
    shell89_fixture_register_process(sh, proc_path);
    rc = run_capture(sh, "fn emit --size=\"16\" --seed=\"1\" | fn upper | fn count",
                     "", 0, out, sizeof out, err, sizeof err, &result);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK(result != NULL);
    if (result != NULL)
    {
        CHECK_EQ(shell89_status(result), 0);
        CHECK_EQ(shell89_pipeline_count(result), 1);
        CHECK_EQ(shell89_stage_count(result, 0), 3);
        CHECK_STR(out, "16\n");
        shell89_result_destroy(result);
    }
    rc = run_capture(sh, "proc emit --size=\"16\" --seed=\"1\" | fn copy", "", 0, out,
                     sizeof out, err, sizeof err, &result);
    CHECK_EQ(rc, SHELL89_OK);
    if (result != NULL)
    {
        CHECK_EQ(shell89_status(result), 0);
        CHECK_EQ(strlen(out), 16);
        shell89_result_destroy(result);
    }
    shell89_destroy(sh);
    return shell89_test_report("smoke");
}
