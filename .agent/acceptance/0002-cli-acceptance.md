# 0002 - CLI acceptance

Run `just cli` in `libshell89`. Acceptance holds when the CLI suite passes and
every item below is exhibited or rejected.

## Must exhibit

- A43. `shell89 -c SCRIPT`, `shell89 FILE`, `shell89 -`, and `shell89` with a
  script on stdin all execute the script and return the shell status; EOF is a
  valid empty script.
- A44. `text echo`, `text upper`, `text count`, `files read`, and `files write`
  are registered; no external process or implicit `PATH` lookup is available.
- A45. `text echo --text="abc" | text upper` writes `ABC\n` and exits 0.
- A46. `text count` reports bytes by default and lines with `--lines`, including
  a final unterminated line and empty input.
- A47. `files write` followed by `files read` round-trips arbitrary bytes,
  including NUL and high bytes; `--append` preserves earlier bytes.
- A48. `shell89 -h` and `shell89 --help` print usage to stdout and exit 0.
- A49. Command data is written to stdout only; diagnostics are written to
  stderr only.

## Must reject (unacceptable behavior)

- B17. An unknown flag, missing `-c` argument, or extra argument exits 2 with
  usage on stderr and executes nothing.
- B18. A script larger than 1 MiB or containing a NUL byte exits 2 with a
  diagnostic and executes nothing.
- B19. A script containing a valid prefix followed by an unknown command or
  malformed option produces no output before the diagnostic.
- B20. An unknown command exits 127; a missing script path or a missing or
  unreadable `files` path exits 1 with a diagnostic.
- B21. Unknown or missing command options are rejected by the shell with
  status 2 before any stage runs.
- B22. `build/libshell89.a` exports no `main` or `shell89_cli_*` symbol; the
  tool is not part of the library archive.
