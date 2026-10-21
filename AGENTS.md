# libshell89

A strict C89 embeddable capability shell. Native callbacks and registered
processes share one command model, binary streams, pipelines, sequences, and
exit statuses. The public essentials header is `include/shell89.h`; private
cross-translation-unit declarations live in `src/shell89_priv.h`.

Use sibling `libunicode89` for UTF-8 validation, scalar encoding, and surrogate
pair decoding. Use sibling `libstring89` for decoded literals and owned
strings. The parser itself is hand-written in `src/shell89_parse.c`; do not
replace it with a generic parser library.

Execution uses POSIX pipes, `posix_spawn()`, and pthread worker threads. Do not
add shell expansion, variables, control flow, implicit `PATH` lookup, or
process-global signal/environment changes.

Rules:

- Four spaces, Allman braces, strict C89 with `_POSIX_C_SOURCE=200809L`.
- Changes follow scenario -> acceptance -> failing test -> implementation ->
  verification.
- Use `just` for build and test actions.
- Keep `main` green; do not commit or push without explicit permission.
- Keep `.agent/{concept,stories,design,testing,acceptance}` current; the project
  must build and run without `.agent`.
