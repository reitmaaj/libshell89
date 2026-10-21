# 0003 - line-oriented CLI commands

`tool/shell89_lines.c` adds four streaming commands to the demonstration CLI:

| command | behavior |
| --- | --- |
| `text lower` | ASCII `A`-`Z` to `a`-`z`, all other bytes unchanged |
| `text grep --pattern="S" [--ignore-case]` | literal line filter; 0 if any match, 1 if none |
| `text head --lines="N"` | write first N lines, then drain the rest |
| `text skip --lines="N"` | discard first N lines, then copy the rest |

## Line reader

A line reader has an 8 KiB input chunk and a growable line buffer capped at
64 KiB. `line_reader_next()` returns a complete line including its newline when
present, returns the final unterminated line at EOF, reports an oversized line,
or reports I/O failure.

After `head` has emitted N lines it drains remaining chunks without buffering
them. After `skip` has discarded N lines it copies remaining chunks directly.
This keeps both commands linear and avoids broken-pipe noise from the demo
`files read` command. `skip --lines="0"` is a raw stream copy.

`grep` must inspect every line, so it uses the bounded line buffer for the whole
stream. A line over 64 KiB is an error rather than silent truncation.

## Matching and counts

`--lines` is a required JSON string parsed as a non-negative decimal
`unsigned long`. Leading zeros are accepted; empty values, signs, non-digits,
and overflow return status 2 with a diagnostic.

`text grep` is literal, not regex. Matching is byte-oriented. `--ignore-case`
folds ASCII letters only. An empty pattern matches every line.

## Statuses

- `text grep`: 0 match, 1 no match or I/O error, 2 invalid arguments.
- `text head`, `text skip`: 0 success, 1 runtime/line-limit error, 2 invalid
  `--lines`.
- `text lower`: 0 success, 1 I/O error.

Diagnostics go to the shell's stderr stream and never to stdout.
