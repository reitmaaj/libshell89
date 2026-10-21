# 0004 - example script scenarios

SCENARIO complete suite
GIVEN `just example-scripts`
WHEN the runner executes all twenty scripts
THEN every script's stdout, stderr, status, and created files match `expected/`.

SCENARIO isolated execution
GIVEN two scripts that write the same relative filename
WHEN the suite runs
THEN each script writes into its own temporary directory and sees only a fresh
copy of `fixtures/`.

SCENARIO no-match status
GIVEN `13-grep-no-match.shell`
WHEN the pipeline finds no matching line
THEN the script exits 1 with empty stdout and stderr and the runner accepts it.

SCENARIO sequence continuation
GIVEN `20-sequence-continues.shell`
WHEN the first pipeline fails to read a missing fixture
THEN the second pipeline still runs, stdout is `continued\n`, the exit status is
0, and stderr contains the read diagnostic.

SCENARIO created-file verification
GIVEN scripts 17, 18, and 19
WHEN they run
THEN `copy.txt`, `normalized.txt`, and `joined.txt` match the expected bytes and
no unexpected files exist.

SCENARIO unexpected files
GIVEN a script that leaves an extra file outside `fixtures/`
WHEN the runner compares created files
THEN the script is reported as failing.
