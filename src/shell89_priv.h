#ifndef SHELL89_PRIV_H
#define SHELL89_PRIV_H

#include <stddef.h>
#include <sys/types.h>

#include <shell89.h>

/* ---- allocation ------------------------------------------------------- */

void *shell89_priv_malloc(size_t size);
void *shell89_priv_realloc(void *ptr, size_t size);
void shell89_priv_free(void *ptr);

/* ---- owned NUL-terminated strings ------------------------------------ */

struct shell89_priv_str
{
    char *data;
    size_t len;
};

void shell89_priv_str_init(struct shell89_priv_str *s);
void shell89_priv_str_free(struct shell89_priv_str *s);
int shell89_priv_str_set(struct shell89_priv_str *s, const unsigned char *data,
                         size_t len);
int shell89_priv_str_set_c(struct shell89_priv_str *s, const char *data);
int shell89_priv_str_equal(const struct shell89_priv_str *a,
                           const struct shell89_priv_str *b);
int shell89_priv_name_valid(const unsigned char *data, size_t len);

/* ---- AST -------------------------------------------------------------- */

struct shell89_priv_option_use
{
    struct shell89_priv_str name;
    int has_value;
    struct shell89_priv_str value;
    size_t offset;
    size_t schema_index;
};

struct shell89_priv_command
{
    struct shell89_priv_str group;
    struct shell89_priv_str operation;
    struct shell89_priv_option_use *options;
    size_t option_count;
    size_t option_capacity;
    size_t offset;
    const struct shell89_command *reg;
    char **proc_argv;
    size_t proc_argv_count;
    size_t proc_argv_owned_from;
};

struct shell89_priv_pipeline
{
    struct shell89_priv_command *commands;
    size_t command_count;
    size_t command_capacity;
};

struct shell89_priv_script
{
    struct shell89_priv_pipeline *pipelines;
    size_t pipeline_count;
    size_t pipeline_capacity;
};

struct shell89_priv_diag
{
    int kind;
    int shell_status;
    size_t offset;
    char message[200];
};

void shell89_priv_script_init(struct shell89_priv_script *script);
void shell89_priv_script_free(struct shell89_priv_script *script);
void shell89_priv_diag_clear(struct shell89_priv_diag *diag);
void shell89_priv_diag_set(struct shell89_priv_diag *diag, int kind,
                           int shell_status, size_t offset,
                           const char *message);

/* Returns 0 on success, SHELL89_ENOMEM on allocation failure.  A malformed
 * script fills diag and leaves the script empty. */
int shell89_priv_parse(const char *source, struct shell89_priv_script *script,
                       struct shell89_priv_diag *diag);

/* ---- registry --------------------------------------------------------- */

struct shell89_priv_schema_option
{
    struct shell89_priv_str name;
    int type;
    int required;
};

struct shell89_command
{
    struct shell89_priv_str group;
    struct shell89_priv_str operation;
    struct shell89_priv_schema_option *options;
    size_t option_count;
    int kind;
    shell89_function function;
    void *context;
    struct shell89_priv_str path;
    char **fixed_argv;
    size_t fixed_argc;
    char **envp;
    size_t envc;
};

struct shell89
{
    struct shell89_command *commands;
    size_t command_count;
    size_t command_capacity;
    int frozen;
};

const struct shell89_command *shell89_priv_lookup(const shell89 *sh,
                                                  const char *group,
                                                  const char *operation);
int shell89_priv_validate(shell89 *sh, struct shell89_priv_script *script,
                          struct shell89_priv_diag *diag);

/* ---- arguments and streams ------------------------------------------- */

struct shell89_args
{
    const struct shell89_command *command;
    const struct shell89_priv_command *call;
};

struct shell89_io
{
    int in_fd;
    int out_fd;
    int err_fd;
    int error;
};

ssize_t shell89_priv_read_fd(int fd, void *buffer, size_t length);
ssize_t shell89_priv_write_fd(int fd, const void *buffer, size_t length);

/* ---- results ---------------------------------------------------------- */

struct shell89_priv_stage_result
{
    int status;
    int kind;
    int termination;
    int signal;
};

struct shell89_priv_pipeline_result
{
    size_t first_stage;
    size_t stage_count;
    int status;
};

struct shell89_result
{
    int status;
    struct shell89_priv_pipeline_result *pipelines;
    size_t pipeline_count;
    struct shell89_priv_stage_result *stages;
    size_t stage_count;
    int error_kind;
    size_t error_offset;
    char error_message[200];
};

/* ---- execution -------------------------------------------------------- */

int shell89_priv_execute(shell89 *sh, const struct shell89_priv_script *script,
                         const struct shell89_run_io *io,
                         struct shell89_result *result);

#endif
