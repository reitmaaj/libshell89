#ifndef SHELL89_H
#define SHELL89_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* libshell89 - a small embeddable capability shell for strict C89 hosts.
     *
     * A script is a sequence of pipelines.  Every command names one explicitly
     * registered (group, operation) pair backed either by a native C callback
     * or by a configured POSIX executable.  Pipelines run their stages
     * concurrently; sequences run unconditionally in order.
     *
     * The public header has no Unicode or string helper dependency.  Those are
     * implementation details.
     */

    typedef struct shell89 shell89;
    typedef struct shell89_args shell89_args;
    typedef struct shell89_io shell89_io;
    typedef struct shell89_result shell89_result;

#define SHELL89_STRING 1
#define SHELL89_FLAG 2

    struct shell89_option
    {
        const char *name;
        int type;
        int required;
    };

    /* NULL-terminated fixed argument and environment arrays. */
    struct shell89_process
    {
        const char *path;
        const char *const *fixed_argv;
        const char *const *envp;
    };

    typedef int (*shell89_function)(void *context, const shell89_args *args,
                                    shell89_io *io);

/* shell89_run() and registration return codes.  Shell-level outcomes are
 * reported through the result object, not through these codes. */
#define SHELL89_OK 0
#define SHELL89_EINVAL (-1)
#define SHELL89_ENOMEM (-2)
#define SHELL89_EINTERNAL (-3)

/* Diagnostic classes for a rejected script. */
#define SHELL89_ERROR_NONE 0
#define SHELL89_ERROR_SYNTAX 1
#define SHELL89_ERROR_ARGUMENT 2
#define SHELL89_ERROR_LIMIT 3
#define SHELL89_ERROR_UNKNOWN_COMMAND 4

/* Stage termination. */
#define SHELL89_TERM_NONE 0
#define SHELL89_TERM_EXIT 1
#define SHELL89_TERM_SIGNAL 2
#define SHELL89_TERM_CALLBACK 3
#define SHELL89_TERM_SHELL 4
#define SHELL89_TERM_INVALID 5

/* Stage implementation kind. */
#define SHELL89_STAGE_NONE 0
#define SHELL89_STAGE_FUNCTION 1
#define SHELL89_STAGE_PROCESS 2

/* Accepted script limits.  Exceeding any of them is a validation error. */
#define SHELL89_MAX_SOURCE (1024UL * 1024UL)
#define SHELL89_MAX_NAME 64UL
#define SHELL89_MAX_VALUE (64UL * 1024UL)
#define SHELL89_MAX_OPTIONS 64UL
#define SHELL89_MAX_STAGES 16UL
#define SHELL89_MAX_PIPELINES 64UL

    struct shell89_run_io
    {
        int stdin_fd;
        int stdout_fd;
        int stderr_fd;
    };

    shell89 *shell89_create(void);
    void shell89_destroy(shell89 *sh);

    int shell89_register_function(shell89 *sh, const char *group,
                                  const char *operation,
                                  const struct shell89_option *options,
                                  shell89_function function, void *context);

    int shell89_register_process(shell89 *sh, const char *group,
                                 const char *operation,
                                 const struct shell89_option *options,
                                 const struct shell89_process *process);

    const char *shell89_get_string(const shell89_args *args, const char *name);
    int shell89_get_flag(const shell89_args *args, const char *name);

    /* Return 0 for success, nonzero for I/O error.  On EOF or error *received
     * is set to zero.  The POSIX error code is available from
     * shell89_stream_error(). */
    int shell89_read(shell89_io *io, void *buffer, size_t capacity,
                     size_t *received);
    int shell89_write_stdout(shell89_io *io, const void *buffer, size_t length);
    int shell89_write_stderr(shell89_io *io, const void *buffer, size_t length);
    int shell89_stream_error(const shell89_io *io);

    /* Execute source.  On SHELL89_OK *result receives an object, even when the
     * script has a nonzero shell status.  On a negative return *result is NULL.
     */
    int shell89_run(shell89 *sh, const char *source,
                    const struct shell89_run_io *io, shell89_result **result);

    int shell89_status(const shell89_result *result);
    size_t shell89_pipeline_count(const shell89_result *result);
    int shell89_pipeline_status(const shell89_result *result, size_t index);
    size_t shell89_stage_count(const shell89_result *result, size_t pipeline);
    int shell89_stage_status(const shell89_result *result, size_t pipeline,
                             size_t stage);

    /* Supplementary structured diagnostics. */
    size_t shell89_error_offset(const shell89_result *result);
    int shell89_error_kind(const shell89_result *result);
    const char *shell89_error_message(const shell89_result *result);

    int shell89_stage_kind(const shell89_result *result, size_t pipeline,
                           size_t stage);
    int shell89_stage_termination(const shell89_result *result, size_t pipeline,
                                  size_t stage);
    int shell89_stage_signal(const shell89_result *result, size_t pipeline,
                             size_t stage);

    void shell89_result_destroy(shell89_result *result);

#ifdef __cplusplus
}
#endif

#endif
