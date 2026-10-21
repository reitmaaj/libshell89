/* shell89.c - public lifecycle, registry, strings, results, and orchestration.
 */

#include <stdlib.h>
#include <string.h>

#include <string89.h>

#include "shell89_priv.h"

#include "shell89_test.h"

/* ---- allocation ------------------------------------------------------- */

void *shell89_priv_malloc(size_t size)
{
#ifdef SHELL89_TESTING
    if (shell89_test_fault_should_fail(SHELL89_TEST_MALLOC))
    {
        return NULL;
    }
#endif
    return malloc(size);
}

void *shell89_priv_realloc(void *ptr, size_t size)
{
#ifdef SHELL89_TESTING
    if (shell89_test_fault_should_fail(SHELL89_TEST_REALLOC))
    {
        return NULL;
    }
#endif
    return realloc(ptr, size);
}

void shell89_priv_free(void *ptr)
{
    free(ptr);
}

/* ---- owned strings ---------------------------------------------------- */

void shell89_priv_str_init(struct shell89_priv_str *s)
{
    s->data = NULL;
    s->len = 0;
}

void shell89_priv_str_free(struct shell89_priv_str *s)
{
    if (s->data != NULL)
    {
        shell89_priv_free(s->data);
    }
    s->data = NULL;
    s->len = 0;
}

int shell89_priv_str_set(struct shell89_priv_str *s, const unsigned char *data,
                         size_t len)
{
    string89_view view;
    char *copy;

    if (string89_view_init(&view, data, len) != STRING89_OK)
    {
        return SHELL89_EINVAL;
    }
    copy = (char *)shell89_priv_malloc(len + 1);
    if (copy == NULL)
    {
        return SHELL89_ENOMEM;
    }
    if (len != 0)
    {
        memcpy(copy, data, len);
    }
    copy[len] = '\0';
    shell89_priv_str_free(s);
    s->data = copy;
    s->len = len;
    return 0;
}

int shell89_priv_str_set_c(struct shell89_priv_str *s, const char *data)
{
    if (data == NULL)
    {
        return SHELL89_EINVAL;
    }
    return shell89_priv_str_set(s, (const unsigned char *)data, strlen(data));
}

int shell89_priv_str_equal(const struct shell89_priv_str *a,
                           const struct shell89_priv_str *b)
{
    if (a->len != b->len)
    {
        return 0;
    }
    if (a->len == 0)
    {
        return 1;
    }
    if (memcmp(a->data, b->data, a->len) == 0)
    {
        return 1;
    }
    return 0;
}

int shell89_priv_name_valid(const unsigned char *data, size_t len)
{
    size_t i;

    if (len == 0 || len > (size_t)SHELL89_MAX_NAME)
    {
        return 0;
    }
    if (data[0] < 'a' || data[0] > 'z')
    {
        return 0;
    }
    for (i = 1; i < len; i++)
    {
        unsigned char c;

        c = data[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
            c == '-')
        {
            continue;
        }
        return 0;
    }
    return 1;
}

/* ---- registry --------------------------------------------------------- */

static void schema_free(struct shell89_command *cmd)
{
    size_t i;

    for (i = 0; i < cmd->option_count; i++)
    {
        shell89_priv_str_free(&cmd->options[i].name);
    }
    if (cmd->options != NULL)
    {
        shell89_priv_free(cmd->options);
    }
    cmd->options = NULL;
    cmd->option_count = 0;
}

static void command_registry_free(struct shell89_command *cmd)
{
    size_t i;

    shell89_priv_str_free(&cmd->group);
    shell89_priv_str_free(&cmd->operation);
    shell89_priv_str_free(&cmd->path);
    schema_free(cmd);
    if (cmd->fixed_argv != NULL)
    {
        for (i = 0; i < cmd->fixed_argc; i++)
        {
            shell89_priv_free(cmd->fixed_argv[i]);
        }
        shell89_priv_free(cmd->fixed_argv);
    }
    if (cmd->envp != NULL)
    {
        for (i = 0; i < cmd->envc; i++)
        {
            shell89_priv_free(cmd->envp[i]);
        }
        shell89_priv_free(cmd->envp);
    }
    cmd->fixed_argv = NULL;
    cmd->fixed_argc = 0;
    cmd->envp = NULL;
    cmd->envc = 0;
}

shell89 *shell89_create(void)
{
    shell89 *sh;

    sh = (shell89 *)shell89_priv_malloc(sizeof *sh);
    if (sh == NULL)
    {
        return NULL;
    }
    sh->commands = NULL;
    sh->command_count = 0;
    sh->command_capacity = 0;
    sh->frozen = 0;
    return sh;
}

void shell89_destroy(shell89 *sh)
{
    size_t i;

    if (sh == NULL)
    {
        return;
    }
    for (i = 0; i < sh->command_count; i++)
    {
        command_registry_free(&sh->commands[i]);
    }
    if (sh->commands != NULL)
    {
        shell89_priv_free(sh->commands);
    }
    shell89_priv_free(sh);
}

static size_t count_public_options(const struct shell89_option *options)
{
    size_t n;

    if (options == NULL)
    {
        return 0;
    }
    n = 0;
    while (options[n].name != NULL)
    {
        n++;
    }
    return n;
}

static int copy_schema(struct shell89_command *cmd,
                       const struct shell89_option *options)
{
    size_t n;
    size_t i;
    size_t j;
    struct shell89_priv_schema_option *copy;

    n = count_public_options(options);
    if (n > (size_t)SHELL89_MAX_OPTIONS)
    {
        return SHELL89_EINVAL;
    }
    if (n == 0)
    {
        cmd->options = NULL;
        cmd->option_count = 0;
        return 0;
    }
    copy = (struct shell89_priv_schema_option *)shell89_priv_malloc(
        n * sizeof *copy);
    if (copy == NULL)
    {
        return SHELL89_ENOMEM;
    }
    for (i = 0; i < n; i++)
    {
        shell89_priv_str_init(&copy[i].name);
        copy[i].type = 0;
        copy[i].required = 0;
    }
    for (i = 0; i < n; i++)
    {
        if (options[i].name == NULL)
        {
            break;
        }
        if (!shell89_priv_name_valid((const unsigned char *)options[i].name,
                                     strlen(options[i].name)))
        {
            for (j = 0; j < i; j++)
            {
                shell89_priv_str_free(&copy[j].name);
            }
            shell89_priv_free(copy);
            return SHELL89_EINVAL;
        }
        if (options[i].type != SHELL89_STRING &&
            options[i].type != SHELL89_FLAG)
        {
            for (j = 0; j < i; j++)
            {
                shell89_priv_str_free(&copy[j].name);
            }
            shell89_priv_free(copy);
            return SHELL89_EINVAL;
        }
        if (options[i].required != 0 && options[i].required != 1)
        {
            for (j = 0; j < i; j++)
            {
                shell89_priv_str_free(&copy[j].name);
            }
            shell89_priv_free(copy);
            return SHELL89_EINVAL;
        }
        if (options[i].type == SHELL89_FLAG && options[i].required != 0)
        {
            for (j = 0; j < i; j++)
            {
                shell89_priv_str_free(&copy[j].name);
            }
            shell89_priv_free(copy);
            return SHELL89_EINVAL;
        }
        for (j = 0; j < i; j++)
        {
            if (strcmp(copy[j].name.data, options[i].name) == 0)
            {
                for (j = 0; j < i; j++)
                {
                    shell89_priv_str_free(&copy[j].name);
                }
                shell89_priv_free(copy);
                return SHELL89_EINVAL;
            }
        }
        if (shell89_priv_str_set_c(&copy[i].name, options[i].name) != 0)
        {
            for (j = 0; j <= i; j++)
            {
                shell89_priv_str_free(&copy[j].name);
            }
            shell89_priv_free(copy);
            return SHELL89_ENOMEM;
        }
        copy[i].type = options[i].type;
        copy[i].required = options[i].required;
    }
    cmd->options = copy;
    cmd->option_count = n;
    return 0;
}

static int grow_registry(shell89 *sh)
{
    size_t cap;
    struct shell89_command *next;
    size_t i;

    if (sh->command_count < sh->command_capacity)
    {
        return 0;
    }
    cap = sh->command_capacity == 0 ? 8 : sh->command_capacity * 2;
    next = (struct shell89_command *)shell89_priv_realloc(sh->commands,
                                                          cap * sizeof *next);
    if (next == NULL)
    {
        return SHELL89_ENOMEM;
    }
    sh->commands = next;
    for (i = sh->command_capacity; i < cap; i++)
    {
        shell89_priv_str_init(&sh->commands[i].group);
        shell89_priv_str_init(&sh->commands[i].operation);
        shell89_priv_str_init(&sh->commands[i].path);
        sh->commands[i].options = NULL;
        sh->commands[i].option_count = 0;
        sh->commands[i].kind = 0;
        sh->commands[i].function = NULL;
        sh->commands[i].context = NULL;
        sh->commands[i].fixed_argv = NULL;
        sh->commands[i].fixed_argc = 0;
        sh->commands[i].envp = NULL;
        sh->commands[i].envc = 0;
    }
    sh->command_capacity = cap;
    return 0;
}

static int find_registration(const shell89 *sh, const char *group,
                             const char *operation)
{
    size_t i;

    for (i = 0; i < sh->command_count; i++)
    {
        if (strcmp(sh->commands[i].group.data, group) == 0 &&
            strcmp(sh->commands[i].operation.data, operation) == 0)
        {
            return (int)i;
        }
    }
    return -1;
}

const struct shell89_command *
shell89_priv_lookup(const shell89 *sh, const char *group, const char *operation)
{
    int i;

    i = find_registration(sh, group, operation);
    if (i < 0)
    {
        return NULL;
    }
    return &sh->commands[(size_t)i];
}

static int copy_string_array(char ***out, size_t *count,
                             const char *const *items)
{
    size_t n;
    size_t i;
    char **copy;

    *out = NULL;
    *count = 0;
    if (items == NULL)
    {
        return 0;
    }
    n = 0;
    while (items[n] != NULL)
    {
        n++;
    }
    if (n == 0)
    {
        return 0;
    }
    copy = (char **)shell89_priv_malloc((n + 1) * sizeof *copy);
    if (copy == NULL)
    {
        return SHELL89_ENOMEM;
    }
    for (i = 0; i <= n; i++)
    {
        copy[i] = NULL;
    }
    for (i = 0; i < n; i++)
    {
        size_t len;

        len = strlen(items[i]);
        copy[i] = (char *)shell89_priv_malloc(len + 1);
        if (copy[i] == NULL)
        {
            size_t j;

            for (j = 0; j < i; j++)
            {
                shell89_priv_free(copy[j]);
            }
            shell89_priv_free(copy);
            return SHELL89_ENOMEM;
        }
        memcpy(copy[i], items[i], len + 1);
    }
    *out = copy;
    *count = n;
    return 0;
}

static int prepare_common(shell89 *sh, const char *group, const char *operation,
                          const struct shell89_option *options,
                          struct shell89_command **out)
{
    struct shell89_command *cmd;
    int r;

    if (sh == NULL || group == NULL || operation == NULL)
    {
        return SHELL89_EINVAL;
    }
    if (sh->frozen != 0)
    {
        return SHELL89_EINVAL;
    }
    if (!shell89_priv_name_valid((const unsigned char *)group, strlen(group)))
    {
        return SHELL89_EINVAL;
    }
    if (!shell89_priv_name_valid((const unsigned char *)operation,
                                 strlen(operation)))
    {
        return SHELL89_EINVAL;
    }
    if (find_registration(sh, group, operation) >= 0)
    {
        return SHELL89_EINVAL;
    }
    r = grow_registry(sh);
    if (r != 0)
    {
        return r;
    }
    cmd = &sh->commands[sh->command_count];
    r = shell89_priv_str_set_c(&cmd->group, group);
    if (r != 0)
    {
        return r;
    }
    r = shell89_priv_str_set_c(&cmd->operation, operation);
    if (r != 0)
    {
        command_registry_free(cmd);
        return r;
    }
    r = copy_schema(cmd, options);
    if (r != 0)
    {
        command_registry_free(cmd);
        return r;
    }
    *out = cmd;
    return 0;
}

int shell89_register_function(shell89 *sh, const char *group,
                              const char *operation,
                              const struct shell89_option *options,
                              shell89_function function, void *context)
{
    struct shell89_command *cmd;
    int r;

    if (function == NULL)
    {
        return SHELL89_EINVAL;
    }
    r = prepare_common(sh, group, operation, options, &cmd);
    if (r != 0)
    {
        return r;
    }
    cmd->kind = SHELL89_STAGE_FUNCTION;
    cmd->function = function;
    cmd->context = context;
    sh->command_count++;
    return 0;
}

int shell89_register_process(shell89 *sh, const char *group,
                             const char *operation,
                             const struct shell89_option *options,
                             const struct shell89_process *process)
{
    struct shell89_command *cmd;
    size_t len;
    int r;

    if (process == NULL || process->path == NULL || process->path[0] == '\0')
    {
        return SHELL89_EINVAL;
    }
    len = strlen(process->path);
    if (process->path[len - 1] == '/')
    {
        return SHELL89_EINVAL;
    }
    r = prepare_common(sh, group, operation, options, &cmd);
    if (r != 0)
    {
        return r;
    }
    cmd->kind = SHELL89_STAGE_PROCESS;
    r = shell89_priv_str_set_c(&cmd->path, process->path);
    if (r == 0)
    {
        r = copy_string_array(&cmd->fixed_argv, &cmd->fixed_argc,
                              process->fixed_argv);
    }
    if (r == 0)
    {
        r = copy_string_array(&cmd->envp, &cmd->envc, process->envp);
    }
    if (r != 0)
    {
        command_registry_free(cmd);
        return r;
    }
    sh->command_count++;
    return 0;
}

/* ---- arguments -------------------------------------------------------- */

const char *shell89_get_string(const shell89_args *args, const char *name)
{
    size_t i;

    if (args == NULL || args->call == NULL || name == NULL)
    {
        return NULL;
    }
    for (i = 0; i < args->call->option_count; i++)
    {
        if (args->call->options[i].has_value != 0 &&
            strcmp(args->call->options[i].name.data, name) == 0)
        {
            return args->call->options[i].value.data;
        }
    }
    return NULL;
}

int shell89_get_flag(const shell89_args *args, const char *name)
{
    size_t i;

    if (args == NULL || args->call == NULL || name == NULL)
    {
        return 0;
    }
    for (i = 0; i < args->call->option_count; i++)
    {
        if (args->call->options[i].has_value == 0 &&
            strcmp(args->call->options[i].name.data, name) == 0)
        {
            return 1;
        }
    }
    return 0;
}

/* ---- result ----------------------------------------------------------- */

static shell89_result *result_create(size_t pipelines, size_t stages)
{
    shell89_result *r;

    if (pipelines > ((size_t)-1) / sizeof(struct shell89_priv_pipeline_result))
    {
        return NULL;
    }
    if (stages > ((size_t)-1) / sizeof(struct shell89_priv_stage_result))
    {
        return NULL;
    }
    r = (shell89_result *)shell89_priv_malloc(sizeof *r);
    if (r == NULL)
    {
        return NULL;
    }
    memset(r, 0, sizeof *r);
    r->status = 0;
    r->error_kind = SHELL89_ERROR_NONE;
    r->error_offset = 0;
    r->error_message[0] = '\0';
    if (pipelines != 0)
    {
        r->pipelines =
            (struct shell89_priv_pipeline_result *)shell89_priv_malloc(
                pipelines * sizeof *r->pipelines);
        if (r->pipelines == NULL)
        {
            shell89_priv_free(r);
            return NULL;
        }
        memset(r->pipelines, 0, pipelines * sizeof *r->pipelines);
        r->pipeline_count = pipelines;
    }
    if (stages != 0)
    {
        r->stages = (struct shell89_priv_stage_result *)shell89_priv_malloc(
            stages * sizeof *r->stages);
        if (r->stages == NULL)
        {
            if (r->pipelines != NULL)
            {
                shell89_priv_free(r->pipelines);
            }
            shell89_priv_free(r);
            return NULL;
        }
        memset(r->stages, 0, stages * sizeof *r->stages);
        r->stage_count = stages;
    }
    return r;
}

static void result_set_diag(shell89_result *r,
                            const struct shell89_priv_diag *diag)
{
    size_t i;

    r->status = diag->shell_status;
    r->error_kind = diag->kind;
    r->error_offset = diag->offset;
    for (i = 0; i + 1 < sizeof r->error_message && diag->message[i] != '\0';
         i++)
    {
        r->error_message[i] = diag->message[i];
    }
    r->error_message[i] = '\0';
}

static size_t count_script_stages(const struct shell89_priv_script *s)
{
    size_t n;
    size_t i;

    n = 0;
    for (i = 0; i < s->pipeline_count; i++)
    {
        n += s->pipelines[i].command_count;
    }
    return n;
}

int shell89_run(shell89 *sh, const char *source,
                const struct shell89_run_io *io, shell89_result **result)
{
    struct shell89_priv_script script;
    struct shell89_priv_diag diag;
    shell89_result *r;
    size_t stages;
    int parsed;
    int valid;

    if (result == NULL)
    {
        return SHELL89_EINVAL;
    }
    *result = NULL;
    if (sh == NULL || source == NULL || io == NULL)
    {
        return SHELL89_EINVAL;
    }
    sh->frozen = 1;
    parsed = shell89_priv_parse(source, &script, &diag);
    if (parsed < 0)
    {
        return SHELL89_ENOMEM;
    }
    if (parsed == 0)
    {
        r = result_create(0, 0);
        if (r == NULL)
        {
            shell89_priv_script_free(&script);
            return SHELL89_ENOMEM;
        }
        result_set_diag(r, &diag);
        shell89_priv_script_free(&script);
        *result = r;
        return 0;
    }
    valid = shell89_priv_validate(sh, &script, &diag);
    if (valid < 0)
    {
        shell89_priv_script_free(&script);
        return SHELL89_ENOMEM;
    }
    if (valid == 0)
    {
        r = result_create(0, 0);
        if (r == NULL)
        {
            shell89_priv_script_free(&script);
            return SHELL89_ENOMEM;
        }
        result_set_diag(r, &diag);
        shell89_priv_script_free(&script);
        *result = r;
        return 0;
    }
    stages = count_script_stages(&script);
    r = result_create(script.pipeline_count, stages);
    if (r == NULL)
    {
        shell89_priv_script_free(&script);
        return SHELL89_ENOMEM;
    }
    if (shell89_priv_execute(sh, &script, io, r) != 0)
    {
        shell89_result_destroy(r);
        shell89_priv_script_free(&script);
        return SHELL89_EINTERNAL;
    }
    shell89_priv_script_free(&script);
    *result = r;
    return 0;
}

int shell89_status(const shell89_result *result)
{
    if (result == NULL)
    {
        return 125;
    }
    return result->status;
}

size_t shell89_pipeline_count(const shell89_result *result)
{
    if (result == NULL)
    {
        return 0;
    }
    return result->pipeline_count;
}

int shell89_pipeline_status(const shell89_result *result, size_t index)
{
    if (result == NULL || index >= result->pipeline_count)
    {
        return 125;
    }
    return result->pipelines[index].status;
}

size_t shell89_stage_count(const shell89_result *result, size_t pipeline)
{
    if (result == NULL || pipeline >= result->pipeline_count)
    {
        return 0;
    }
    return result->pipelines[pipeline].stage_count;
}

static const struct shell89_priv_stage_result *
stage_at(const shell89_result *result, size_t pipeline, size_t stage)
{
    const struct shell89_priv_pipeline_result *pr;
    size_t index;

    if (result == NULL || pipeline >= result->pipeline_count)
    {
        return NULL;
    }
    pr = &result->pipelines[pipeline];
    if (stage >= pr->stage_count)
    {
        return NULL;
    }
    index = pr->first_stage + stage;
    if (index >= result->stage_count)
    {
        return NULL;
    }
    return &result->stages[index];
}

int shell89_stage_status(const shell89_result *result, size_t pipeline,
                         size_t stage)
{
    const struct shell89_priv_stage_result *s;

    s = stage_at(result, pipeline, stage);
    if (s == NULL)
    {
        return 125;
    }
    return s->status;
}

size_t shell89_error_offset(const shell89_result *result)
{
    if (result == NULL)
    {
        return (size_t)-1;
    }
    if (result->error_kind == SHELL89_ERROR_NONE)
    {
        return (size_t)-1;
    }
    return result->error_offset;
}

int shell89_error_kind(const shell89_result *result)
{
    if (result == NULL)
    {
        return SHELL89_ERROR_NONE;
    }
    return result->error_kind;
}

const char *shell89_error_message(const shell89_result *result)
{
    if (result == NULL)
    {
        return "";
    }
    return result->error_message;
}

int shell89_stage_kind(const shell89_result *result, size_t pipeline,
                       size_t stage)
{
    const struct shell89_priv_stage_result *s;

    s = stage_at(result, pipeline, stage);
    if (s == NULL)
    {
        return SHELL89_STAGE_NONE;
    }
    return s->kind;
}

int shell89_stage_termination(const shell89_result *result, size_t pipeline,
                              size_t stage)
{
    const struct shell89_priv_stage_result *s;

    s = stage_at(result, pipeline, stage);
    if (s == NULL)
    {
        return SHELL89_TERM_NONE;
    }
    return s->termination;
}

int shell89_stage_signal(const shell89_result *result, size_t pipeline,
                         size_t stage)
{
    const struct shell89_priv_stage_result *s;

    s = stage_at(result, pipeline, stage);
    if (s == NULL)
    {
        return 0;
    }
    return s->signal;
}

void shell89_result_destroy(shell89_result *result)
{
    if (result == NULL)
    {
        return;
    }
    if (result->pipelines != NULL)
    {
        shell89_priv_free(result->pipelines);
    }
    if (result->stages != NULL)
    {
        shell89_priv_free(result->stages);
    }
    shell89_priv_free(result);
}
