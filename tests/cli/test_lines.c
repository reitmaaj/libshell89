#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shell89_capture.h"
#include "shell89_cli.h"
#include "shell89_lines.h"
#include "shell89_support.h"

static void run_case(shell89 *sh, const char *source, const void *input,
                     size_t input_len, int want_status, const void *want_out,
                     size_t want_out_len, int want_err)
{
    shell89_result *result;
    unsigned char *out;
    unsigned char *err;
    size_t out_len;
    size_t err_len;
    int rc;

    result = NULL;
    rc = shell89_capture_run(sh, source, input, input_len, &out, &out_len, &err,
                             &err_len, &result);
    CHECK_EQ(rc, SHELL89_OK);
    CHECK(result != NULL);
    if (result == NULL)
    {
        return;
    }
    CHECK_EQ(shell89_status(result), want_status);
    CHECK_EQ(out_len, want_out_len);
    if (out_len == want_out_len && want_out_len != 0U)
    {
        CHECK_EQ(memcmp(out, want_out, want_out_len), 0);
    }
    if (want_err == 0)
    {
        CHECK_EQ(err_len, 0U);
    }
    else
    {
        CHECK(err_len != 0U);
    }
    shell89_result_destroy(result);
    shell89_capture_free(out);
    shell89_capture_free(err);
}

static void test_parse_lines(void)
{
    unsigned long value;
    char maximum[64];

    CHECK_EQ(shell89_cli_parse_lines("0", &value), 0);
    CHECK_EQ(value, 0UL);
    CHECK_EQ(shell89_cli_parse_lines("007", &value), 0);
    CHECK_EQ(value, 7UL);
    (void)sprintf(maximum, "%lu", ULONG_MAX);
    CHECK_EQ(shell89_cli_parse_lines(maximum, &value), 0);
    CHECK_EQ(value, ULONG_MAX);
    CHECK_EQ(shell89_cli_parse_lines("", &value), -1);
    CHECK_EQ(shell89_cli_parse_lines("-1", &value), -1);
    CHECK_EQ(shell89_cli_parse_lines("+1", &value), -1);
    CHECK_EQ(shell89_cli_parse_lines("1x", &value), -1);
    CHECK_EQ(shell89_cli_parse_lines(" 1", &value), -1);
    CHECK_EQ(shell89_cli_parse_lines("999999999999999999999999999999", &value),
             -1);
    CHECK_EQ(shell89_cli_parse_lines(NULL, &value), -1);
}

static void test_match(void)
{
    const unsigned char line[] = "an ERROR here\n";
    const unsigned char error[] = "ERROR";
    const unsigned char lower[] = "error";
    const unsigned char empty[] = "";

    CHECK_EQ(shell89_cli_match(line, sizeof line - 1U, error, 5U, 0), 1);
    CHECK_EQ(shell89_cli_match(line, sizeof line - 1U, lower, 5U, 0), 0);
    CHECK_EQ(shell89_cli_match(line, sizeof line - 1U, lower, 5U, 1), 1);
    CHECK_EQ(shell89_cli_match(line, sizeof line - 1U, empty, 0U, 0), 1);
    CHECK_EQ(shell89_cli_match(line, sizeof line - 1U, error, 6U, 0), 0);

    CHECK_EQ(shell89_cli_match((const unsigned char *)"abc", 3U,
                               (const unsigned char *)"bc", 2U, 0),
             1);
    CHECK_EQ(shell89_cli_match((const unsigned char *)"\xff\x00\x80", 3U,
                               (const unsigned char *)"\x00", 1U, 0),
             1);
}

static void test_lower(void)
{
    unsigned char buffer[8];

    memcpy(buffer, "AZaz09\x80\xff", 8);
    shell89_cli_lower(buffer, 8);
    CHECK_EQ(memcmp(buffer, "azaz09\x80\xff", 8), 0);

    buffer[0] = 'Q';
    shell89_cli_lower(buffer, 0);
    CHECK_EQ(buffer[0], 'Q');
}

static void test_grep(shell89 *sh)
{
    const char input[] = "alpha\nERROR one\nbeta\nERROR two\n";
    const char matches[] = "ERROR one\nERROR two\n";
    const char mixed[] = "ERROR\nerror\nInfo\n";
    const char both[] = "ERROR\nerror\n";

    run_case(sh, "text grep --pattern=\"ERROR\"", input, sizeof input - 1U, 0,
             matches, sizeof matches - 1U, 0);
    run_case(sh, "text grep --pattern=\"error\" --ignore-case", mixed,
             sizeof mixed - 1U, 0, both, sizeof both - 1U, 0);
    run_case(sh, "text grep --pattern=\"FATAL\"", input, sizeof input - 1U, 1,
             "", 0U, 0);
    run_case(sh, "text grep --pattern=\"\"", "a\nb", 3U, 0, "a\nb", 3U, 0);
    run_case(sh, "text grep --pattern=\"match\"", "skip\nmatch", 10U, 0,
             "match", 5U, 0);
}

static void test_chunk_boundary(shell89 *sh)
{
    char input[9100];
    char pattern[9001];
    char source[9100];
    const char tail[] = "\nneedle here\n";
    size_t n;

    memset(input, 'x', 9000U);
    memcpy(input + 9000U, tail, sizeof tail - 1U);
    n = 9000U + sizeof tail - 1U;
    run_case(sh, "text grep --pattern=\"needle\"", input, n, 0, "needle here\n",
             12U, 0);

    memset(pattern, 'a', 9000U);
    pattern[9000U] = '\0';
    memcpy(input, pattern, 9000U);
    input[9000U] = '\n';
    (void)sprintf(source, "text grep --pattern=\"%s\"", pattern);
    run_case(sh, source, input, 9001U, 0, input, 9001U, 0);
}

static void test_head_skip(shell89 *sh)
{
    const char input[] = "a\nb\nc\nd";
    const char two[] = "a\nb\n";
    const char rest[] = "c\nd";

    run_case(sh, "text head --lines=\"2\"", input, sizeof input - 1U, 0, two,
             sizeof two - 1U, 0);
    run_case(sh, "text head --lines=\"0\"", input, sizeof input - 1U, 0, "", 0U,
             0);
    run_case(sh, "text skip --lines=\"2\"", input, sizeof input - 1U, 0, rest,
             sizeof rest - 1U, 0);
    run_case(sh, "text skip --lines=\"10\"", input, sizeof input - 1U, 0, "",
             0U, 0);
    run_case(sh, "text skip --lines=\"0\"", input, sizeof input - 1U, 0, input,
             sizeof input - 1U, 0);
}

static void test_invalid_counts(shell89 *sh)
{
    run_case(sh, "text head --lines=\"\"", "", 0U, 2, "", 0U, 1);
    run_case(sh, "text head --lines=\"-1\"", "", 0U, 2, "", 0U, 1);
    run_case(sh, "text head --lines=\"x\"", "", 0U, 2, "", 0U, 1);
    run_case(sh, "text skip --lines=\"1x\"", "", 0U, 2, "", 0U, 1);
    run_case(sh, "text skip --lines=\"999999999999999999999999999999\"", "", 0U,
             2, "", 0U, 1);
}

static void test_line_limit(shell89 *sh)
{
    size_t length;
    unsigned char *input;

    length = (size_t)SHELL89_CLI_MAX_LINE + 1U;
    input = (unsigned char *)malloc(length);
    CHECK(input != NULL);
    if (input == NULL)
    {
        return;
    }
    memset(input, 'a', length);
    run_case(sh, "text grep --pattern=\"a\"", input, length, 1, "", 0U, 1);
    run_case(sh, "text head --lines=\"1\"", input, length, 1, "", 0U, 1);
    run_case(sh, "text skip --lines=\"1\"", input, length, 1, "", 0U, 1);
    free(input);
}

int main(void)
{
    shell89 *sh;

    test_parse_lines();
    test_match();
    test_lower();
    sh = shell89_create();
    CHECK(sh != NULL);
    if (sh == NULL)
    {
        return shell89_test_report("cli_lines");
    }
    CHECK_EQ(shell89_cli_register(sh), SHELL89_OK);
    test_grep(sh);
    test_chunk_boundary(sh);
    test_head_skip(sh);
    test_invalid_counts(sh);
    test_line_limit(sh);
    shell89_destroy(sh);
    return shell89_test_report("cli_lines");
}
