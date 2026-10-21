# libshell89 — Embeddable Capability Shell

Specification v1 · C89 source · POSIX execution backend

## 1. Scope

`libshell89` implements a small command shell embeddable in a C application.
Registered C functions and external executables are interchangeable commands.
The shell supplies pipelines, unconditional command sequences, byte streams,
and exit statuses. It knows nothing about application policy, agents, databases,
or LLMs.

Architectural decisions:

| Property | Decision |
| --- | --- |
| Language | strict ANSI C89 |
| Platform | POSIX.1-2008 execution; parser portable C89 |
| Command naming | `group operation` |
| Arguments | `--key="string"` and `--flag` |
| Composition | `|`, `;`, newline |
| Transport | binary byte streams |
| C commands | in-process callbacks on worker threads |
| Process commands | registered executables launched without a shell |
| Failure | integer exit statuses, rightmost-nonzero `pipefail` |
| State | no variables or persistent execution state |

## 2. Language

Valid examples:

```text
files read --path="data.txt"
files write --path="out.txt" --append
git status --short
git log --limit="10"
text grep --pattern="error" --ignore-case
files read --path="input" | text grep --pattern="abc"
git status; git diff
```

Grammar:

```text
script      = { separator }, [ pipeline,
              { separator, pipeline } ],
              { separator } ;
pipeline    = command, { ws, "|", ws, command } ;
command     = name, hspace, name,
              { hspace, option } ;
option      = "--", name, [ "=", json_string ] ;
name        = letter, { letter | digit | "_" | "-" } ;
separator   = ";" | newline ;
hspace      = " " | tab, { " " | tab } ;
ws          = { " " | tab | newline } ;
```

Names consist of ASCII lowercase letters, digits, `_`, and `-`, beginning with
a letter. Option values use JSON string syntax, including JSON escapes.
Decoding produces UTF-8 bytes. Reject NUL, malformed Unicode, lone surrogates,
and malformed UTF-8.

Empty scripts and trailing separators are valid. Dangling pipes, duplicate
option names, unsupported operators, and incomplete commands are errors.
CRLF is accepted as newline; a lone CR is rejected.

Excluded: positional arguments, variables, substitution, redirection,
globbing, conditionals, loops, background jobs, subshells, aliases, implicit
filesystem lookup.

## 3. Command model

Each `(group, operation)` pair maps to exactly one implementation:

- `SHELL89_FUNCTION`: native C callback in the host process.
- `SHELL89_PROCESS`: explicitly registered executable.

Both receive named options and stdin/stdout/stderr streams.

Native callbacks run in worker threads during pipeline execution. Side effects
remain visible in the embedding process. Callbacks must synchronize shared
mutable state.

Process commands have a registered path, fixed prefix arguments, and an
explicit environment. No `system()`, `popen()`, `/bin/sh -c`, or `PATH`
lookup is used. A string argument is passed as one `--name=value` argument.
`envp == NULL` means an empty environment.

## 4. Public C89 API

One public header, `shell89.h`, exposes the registry, callback ABI, I/O
operations, run configuration, and results.

Core types and constants:

```c
#define SHELL89_STRING 1
#define SHELL89_FLAG   2

struct shell89_option {
    const char *name;
    int type;
    int required;
};

struct shell89_process {
    const char *path;
    const char *const *fixed_argv;
    const char *const *envp;
};

typedef int (*shell89_function)(
    void *context,
    const shell89_args *args,
    shell89_io *io
);
```

Option arrays terminate with `{ NULL, 0, 0 }`. Strings remain strings at the
ABI boundary.

Registration:

```c
shell89 *shell89_create(void);
void shell89_destroy(shell89 *sh);

int shell89_register_function(...);
int shell89_register_process(...);
```

Registrations use unique `(group, operation)` pairs. The registry copies names,
schemas, executable paths, fixed arguments, and environment strings. It borrows
function-context pointers. Registry contents freeze on the first run. Duplicate
or malformed registrations fail.

Capabilities:

```c
const char *shell89_get_string(const shell89_args *args, const char *name);
int shell89_get_flag(const shell89_args *args, const char *name);

int shell89_read(shell89_io *io, void *buffer, size_t capacity,
                 size_t *received);
int shell89_write_stdout(shell89_io *io, const void *buffer, size_t length);
int shell89_write_stderr(shell89_io *io, const void *buffer, size_t length);
int shell89_stream_error(const shell89_io *io);
```

`read` sets `received` to zero on EOF or error. Writes complete fully, retrying
short writes and `EINTR`. The POSIX error code is available through
`shell89_stream_error`. I/O objects are valid only during the callback.

Execution and results:

```c
struct shell89_run_io {
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
};

int shell89_run(shell89 *sh, const char *source,
                const struct shell89_run_io *io, shell89_result **result);

int shell89_status(const shell89_result *result);
size_t shell89_pipeline_count(const shell89_result *result);
int shell89_pipeline_status(const shell89_result *result, size_t index);
size_t shell89_stage_count(const shell89_result *result, size_t pipeline);
int shell89_stage_status(const shell89_result *result, size_t pipeline,
                         size_t stage);

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
```

`stdin_fd = -1` is an empty input stream. `stdout_fd = -1` and
`stderr_fd = -1` discard output. Other values are borrowed caller descriptors;
the shell duplicates them and never closes the caller's descriptors.

`shell89_run` returns `SHELL89_OK` when it produced a result, even for a
nonzero shell status. Negative return values indicate that no result could be
constructed. Syntax and argument errors are ordinary shell outcomes (status 2)
reported in the result.

## 5. Execution

For a pipeline of N stages:

1. Parse and validate the entire script before executing anything.
2. Allocate pipes and stage records.
3. Spawn process stages with `posix_spawn()` before creating worker threads.
4. Start native stages with `pthread_create()`.
5. Close unnecessary parent descriptors.
6. Join threads and reap children.
7. Compute pipeline status.
8. Execute the next pipeline.

All native/process combinations are supported. Intermediate buffering is
bounded by OS pipe capacity.

Safety guarantees:

- Internal descriptors are close-on-exec.
- Child processes receive their configured descriptors; host descriptors
  without `FD_CLOEXEC` may be inherited by POSIX rules.
- Worker threads block `SIGPIPE`, so broken pipes return `EPIPE` rather than
  terminating the host.
- The shell never changes process-wide signal dispositions, environment, or
  working directory.
- Descriptors close on success and failure paths.
- Callback side effects occur in-process.
- Stderr from concurrent stages may interleave.

Statuses:

| Status | Meaning |
| --- | --- |
| 0 | success |
| 1 | general callback failure |
| 2 | syntax or argument error |
| 125 | shell launch/infrastructure failure |
| 126 | registered executable cannot be invoked |
| 127 | unknown command or executable not found |
| 128 + signal | child terminated by signal, capped at 255 |

Pipeline status is the rightmost nonzero stage status. Individual stage results
remain available. Sequences execute every pipeline unconditionally; the script
status equals the last pipeline's status. Termination-kind and signal fields
distinguish a real process exit from shell-generated failure.

## 6. Validation and limits

The default limits are:

| Limit | Value |
| --- | --- |
| source bytes | 1 MiB |
| name bytes | 64 |
| decoded value bytes | 64 KiB |
| options per command | 64 |
| stages per pipeline | 16 |
| pipelines per script | 64 |

A syntactically or semantically invalid script executes nothing. A pipeline
startup failure may have partial effects from already-started stages, but the
executor closes pipes, terminates started children where necessary, and collects
reachable results. The library cannot forcibly stop non-cooperative in-process
callbacks.

## 7. Intentional non-goals

No variables, exprs, conditionals, loops, redirection, globbing, aliases,
background jobs, implicit command lookup, persistent shell state, or
application policy. The embedding application supplies commands, permissions,
and higher-level decisions.
