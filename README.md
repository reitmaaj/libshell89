# libshell89 — embeddable capability shell

`libshell89` is a small command shell embedded in a C host. Commands are
explicitly registered as native C callbacks or as standalone executables. The
shell provides pipelines, unconditional sequences, byte streams, and exit
statuses. It has no variables, expansions, redirection, globbing, conditionals,
loops, background jobs, subshells, aliases, or implicit `PATH` lookup.

```text
files read --path="README.md" |
    text grep --pattern="TODO" |
    text count --lines

git status --short; git diff --cached
```

Implementation language is strict C89. Execution uses POSIX pipes,
`posix_spawn()`, and pthread worker threads. Unicode literals are validated and
decoded through sibling `libunicode89`; owned strings use sibling `libstring89`.
The public header has no dependency on either library.

## Build

```sh
just build          # library archive
just test           # smoke, unit, integration, fault, property, stress
just check          # build, lint, tests, examples, header probes
just release        # check + matrix + ASan/UBSan + TSan + Valgrind + fuzz + ILP32
just build32        # best-effort ILP32 build (skips without multilib)
just test32         # ILP32 smoke and unit tests when available
```

Dependencies are built with `just deps`:

- `../libstring89`
- `../libunicode89`

Link in this order:

```text
libshell89.a libstring89.a libunicode89.a -pthread
```

Public includes need `libshell89/include`; implementation builds also need the
sibling include directories.

## Embedding

```c
#include <shell89.h>

static int hello(void *ctx, const shell89_args *args, shell89_io *io)
{
    (void)ctx;
    (void)args;
    return shell89_write_stdout(io, "hello\n", 6) != 0;
}

int main(void)
{
    shell89 *sh;
    shell89_result *result;
    struct shell89_run_io io;

    sh = shell89_create();
    shell89_register_function(sh, "demo", "hello", NULL, hello, NULL);
    io.stdin_fd = -1;
    io.stdout_fd = 1;
    io.stderr_fd = 2;

    if (shell89_run(sh, "demo hello", &io, &result) != SHELL89_OK)
        return 1;
    if (shell89_status(result) != 0)
        return 1;
    shell89_result_destroy(result);
    shell89_destroy(sh);
    return 0;
}
```

Native callbacks execute on worker threads in the embedding process, so host
state changes remain visible. Callbacks must synchronize their own shared
state. Process commands receive only explicitly registered options and a
configured environment.

See `examples/embed.c` and `examples/cli.c`.

## Language

A command is `group operation` followed by named options:

```text
--key="JSON string"     --flag
```

Names match `[a-z][a-z0-9_-]*`. Option values use JSON string syntax and are
decoded to UTF-8. `\u0000`, malformed UTF-8, and lone surrogates are rejected.
Pipelines use `|`; sequences use `;` or newline. Empty scripts and trailing
separators are valid. Every command and option is validated before any stage
executes.

## Status model

- `0` success
- `1` general callback failure
- `2` syntax or argument error
- `125` shell launch or infrastructure failure
- `126` registered executable cannot be invoked
- `127` unknown command or executable not found
- `128 + signal` child terminated by signal, capped at 255

Pipeline status is the rightmost nonzero stage status. Script status is the last
pipeline's status. A completed run returns `SHELL89_OK` from `shell89_run()`
even when the shell status is nonzero.

Additional result accessors expose parse-error offsets, stage kinds,
termination kinds, and native signal numbers.

## Limits

| Limit | Value |
| --- | --- |
| source bytes | 1 MiB |
| name bytes | 64 |
| decoded value bytes | 64 KiB |
| options per command | 64 |
| stages per pipeline | 16 |
| pipelines per script | 64 |

## Specification and tests

- Normative specification: `spec/shell89-spec.md`
- Project design/testing notes: `.agent/`
- Unit tests: parser, registry, streams
- Integration tests: native/process equivalence, all mixed pipeline
  combinations, sequences, embedding isolation
- Fault injection: allocation, descriptor, spawn, thread, wait, read, write
- Property tests: parse/format round-trip, byte identity, preflight atomicity
- Stress: 64 MiB streams, 16-stage pipelines, 64-pipeline scripts, 10,000 runs,
  16 concurrent shells
