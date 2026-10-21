#include <stdlib.h>
#include <string.h>

#include "shell89_capture.h"
#include "shell89_fixtures.h"
#include "shell89_support.h"

static int bogus(void *ctx, const shell89_args *args, shell89_io *io)
{
    (void)ctx;
    (void)args;
    (void)io;
    return 999;
}

static void run_ok(shell89 *sh, const char *source, const void *in,
                   size_t in_len, unsigned char **out, size_t *out_len,
                   unsigned char **err, size_t *err_len, shell89_result **res)
{
    int rc;

    rc = shell89_capture_run(sh, source, in, in_len, out, out_len, err, err_len,
                             res);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK(*res != NULL);
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
    struct shell89_run_io io;

    sh = shell89_create();
    ctx.counter = 0;
    shell89_fixture_register_native(sh, &ctx);
    CHECK_EQ(shell89_register_function(sh, "fn", "bogus", NULL, bogus, NULL),
             SHELL89_OK);

    run_ok(sh, "fn args --alpha=\"a b;|$`\" --beta --gamma=\"\xc3\xa9\"", "", 0,
           &out, &out_len, &err, &err_len, &res);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(out_len, strlen("alpha=a b;|$`\nbeta=1\ngamma=\xc3\xa9\n"));
    CHECK_EQ(memcmp(out, "alpha=a b;|$`\nbeta=1\ngamma=\xc3\xa9\n", out_len),
             0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    run_ok(sh, "fn state", "", 0, &out, &out_len, &err, &err_len, &res);
    CHECK_EQ(ctx.counter, 1);
    CHECK_EQ(out_len, 6);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    run_ok(sh, "fn upper", "abcXYZ\xff", 7, &out, &out_len, &err, &err_len,
           &res);
    CHECK_EQ(out_len, 7);
    CHECK_EQ(memcmp(out, "ABCXYZ\xff", 7), 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    run_ok(sh, "fn count --lines", "a\nb\nc", 5, &out, &out_len, &err, &err_len,
           &res);
    CHECK_EQ(out_len, 2);
    CHECK_EQ(memcmp(out, "2\n", 2), 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    run_ok(sh, "fn error --text=\"boom\"", "", 0, &out, &out_len, &err,
           &err_len, &res);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(err_len, 4);
    CHECK_EQ(memcmp(err, "boom", 4), 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    run_ok(sh, "fn fail --status=\"7\"", "", 0, &out, &out_len, &err, &err_len,
           &res);
    CHECK_EQ(shell89_status(res), 7);
    CHECK_EQ(shell89_stage_status(res, 0, 0), 7);
    CHECK_EQ(shell89_stage_termination(res, 0, 0), SHELL89_TERM_CALLBACK);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    run_ok(sh, "fn bogus", "", 0, &out, &out_len, &err, &err_len, &res);
    CHECK_EQ(shell89_status(res), 125);
    CHECK_EQ(shell89_stage_termination(res, 0, 0), SHELL89_TERM_INVALID);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    run_ok(sh, "fn fail --status=\"1\" | fn copy | fn fail --status=\"2\"", "",
           0, &out, &out_len, &err, &err_len, &res);
    CHECK_EQ(shell89_status(res), 2);
    CHECK_EQ(shell89_pipeline_status(res, 0), 2);
    CHECK_EQ(shell89_stage_status(res, 0, 0), 1);
    CHECK_EQ(shell89_stage_status(res, 0, 1), 0);
    CHECK_EQ(shell89_stage_status(res, 0, 2), 2);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    run_ok(sh, "fn fail --status=\"5\"; fn state", "", 0, &out, &out_len, &err,
           &err_len, &res);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(ctx.counter, 2);
    CHECK_EQ(shell89_pipeline_count(res), 2);
    CHECK_EQ(shell89_pipeline_status(res, 0), 5);
    CHECK_EQ(shell89_pipeline_status(res, 1), 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    run_ok(sh, " ; \n ; ", "", 0, &out, &out_len, &err, &err_len, &res);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(shell89_pipeline_count(res), 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    io.stdin_fd = -1;
    io.stdout_fd = -1;
    io.stderr_fd = -1;
    CHECK_EQ(shell89_run(sh, "fn copy; fn state", &io, &res), SHELL89_OK);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(ctx.counter, 3);
    shell89_result_destroy(res);

    shell89_destroy(sh);
    return shell89_test_report("integration_native");
}
