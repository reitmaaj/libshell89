#include <stdio.h>
#include <string.h>

#include <shell89.h>

static int hello(void *ctx, const shell89_args *args, shell89_io *io)
{
    (void)ctx;
    (void)args;
    return shell89_write_stdout(io, "hello\n", 6) != 0;
}

static int upper(void *ctx, const shell89_args *args, shell89_io *io)
{
    unsigned char buf[256];
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

int main(void)
{
    shell89 *sh;
    shell89_result *result;
    struct shell89_run_io io;
    const char *source;
    int rc;

    sh = shell89_create();
    if (sh == NULL)
    {
        return 1;
    }
    if (shell89_register_function(sh, "demo", "hello", NULL, hello, NULL) != 0)
    {
        shell89_destroy(sh);
        return 1;
    }
    if (shell89_register_function(sh, "text", "upper", NULL, upper, NULL) != 0)
    {
        shell89_destroy(sh);
        return 1;
    }
    source = "demo hello | text upper";
    io.stdin_fd = -1;
    io.stdout_fd = 1;
    io.stderr_fd = 2;
    rc = shell89_run(sh, source, &io, &result);
    if (rc != SHELL89_OK)
    {
        fprintf(stderr, "run failed: %d\n", rc);
        shell89_destroy(sh);
        return 1;
    }
    fprintf(stderr, "status=%d\n", shell89_status(result));
    shell89_result_destroy(result);
    shell89_destroy(sh);
    return 0;
}
