#include <stdlib.h>
#include <string.h>

#include "shell89_capture.h"
#include "shell89_fixtures.h"
#include "shell89_support.h"

static int calls = 0;

static int count_call(void *ctx, const shell89_args *args, shell89_io *io)
{
    (void)ctx;
    (void)args;
    (void)io;
    calls++;
    return 0;
}

static const struct shell89_option required_options[] = {
    {"path", SHELL89_STRING, 1}, {"flag", SHELL89_FLAG, 0}, {NULL, 0, 0}};

static void test_registration(void)
{
    shell89 *sh;
    const struct shell89_option bad_type[] = {{"x", 99, 0}, {NULL, 0, 0}};
    const struct shell89_option required_flag[] = {{"x", SHELL89_FLAG, 1},
                                                   {NULL, 0, 0}};
    const struct shell89_option dup[] = {
        {"x", SHELL89_STRING, 0}, {"x", SHELL89_FLAG, 0}, {NULL, 0, 0}};
    char long_name[80];
    struct shell89_process p;

    memset(long_name, 'a', sizeof long_name - 1);
    long_name[sizeof long_name - 1] = '\0';
    p.path = "/bin/true";
    p.fixed_argv = NULL;
    p.envp = NULL;

    sh = shell89_create();
    CHECK(sh != NULL);
    CHECK_EQ(shell89_register_function(sh, "g", "op", required_options,
                                       count_call, NULL),
             SHELL89_OK);
    CHECK_EQ(shell89_register_function(sh, "g", "op", required_options,
                                       count_call, NULL),
             SHELL89_EINVAL);
    CHECK_EQ(shell89_register_function(sh, "G", "op", required_options,
                                       count_call, NULL),
             SHELL89_EINVAL);
    CHECK_EQ(shell89_register_function(sh, "g", "", required_options,
                                       count_call, NULL),
             SHELL89_EINVAL);
    CHECK_EQ(
        shell89_register_function(sh, "g", "bad", bad_type, count_call, NULL),
        SHELL89_EINVAL);
    CHECK_EQ(shell89_register_function(sh, "g", "reqflag", required_flag,
                                       count_call, NULL),
             SHELL89_EINVAL);
    CHECK_EQ(shell89_register_function(sh, "g", "dup", dup, count_call, NULL),
             SHELL89_EINVAL);
    CHECK_EQ(shell89_register_function(sh, "g", "nofn", required_options, NULL,
                                       NULL),
             SHELL89_EINVAL);
    CHECK_EQ(shell89_register_function(sh, long_name, "op", required_options,
                                       count_call, NULL),
             SHELL89_EINVAL);
    CHECK_EQ(shell89_register_process(sh, "p", "x", NULL, NULL),
             SHELL89_EINVAL);
    p.path = "/bin/true/";
    CHECK_EQ(shell89_register_process(sh, "p", "x", NULL, &p), SHELL89_EINVAL);
    p.path = "/bin/true";
    CHECK_EQ(shell89_register_process(sh, "p", "x", NULL, &p), SHELL89_OK);
    shell89_destroy(sh);
}

static void test_validation(void)
{
    shell89 *sh;
    shell89_result *res;
    unsigned char *out;
    unsigned char *err;
    size_t out_len;
    size_t err_len;
    struct shell89_fixture_context ctx;
    int rc;

    sh = shell89_create();
    ctx.counter = 0;
    shell89_fixture_register_native(sh, &ctx);

    calls = 0;
    rc = shell89_capture_run(sh, "unknown op", "", 0, &out, &out_len, &err,
                             &err_len, &res);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK_EQ(shell89_status(res), 127);
    CHECK_EQ(shell89_pipeline_count(res), 0);
    CHECK_EQ(shell89_error_kind(res), SHELL89_ERROR_UNKNOWN_COMMAND);
    CHECK_EQ(calls, 0);
    CHECK_EQ(out_len, 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    rc = shell89_capture_run(sh, "fn state; fn count --bogus=\"x\"", "", 0,
                             &out, &out_len, &err, &err_len, &res);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK_EQ(shell89_status(res), 2);
    CHECK_EQ(shell89_error_kind(res), SHELL89_ERROR_ARGUMENT);
    CHECK_EQ(ctx.counter, 0);
    CHECK_EQ(out_len, 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    rc = shell89_capture_run(sh, "fn emit --size=\"1\"", "", 0, &out, &out_len,
                             &err, &err_len, &res);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(out_len, 1);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    rc = shell89_capture_run(sh, "fn emit --size=\"1\" --size=\"2\"", "", 0,
                             &out, &out_len, &err, &err_len, &res);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK_EQ(shell89_status(res), 2);
    CHECK_EQ(shell89_error_kind(res), SHELL89_ERROR_SYNTAX);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    rc = shell89_capture_run(sh, "fn state", "", 0, &out, &out_len, &err,
                             &err_len, &res);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(ctx.counter, 1);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    rc = shell89_capture_run(sh, "fn state", "", 0, &out, &out_len, &err,
                             &err_len, &res);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK_EQ(ctx.counter, 2);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    /* Registry is frozen after execution. */
    CHECK_EQ(
        shell89_register_function(sh, "late", "op", NULL, count_call, NULL),
        SHELL89_EINVAL);
    shell89_destroy(sh);
}

static void test_required(void)
{
    shell89 *sh;
    shell89_result *res;
    unsigned char *out;
    unsigned char *err;
    size_t out_len;
    size_t err_len;
    int rc;

    sh = shell89_create();
    CHECK_EQ(shell89_register_function(sh, "r", "need", required_options,
                                       count_call, NULL),
             SHELL89_OK);
    rc = shell89_capture_run(sh, "r need", "", 0, &out, &out_len, &err,
                             &err_len, &res);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK_EQ(shell89_status(res), 2);
    CHECK_EQ(calls, 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    rc = shell89_capture_run(sh, "r need --path=\"x\" --flag=\"v\"", "", 0,
                             &out, &out_len, &err, &err_len, &res);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK_EQ(shell89_status(res), 2);
    CHECK_EQ(calls, 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    rc = shell89_capture_run(sh, "r need --path=\"x\" --flag", "", 0, &out,
                             &out_len, &err, &err_len, &res);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(calls, 1);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);
    shell89_destroy(sh);
}

int main(void)
{
    test_registration();
    test_validation();
    test_required();
    return shell89_test_report("unit_registry");
}
