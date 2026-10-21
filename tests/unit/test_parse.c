#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shell89_priv.h"
#include "shell89_support.h"

static int parse_ok(const char *source, struct shell89_priv_script *script)
{
    struct shell89_priv_diag diag;
    int r;

    r = shell89_priv_parse(source, script, &diag);
    if (r != 1)
    {
        fprintf(stderr, "parse unexpectedly failed [%s]: %s at %lu\n", source,
                diag.message, (unsigned long)diag.offset);
        return 0;
    }
    return 1;
}

static int parse_bad(const char *source, int kind)
{
    struct shell89_priv_script script;
    struct shell89_priv_diag diag;
    int r;

    r = shell89_priv_parse(source, &script, &diag);
    if (r != 0)
    {
        fprintf(stderr, "parse unexpectedly accepted [%s]\n", source);
        return 0;
    }
    if (diag.kind != kind)
    {
        fprintf(stderr, "wrong error kind for [%s]: %d\n", source, diag.kind);
        return 0;
    }
    if (script.pipeline_count != 0)
    {
        fprintf(stderr, "failed parse left a script\n");
        return 0;
    }
    return 1;
}

static const char *value_of(const struct shell89_priv_script *script,
                            const char *name)
{
    size_t i;

    for (i = 0; i < script->pipelines[0].commands[0].option_count; i++)
    {
        const struct shell89_priv_option_use *o;

        o = &script->pipelines[0].commands[0].options[i];
        if (strcmp(o->name.data, name) == 0)
        {
            return o->value.data;
        }
    }
    return NULL;
}

static void test_basic(void)
{
    struct shell89_priv_script s;

    CHECK(parse_ok("", &s));
    CHECK_EQ(s.pipeline_count, 0);
    shell89_priv_script_free(&s);

    CHECK(parse_ok("  \t\n; ;\n\t", &s));
    CHECK_EQ(s.pipeline_count, 0);
    shell89_priv_script_free(&s);

    CHECK(parse_ok("alpha beta", &s));
    CHECK_EQ(s.pipeline_count, 1);
    CHECK_EQ(s.pipelines[0].command_count, 1);
    CHECK_STR(s.pipelines[0].commands[0].group.data, "alpha");
    CHECK_STR(s.pipelines[0].commands[0].operation.data, "beta");
    shell89_priv_script_free(&s);

    CHECK(parse_ok("a b; c d\ne f\n;\n", &s));
    CHECK_EQ(s.pipeline_count, 3);
    shell89_priv_script_free(&s);

    CHECK(parse_ok("a b\r\nc d\r\n", &s));
    CHECK_EQ(s.pipeline_count, 2);
    shell89_priv_script_free(&s);

    CHECK(parse_ok("a b |\n c d\n| e f", &s));
    CHECK_EQ(s.pipeline_count, 1);
    CHECK_EQ(s.pipelines[0].command_count, 3);
    shell89_priv_script_free(&s);
}

static void test_options(void)
{
    struct shell89_priv_script s;

    CHECK(parse_ok("g op --x=\"\" --flag", &s));
    CHECK_EQ(s.pipelines[0].commands[0].option_count, 2);
    CHECK_STR(value_of(&s, "x"), "");
    shell89_priv_script_free(&s);

    CHECK(parse_ok("g op --x=\"a b|c;d$e`f\"", &s));
    CHECK_STR(value_of(&s, "x"), "a b|c;d$e`f");
    shell89_priv_script_free(&s);

    CHECK(parse_ok("g op --x=\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"", &s));
    CHECK_STR(value_of(&s, "x"), "\"\\/\b\f\n\r\t");
    shell89_priv_script_free(&s);

    CHECK(parse_ok("g op --x=\"\\u0041\\u00e9\\ud83d\\ude00\"", &s));
    CHECK_STR(value_of(&s, "x"), "A\xc3\xa9\xf0\x9f\x98\x80");
    shell89_priv_script_free(&s);
}

static void test_invalid(void)
{
    CHECK(parse_bad("a", SHELL89_ERROR_SYNTAX));
    CHECK(parse_bad("a b c", SHELL89_ERROR_SYNTAX));
    CHECK(parse_bad("a b --x=bare", SHELL89_ERROR_SYNTAX));
    CHECK(parse_bad("a b --x='bare'", SHELL89_ERROR_SYNTAX));
    CHECK(parse_bad("a b --x=$HOME", SHELL89_ERROR_SYNTAX));
    CHECK(parse_bad("a b && c d", SHELL89_ERROR_SYNTAX));
    CHECK(parse_bad("a b |", SHELL89_ERROR_SYNTAX));
    CHECK(parse_bad("| a b", SHELL89_ERROR_SYNTAX));
    CHECK(parse_bad("a b --x=\"1\" --x=\"2\"", SHELL89_ERROR_SYNTAX));
    CHECK(parse_bad("a b --x=\"\\q\"", SHELL89_ERROR_SYNTAX));
    CHECK(parse_bad("a b --x=\"\\ud800\"", SHELL89_ERROR_SYNTAX));
    CHECK(parse_bad("a b --x=\"\\udc00\"", SHELL89_ERROR_SYNTAX));
    CHECK(parse_bad("a b --x=\"\\u0000\"", SHELL89_ERROR_SYNTAX));
    CHECK(parse_bad("a b --x=\"bad\nraw\"", SHELL89_ERROR_SYNTAX));
    CHECK(parse_bad("A b", SHELL89_ERROR_SYNTAX));
    CHECK(parse_bad("a B", SHELL89_ERROR_SYNTAX));
    CHECK(parse_bad("a b\r c d", SHELL89_ERROR_SYNTAX));
    CHECK(parse_bad("a b --x=\"\xff\"", SHELL89_ERROR_SYNTAX));
}

static void test_limits(void)
{
    char name[80];
    char *stages;
    char *pipelines;
    size_t i;
    size_t pos;

    memset(name, 'a', sizeof name - 1);
    name[sizeof name - 1] = '\0';
    name[0] = 'a';
    {
        char src[128];
        sprintf(src, "a %s", name);
        CHECK(parse_bad(src, SHELL89_ERROR_LIMIT));
    }

    stages = (char *)malloc(17 * 6 + 1);
    CHECK(stages != NULL);
    pos = 0;
    for (i = 0; i < 17; i++)
    {
        memcpy(stages + pos, i == 0 ? "a b" : " | a b", i == 0 ? 3 : 6);
        pos += i == 0 ? 3 : 6;
    }
    stages[pos] = '\0';
    CHECK(parse_bad(stages, SHELL89_ERROR_LIMIT));
    free(stages);

    {
        char *many;
        char *large;

        many = (char *)malloc(65 * 16 + 16);
        CHECK(many != NULL);
        strcpy(many, "a b");
        pos = 3;
        for (i = 0; i < 65; i++)
        {
            sprintf(many + pos, " --o%lu", (unsigned long)i);
            pos += strlen(many + pos);
        }
        CHECK(parse_bad(many, SHELL89_ERROR_LIMIT));
        free(many);

        large = (char *)malloc((size_t)SHELL89_MAX_SOURCE + 2);
        CHECK(large != NULL);
        memset(large, 'a', (size_t)SHELL89_MAX_SOURCE + 1);
        large[(size_t)SHELL89_MAX_SOURCE + 1] = '\0';
        CHECK(parse_bad(large, SHELL89_ERROR_LIMIT));
        free(large);

        large = (char *)malloc((size_t)SHELL89_MAX_VALUE + 64);
        CHECK(large != NULL);
        strcpy(large, "a b --x=\"");
        memset(large + strlen(large), 'a', (size_t)SHELL89_MAX_VALUE + 1);
        large[strlen("a b --x=\"") + (size_t)SHELL89_MAX_VALUE + 1] = '\0';
        CHECK(parse_bad(large, SHELL89_ERROR_LIMIT));
        free(large);
    }

    pipelines = (char *)malloc(3 + 64 * 4 + 1);
    CHECK(pipelines != NULL);
    memcpy(pipelines, "a b", 3);
    for (i = 1; i < 65; i++)
    {
        memcpy(pipelines + 3 + (i - 1) * 4, ";a b", 4);
    }
    pipelines[3 + 64 * 4] = '\0';
    CHECK(parse_bad(pipelines, SHELL89_ERROR_LIMIT));
    free(pipelines);
}

int main(void)
{
    test_basic();
    test_options();
    test_invalid();
    test_limits();
    return shell89_test_report("unit_parse");
}
