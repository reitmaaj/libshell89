#include <stdlib.h>
#include <string.h>

#include "shell89_capture.h"
#include "shell89_fixtures.h"
#include "shell89_support.h"

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

    path = getenv("SHELL89_PROC_FIXTURE");
    if (path == NULL)
    {
        path = "./build/fixture_proc";
    }
    sh = shell89_create();
    ctx.counter = 0;
    shell89_fixture_register_native(sh, &ctx);
    shell89_fixture_register_process(sh, path);

    CHECK_EQ(shell89_capture_run(sh,
                                 "fn fail --status=\"3\"; fn state; "
                                 "fn fail --status=\"5\"; fn state",
                                 "", 0, &out, &out_len, &err, &err_len, &res),
             SHELL89_OK);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(shell89_pipeline_count(res), 4);
    CHECK_EQ(ctx.counter, 2);
    CHECK_EQ(shell89_pipeline_status(res, 0), 3);
    CHECK_EQ(shell89_pipeline_status(res, 1), 0);
    CHECK_EQ(shell89_pipeline_status(res, 2), 5);
    CHECK_EQ(shell89_pipeline_status(res, 3), 0);
    CHECK_EQ(out_len, 12);
    CHECK_EQ(memcmp(out, "state\nstate\n", 12), 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    CHECK_EQ(shell89_capture_run(sh, "fn state; fn state; fn state", "", 0,
                                 &out, &out_len, &err, &err_len, &res),
             SHELL89_OK);
    CHECK_EQ(ctx.counter, 5);
    CHECK_EQ(out_len, 18);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    CHECK_EQ(shell89_capture_run(sh, "fn count; fn count", "abcdef", 6, &out,
                                 &out_len, &err, &err_len, &res),
             SHELL89_OK);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(out_len, 4);
    CHECK_EQ(memcmp(out, "6\n0\n", 4), 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    CHECK_EQ(shell89_capture_run(sh,
                                 "proc emit --size=\"8\" --seed=\"1\" | "
                                 "fn count; fn emit --size=\"4\" --seed=\"1\"",
                                 "", 0, &out, &out_len, &err, &err_len, &res),
             SHELL89_OK);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(out_len, 6);
    CHECK_EQ(memcmp(out, "8\n", 2), 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    shell89_destroy(sh);
    return shell89_test_report("integration_sequence");
}
