# 0002 - CLI scenarios

## CL1 - script source

SCENARIO `-c` inline script
GIVEN `shell89 -c 'text echo --text="hello"'`
WHEN it runs
THEN stdout is `hello\n`, stderr is empty, and it exits 0.

SCENARIO script file
GIVEN a file containing a valid script
WHEN `shell89 FILE` runs
THEN the script executes and the process exits with the shell status.

SCENARIO script from stdin
GIVEN a valid script on stdin
WHEN `shell89` runs without arguments
THEN the script executes, the first stage receives empty stdin, and it exits
with the shell status.

SCENARIO explicit stdin and empty script
GIVEN `shell89 -` with a script on stdin, or `shell89` with EOF
WHEN it runs
THEN `-` executes the script like no arguments, and EOF is a valid empty
script.

SCENARIO help
GIVEN `shell89 -h` or `shell89 --help`
WHEN it runs
THEN usage is written to stdout and it exits 0.

## CL2 - pipelines and native commands

SCENARIO echo through upper
GIVEN `text echo --text="abc" | text upper`
WHEN it runs
THEN stdout is `ABC\n` and it exits 0.

SCENARIO byte and line counts
GIVEN inputs ``, `abc`, `a\n`, and `a\nb`
WHEN `text count` runs without `--lines`
THEN it prints the byte count.
WHEN it runs with `--lines`
THEN it prints the line count, counting a final unterminated line and not
counting an empty input.

## CL3 - files

SCENARIO write then read
GIVEN `files write --path="out"` receiving binary input including NUL and high
bytes
WHEN `files read --path="out"` runs
THEN stdout is byte-identical and both commands exit 0.

SCENARIO append
GIVEN an existing file and `files write --path="out" --append`
WHEN input is written
THEN the bytes are appended and earlier bytes are preserved.

## CL4 - diagnostics and statuses

SCENARIO validation is atomic
GIVEN a script whose valid prefix would write output followed by an unknown
command or malformed option
WHEN it runs
THEN no output is written, a diagnostic goes to stderr, and the exit status is
the shell status.

SCENARIO shell statuses
GIVEN an unknown command, a missing script path, or a missing data file
WHEN it runs
THEN the CLI exits 127 for an unknown command and 1 for a missing script or
data file.

SCENARIO CLI usage errors
GIVEN an unknown flag, a missing `-c` argument, or extra arguments
WHEN it runs
THEN it exits 2, writes usage to stderr, and executes nothing.

SCENARIO oversized or NUL-containing script
GIVEN more than 1 MiB of script text, or a script containing a NUL byte
WHEN it runs
THEN it exits 2 with a diagnostic and executes nothing.

## CL5 - distribution

SCENARIO library remains CLI-free
GIVEN `build/libshell89.a`
WHEN its symbols are inspected
THEN no CLI helper or `main` symbol is present.
