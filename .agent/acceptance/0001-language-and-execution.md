# Acceptance criteria

## Must accept

- Empty scripts and trailing separators.
- `group operation`, named string options, and flags.
- JSON escapes and valid UTF-8 including supplementary code points.
- `|` pipelines, `;` and newline sequences, CRLF.
- Operator characters inside quoted values.
- All native/process pipeline combinations.
- Borrowed caller descriptors, `-1` input/output sentinels.
- Empty environment when `envp == NULL`.

## Must reject

- Unknown/uppercase/incomplete names and bare positionals.
- Unquoted values, single-quoted values, `$HOME`, `&&`, redirects, globbing.
- Duplicate options, unknown options, missing required strings, values on flags.
- NUL, malformed UTF-8, lone surrogates, bad JSON escapes.
- Dangling/leading pipes and tokens after a command.
- Limits on source, names, values, options, stages, and pipelines.
- Registrations after the registry freezes; duplicate registrations; malformed
  schemas.

## Must guarantee

- Invalid scripts execute nothing, including after a valid prefix.
- Native callbacks run in-process and their state changes remain visible.
- Pipeline stages run concurrently with bounded pipe buffering.
- Pipeline status is the rightmost nonzero stage status.
- Sequences execute unconditionally in order.
- Child termination by signal is reported as `128 + signal` capped at 255.
- Missing executable is shell-generated 127; permission/exec failure is 126.
- Caller descriptors remain open.
- Internal descriptors close on all paths.
- No orphan children or joined-thread leaks after cooperative failures.
- SIGPIPE cannot terminate the host from a callback.
- Process-global signals, environment, and working directory are unchanged.
