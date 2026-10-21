#include <string.h>

#include "shell89_cli.h"
#include "shell89_support.h"

static void test_parse_script(void)
{
    char *argv[4];
    const char *value;
    shell89_cli_mode mode;

    argv[0] = (char *)"shell89";
    value = (const char *)"unset";
    mode = shell89_cli_parse(1, argv, &value);
    CHECK_EQ(mode, SHELL89_CLI_MODE_SCRIPT);
    CHECK(value == NULL);

    argv[1] = (char *)"-c";
    argv[2] = (char *)"text count";
    mode = shell89_cli_parse(3, argv, &value);
    CHECK_EQ(mode, SHELL89_CLI_MODE_SCRIPT);
    CHECK_STR(value, "text count");

    argv[2] = (char *)"";
    mode = shell89_cli_parse(3, argv, &value);
    CHECK_EQ(mode, SHELL89_CLI_MODE_SCRIPT);
    CHECK_STR(value, "");
}

static void test_parse_file(void)
{
    char *argv[3];
    const char *value;
    shell89_cli_mode mode;

    argv[0] = (char *)"shell89";
    argv[1] = (char *)"script.shell";
    mode = shell89_cli_parse(2, argv, &value);
    CHECK_EQ(mode, SHELL89_CLI_MODE_FILE);
    CHECK_STR(value, "script.shell");

    argv[1] = (char *)"-";
    mode = shell89_cli_parse(2, argv, &value);
    CHECK_EQ(mode, SHELL89_CLI_MODE_FILE);
    CHECK_STR(value, "-");
}

static void test_parse_help_and_usage(void)
{
    char *argv[3];
    const char *value;
    shell89_cli_mode mode;

    argv[0] = (char *)"shell89";
    argv[1] = (char *)"-h";
    mode = shell89_cli_parse(2, argv, &value);
    CHECK_EQ(mode, SHELL89_CLI_MODE_HELP);

    argv[1] = (char *)"--help";
    mode = shell89_cli_parse(2, argv, &value);
    CHECK_EQ(mode, SHELL89_CLI_MODE_HELP);

    argv[1] = (char *)"-c";
    mode = shell89_cli_parse(2, argv, &value);
    CHECK_EQ(mode, SHELL89_CLI_MODE_USAGE);

    argv[1] = (char *)"-x";
    mode = shell89_cli_parse(2, argv, &value);
    CHECK_EQ(mode, SHELL89_CLI_MODE_USAGE);

    argv[1] = (char *)"-e";
    mode = shell89_cli_parse(2, argv, &value);
    CHECK_EQ(mode, SHELL89_CLI_MODE_USAGE);

    argv[1] = (char *)"file";
    argv[2] = (char *)"extra";
    mode = shell89_cli_parse(3, argv, &value);
    CHECK_EQ(mode, SHELL89_CLI_MODE_USAGE);
}

static void test_upper(void)
{
    unsigned char buffer[8];

    memcpy(buffer, "aZ09\x80\xff", 6);
    shell89_cli_upper(buffer, 6);
    CHECK_EQ(memcmp(buffer, "AZ09\x80\xff", 6), 0);

    buffer[0] = 'q';
    shell89_cli_upper(buffer, 0);
    CHECK_EQ(buffer[0], 'q');
}

static void test_lower(void)
{
    unsigned char buffer[8];

    memcpy(buffer, "Az09\x80\xffQ", 8);
    shell89_cli_lower(buffer, 8);
    CHECK_EQ(memcmp(buffer, "az09\x80\xffq", 8), 0);

    buffer[0] = 'Q';
    shell89_cli_lower(buffer, 0);
    CHECK_EQ(buffer[0], 'Q');
}

static void update(struct shell89_cli_line_state *state, const char *text)
{
    shell89_cli_line_update(state, (const unsigned char *)text, strlen(text));
}

static void test_line_count(void)
{
    struct shell89_cli_line_state state;

    shell89_cli_line_init(&state);
    CHECK_EQ(shell89_cli_line_total(&state), 0);

    shell89_cli_line_init(&state);
    update(&state, "a");
    CHECK_EQ(shell89_cli_line_total(&state), 1);

    shell89_cli_line_init(&state);
    update(&state, "a\n");
    CHECK_EQ(shell89_cli_line_total(&state), 1);

    shell89_cli_line_init(&state);
    update(&state, "a\nb");
    CHECK_EQ(shell89_cli_line_total(&state), 2);

    shell89_cli_line_init(&state);
    update(&state, "\n\n");
    CHECK_EQ(shell89_cli_line_total(&state), 2);

    shell89_cli_line_init(&state);
    update(&state, "a\n");
    update(&state, "b");
    CHECK_EQ(shell89_cli_line_total(&state), 2);

    shell89_cli_line_init(&state);
    update(&state, "a");
    update(&state, "\n");
    CHECK_EQ(shell89_cli_line_total(&state), 1);
}

int main(void)
{
    test_parse_script();
    test_parse_file();
    test_parse_help_and_usage();
    test_upper();
    test_lower();
    test_line_count();
    return shell89_test_report("cli");
}
