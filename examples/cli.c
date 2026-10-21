#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <shell89.h>

static int echo(void *ctx, const shell89_args *args, shell89_io *io)
{
    const char *text;

    (void)ctx;
    text = shell89_get_string(args, "text");
    if (text == NULL)
    {
        return 2;
    }
    if (shell89_write_stdout(io, text, strlen(text)) != 0)
    {
        return 1;
    }
    return shell89_write_stdout(io, "\n", 1) != 0;
}

static int upper(void *ctx, const shell89_args *args, shell89_io *io)
{
    unsigned char buf[1024];
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

static int count(void *ctx, const shell89_args *args, shell89_io *io)
{
    unsigned char buf[1024];
    size_t n;
    unsigned long total;
    char text[64];
    int len;

    (void)ctx;
    (void)args;
    total = 0;
    while (shell89_read(io, buf, sizeof buf, &n) == 0 && n != 0)
    {
        total += (unsigned long)n;
    }
    if (shell89_stream_error(io) != 0)
    {
        return 1;
    }
    len = sprintf(text, "%lu\n", total);
    return shell89_write_stdout(io, text, (size_t)len) != 0;
}

static void usage(const char *name)
{
    fprintf(stderr, "usage: %s [-e script] [file]\n", name);
}

int main(int argc, char **argv)
{
    static const struct shell89_option echo_options[] = {
        {"text", SHELL89_STRING, 1}, {NULL, 0, 0}};
    shell89 *sh;
    shell89_result *result;
    struct shell89_run_io io;
    char *source;
    long size;
    FILE *f;
    int rc;

    source = NULL;
    if (argc > 1 && strcmp(argv[1], "-e") == 0 && argc > 2)
    {
        source = argv[2];
    }
    else if (argc > 1 && strcmp(argv[1], "-h") == 0)
    {
        usage(argv[0]);
        return 0;
    }
    else if (argc > 1)
    {
        f = fopen(argv[1], "rb");
        if (f == NULL)
        {
            perror(argv[1]);
            return 1;
        }
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        rewind(f);
        if (size < 0 || size > 1024 * 1024)
        {
            fclose(f);
            return 1;
        }
        source = (char *)malloc((size_t)size + 1);
        if (source == NULL)
        {
            fclose(f);
            return 1;
        }
        if (fread(source, 1, (size_t)size, f) != (size_t)size)
        {
            fclose(f);
            free(source);
            return 1;
        }
        fclose(f);
        source[size] = '\0';
    }
    else
    {
        usage(argv[0]);
        return 1;
    }
    sh = shell89_create();
    if (sh == NULL)
    {
        return 1;
    }
    if (shell89_register_function(sh, "text", "echo", echo_options, echo,
                                  NULL) != 0 ||
        shell89_register_function(sh, "text", "upper", NULL, upper, NULL) !=
            0 ||
        shell89_register_function(sh, "text", "count", NULL, count, NULL) != 0)
    {
        shell89_destroy(sh);
        return 1;
    }
    io.stdin_fd = 0;
    io.stdout_fd = 1;
    io.stderr_fd = 2;
    rc = shell89_run(sh, source, &io, &result);
    if (argc > 2 && strcmp(argv[1], "-e") == 0)
    {
        /* source points into argv */
    }
    else if (argc > 1 && strcmp(argv[1], "-h") != 0)
    {
        free(source);
    }
    if (rc != SHELL89_OK)
    {
        fprintf(stderr, "libshell89: run failed: %d\n", rc);
        shell89_destroy(sh);
        return 1;
    }
    rc = shell89_status(result);
    if (shell89_error_kind(result) != SHELL89_ERROR_NONE)
    {
        fprintf(stderr, "libshell89:%lu: %s\n",
                (unsigned long)shell89_error_offset(result),
                shell89_error_message(result));
    }
    shell89_result_destroy(result);
    shell89_destroy(sh);
    return rc;
}
