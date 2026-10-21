# shell89 example scripts

Twenty runnable scripts that use only the commands registered by
`build/shell89`: `text echo`, `text upper`, `text lower`, `text count`,
`text grep`, `text head`, `text skip`, `files read`, and `files write`.

Run the whole suite from the repository root:

```sh
just example-scripts
```

The runner executes each script in an isolated directory, copies `fixtures/`
into it, and compares stdout, stderr, exit status, and every created file
against `expected/`.

## Scripts

| Script | Command chain | Demonstrates |
| --- | --- | --- |
| `01-hello.shell` | `text echo` | Smallest script. |
| `02-uppercase.shell` | `text echo \| text upper` | Two-stage pipeline. |
| `03-lowercase.shell` | `text echo \| text lower` | ASCII lowercasing. |
| `04-count-bytes.shell` | `text echo \| text count` | Byte counting. |
| `05-count-lines-json-escapes.shell` | `text echo \| text count --lines` | JSON escapes and line counting. |
| `06-read-file.shell` | `files read` | Reading a fixture. |
| `07-read-uppercase.shell` | `files read \| text upper` | File-to-stream transform. |
| `08-ascii-only-lower.shell` | `files read \| text lower` | `É` is not folded; only ASCII `A`-`Z` changes. |
| `09-binary-count.shell` | `files read \| text count` | NUL and high bytes are streamed unchanged. |
| `10-file-lines.shell` | `files read \| text count --lines` | A final unterminated line counts. |
| `11-grep-log.shell` | `files read \| text grep` | Literal line filtering. |
| `12-grep-ignore-case.shell` | `files read \| text grep --ignore-case` | ASCII case-insensitive matching. |
| `13-grep-no-match.shell` | `files read \| text grep` | No match exits 1 without diagnostics. |
| `14-head.shell` | `files read \| text head` | First N lines, exact newline state. |
| `15-skip.shell` | `files read \| text skip` | Remaining lines, including an unterminated final line. |
| `16-filter-window.shell` | `files read \| text grep \| text skip \| text head` | Four-stage pipeline. |
| `17-copy-then-verify.shell` | `files read \| files write; files read \| text upper` | Sequence, file write, then read-back. |
| `18-normalize-copy.shell` | `files read \| text lower \| files write` | Transform and persist. |
| `19-append-join.shell` | `files read \| files write; files read \| files write --append` | Append across a sequence. |
| `20-sequence-continues.shell` | `files read; text echo` | A failing pipeline does not stop the next one. |

## Fixtures

- `hello.txt` - one terminated line
- `multiline.txt` - four lines, last unterminated
- `mixed-case.txt` - mixed ASCII case
- `accent.txt` - UTF-8 `CAFÉ`
- `binary.bin` - `a`, NUL, `b`, `0xff`
- `log.txt` - mixed `INFO`, `ERROR`, `WARN`, and lowercase `error` lines
- `a.txt`, `b.txt` - append inputs

## Semantics worth noting

- `text grep` is a literal substring search, not a regular expression.
- `--ignore-case` and `text lower`/`text upper` fold ASCII only.
- `text head` drains its input after the requested lines, so an upstream
  `files read` does not see a broken pipe.
- Line-oriented commands accept lines up to 64 KiB; longer lines fail with a
  diagnostic.
- `text grep` exits 0 if anything matched and 1 if nothing matched. An empty
  pattern matches every line.
- Sequences use `;` or newline. Validation still covers the whole script before
  execution; script 20 fails only at runtime, after validation succeeded.
