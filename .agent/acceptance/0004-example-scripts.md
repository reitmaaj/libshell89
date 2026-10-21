# 0004 - example script acceptance

## Must exhibit

- A59. All twenty scripts run successfully under `just example-scripts`.
- A60. Each script runs in an isolated directory containing a fresh copy of
  `fixtures/`.
- A61. Outputs, statuses, and created files match `expected/` byte-for-byte.
- A62. `13-grep-no-match.shell` exits 1 with no output or diagnostics.
- A63. `20-sequence-continues.shell` continues after the missing-file pipeline,
  exits 0, writes `continued\n`, and writes the read diagnostic to stderr.
- A64. Scripts 17-19 create exactly `copy.txt`, `normalized.txt`, and
  `joined.txt` with the expected bytes.

## Must reject (unacceptable behavior)

- B28. A script that creates an extra file outside `fixtures/` is reported as a
  failure.
- B29. A missing or non-executable CLI binary stops the suite with an error.
- B30. A wrong status, stdout, stderr, or created-file byte difference is
  reported with the script name.
- B31. Scripts must not depend on absolute repository paths or leave files in
  the repository root.
