/* shell89_parse.c - hand-written lexer, grammar, JSON literals, and AST. */

#include <string.h>

#include <string89.h>
#include <unicode89.h>

#include "shell89_priv.h"

struct shell89_priv_parser
{
    const unsigned char *src;
    size_t len;
    size_t pos;
    struct shell89_priv_script *script;
    struct shell89_priv_diag *diag;
};

static void *parse_malloc(void *ctx, size_t size)
{
    (void)ctx;
    return shell89_priv_malloc(size);
}

static void *parse_realloc(void *ctx, void *ptr, size_t size)
{
    (void)ctx;
    return shell89_priv_realloc(ptr, size);
}

static void parse_free(void *ctx, void *ptr)
{
    (void)ctx;
    shell89_priv_free(ptr);
}

static const string89_alloc parse_alloc = {NULL, parse_malloc, parse_realloc,
                                           parse_free};

void shell89_priv_script_init(struct shell89_priv_script *script)
{
    script->pipelines = NULL;
    script->pipeline_count = 0;
    script->pipeline_capacity = 0;
}

static void command_free(struct shell89_priv_command *cmd);

static void options_free(struct shell89_priv_command *cmd)
{
    size_t i;

    for (i = 0; i < cmd->option_count; i++)
    {
        shell89_priv_str_free(&cmd->options[i].name);
        shell89_priv_str_free(&cmd->options[i].value);
    }
    if (cmd->options != NULL)
    {
        shell89_priv_free(cmd->options);
    }
    cmd->options = NULL;
    cmd->option_count = 0;
    cmd->option_capacity = 0;
}

static void command_free(struct shell89_priv_command *cmd)
{
    shell89_priv_str_free(&cmd->group);
    shell89_priv_str_free(&cmd->operation);
    options_free(cmd);
    if (cmd->proc_argv != NULL)
    {
        size_t i;

        for (i = cmd->proc_argv_owned_from; i < cmd->proc_argv_count; i++)
        {
            shell89_priv_free(cmd->proc_argv[i]);
        }
        if (cmd->proc_argv_count != 0 && cmd->proc_argv_owned_from != 0)
        {
            shell89_priv_free(cmd->proc_argv[0]);
        }
        shell89_priv_free(cmd->proc_argv);
    }
    cmd->proc_argv = NULL;
    cmd->proc_argv_count = 0;
    cmd->proc_argv_owned_from = 0;
    cmd->reg = NULL;
}

static void pipeline_free(struct shell89_priv_pipeline *pipeline)
{
    size_t i;

    for (i = 0; i < pipeline->command_count; i++)
    {
        command_free(&pipeline->commands[i]);
    }
    if (pipeline->commands != NULL)
    {
        shell89_priv_free(pipeline->commands);
    }
    pipeline->commands = NULL;
    pipeline->command_count = 0;
    pipeline->command_capacity = 0;
}

void shell89_priv_script_free(struct shell89_priv_script *script)
{
    size_t i;

    for (i = 0; i < script->pipeline_count; i++)
    {
        pipeline_free(&script->pipelines[i]);
    }
    if (script->pipelines != NULL)
    {
        shell89_priv_free(script->pipelines);
    }
    script->pipelines = NULL;
    script->pipeline_count = 0;
    script->pipeline_capacity = 0;
}

void shell89_priv_diag_clear(struct shell89_priv_diag *diag)
{
    diag->kind = SHELL89_ERROR_NONE;
    diag->shell_status = 0;
    diag->offset = 0;
    diag->message[0] = '\0';
}

void shell89_priv_diag_set(struct shell89_priv_diag *diag, int kind,
                           int shell_status, size_t offset, const char *message)
{
    size_t n;
    size_t i;

    diag->kind = kind;
    diag->shell_status = shell_status;
    diag->offset = offset;
    n = strlen(message);
    if (n >= sizeof diag->message)
    {
        n = sizeof diag->message - 1;
    }
    for (i = 0; i < n; i++)
    {
        diag->message[i] = message[i];
    }
    diag->message[n] = '\0';
}

static int fail(struct shell89_priv_parser *p, size_t offset,
                const char *message)
{
    shell89_priv_diag_set(p->diag, SHELL89_ERROR_SYNTAX, 2, offset, message);
    return 0;
}

static int fail_limit(struct shell89_priv_parser *p, size_t offset,
                      const char *message)
{
    shell89_priv_diag_set(p->diag, SHELL89_ERROR_LIMIT, 2, offset, message);
    return 0;
}

static int is_hspace(unsigned char c)
{
    return c == ' ' || c == '\t';
}

static int is_separator_at(const struct shell89_priv_parser *p, size_t at)
{
    if (at >= p->len)
    {
        return 0;
    }
    if (p->src[at] == ';' || p->src[at] == '\n')
    {
        return 1;
    }
    if (p->src[at] == '\r' && at + 1 < p->len && p->src[at + 1] == '\n')
    {
        return 1;
    }
    return 0;
}

static int consume_separator(struct shell89_priv_parser *p)
{
    if (p->pos >= p->len)
    {
        return 0;
    }
    if (p->src[p->pos] == ';' || p->src[p->pos] == '\n')
    {
        p->pos++;
        return 1;
    }
    if (p->src[p->pos] == '\r' && p->pos + 1 < p->len &&
        p->src[p->pos + 1] == '\n')
    {
        p->pos += 2;
        return 1;
    }
    return 0;
}

static void skip_hspace(struct shell89_priv_parser *p)
{
    while (p->pos < p->len && is_hspace(p->src[p->pos]))
    {
        p->pos++;
    }
}

static void skip_ws(struct shell89_priv_parser *p)
{
    int again;

    again = 1;
    while (again)
    {
        again = 0;
        while (p->pos < p->len && is_hspace(p->src[p->pos]))
        {
            p->pos++;
            again = 1;
        }
        if (p->pos < p->len && p->src[p->pos] == '\n')
        {
            p->pos++;
            again = 1;
        }
        else if (p->pos + 1 < p->len && p->src[p->pos] == '\r' &&
                 p->src[p->pos + 1] == '\n')
        {
            p->pos += 2;
            again = 1;
        }
    }
}

static void skip_script_space(struct shell89_priv_parser *p)
{
    int again;

    again = 1;
    while (again)
    {
        again = 0;
        while (p->pos < p->len && is_hspace(p->src[p->pos]))
        {
            p->pos++;
            again = 1;
        }
        if (consume_separator(p))
        {
            again = 1;
        }
    }
}

static int is_letter(unsigned char c)
{
    return c >= 'a' && c <= 'z';
}

static int is_name_char(unsigned char c)
{
    return is_letter(c) || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

static int parse_name(struct shell89_priv_parser *p,
                      struct shell89_priv_str *out)
{
    size_t start;
    size_t n;

    start = p->pos;
    if (p->pos >= p->len || !is_letter(p->src[p->pos]))
    {
        return fail(p, p->pos, "expected a lowercase command name");
    }
    p->pos++;
    while (p->pos < p->len && is_name_char(p->src[p->pos]))
    {
        p->pos++;
    }
    n = p->pos - start;
    if (n > SHELL89_MAX_NAME)
    {
        return fail_limit(p, start, "name exceeds the configured limit");
    }
    if (shell89_priv_str_set(out, p->src + start, n) != 0)
    {
        return -1;
    }
    return 1;
}

static int hex_value(unsigned char c)
{
    if (c >= '0' && c <= '9')
    {
        return (int)(c - '0');
    }
    if (c >= 'a' && c <= 'f')
    {
        return (int)(c - 'a') + 10;
    }
    if (c >= 'A' && c <= 'F')
    {
        return (int)(c - 'A') + 10;
    }
    return -1;
}

static int parse_hex4(struct shell89_priv_parser *p, unsigned int *out)
{
    unsigned int v;
    int d;
    size_t i;

    v = 0;
    for (i = 0; i < 4; i++)
    {
        if (p->pos >= p->len)
        {
            return fail(p, p->pos, "incomplete Unicode escape");
        }
        d = hex_value(p->src[p->pos]);
        if (d < 0)
        {
            return fail(p, p->pos, "invalid Unicode escape");
        }
        v = (v << 4) | (unsigned int)d;
        p->pos++;
    }
    *out = v;
    return 1;
}

static int append_cp(struct shell89_priv_parser *p, string89_buf *buf,
                     unicode89_cp cp, size_t offset)
{
    int r;

    if (cp == 0)
    {
        return fail(p, offset, "NUL is not permitted in an option value");
    }
    r = string89_buf_append_cp(buf, &parse_alloc, cp);
    if (r == STRING89_ENOMEM)
    {
        return -1;
    }
    if (r != STRING89_OK)
    {
        return fail(p, offset, "invalid Unicode scalar value");
    }
    if (buf->len > (size_t)SHELL89_MAX_VALUE)
    {
        return fail_limit(p, offset, "option value exceeds the limit");
    }
    return 1;
}

static int parse_unicode_escape(struct shell89_priv_parser *p,
                                string89_buf *buf, size_t offset)
{
    unsigned int hi;
    unsigned int lo;
    unicode89_cp cp;
    int r;

    if (parse_hex4(p, &hi) <= 0)
    {
        return 0;
    }
    if (unicode89_utf16_is_high_surrogate(hi))
    {
        if (p->pos + 1 >= p->len || p->src[p->pos] != '\\' ||
            p->src[p->pos + 1] != 'u')
        {
            return fail(p, offset, "unpaired high surrogate");
        }
        p->pos += 2;
        if (parse_hex4(p, &lo) <= 0)
        {
            return 0;
        }
        if (!unicode89_utf16_is_low_surrogate(lo))
        {
            return fail(p, offset, "invalid surrogate pair");
        }
        if (!unicode89_utf16_decode_pair(hi, lo, &cp))
        {
            return fail(p, offset, "invalid surrogate pair");
        }
    }
    else if (unicode89_utf16_is_low_surrogate(hi))
    {
        return fail(p, offset, "unpaired low surrogate");
    }
    else
    {
        cp = (unicode89_cp)hi;
    }
    r = append_cp(p, buf, cp, offset);
    return r;
}

static int parse_json_string(struct shell89_priv_parser *p,
                             struct shell89_priv_str *out, size_t offset)
{
    string89_buf buf;
    string89_view raw;
    unicode89_cp cp;
    unicode89_status st;
    size_t next;
    size_t start;
    unsigned char c;
    int r;

    if (p->pos >= p->len || p->src[p->pos] != '"')
    {
        return fail(p, offset, "expected a JSON string");
    }
    p->pos++;
    string89_buf_init(&buf);
    while (p->pos < p->len)
    {
        start = p->pos;
        c = p->src[p->pos];
        if (c == '"')
        {
            p->pos++;
            r = shell89_priv_str_set(out, buf.data, buf.len);
            string89_buf_free(&buf, &parse_alloc);
            if (r != 0)
            {
                return -1;
            }
            return 1;
        }
        if (c == '\\')
        {
            p->pos++;
            if (p->pos >= p->len)
            {
                string89_buf_free(&buf, &parse_alloc);
                return fail(p, offset, "unterminated escape");
            }
            c = p->src[p->pos];
            p->pos++;
            if (c == '"' || c == '\\' || c == '/')
            {
                raw.data = p->src + p->pos - 1;
                raw.len = 1;
            }
            else if (c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't')
            {
                unicode89_cp ctl;

                if (c == 'b')
                {
                    ctl = 8;
                }
                else if (c == 'f')
                {
                    ctl = 12;
                }
                else if (c == 'n')
                {
                    ctl = 10;
                }
                else if (c == 'r')
                {
                    ctl = 13;
                }
                else
                {
                    ctl = 9;
                }
                r = append_cp(p, &buf, ctl, start);
                if (r <= 0)
                {
                    string89_buf_free(&buf, &parse_alloc);
                    return r;
                }
                continue;
            }
            else if (c == 'u')
            {
                r = parse_unicode_escape(p, &buf, start);
                if (r <= 0)
                {
                    string89_buf_free(&buf, &parse_alloc);
                    return r;
                }
                continue;
            }
            else
            {
                string89_buf_free(&buf, &parse_alloc);
                return fail(p, start, "invalid JSON escape");
            }
            r = string89_buf_append(&buf, &parse_alloc, raw);
            if (r == STRING89_ENOMEM)
            {
                string89_buf_free(&buf, &parse_alloc);
                return -1;
            }
            if (r != STRING89_OK)
            {
                string89_buf_free(&buf, &parse_alloc);
                return fail(p, start, "option value exceeds the limit");
            }
            continue;
        }
        if (c < 0x20)
        {
            string89_buf_free(&buf, &parse_alloc);
            return fail(p, start, "unescaped control byte in string");
        }
        if (c < 0x80)
        {
            raw.data = p->src + p->pos;
            raw.len = 1;
            p->pos++;
        }
        else
        {
            st = unicode89_utf8_decode(p->src, p->len, p->pos, &cp, &next);
            if (st != UNICODE89_OK)
            {
                string89_buf_free(&buf, &parse_alloc);
                return fail(p, start, "malformed UTF-8 in string");
            }
            raw.data = p->src + p->pos;
            raw.len = next - p->pos;
            p->pos = next;
        }
        r = string89_buf_append(&buf, &parse_alloc, raw);
        if (r == STRING89_ENOMEM)
        {
            string89_buf_free(&buf, &parse_alloc);
            return -1;
        }
        if (r != STRING89_OK)
        {
            string89_buf_free(&buf, &parse_alloc);
            return fail(p, start, "option value exceeds the limit");
        }
        if (buf.len > (size_t)SHELL89_MAX_VALUE)
        {
            string89_buf_free(&buf, &parse_alloc);
            return fail_limit(p, start, "option value exceeds the limit");
        }
    }
    string89_buf_free(&buf, &parse_alloc);
    return fail(p, offset, "unterminated JSON string");
}

static int grow_options(struct shell89_priv_parser *p,
                        struct shell89_priv_command *cmd, size_t at)
{
    size_t cap;
    size_t bytes;
    struct shell89_priv_option_use *next;
    size_t i;

    if (cmd->option_count < cmd->option_capacity)
    {
        return 1;
    }
    cap = cmd->option_capacity == 0 ? 4 : cmd->option_capacity * 2;
    if (cap > (size_t)SHELL89_MAX_OPTIONS)
    {
        cap = (size_t)SHELL89_MAX_OPTIONS;
    }
    if (cap <= cmd->option_capacity)
    {
        return fail_limit(p, at, "too many options");
    }
    bytes = cap * sizeof *next;
    next = (struct shell89_priv_option_use *)shell89_priv_realloc(cmd->options,
                                                                  bytes);
    if (next == NULL)
    {
        return -1;
    }
    cmd->options = next;
    for (i = cmd->option_capacity; i < cap; i++)
    {
        shell89_priv_str_init(&cmd->options[i].name);
        shell89_priv_str_init(&cmd->options[i].value);
        cmd->options[i].has_value = 0;
        cmd->options[i].offset = 0;
        cmd->options[i].schema_index = 0;
    }
    cmd->option_capacity = cap;
    return 1;
}

static int parse_option(struct shell89_priv_parser *p,
                        struct shell89_priv_command *cmd)
{
    struct shell89_priv_option_use *opt;
    size_t at;
    int r;

    at = p->pos;
    p->pos += 2;
    if (p->pos >= p->len || !is_letter(p->src[p->pos]))
    {
        return fail(p, at, "expected an option name");
    }
    if (cmd->option_count >= (size_t)SHELL89_MAX_OPTIONS)
    {
        return fail_limit(p, at, "too many options");
    }
    r = grow_options(p, cmd, at);
    if (r <= 0)
    {
        return r;
    }
    opt = &cmd->options[cmd->option_count];
    cmd->option_count++;
    r = parse_name(p, &opt->name);
    if (r <= 0)
    {
        return r;
    }
    opt->offset = at;
    {
        size_t i;

        for (i = 0; i + 1 < cmd->option_count; i++)
        {
            if (shell89_priv_str_equal(&cmd->options[i].name, &opt->name))
            {
                return fail(p, at, "duplicate option name");
            }
        }
    }
    if (p->pos < p->len && p->src[p->pos] == '=')
    {
        p->pos++;
        r = parse_json_string(p, &opt->value, at);
        if (r <= 0)
        {
            return r;
        }
        opt->has_value = 1;
    }
    return 1;
}

static int grow_commands(struct shell89_priv_parser *p,
                         struct shell89_priv_pipeline *pipeline, size_t at)
{
    size_t cap;
    struct shell89_priv_command *next;
    size_t i;

    if (pipeline->command_count < pipeline->command_capacity)
    {
        return 1;
    }
    cap = pipeline->command_capacity == 0 ? 2 : pipeline->command_capacity * 2;
    if (cap > (size_t)SHELL89_MAX_STAGES)
    {
        cap = (size_t)SHELL89_MAX_STAGES;
    }
    if (cap <= pipeline->command_capacity)
    {
        return fail_limit(p, at, "too many pipeline stages");
    }
    next = (struct shell89_priv_command *)shell89_priv_realloc(
        pipeline->commands, cap * sizeof *next);
    if (next == NULL)
    {
        return -1;
    }
    pipeline->commands = next;
    for (i = pipeline->command_capacity; i < cap; i++)
    {
        shell89_priv_str_init(&pipeline->commands[i].group);
        shell89_priv_str_init(&pipeline->commands[i].operation);
        pipeline->commands[i].options = NULL;
        pipeline->commands[i].option_count = 0;
        pipeline->commands[i].option_capacity = 0;
        pipeline->commands[i].offset = 0;
        pipeline->commands[i].reg = NULL;
        pipeline->commands[i].proc_argv = NULL;
        pipeline->commands[i].proc_argv_count = 0;
        pipeline->commands[i].proc_argv_owned_from = 0;
    }
    pipeline->command_capacity = cap;
    return 1;
}

static int parse_command(struct shell89_priv_parser *p,
                         struct shell89_priv_pipeline *pipeline)
{
    struct shell89_priv_command *cmd;
    size_t at;
    int r;

    skip_hspace(p);
    at = p->pos;
    if (pipeline->command_count >= (size_t)SHELL89_MAX_STAGES)
    {
        return fail_limit(p, at, "too many pipeline stages");
    }
    r = grow_commands(p, pipeline, at);
    if (r <= 0)
    {
        return r;
    }
    cmd = &pipeline->commands[pipeline->command_count];
    pipeline->command_count++;
    cmd->offset = at;
    r = parse_name(p, &cmd->group);
    if (r <= 0)
    {
        return r;
    }
    if (p->pos >= p->len || !is_hspace(p->src[p->pos]))
    {
        return fail(p, p->pos, "expected whitespace before the operation");
    }
    skip_hspace(p);
    r = parse_name(p, &cmd->operation);
    if (r <= 0)
    {
        return r;
    }
    while (1)
    {
        size_t save;

        save = p->pos;
        skip_hspace(p);
        if (p->pos + 1 < p->len && p->src[p->pos] == '-' &&
            p->src[p->pos + 1] == '-')
        {
            r = parse_option(p, cmd);
            if (r <= 0)
            {
                return r;
            }
            continue;
        }
        if (p->pos < p->len && p->src[p->pos] == '=')
        {
            return fail(p, p->pos, "unexpected '='");
        }
        p->pos = save;
        return 1;
    }
}

static size_t pipe_lookahead(struct shell89_priv_parser *p)
{
    size_t at;

    at = p->pos;
    while (at < p->len)
    {
        if (is_hspace(p->src[at]) || p->src[at] == '\n')
        {
            at++;
            continue;
        }
        if (p->src[at] == '\r' && at + 1 < p->len && p->src[at + 1] == '\n')
        {
            at += 2;
            continue;
        }
        break;
    }
    if (at < p->len && p->src[at] == '|')
    {
        return at;
    }
    return (size_t)-1;
}

static int grow_pipelines(struct shell89_priv_parser *p)
{
    size_t cap;
    struct shell89_priv_pipeline *next;
    size_t i;

    if (p->script->pipeline_count < p->script->pipeline_capacity)
    {
        return 1;
    }
    cap = p->script->pipeline_capacity == 0 ? 4
                                            : p->script->pipeline_capacity * 2;
    if (cap > (size_t)SHELL89_MAX_PIPELINES)
    {
        cap = (size_t)SHELL89_MAX_PIPELINES;
    }
    if (cap <= p->script->pipeline_capacity)
    {
        return fail_limit(p, p->pos, "too many pipelines");
    }
    next = (struct shell89_priv_pipeline *)shell89_priv_realloc(
        p->script->pipelines, cap * sizeof *next);
    if (next == NULL)
    {
        return -1;
    }
    p->script->pipelines = next;
    for (i = p->script->pipeline_capacity; i < cap; i++)
    {
        p->script->pipelines[i].commands = NULL;
        p->script->pipelines[i].command_count = 0;
        p->script->pipelines[i].command_capacity = 0;
    }
    p->script->pipeline_capacity = cap;
    return 1;
}

static int parse_pipeline(struct shell89_priv_parser *p)
{
    struct shell89_priv_pipeline *pipeline;
    size_t at;
    size_t pipe_at;
    int r;

    r = grow_pipelines(p);
    if (r <= 0)
    {
        return r;
    }
    pipeline = &p->script->pipelines[p->script->pipeline_count];
    at = p->pos;
    r = parse_command(p, pipeline);
    if (r <= 0)
    {
        pipeline_free(pipeline);
        return r;
    }
    while (1)
    {
        size_t save;

        save = p->pos;
        pipe_at = pipe_lookahead(p);
        if (pipe_at == (size_t)-1)
        {
            p->pos = save;
            break;
        }
        p->pos = pipe_at + 1;
        skip_ws(p);
        r = parse_command(p, pipeline);
        if (r <= 0)
        {
            pipeline_free(pipeline);
            return r;
        }
    }
    p->script->pipeline_count++;
    (void)at;
    return 1;
}

int shell89_priv_parse(const char *source, struct shell89_priv_script *script,
                       struct shell89_priv_diag *diag)
{
    struct shell89_priv_parser p;
    size_t len;
    int r;

    shell89_priv_script_init(script);
    shell89_priv_diag_clear(diag);
    if (source == NULL)
    {
        shell89_priv_diag_set(diag, SHELL89_ERROR_SYNTAX, 2, 0,
                              "source is null");
        return 0;
    }
    len = strlen(source);
    if (len > (size_t)SHELL89_MAX_SOURCE)
    {
        shell89_priv_diag_set(diag, SHELL89_ERROR_LIMIT, 2, 0,
                              "source exceeds the configured limit");
        return 0;
    }
    p.src = (const unsigned char *)source;
    p.len = len;
    p.pos = 0;
    p.script = script;
    p.diag = diag;
    skip_script_space(&p);
    while (p.pos < p.len)
    {
        if (p.src[p.pos] == '|')
        {
            fail(&p, p.pos, "pipeline cannot start with '|'");
            shell89_priv_script_free(script);
            return 0;
        }
        r = parse_pipeline(&p);
        if (r <= 0)
        {
            shell89_priv_script_free(script);
            return r;
        }
        skip_hspace(&p);
        if (p.pos < p.len && !is_separator_at(&p, p.pos))
        {
            fail(&p, p.pos, "unexpected token after pipeline");
            shell89_priv_script_free(script);
            return 0;
        }
        skip_script_space(&p);
    }
    return 1;
}
