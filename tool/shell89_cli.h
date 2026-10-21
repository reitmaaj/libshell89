#ifndef SHELL89_CLI_H
#define SHELL89_CLI_H

/*
 * shell89_cli.h - helpers for the shell89 demonstration CLI.
 *
 * Not part of the library: tool/ is never compiled into libshell89.a. The CLI
 * uses only the public shell89 API.
 */

#include <stddef.h>

#include "shell89.h"

typedef enum
{
    SHELL89_CLI_MODE_SCRIPT = 0,
    SHELL89_CLI_MODE_FILE,
    SHELL89_CLI_MODE_HELP,
    SHELL89_CLI_MODE_USAGE
} shell89_cli_mode;

struct shell89_cli_line_state
{
    unsigned long lines;
    int any;
    int last_nl;
};

/* Parse argv. On SCRIPT or FILE, *value borrows the argument. Pure. */
shell89_cli_mode shell89_cli_parse(int argc, char **argv, const char **value);

/* Read at most SHELL89_MAX_SOURCE bytes from fd into a NUL-terminated buffer.
 * Return 0 on success, -1 on read/allocation failure, -2 when the input is
 * larger than the limit. */
int shell89_cli_read_source(int fd, char **out, size_t *out_len);

/* ASCII uppercase in place. Pure. */
void shell89_cli_upper(unsigned char *buffer, size_t length);

/* ASCII lowercase in place. Pure. */
void shell89_cli_lower(unsigned char *buffer, size_t length);

void shell89_cli_line_init(struct shell89_cli_line_state *state);
void shell89_cli_line_update(struct shell89_cli_line_state *state,
                             const unsigned char *buffer, size_t length);
unsigned long
shell89_cli_line_total(const struct shell89_cli_line_state *state);

/* Commands. Each returns a process exit status. */
int shell89_cli_text_echo(void *context, const shell89_args *args,
                          shell89_io *io);
int shell89_cli_text_upper(void *context, const shell89_args *args,
                           shell89_io *io);
int shell89_cli_text_lower(void *context, const shell89_args *args,
                           shell89_io *io);
int shell89_cli_text_count(void *context, const shell89_args *args,
                           shell89_io *io);
int shell89_cli_files_read(void *context, const shell89_args *args,
                           shell89_io *io);
int shell89_cli_files_write(void *context, const shell89_args *args,
                            shell89_io *io);

/* Register the demonstration vocabulary. Return 0 or an API error. */
int shell89_cli_register(shell89 *sh);

#endif /* SHELL89_CLI_H */
