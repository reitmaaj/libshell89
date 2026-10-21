#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "shell89_fixtures.h"

static unsigned long number(const shell89_args *args, const char *name,
                            unsigned long fallback)
{
    const char *s;
    char *end;
    unsigned long v;

    s = shell89_get_string(args, name);
    if (s == NULL)
    {
        return fallback;
    }
    errno = 0;
    end = NULL;
    v = strtoul(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0')
    {
        return fallback;
    }
    return v;
}

static unsigned char pattern(unsigned long offset, unsigned long seed)
{
    unsigned long v;

    v = offset * 131UL + seed * 17UL + (offset >> 8);
    return (unsigned char)(v & 0xffUL);
}

static int write_pattern(shell89_io *io, unsigned long size, unsigned long seed)
{
    unsigned char buf[8192];
    unsigned long offset;
    size_t n;
    size_t i;

    offset = 0;
    while (offset < size)
    {
        n = sizeof buf;
        if ((unsigned long)n > size - offset)
        {
            n = (size_t)(size - offset);
        }
        for (i = 0; i < n; i++)
        {
            buf[i] = pattern(offset + (unsigned long)i, seed);
        }
        if (shell89_write_stdout(io, buf, n) != 0)
        {
            return 1;
        }
        offset += (unsigned long)n;
    }
    return 0;
}

static int fn_emit(void *ctx, const shell89_args *args, shell89_io *io)
{
    (void)ctx;
    return write_pattern(io, number(args, "size", 0), number(args, "seed", 0));
}

static int fn_copy(void *ctx, const shell89_args *args, shell89_io *io)
{
    unsigned char buf[8192];
    size_t n;

    (void)ctx;
    (void)args;
    while (shell89_read(io, buf, sizeof buf, &n) == 0 && n != 0)
    {
        if (shell89_write_stdout(io, buf, n) != 0)
        {
            return 1;
        }
    }
    if (shell89_stream_error(io) != 0 && n == 0)
    {
        return 1;
    }
    return 0;
}

static int fn_upper(void *ctx, const shell89_args *args, shell89_io *io)
{
    unsigned char buf[8192];
    size_t n;
    size_t i;

    (void)ctx;
    (void)args;
    while (shell89_read(io, buf, sizeof buf, &n) == 0 && n != 0)
    {
        for (i = 0; i < n; i++)
        {
            if (buf[i] >= 'a' && buf[i] <= 'z')
            {
                buf[i] = (unsigned char)(buf[i] - 'a' + 'A');
            }
        }
        if (shell89_write_stdout(io, buf, n) != 0)
        {
            return 1;
        }
    }
    return shell89_stream_error(io) != 0;
}

static int fn_count(void *ctx, const shell89_args *args, shell89_io *io)
{
    unsigned char buf[8192];
    size_t n;
    size_t i;
    unsigned long total;
    unsigned long lines;
    char text[64];
    int len;

    (void)ctx;
    total = 0;
    lines = 0;
    while (shell89_read(io, buf, sizeof buf, &n) == 0 && n != 0)
    {
        total += (unsigned long)n;
        if (shell89_get_flag(args, "lines") != 0)
        {
            for (i = 0; i < n; i++)
            {
                if (buf[i] == '\n')
                {
                    lines++;
                }
            }
        }
    }
    if (shell89_stream_error(io) != 0)
    {
        return 1;
    }
    if (shell89_get_flag(args, "lines") != 0)
    {
        total = lines;
    }
    len = sprintf(text, "%lu\n", total);
    return shell89_write_stdout(io, text, (size_t)len) != 0;
}

static int fn_fail(void *ctx, const shell89_args *args, shell89_io *io)
{
    (void)ctx;
    (void)io;
    return (int)(number(args, "status", 1) & 0xffUL);
}

static int fn_error(void *ctx, const shell89_args *args, shell89_io *io)
{
    const char *text;

    (void)ctx;
    text = shell89_get_string(args, "text");
    if (text == NULL)
    {
        return 1;
    }
    return shell89_write_stderr(io, text, strlen(text)) != 0;
}

static int fn_partial(void *ctx, const shell89_args *args, shell89_io *io)
{
    unsigned char buf[1024];
    unsigned long left;
    size_t n;
    size_t want;

    (void)ctx;
    left = number(args, "size", 0);
    while (left != 0)
    {
        want = sizeof buf;
        if ((unsigned long)want > left)
        {
            want = (size_t)left;
        }
        if (shell89_read(io, buf, want, &n) != 0 || n == 0)
        {
            break;
        }
        left -= (unsigned long)n;
    }
    return 0;
}

static int emit_args(const shell89_args *args, shell89_io *io)
{
    static const char *const names[] = {"alpha", "beta", "gamma"};
    const char *value;
    char line[4096];
    int len;
    size_t i;

    for (i = 0; i < sizeof names / sizeof names[0]; i++)
    {
        if (strcmp(names[i], "beta") == 0)
        {
            if (shell89_get_flag(args, names[i]) != 0)
            {
                len = sprintf(line, "%s=1\n", names[i]);
                if (shell89_write_stdout(io, line, (size_t)len) != 0)
                {
                    return 1;
                }
            }
        }
        else
        {
            value = shell89_get_string(args, names[i]);
            if (value != NULL)
            {
                len = sprintf(line, "%s=%s\n", names[i], value);
                if (shell89_write_stdout(io, line, (size_t)len) != 0)
                {
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int fn_args(void *ctx, const shell89_args *args, shell89_io *io)
{
    (void)ctx;
    return emit_args(args, io);
}

static int fn_state(void *ctx, const shell89_args *args, shell89_io *io)
{
    struct shell89_fixture_context *fc;

    (void)args;
    fc = (struct shell89_fixture_context *)ctx;
    fc->counter++;
    return shell89_write_stdout(io, "state\n", 6) != 0;
}

static int fn_slow(void *ctx, const shell89_args *args, shell89_io *io)
{
    unsigned char buf[65536];
    unsigned long chunk;
    unsigned long delay;
    struct timespec req;
    size_t n;
    size_t want;

    (void)ctx;
    chunk = number(args, "chunk", 4096);
    delay = number(args, "delay", 0);
    if (chunk == 0 || chunk > sizeof buf)
    {
        chunk = sizeof buf;
    }
    req.tv_sec = (time_t)(delay / 1000000UL);
    req.tv_nsec = (long)((delay % 1000000UL) * 1000UL);
    while (shell89_read(io, buf, chunk, &n) == 0 && n != 0)
    {
        if (shell89_write_stdout(io, buf, n) != 0)
        {
            return 1;
        }
        if (delay != 0)
        {
            nanosleep(&req, NULL);
        }
    }
    (void)want;
    return shell89_stream_error(io) != 0;
}

static const struct shell89_option emit_options[] = {
    {"size", SHELL89_STRING, 1}, {"seed", SHELL89_STRING, 0}, {NULL, 0, 0}};
static const struct shell89_option count_options[] = {
    {"lines", SHELL89_FLAG, 0}, {NULL, 0, 0}};
static const struct shell89_option fail_options[] = {
    {"status", SHELL89_STRING, 1}, {NULL, 0, 0}};
static const struct shell89_option error_options[] = {
    {"text", SHELL89_STRING, 1}, {NULL, 0, 0}};
static const struct shell89_option partial_options[] = {
    {"size", SHELL89_STRING, 1}, {NULL, 0, 0}};
static const struct shell89_option args_options[] = {
    {"alpha", SHELL89_STRING, 0},
    {"beta", SHELL89_FLAG, 0},
    {"gamma", SHELL89_STRING, 0},
    {NULL, 0, 0}};
static const struct shell89_option slow_options[] = {
    {"chunk", SHELL89_STRING, 0}, {"delay", SHELL89_STRING, 0}, {NULL, 0, 0}};
static const struct shell89_option signal_options[] = {
    {"sig", SHELL89_STRING, 1}, {NULL, 0, 0}};
static const struct shell89_option none_options[] = {{NULL, 0, 0}};

static void reg(shell89 *sh, struct shell89_fixture_context *ctx,
                const char *op, const struct shell89_option *opts,
                shell89_function fn)
{
    if (shell89_register_function(sh, "fn", op, opts, fn, ctx) != 0)
    {
        fprintf(stderr, "register fn.%s failed\n", op);
    }
}

void shell89_fixture_register_native(shell89 *sh,
                                     struct shell89_fixture_context *ctx)
{
    reg(sh, ctx, "emit", emit_options, fn_emit);
    reg(sh, ctx, "copy", none_options, fn_copy);
    reg(sh, ctx, "upper", none_options, fn_upper);
    reg(sh, ctx, "count", count_options, fn_count);
    reg(sh, ctx, "fail", fail_options, fn_fail);
    reg(sh, ctx, "error", error_options, fn_error);
    reg(sh, ctx, "partial", partial_options, fn_partial);
    reg(sh, ctx, "args", args_options, fn_args);
    reg(sh, ctx, "record", args_options, fn_args);
    reg(sh, ctx, "slow", slow_options, fn_slow);
    reg(sh, ctx, "state", none_options, fn_state);
}

static void regp(shell89 *sh, const char *path, const char *op,
                 const struct shell89_option *opts)
{
    const char *argv[2];
    struct shell89_process proc;
    static const char *const envp[] = {"LC_ALL=C", NULL};

    argv[0] = op;
    argv[1] = NULL;
    proc.path = path;
    proc.fixed_argv = argv;
    proc.envp = envp;
    if (shell89_register_process(sh, "proc", op, opts, &proc) != 0)
    {
        fprintf(stderr, "register proc.%s failed\n", op);
    }
}

void shell89_fixture_register_process(shell89 *sh, const char *path)
{
    regp(sh, path, "emit", emit_options);
    regp(sh, path, "copy", none_options);
    regp(sh, path, "upper", none_options);
    regp(sh, path, "count", count_options);
    regp(sh, path, "fail", fail_options);
    regp(sh, path, "error", error_options);
    regp(sh, path, "partial", partial_options);
    regp(sh, path, "args", args_options);
    regp(sh, path, "record", args_options);
    regp(sh, path, "slow", slow_options);
    regp(sh, path, "env", none_options);
    regp(sh, path, "signal", signal_options);
}
