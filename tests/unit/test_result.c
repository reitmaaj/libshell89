#include <string.h>

#include "shell89_capture.h"
#include "shell89_support.h"

int main(void)
{
    shell89 *sh;
    shell89_result *res;
    unsigned char *out;
    unsigned char *err;
    size_t out_len;
    size_t err_len;

    CHECK_EQ(shell89_status(NULL), 125);
    CHECK_EQ(shell89_pipeline_count(NULL), 0);
    CHECK_EQ(shell89_pipeline_status(NULL, 0), 125);
    CHECK_EQ(shell89_stage_count(NULL, 0), 0);
    CHECK_EQ(shell89_stage_status(NULL, 0, 0), 125);
    CHECK_EQ(shell89_error_offset(NULL), (size_t)-1);
    CHECK_EQ(shell89_error_kind(NULL), SHELL89_ERROR_NONE);
    CHECK_STR(shell89_error_message(NULL), "");
    CHECK_EQ(shell89_stage_kind(NULL, 0, 0), SHELL89_STAGE_NONE);
    CHECK_EQ(shell89_stage_termination(NULL, 0, 0), SHELL89_TERM_NONE);
    CHECK_EQ(shell89_stage_signal(NULL, 0, 0), 0);
    shell89_result_destroy(NULL);

    sh = shell89_create();
    CHECK(sh != NULL);
    CHECK_EQ(shell89_capture_run(sh, "a b |", "", 0, &out, &out_len, &err,
                                 &err_len, &res),
             SHELL89_OK);
    CHECK_EQ(shell89_status(res), 2);
    CHECK_EQ(shell89_error_kind(res), SHELL89_ERROR_SYNTAX);
    CHECK_EQ(shell89_error_offset(res), 5);
    CHECK(strlen(shell89_error_message(res)) != 0);
    CHECK_EQ(shell89_pipeline_count(res), 0);
    CHECK_EQ(shell89_stage_status(res, 0, 0), 125);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);

    CHECK_EQ(shell89_capture_run(sh, "unknown op", "", 0, &out, &out_len, &err,
                                 &err_len, &res),
             SHELL89_OK);
    CHECK_EQ(shell89_status(res), 127);
    CHECK_EQ(shell89_error_kind(res), SHELL89_ERROR_UNKNOWN_COMMAND);
    CHECK_EQ(shell89_error_offset(res), 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);
    shell89_destroy(sh);
    return shell89_test_report("unit_result");
}
