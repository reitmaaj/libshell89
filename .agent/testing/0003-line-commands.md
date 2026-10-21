# 0003 - line-oriented CLI scenarios

## LC1 - lower

SCENARIO ASCII lowercasing
GIVEN `text lower` receives `Hello 123\n`
WHEN it runs
THEN stdout is `hello 123\n`.

SCENARIO non-ASCII bytes are unchanged
GIVEN `text lower` receives `CAF\xc3\x89\n`
WHEN it runs
THEN stdout is `caf\xc3\x89\n`.

## LC2 - literal grep

SCENARIO matching lines
GIVEN a stream with multiple lines and `text grep --pattern="ERROR"`
WHEN it runs
THEN every line containing the literal pattern is written byte-exactly.

SCENARIO no match
GIVEN a stream with no line containing the pattern
WHEN `text grep` runs
THEN stdout is empty, stderr is empty, and the command status is 1.

SCENARIO ignore case
GIVEN `text grep --pattern="error" --ignore-case`
WHEN lines contain `ERROR` and `error`
THEN both forms match.

SCENARIO empty pattern
GIVEN `text grep --pattern=""`
WHEN any line is read
THEN every line matches.

SCENARIO final unterminated line
GIVEN the matching last line has no trailing newline
WHEN `text grep` emits it
THEN no newline is added.

## LC3 - head and skip

SCENARIO head N lines
GIVEN `text head --lines="2"` receives four lines
WHEN it runs
THEN it writes the first two lines exactly and drains the remaining input.

SCENARIO head zero
GIVEN `text head --lines="0"`
WHEN it runs
THEN it writes nothing and drains all input.

SCENARIO skip N lines
GIVEN `text skip --lines="2"` receives four lines
WHEN it runs
THEN it discards the first two lines and writes the rest exactly.

SCENARIO skip beyond end
GIVEN `text skip --lines="10"` receives fewer than ten lines
WHEN it runs
THEN it writes nothing and exits 0.

SCENARIO skip zero
GIVEN `text skip --lines="0"`
WHEN it runs
THEN it copies all input.

## LC4 - counts and limits

SCENARIO decimal line counts
GIVEN `--lines` values `0`, `007`, or a maximum unsigned long
WHEN the command parses them
THEN they are accepted.
GIVEN an empty value, a sign, a non-digit, or an overflowing value
WHEN the command parses it
THEN it returns status 2 with a diagnostic and writes no data.

SCENARIO line length limit
GIVEN a line longer than 64 KiB
WHEN `text grep`, `text head`, or `text skip` processes it as a line
THEN the command returns status 1 with a diagnostic.

SCENARIO chunk boundaries
GIVEN a line or pattern that crosses the internal read-buffer boundary
WHEN a line command runs
THEN matching and output are byte-identical to the unbuffered result.
