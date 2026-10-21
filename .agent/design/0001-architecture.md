# Design — architecture

Modules:

- `shell89.c` — lifecycle, registry, owned strings, orchestration, results.
- `shell89_parse.c` — hand-written lexer, grammar, JSON/UTF-8 literals, AST.
- `shell89_validate.c` — whole-script registry/schema validation and process
  argument construction.
- `shell89_stream.c` — callback read/write primitives and captured POSIX errors.
- `shell89_posix.c` — descriptor management, pipes, `posix_spawn`, pthread
  workers, `waitpid`.

Sibling dependencies:

- `libunicode89` validates raw UTF-8, encodes scalars, checks surrogates, and
  decodes surrogate pairs.
- `libstring89` builds decoded literals and owns string bytes. A custom
  allocator integrates it with shell89 fault injection.

Key decisions:

- Parse and validate the full script before any effects.
- Preallocate the result before execution.
- Build process argv during validation so execution needs no allocation.
- Native workers receive duplicated endpoint descriptors and close them before
  returning. The parent closes its copies after thread creation.
- Process stages spawn before worker threads.
- Worker threads block SIGPIPE only in their own signal masks.
- A stage status outside 0..255 from a callback becomes 125 with an invalid
  termination kind.
- Unknown command uses status 127; option/schema problems use 2.

Ownership:

- Registry deep-copies all names, schemas, paths, arguments, and environment.
- Function-context pointers are borrowed.
- Result objects own pipeline/stage arrays and diagnostics.
- Caller descriptors are only duplicated, never closed.
