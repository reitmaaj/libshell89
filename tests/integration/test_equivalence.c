#include <stdlib.h>
#include <string.h>

#include "shell89_capture.h"
#include "shell89_fixtures.h"
#include "shell89_support.h"

static void compare(shell89 *sh, const char *fn_script, const char *proc_script,
                    const unsigned char *input, size_t input_len)
{
    shell89_result *ra;
    shell89_result *rb;
    unsigned char *oa;
    unsigned char *ob;
    unsigned char *ea;
    unsigned char *eb;
    size_t oa_len;
    size_t ob_len;
    size_t ea_len;
    size_t eb_len;

    CHECK_EQ(shell89_capture_run(sh, fn_script, input, input_len, &oa, &oa_len,
                                 &ea, &ea_len, &ra),
             SHELL89_OK);
    CHECK_EQ(shell89_capture_run(sh, proc_script, input, input_len, &ob,
                                 &ob_len, &eb, &eb_len, &rb),
             SHELL89_OK);
    CHECK_EQ(shell89_status(ra), shell89_status(rb));
    CHECK_EQ(oa_len, ob_len);
    CHECK_EQ(ea_len, eb_len);
    if (oa_len == ob_len)
    {
        CHECK_EQ(memcmp(oa, ob, oa_len), 0);
    }
    if (ea_len == eb_len)
    {
        CHECK_EQ(memcmp(ea, eb, ea_len), 0);
    }
    shell89_result_destroy(ra);
    shell89_result_destroy(rb);
    shell89_capture_free(oa);
    shell89_capture_free(ob);
    shell89_capture_free(ea);
    shell89_capture_free(eb);
}

int main(void)
{
    shell89 *sh;
    struct shell89_fixture_context ctx;
    unsigned char *input;
    size_t input_len;
    size_t i;
    const char *path;

    path = getenv("SHELL89_PROC_FIXTURE");
    if (path == NULL)
    {
        path = "./build/fixture_proc";
    }
    input_len = 20000;
    input = (unsigned char *)malloc(input_len);
    CHECK(input != NULL);
    for (i = 0; i < input_len; i++)
    {
        input[i] = (unsigned char)((i * 73U + 19U) & 0xffU);
    }
    sh = shell89_create();
    ctx.counter = 0;
    shell89_fixture_register_native(sh, &ctx);
    shell89_fixture_register_process(sh, path);

    compare(sh, "fn copy", "proc copy", input, input_len);
    compare(sh, "fn upper", "proc upper", input, input_len);
    compare(sh, "fn count", "proc count", input, input_len);
    compare(sh, "fn count --lines", "proc count --lines", input, input_len);
    compare(sh, "fn fail --status=\"9\"", "proc fail --status=\"9\"", input, 0);
    compare(sh, "fn error --text=\"x y\"", "proc error --text=\"x y\"", input,
            0);
    compare(sh, "fn args --alpha=\"a b;|$`\" --beta --gamma=\"\xc3\xa9\"",
            "proc args --alpha=\"a b;|$`\" --beta --gamma=\"\xc3\xa9\"", input,
            0);
    compare(sh, "fn copy", "proc copy", input, 0);
    compare(sh, "fn copy", "proc copy", input, 1);
    compare(sh, "fn copy", "proc copy", input, 4095);
    compare(sh, "fn copy", "proc copy", input, 4096);
    compare(sh, "fn copy", "proc copy", input, 4097);

    free(input);
    shell89_destroy(sh);
    return shell89_test_report("integration_equivalence");
}
