/* shell89_lines.c - line-oriented demonstration commands. */

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "shell89_lines.h"

#define SHELL89_CLI_READ_CHUNK 8192U
#define SHELL89_CLI_LINE_START 256U

#define READER_LINE 1
#define READER_EOF 0
#define READER_ERROR (-1)
#define READER_TOO_LONG (-2)

struct line_reader
{
    unsigned char chunk[SHELL89_CLI_READ_CHUNK];
    size_t chunk_length;
    size_t chunk_position;
    unsigned char *line;
    size_t line_capacity;
    size_t line_length;
    int eof;
};

static void line_reader_init(struct line_reader *reader)
{
    reader->chunk_length = 0U;
    reader->chunk_position = 0U;
    reader->line = NULL;
    reader->line_capacity = 0U;
    reader->line_length = 0U;
    reader->eof = 0;
}

static void line_reader_free(struct line_reader *reader)
{
    free(reader->line);
    reader->line = NULL;
    reader->line_capacity = 0U;
}

static int line_reader_chunk(struct line_reader *reader, shell89_io *io,
                             const unsigned char **data, size_t *length)
{
    size_t received;

    if (reader->chunk_position < reader->chunk_length)
    {
        *data = reader->chunk + reader->chunk_position;
        *length = reader->chunk_length - reader->chunk_position;
        return 1;
    }
    if (reader->eof != 0)
    {
        return 0;
    }
    received = 0U;
    if (shell89_read(io, reader->chunk, sizeof reader->chunk, &received) != 0)
    {
        return -1;
    }
    if (received == 0U)
    {
        reader->eof = 1;
        return 0;
    }
    reader->chunk_length = received;
    reader->chunk_position = 0U;
    *data = reader->chunk;
    *length = received;
    return 1;
}

static int line_reader_grow(struct line_reader *reader, size_t needed)
{
    size_t capacity;
    unsigned char *grown;

    if (needed <= reader->line_capacity)
    {
        return 0;
    }
    capacity = reader->line_capacity;
    if (capacity == 0U)
    {
        capacity = SHELL89_CLI_LINE_START;
    }
    while (capacity < needed)
    {
        capacity *= 2U;
    }
    grown = (unsigned char *)realloc(reader->line, capacity);
    if (grown == NULL)
    {
        return -1;
    }
    reader->line = grown;
    reader->line_capacity = capacity;
    return 0;
}

static int line_reader_next(struct line_reader *reader, shell89_io *io,
                            const unsigned char **line, size_t *length)
{
    const unsigned char *chunk;
    const unsigned char *newline;
    size_t chunk_length;
    size_t take;
    int rc;

    reader->line_length = 0U;
    for (;;)
    {
        rc = line_reader_chunk(reader, io, &chunk, &chunk_length);
        if (rc < 0)
        {
            return READER_ERROR;
        }
        if (rc == 0)
        {
            if (reader->line_length == 0U)
            {
                return READER_EOF;
            }
            *line = reader->line;
            *length = reader->line_length;
            return READER_LINE;
        }
        newline = (const unsigned char *)memchr(chunk, '\n', chunk_length);
        if (newline == NULL)
        {
            take = chunk_length;
        }
        else
        {
            take = (size_t)(newline - chunk) + 1U;
        }
        if (take > (size_t)SHELL89_CLI_MAX_LINE - reader->line_length)
        {
            return READER_TOO_LONG;
        }
        if (line_reader_grow(reader, reader->line_length + take) != 0)
        {
            return READER_ERROR;
        }
        memcpy(reader->line + reader->line_length, chunk, take);
        reader->line_length += take;
        reader->chunk_position += take;
        if (newline != NULL)
        {
            *line = reader->line;
            *length = reader->line_length;
            return READER_LINE;
        }
    }
}

static int line_reader_drain(struct line_reader *reader, shell89_io *io)
{
    const unsigned char *chunk;
    size_t length;
    int rc;

    for (;;)
    {
        rc = line_reader_chunk(reader, io, &chunk, &length);
        if (rc < 0)
        {
            return -1;
        }
        if (rc == 0)
        {
            return 0;
        }
        reader->chunk_position += length;
    }
}

static int line_reader_copy(struct line_reader *reader, shell89_io *io)
{
    const unsigned char *chunk;
    size_t length;
    int rc;

    for (;;)
    {
        rc = line_reader_chunk(reader, io, &chunk, &length);
        if (rc < 0)
        {
            return -1;
        }
        if (rc == 0)
        {
            return 0;
        }
        if (shell89_write_stdout(io, chunk, length) != 0)
        {
            return -1;
        }
        reader->chunk_position += length;
    }
}

static unsigned char fold_ascii(unsigned char c)
{
    if (c >= (unsigned char)'A' && c <= (unsigned char)'Z')
    {
        return (unsigned char)(c - (unsigned char)'A' + (unsigned char)'a');
    }
    return c;
}

static void line_diag(shell89_io *io, const char *command, const char *detail)
{
    (void)shell89_write_stderr(io, "shell89: ", 9U);
    (void)shell89_write_stderr(io, command, strlen(command));
    (void)shell89_write_stderr(io, ": ", 2U);
    (void)shell89_write_stderr(io, detail, strlen(detail));
    (void)shell89_write_stderr(io, "\n", 1U);
}

static int read_count(const char *command, const shell89_args *args,
                      shell89_io *io, unsigned long *value)
{
    const char *text;

    text = shell89_get_string(args, "lines");
    if (text == NULL)
    {
        line_diag(io, command, "missing --lines");
        return -1;
    }
    if (shell89_cli_parse_lines(text, value) != 0)
    {
        line_diag(io, command, "invalid --lines");
        return -1;
    }
    return 0;
}

int shell89_cli_parse_lines(const char *text, unsigned long *out)
{
    unsigned long value;
    unsigned long digit;
    size_t i;

    if (text == NULL || text[0] == '\0')
    {
        return -1;
    }
    value = 0UL;
    for (i = 0U; text[i] != '\0'; ++i)
    {
        if (text[i] < '0' || text[i] > '9')
        {
            return -1;
        }
        digit = (unsigned long)(text[i] - '0');
        if (value > (ULONG_MAX - digit) / 10UL)
        {
            return -1;
        }
        value = value * 10UL + digit;
    }
    *out = value;
    return 0;
}

int shell89_cli_match(const unsigned char *line, size_t length,
                      const unsigned char *pattern, size_t pattern_length,
                      int ignore_case)
{
    size_t i;
    size_t j;
    unsigned char left;
    unsigned char right;

    if (pattern_length == 0U)
    {
        return 1;
    }
    if (pattern_length > length)
    {
        return 0;
    }
    for (i = 0U; i <= length - pattern_length; ++i)
    {
        for (j = 0U; j < pattern_length; ++j)
        {
            left = line[i + j];
            right = pattern[j];
            if (ignore_case != 0)
            {
                left = fold_ascii(left);
                right = fold_ascii(right);
            }
            if (left != right)
            {
                break;
            }
        }
        if (j == pattern_length)
        {
            return 1;
        }
    }
    return 0;
}

int shell89_cli_text_grep(void *context, const shell89_args *args,
                          shell89_io *io)
{
    const char *pattern_text;
    const unsigned char *pattern;
    const unsigned char *line;
    struct line_reader reader;
    size_t pattern_length;
    size_t length;
    int ignore_case;
    int matched;
    int rc;

    (void)context;
    pattern_text = shell89_get_string(args, "pattern");
    if (pattern_text == NULL)
    {
        line_diag(io, "text grep", "missing --pattern");
        return 2;
    }
    pattern = (const unsigned char *)pattern_text;
    pattern_length = strlen(pattern_text);
    ignore_case = shell89_get_flag(args, "ignore-case");
    matched = 0;
    line_reader_init(&reader);
    for (;;)
    {
        rc = line_reader_next(&reader, io, &line, &length);
        if (rc == READER_TOO_LONG)
        {
            line_diag(io, "text grep", "line exceeds 65536 bytes");
            line_reader_free(&reader);
            return 1;
        }
        if (rc == READER_ERROR)
        {
            line_diag(io, "text grep", "read failed");
            line_reader_free(&reader);
            return 1;
        }
        if (rc == READER_EOF)
        {
            break;
        }
        if (shell89_cli_match(line, length, pattern, pattern_length,
                              ignore_case) != 0)
        {
            if (shell89_write_stdout(io, line, length) != 0)
            {
                line_diag(io, "text grep", "write failed");
                line_reader_free(&reader);
                return 1;
            }
            matched = 1;
        }
    }
    line_reader_free(&reader);
    return matched != 0 ? 0 : 1;
}

int shell89_cli_text_head(void *context, const shell89_args *args,
                          shell89_io *io)
{
    const unsigned char *line;
    struct line_reader reader;
    unsigned long limit;
    unsigned long seen;
    size_t length;
    int rc;

    (void)context;
    if (read_count("text head", args, io, &limit) != 0)
    {
        return 2;
    }
    line_reader_init(&reader);
    seen = 0UL;
    while (seen < limit)
    {
        rc = line_reader_next(&reader, io, &line, &length);
        if (rc == READER_TOO_LONG)
        {
            line_diag(io, "text head", "line exceeds 65536 bytes");
            line_reader_free(&reader);
            return 1;
        }
        if (rc == READER_ERROR)
        {
            line_diag(io, "text head", "read failed");
            line_reader_free(&reader);
            return 1;
        }
        if (rc == READER_EOF)
        {
            break;
        }
        if (shell89_write_stdout(io, line, length) != 0)
        {
            line_diag(io, "text head", "write failed");
            line_reader_free(&reader);
            return 1;
        }
        seen += 1UL;
    }
    if (line_reader_drain(&reader, io) != 0)
    {
        line_diag(io, "text head", "read failed");
        line_reader_free(&reader);
        return 1;
    }
    line_reader_free(&reader);
    return 0;
}

int shell89_cli_text_skip(void *context, const shell89_args *args,
                          shell89_io *io)
{
    const unsigned char *line;
    struct line_reader reader;
    unsigned long limit;
    unsigned long seen;
    size_t length;
    int rc;

    (void)context;
    if (read_count("text skip", args, io, &limit) != 0)
    {
        return 2;
    }
    line_reader_init(&reader);
    seen = 0UL;
    while (seen < limit)
    {
        rc = line_reader_next(&reader, io, &line, &length);
        if (rc == READER_TOO_LONG)
        {
            line_diag(io, "text skip", "line exceeds 65536 bytes");
            line_reader_free(&reader);
            return 1;
        }
        if (rc == READER_ERROR)
        {
            line_diag(io, "text skip", "read failed");
            line_reader_free(&reader);
            return 1;
        }
        if (rc == READER_EOF)
        {
            line_reader_free(&reader);
            return 0;
        }
        seen += 1UL;
    }
    if (line_reader_copy(&reader, io) != 0)
    {
        line_diag(io, "text skip", "write failed");
        line_reader_free(&reader);
        return 1;
    }
    line_reader_free(&reader);
    return 0;
}
