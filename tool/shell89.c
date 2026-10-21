/* shell89.c - minimal standalone runner for libshell89. */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "shell89.h"
#include "shell89_cli.h"

struct cli_input
{
    const char *script;
    char *owned;
    size_t length;
    int stage_stdin;
};

static void usage(FILE *stream)
{
    (void)fprintf(stream, "usage: shell89 [-c script] [file]\n");
    (void)fprintf(stream, "       shell89            read script from stdin\n");
}

static int input_error(int rc, const char *path)
{
    if (rc == -2)
    {
        (void)fprintf(stderr, "shell89: script exceeds %lu bytes\n",
                      (unsigned long)SHELL89_MAX_SOURCE);
        return 2;
    }
    if (path != NULL)
    {
        (void)fprintf(stderr, "shell89: cannot read script '%s': %s\n", path,
                      strerror(errno));
    }
    else
    {
        (void)fprintf(stderr, "shell89: cannot read script: %s\n",
                      strerror(errno));
    }
    return 1;
}

static int input_stream(int fd, struct cli_input *input)
{
    int rc;

    rc = shell89_cli_read_source(fd, &input->owned, &input->length);
    if (rc != 0)
    {
        return rc;
    }
    input->script = input->owned;
    return 0;
}

static int input_stdin(struct cli_input *input)
{
    input->stage_stdin = -1;
    return input_stream(STDIN_FILENO, input);
}

static int input_file(const char *path, struct cli_input *input)
{
    int fd;
    int rc;
    int saved;

    if (strcmp(path, "-") == 0)
    {
        return input_stdin(input);
    }
    fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        return -1;
    }
    input->stage_stdin = STDIN_FILENO;
    rc = input_stream(fd, input);
    saved = errno;
    if (close(fd) != 0 && rc == 0)
    {
        rc = -1;
        saved = errno;
    }
    errno = saved;
    return rc;
}

static int execute(const char *script, int stage_stdin)
{
    shell89 *sh;
    shell89_result *result;
    struct shell89_run_io io;
    int rc;
    int status;

    sh = shell89_create();
    if (sh == NULL)
    {
        (void)fprintf(stderr, "shell89: cannot create shell\n");
        return 1;
    }
    rc = shell89_cli_register(sh);
    if (rc != SHELL89_OK)
    {
        (void)fprintf(stderr, "shell89: registration failed: %d\n", rc);
        shell89_destroy(sh);
        return 1;
    }
    io.stdin_fd = stage_stdin;
    io.stdout_fd = STDOUT_FILENO;
    io.stderr_fd = STDERR_FILENO;
    rc = shell89_run(sh, script, &io, &result);
    if (rc != SHELL89_OK)
    {
        (void)fprintf(stderr, "shell89: run failed: %d\n", rc);
        shell89_destroy(sh);
        return 1;
    }
    if (shell89_error_kind(result) != SHELL89_ERROR_NONE)
    {
        (void)fprintf(stderr, "shell89:%lu: %s\n",
                      (unsigned long)shell89_error_offset(result),
                      shell89_error_message(result));
    }
    status = shell89_status(result);
    shell89_result_destroy(result);
    shell89_destroy(sh);
    return status;
}

int main(int argc, char **argv)
{
    struct cli_input input;
    shell89_cli_mode mode;
    const char *value;
    int rc;

    value = NULL;
    mode = shell89_cli_parse(argc, argv, &value);
    if (mode == SHELL89_CLI_MODE_HELP)
    {
        usage(stdout);
        return 0;
    }
    if (mode == SHELL89_CLI_MODE_USAGE)
    {
        usage(stderr);
        return 2;
    }
    input.script = NULL;
    input.owned = NULL;
    input.length = 0U;
    input.stage_stdin = STDIN_FILENO;
    if (mode == SHELL89_CLI_MODE_SCRIPT && value != NULL)
    {
        input.script = value;
        input.length = strlen(value);
        rc = 0;
    }
    else if (mode == SHELL89_CLI_MODE_SCRIPT)
    {
        rc = input_stdin(&input);
    }
    else
    {
        rc = input_file(value, &input);
    }
    if (rc != 0)
    {
        free(input.owned);
        return input_error(rc, mode == SHELL89_CLI_MODE_FILE ? value : NULL);
    }
    if (strlen(input.script) != input.length)
    {
        (void)fprintf(stderr, "shell89: script contains NUL\n");
        free(input.owned);
        return 2;
    }
    rc = execute(input.script, input.stage_stdin);
    free(input.owned);
    return rc;
}
