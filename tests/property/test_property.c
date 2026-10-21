#include <stdlib.h>
#include <string.h>

#include "shell89_capture.h"
#include "shell89_fixtures.h"
#include "shell89_priv.h"
#include "shell89_support.h"

static unsigned long rng_state = 1;

static unsigned long rnd(void)
{
    rng_state = rng_state * 1103515245UL + 12345UL;
    return rng_state;
}

static size_t rnd_range(size_t n)
{
    return (size_t)(rnd() % (unsigned long)n);
}

static void put(char **p, const char *s)
{
    size_t n;

    n = strlen(s);
    memcpy(*p, s, n);
    *p += n;
}

static void put_json(char **p, const char *s)
{
    const unsigned char *u;

    u = (const unsigned char *)s;
    put(p, "\"");
    while (*u != 0)
    {
        if (*u == '"' || *u == '\\')
        {
            *(*p)++ = '\\';
            *(*p)++ = (char)*u;
        }
        else if (*u == '\b')
        {
            put(p, "\\b");
        }
        else if (*u == '\f')
        {
            put(p, "\\f");
        }
        else if (*u == '\n')
        {
            put(p, "\\n");
        }
        else if (*u == '\r')
        {
            put(p, "\\r");
        }
        else if (*u == '\t')
        {
            put(p, "\\t");
        }
        else
        {
            *(*p)++ = (char)*u;
        }
        u++;
    }
    put(p, "\"");
}

static const char *const names[] = {"a",  "b",   "cmd",
                                    "x1", "z-z", "under_score"};
static const char *const values[] = {
    "",           "plain", "a b",      "quote\"x",         "back\\slash",
    "semi;pipe|", "$HOME", "\xc3\xa9", "\xf0\x9f\x98\x80", "line\nfeed"};

static void build_source(char *buffer)
{
    char *p;
    size_t pipelines;
    size_t i;
    size_t j;
    size_t k;

    p = buffer;
    pipelines = rnd_range(5);
    for (i = 0; i < pipelines; i++)
    {
        size_t commands;

        if (i != 0)
        {
            put(&p, (rnd() & 1UL) != 0 ? "; " : ";\n");
        }
        commands = 1 + rnd_range(4);
        for (j = 0; j < commands; j++)
        {
            size_t options;

            if (j != 0)
            {
                put(&p, (rnd() & 1UL) != 0 ? " | " : "\n| ");
            }
            put(&p, names[rnd_range(6)]);
            put(&p, " ");
            put(&p, names[rnd_range(6)]);
            options = rnd_range(4);
            for (k = 0; k < options; k++)
            {
                static const char *const opt_names[] = {"alpha", "beta",
                                                        "gamma", "delta"};
                size_t index;

                index = k;
                put(&p, " --");
                put(&p, opt_names[index]);
                if ((rnd() & 1UL) != 0)
                {
                    put(&p, "=");
                    put_json(&p, values[rnd_range(10)]);
                }
            }
        }
    }
    if (pipelines != 0 && (rnd() & 1UL) != 0)
    {
        put(&p, (rnd() & 1UL) != 0 ? ";" : "\n");
    }
    *p = '\0';
}

static int ast_equal(const struct shell89_priv_script *a,
                     const struct shell89_priv_script *b)
{
    size_t i;
    size_t j;
    size_t k;

    if (a->pipeline_count != b->pipeline_count)
    {
        return 0;
    }
    for (i = 0; i < a->pipeline_count; i++)
    {
        if (a->pipelines[i].command_count != b->pipelines[i].command_count)
        {
            return 0;
        }
        for (j = 0; j < a->pipelines[i].command_count; j++)
        {
            const struct shell89_priv_command *ca;
            const struct shell89_priv_command *cb;

            ca = &a->pipelines[i].commands[j];
            cb = &b->pipelines[i].commands[j];
            if (strcmp(ca->group.data, cb->group.data) != 0 ||
                strcmp(ca->operation.data, cb->operation.data) != 0 ||
                ca->option_count != cb->option_count)
            {
                return 0;
            }
            for (k = 0; k < ca->option_count; k++)
            {
                if (strcmp(ca->options[k].name.data,
                           cb->options[k].name.data) != 0 ||
                    ca->options[k].has_value != cb->options[k].has_value ||
                    (ca->options[k].has_value &&
                     strcmp(ca->options[k].value.data,
                            cb->options[k].value.data) != 0))
                {
                    return 0;
                }
            }
        }
    }
    return 1;
}

static void format_ast(const struct shell89_priv_script *s, char *out)
{
    char *p;
    size_t i;
    size_t j;
    size_t k;

    p = out;
    for (i = 0; i < s->pipeline_count; i++)
    {
        if (i != 0)
        {
            put(&p, "; ");
        }
        for (j = 0; j < s->pipelines[i].command_count; j++)
        {
            const struct shell89_priv_command *c;

            if (j != 0)
            {
                put(&p, " | ");
            }
            c = &s->pipelines[i].commands[j];
            put(&p, c->group.data);
            put(&p, " ");
            put(&p, c->operation.data);
            for (k = 0; k < c->option_count; k++)
            {
                put(&p, " --");
                put(&p, c->options[k].name.data);
                if (c->options[k].has_value)
                {
                    put(&p, "=");
                    put_json(&p, c->options[k].value.data);
                }
            }
        }
    }
    *p = '\0';
}

static void property_roundtrip(void)
{
    size_t i;

    for (i = 0; i < 2000; i++)
    {
        char source[8192];
        char formatted[8192];
        struct shell89_priv_script a;
        struct shell89_priv_script b;
        struct shell89_priv_diag diag;
        struct shell89_priv_diag diag2;

        build_source(source);
        CHECK_EQ(shell89_priv_parse(source, &a, &diag), 1);
        format_ast(&a, formatted);
        CHECK_EQ(shell89_priv_parse(formatted, &b, &diag2), 1);
        if (!ast_equal(&a, &b))
        {
            fprintf(stderr, "roundtrip mismatch source=[%s] formatted=[%s]\n",
                    source, formatted);
            CHECK(0);
            shell89_priv_script_free(&b);
            shell89_priv_script_free(&a);
            return;
        }
        shell89_priv_script_free(&b);
        shell89_priv_script_free(&a);
    }
}

static void property_identity(shell89 *sh)
{
    unsigned char *input;
    size_t i;

    for (i = 0; i < 100; i++)
    {
        size_t len;
        size_t j;
        shell89_result *res;
        unsigned char *out;
        unsigned char *err;
        size_t out_len;
        size_t err_len;
        const char *const scripts[] = {
            "fn copy", "proc copy", "fn copy | proc copy",
            "proc copy | fn copy", "fn copy | proc copy | fn copy"};

        len = rnd_range(65536);
        input = (unsigned char *)malloc(len == 0 ? 1 : len);
        CHECK(input != NULL);
        for (j = 0; j < len; j++)
        {
            input[j] = (unsigned char)(rnd() & 0xffUL);
        }
        CHECK_EQ(shell89_capture_run(sh, scripts[rnd_range(5)], input, len,
                                     &out, &out_len, &err, &err_len, &res),
                 SHELL89_OK);
        CHECK_EQ(shell89_status(res), 0);
        CHECK_EQ(out_len, len);
        if (out_len == len)
        {
            CHECK_EQ(memcmp(out, input, len), 0);
        }
        shell89_result_destroy(res);
        shell89_capture_free(out);
        shell89_capture_free(err);
        free(input);
    }
}

static void property_invalid_atomic(shell89 *sh, int *counter)
{
    static const char *const bad[] = {
        "fn state; nope nope",
        "fn state; fn count --bogus=\"x\"",
        "fn state; fn emit",
        "fn state; fn emit --size=\"1\" --size=\"2\"",
        "fn state; fn emit --size=\"\\u0000\"",
        "fn state; fn emit --size=bare",
        "fn state; fn emit --size=\"1\" |",
        "fn state; fn emit --size=\"1\" && fn state"};
    size_t i;
    shell89_result *res;
    unsigned char *out;
    unsigned char *err;
    size_t out_len;
    size_t err_len;

    for (i = 0; i < sizeof bad / sizeof bad[0]; i++)
    {
        CHECK_EQ(shell89_capture_run(sh, bad[i], "", 0, &out, &out_len, &err,
                                     &err_len, &res),
                 SHELL89_OK);
        CHECK(shell89_status(res) != 0);
        CHECK_EQ(*counter, 0);
        CHECK_EQ(out_len, 0);
        shell89_result_destroy(res);
        shell89_capture_free(out);
        shell89_capture_free(err);
    }
}

int main(void)
{
    shell89 *sh;
    struct shell89_fixture_context ctx;
    const char *path;

    path = getenv("SHELL89_PROC_FIXTURE");
    if (path == NULL)
    {
        path = "./build/fixture_proc";
    }
    property_roundtrip();
    sh = shell89_create();
    ctx.counter = 0;
    shell89_fixture_register_native(sh, &ctx);
    shell89_fixture_register_process(sh, path);
    property_identity(sh);
    ctx.counter = 0;
    property_invalid_atomic(sh, &ctx.counter);
    CHECK_EQ(ctx.counter, 0);
    shell89_destroy(sh);
    return shell89_test_report("property");
}
