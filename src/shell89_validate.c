/* shell89_validate.c - whole-script registry, schema, and preflight checks. */

#include <string.h>

#include "shell89_priv.h"

static int schema_find(const struct shell89_command *cmd, const char *name)
{
    size_t i;

    for (i = 0; i < cmd->option_count; i++)
    {
        if (strcmp(cmd->options[i].name.data, name) == 0)
        {
            return (int)i;
        }
    }
    return -1;
}

static struct shell89_priv_option_use *
usage_find(struct shell89_priv_command *cmd, size_t schema_index)
{
    size_t i;

    for (i = 0; i < cmd->option_count; i++)
    {
        if (cmd->options[i].schema_index == schema_index)
        {
            return &cmd->options[i];
        }
    }
    return NULL;
}

static const char *base_name(const char *path)
{
    const char *slash;
    const char *p;

    slash = NULL;
    p = path;
    while (*p != '\0')
    {
        if (*p == '/')
        {
            slash = p;
        }
        p++;
    }
    if (slash == NULL)
    {
        return path;
    }
    return slash + 1;
}

static void free_partial_argv(char **argv, size_t from, size_t to)
{
    size_t i;

    if (argv == NULL)
    {
        return;
    }
    for (i = from; i < to; i++)
    {
        if (argv[i] != NULL)
        {
            shell89_priv_free(argv[i]);
        }
    }
}

static char *join_option(const struct shell89_priv_schema_option *schema,
                         const struct shell89_priv_option_use *use)
{
    size_t need;
    size_t i;
    char *s;

    need = 2 + schema->name.len + 1;
    if (use->has_value != 0)
    {
        need += 1 + use->value.len;
    }
    s = (char *)shell89_priv_malloc(need);
    if (s == NULL)
    {
        return NULL;
    }
    s[0] = '-';
    s[1] = '-';
    for (i = 0; i < schema->name.len; i++)
    {
        s[2 + i] = schema->name.data[i];
    }
    i = 2 + schema->name.len;
    if (use->has_value != 0)
    {
        size_t j;

        s[i] = '=';
        i++;
        for (j = 0; j < use->value.len; j++)
        {
            s[i + j] = use->value.data[j];
        }
        i += use->value.len;
    }
    s[i] = '\0';
    return s;
}

static int build_process_argv(struct shell89_priv_command *call,
                              const struct shell89_command *cmd)
{
    size_t count;
    size_t i;
    char **argv;
    size_t at;
    const char *name;

    count = 1 + cmd->fixed_argc;
    for (i = 0; i < cmd->option_count; i++)
    {
        if (usage_find(call, i) != NULL)
        {
            count++;
        }
    }
    argv = (char **)shell89_priv_malloc((count + 1) * sizeof *argv);
    if (argv == NULL)
    {
        return -1;
    }
    for (i = 0; i <= count; i++)
    {
        argv[i] = NULL;
    }
    name = base_name(cmd->path.data);
    argv[0] = (char *)shell89_priv_malloc(strlen(name) + 1);
    if (argv[0] == NULL)
    {
        shell89_priv_free(argv);
        return -1;
    }
    strcpy(argv[0], name);
    at = 1;
    for (i = 0; i < cmd->fixed_argc; i++)
    {
        argv[at] = cmd->fixed_argv[i];
        at++;
    }
    for (i = 0; i < cmd->option_count; i++)
    {
        struct shell89_priv_option_use *use;

        use = usage_find(call, i);
        if (use == NULL)
        {
            continue;
        }
        argv[at] = join_option(&cmd->options[i], use);
        if (argv[at] == NULL)
        {
            free_partial_argv(argv, 1 + cmd->fixed_argc, at);
            shell89_priv_free(argv[0]);
            shell89_priv_free(argv);
            return -1;
        }
        at++;
    }
    argv[at] = NULL;
    call->proc_argv = argv;
    call->proc_argv_count = count;
    call->proc_argv_owned_from = 1 + cmd->fixed_argc;
    return 0;
}

int shell89_priv_validate(shell89 *sh, struct shell89_priv_script *script,
                          struct shell89_priv_diag *diag)
{
    size_t p;
    size_t c;

    shell89_priv_diag_clear(diag);
    for (p = 0; p < script->pipeline_count; p++)
    {
        for (c = 0; c < script->pipelines[p].command_count; c++)
        {
            struct shell89_priv_command *call;
            const struct shell89_command *cmd;
            size_t i;

            call = &script->pipelines[p].commands[c];
            cmd =
                shell89_priv_lookup(sh, call->group.data, call->operation.data);
            if (cmd == NULL)
            {
                shell89_priv_diag_set(diag, SHELL89_ERROR_UNKNOWN_COMMAND, 127,
                                      call->offset, "unknown command");
                return 0;
            }
            call->reg = cmd;
            for (i = 0; i < call->option_count; i++)
            {
                int index;

                index = schema_find(cmd, call->options[i].name.data);
                if (index < 0)
                {
                    shell89_priv_diag_set(diag, SHELL89_ERROR_ARGUMENT, 2,
                                          call->options[i].offset,
                                          "unknown option");
                    return 0;
                }
                call->options[i].schema_index = (size_t)index;
                if (cmd->options[(size_t)index].type == SHELL89_FLAG)
                {
                    if (call->options[i].has_value != 0)
                    {
                        shell89_priv_diag_set(
                            diag, SHELL89_ERROR_ARGUMENT, 2,
                            call->options[i].offset,
                            "flag option does not take a value");
                        return 0;
                    }
                }
                else if (call->options[i].has_value == 0)
                {
                    shell89_priv_diag_set(diag, SHELL89_ERROR_ARGUMENT, 2,
                                          call->options[i].offset,
                                          "string option requires a value");
                    return 0;
                }
            }
            for (i = 0; i < cmd->option_count; i++)
            {
                if (cmd->options[i].required != 0 &&
                    usage_find(call, i) == NULL)
                {
                    shell89_priv_diag_set(diag, SHELL89_ERROR_ARGUMENT, 2,
                                          call->offset,
                                          "required option is missing");
                    return 0;
                }
            }
            if (cmd->kind == SHELL89_STAGE_PROCESS)
            {
                if (build_process_argv(call, cmd) != 0)
                {
                    return -1;
                }
            }
        }
    }
    return 1;
}
