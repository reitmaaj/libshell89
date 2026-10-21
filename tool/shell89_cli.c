/* shell89_cli.c - demonstration vocabulary and helpers for the shell89 CLI. */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "shell89_cli.h"
#include "shell89_lines.h"

#define SHELL89_CLI_CHUNK 8192U
#define SHELL89_CLI_START 4096U
#define SHELL89_CLI_FILE_MODE ((mode_t)0666)

static const struct shell89_option echo_options[] = {
    {"text", SHELL89_STRING, 1}, {NULL, 0, 0}};
static const struct shell89_option count_options[] = {
    {"lines", SHELL89_FLAG, 0}, {NULL, 0, 0}};
static const struct shell89_option grep_options[] = {
    {"pattern", SHELL89_STRING, 1},
    {"ignore-case", SHELL89_FLAG, 0},
    {NULL, 0, 0}};
static const struct shell89_option line_count_options[] = {
    {"lines", SHELL89_STRING, 1}, {NULL, 0, 0}};
static const struct shell89_option path_options[] = {
    {"path", SHELL89_STRING, 1}, {NULL, 0, 0}};
static const struct shell89_option write_options[] = {
    {"path", SHELL89_STRING, 1}, {"append", SHELL89_FLAG, 0}, {NULL, 0, 0}};
static const struct shell89_option no_options[] = {{NULL, 0, 0}};

shell89_cli_mode shell89_cli_parse(int argc, char **argv, const char **value)
{
    *value = NULL;
    if (argc == 1)
    {
        return SHELL89_CLI_MODE_SCRIPT;
    }
    if (argc == 2)
    {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)
        {
            return SHELL89_CLI_MODE_HELP;
        }
        if (strcmp(argv[1], "-c") == 0)
        {
            return SHELL89_CLI_MODE_USAGE;
        }
        if (argv[1][0] == '-' && argv[1][1] != '\0')
        {
            return SHELL89_CLI_MODE_USAGE;
        }
        *value = argv[1];
        return SHELL89_CLI_MODE_FILE;
    }
    if (argc == 3 && strcmp(argv[1], "-c") == 0)
    {
        *value = argv[2];
        return SHELL89_CLI_MODE_SCRIPT;
    }
    return SHELL89_CLI_MODE_USAGE;
}

static int read_step(int fd, char *buffer, size_t length, size_t capacity,
                     size_t *length_out)
{
    ssize_t n;

    n = read(fd, buffer + length, capacity - length);
    if (n < 0)
    {
        if (errno == EINTR)
        {
            return 1;
        }
        return -1;
    }
    if (n == 0)
    {
        return 0;
    }
    *length_out = length + (size_t)n;
    return 2;
}

static int grow_buffer(char **buffer, size_t *capacity, size_t maximum)
{
    size_t next;
    char *grown;

    next = *capacity * 2U;
    if (next > maximum)
    {
        next = maximum;
    }
    grown = (char *)realloc(*buffer, next);
    if (grown == NULL)
    {
        errno = ENOMEM;
        return -1;
    }
    *buffer = grown;
    *capacity = next;
    return 0;
}

int shell89_cli_read_source(int fd, char **out, size_t *out_len)
{
    size_t maximum;
    size_t capacity;
    size_t length;
    char *buffer;
    char *final;
    int rc;

    maximum = (size_t)SHELL89_MAX_SOURCE + 1U;
    capacity = SHELL89_CLI_START;
    if (capacity > maximum)
    {
        capacity = maximum;
    }
    buffer = (char *)malloc(capacity);
    if (buffer == NULL)
    {
        errno = ENOMEM;
        return -1;
    }
    length = 0U;
    for (;;)
    {
        if (length == capacity && capacity < maximum)
        {
            rc = grow_buffer(&buffer, &capacity, maximum);
            if (rc != 0)
            {
                free(buffer);
                return -1;
            }
        }
        rc = read_step(fd, buffer, length, capacity, &length);
        if (rc < 0)
        {
            rc = errno;
            free(buffer);
            errno = rc;
            return -1;
        }
        if (rc == 0)
        {
            break;
        }
        if (length > (size_t)SHELL89_MAX_SOURCE)
        {
            free(buffer);
            errno = EFBIG;
            return -2;
        }
    }
    final = (char *)realloc(buffer, length + 1U);
    if (final == NULL)
    {
        free(buffer);
        errno = ENOMEM;
        return -1;
    }
    final[length] = '\0';
    *out = final;
    *out_len = length;
    return 0;
}

void shell89_cli_upper(unsigned char *buffer, size_t length)
{
    size_t i;

    for (i = 0U; i < length; ++i)
    {
        if (buffer[i] >= (unsigned char)'a' && buffer[i] <= (unsigned char)'z')
        {
            buffer[i] = (unsigned char)(buffer[i] - (unsigned char)'a' +
                                        (unsigned char)'A');
        }
    }
}

void shell89_cli_lower(unsigned char *buffer, size_t length)
{
    size_t i;

    for (i = 0U; i < length; ++i)
    {
        if (buffer[i] >= (unsigned char)'A' && buffer[i] <= (unsigned char)'Z')
        {
            buffer[i] = (unsigned char)(buffer[i] - (unsigned char)'A' +
                                        (unsigned char)'a');
        }
    }
}

void shell89_cli_line_init(struct shell89_cli_line_state *state)
{
    state->lines = 0UL;
    state->any = 0;
    state->last_nl = 0;
}

void shell89_cli_line_update(struct shell89_cli_line_state *state,
                             const unsigned char *buffer, size_t length)
{
    size_t i;

    for (i = 0U; i < length; ++i)
    {
        state->any = 1;
        if (buffer[i] == (unsigned char)'\n')
        {
            state->lines += 1UL;
            state->last_nl = 1;
        }
        else
        {
            state->last_nl = 0;
        }
    }
}

unsigned long shell89_cli_line_total(const struct shell89_cli_line_state *state)
{
    if (state->any != 0 && state->last_nl == 0)
    {
        return state->lines + 1UL;
    }
    return state->lines;
}

static void cli_error(shell89_io *io, const char *command, const char *path,
                      int error_number)
{
    char code[32];
    int length;

    (void)shell89_write_stderr(io, "shell89: ", 9U);
    (void)shell89_write_stderr(io, command, strlen(command));
    (void)shell89_write_stderr(io, ": ", 2U);
    if (path != NULL)
    {
        (void)shell89_write_stderr(io, path, strlen(path));
    }
    length = sprintf(code, " (error %d)", error_number);
    if (length > 0)
    {
        (void)shell89_write_stderr(io, code, (size_t)length);
    }
    (void)shell89_write_stderr(io, "\n", 1U);
}

static int write_fd_all(int fd, const unsigned char *buffer, size_t length)
{
    size_t done;
    ssize_t n;

    done = 0U;
    while (done < length)
    {
        n = write(fd, buffer + done, length - done);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        if (n == 0)
        {
            errno = EIO;
            return -1;
        }
        done += (size_t)n;
    }
    return 0;
}

int shell89_cli_text_echo(void *context, const shell89_args *args,
                          shell89_io *io)
{
    const char *text;

    (void)context;
    text = shell89_get_string(args, "text");
    if (text == NULL)
    {
        return 2;
    }
    if (shell89_write_stdout(io, text, strlen(text)) != 0)
    {
        return 1;
    }
    return shell89_write_stdout(io, "\n", 1U) != 0;
}

int shell89_cli_text_upper(void *context, const shell89_args *args,
                           shell89_io *io)
{
    unsigned char buffer[SHELL89_CLI_CHUNK];
    size_t received;

    (void)context;
    (void)args;
    for (;;)
    {
        if (shell89_read(io, buffer, sizeof buffer, &received) != 0)
        {
            return 1;
        }
        if (received == 0U)
        {
            return 0;
        }
        shell89_cli_upper(buffer, received);
        if (shell89_write_stdout(io, buffer, received) != 0)
        {
            return 1;
        }
    }
}

int shell89_cli_text_lower(void *context, const shell89_args *args,
                           shell89_io *io)
{
    unsigned char buffer[SHELL89_CLI_CHUNK];
    size_t received;

    (void)context;
    (void)args;
    for (;;)
    {
        if (shell89_read(io, buffer, sizeof buffer, &received) != 0)
        {
            return 1;
        }
        if (received == 0U)
        {
            return 0;
        }
        shell89_cli_lower(buffer, received);
        if (shell89_write_stdout(io, buffer, received) != 0)
        {
            return 1;
        }
    }
}

int shell89_cli_text_count(void *context, const shell89_args *args,
                           shell89_io *io)
{
    unsigned char buffer[SHELL89_CLI_CHUNK];
    struct shell89_cli_line_state line_state;
    unsigned long bytes;
    unsigned long value;
    size_t received;
    char text[32];
    int length;
    int want_lines;

    (void)context;
    shell89_cli_line_init(&line_state);
    bytes = 0UL;
    want_lines = shell89_get_flag(args, "lines");
    for (;;)
    {
        if (shell89_read(io, buffer, sizeof buffer, &received) != 0)
        {
            return 1;
        }
        if (received == 0U)
        {
            break;
        }
        bytes += (unsigned long)received;
        if (want_lines != 0)
        {
            shell89_cli_line_update(&line_state, buffer, received);
        }
    }
    value = want_lines != 0 ? shell89_cli_line_total(&line_state) : bytes;
    length = sprintf(text, "%lu\n", value);
    if (length < 0)
    {
        return 1;
    }
    return shell89_write_stdout(io, text, (size_t)length) != 0;
}

int shell89_cli_files_read(void *context, const shell89_args *args,
                           shell89_io *io)
{
    const char *path;
    unsigned char buffer[SHELL89_CLI_CHUNK];
    ssize_t n;
    int fd;
    int rc;

    (void)context;
    path = shell89_get_string(args, "path");
    if (path == NULL)
    {
        return 1;
    }
    fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        rc = errno;
        cli_error(io, "files read", path, rc);
        return 1;
    }
    rc = 0;
    for (;;)
    {
        n = read(fd, buffer, sizeof buffer);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            rc = errno;
            break;
        }
        if (n == 0)
        {
            break;
        }
        if (shell89_write_stdout(io, buffer, (size_t)n) != 0)
        {
            rc = shell89_stream_error(io);
            if (rc == 0)
            {
                rc = EIO;
            }
            break;
        }
    }
    if (close(fd) != 0 && rc == 0)
    {
        rc = errno;
    }
    if (rc != 0)
    {
        cli_error(io, "files read", path, rc);
        return 1;
    }
    return 0;
}

int shell89_cli_files_write(void *context, const shell89_args *args,
                            shell89_io *io)
{
    const char *path;
    unsigned char buffer[SHELL89_CLI_CHUNK];
    size_t received;
    int flags;
    int fd;
    int rc;

    (void)context;
    path = shell89_get_string(args, "path");
    if (path == NULL)
    {
        return 1;
    }
    flags = O_WRONLY | O_CREAT;
    if (shell89_get_flag(args, "append") != 0)
    {
        flags |= O_APPEND;
    }
    else
    {
        flags |= O_TRUNC;
    }
    fd = open(path, flags, SHELL89_CLI_FILE_MODE);
    if (fd < 0)
    {
        rc = errno;
        cli_error(io, "files write", path, rc);
        return 1;
    }
    rc = 0;
    for (;;)
    {
        if (shell89_read(io, buffer, sizeof buffer, &received) != 0)
        {
            rc = shell89_stream_error(io);
            if (rc == 0)
            {
                rc = EIO;
            }
            break;
        }
        if (received == 0U)
        {
            break;
        }
        if (write_fd_all(fd, buffer, received) != 0)
        {
            rc = errno;
            break;
        }
    }
    if (close(fd) != 0 && rc == 0)
    {
        rc = errno;
    }
    if (rc != 0)
    {
        cli_error(io, "files write", path, rc);
        return 1;
    }
    return 0;
}

int shell89_cli_register(shell89 *sh)
{
    int rc;

    rc = shell89_register_function(sh, "text", "echo", echo_options,
                                   shell89_cli_text_echo, NULL);
    if (rc != SHELL89_OK)
    {
        return rc;
    }
    rc = shell89_register_function(sh, "text", "upper", no_options,
                                   shell89_cli_text_upper, NULL);
    if (rc != SHELL89_OK)
    {
        return rc;
    }
    rc = shell89_register_function(sh, "text", "lower", no_options,
                                   shell89_cli_text_lower, NULL);
    if (rc != SHELL89_OK)
    {
        return rc;
    }
    rc = shell89_register_function(sh, "text", "count", count_options,
                                   shell89_cli_text_count, NULL);
    if (rc != SHELL89_OK)
    {
        return rc;
    }
    rc = shell89_register_function(sh, "text", "grep", grep_options,
                                   shell89_cli_text_grep, NULL);
    if (rc != SHELL89_OK)
    {
        return rc;
    }
    rc = shell89_register_function(sh, "text", "head", line_count_options,
                                   shell89_cli_text_head, NULL);
    if (rc != SHELL89_OK)
    {
        return rc;
    }
    rc = shell89_register_function(sh, "text", "skip", line_count_options,
                                   shell89_cli_text_skip, NULL);
    if (rc != SHELL89_OK)
    {
        return rc;
    }
    rc = shell89_register_function(sh, "files", "read", path_options,
                                   shell89_cli_files_read, NULL);
    if (rc != SHELL89_OK)
    {
        return rc;
    }
    return shell89_register_function(sh, "files", "write", write_options,
                                     shell89_cli_files_write, NULL);
}
