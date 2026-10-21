#ifndef SHELL89_LINES_H
#define SHELL89_LINES_H

/*
 * shell89_lines.h - line-oriented commands for the shell89 demonstration CLI.
 *
 * Not part of the library. Lines are bounded so command memory stays bounded.
 */

#include <stddef.h>

#include "shell89.h"

#define SHELL89_CLI_MAX_LINE (64U * 1024U)

/* Parse a non-negative decimal line count. Return 0 or -1. Pure. */
int shell89_cli_parse_lines(const char *text, unsigned long *out);

/* Literal byte-substring match, optionally folding ASCII case. Pure. */
int shell89_cli_match(const unsigned char *line, size_t length,
                      const unsigned char *pattern, size_t pattern_length,
                      int ignore_case);

/* Commands. Each returns a process exit status. */
int shell89_cli_text_grep(void *context, const shell89_args *args,
                          shell89_io *io);
int shell89_cli_text_head(void *context, const shell89_args *args,
                          shell89_io *io);
int shell89_cli_text_skip(void *context, const shell89_args *args,
                          shell89_io *io);

#endif /* SHELL89_LINES_H */
