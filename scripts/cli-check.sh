#!/bin/sh -eu
# cli-check.sh <binary> [library] - end-to-end checks for the shell89 CLI.

bin=${1:?usage: cli-check.sh <binary> [library]}
lib=${2:-}

tmp=build/tmp
mkdir -p "$tmp"
file="$tmp/shell89-cli-$$.dat"
append_file="$tmp/shell89-cli-$$.append"
empty_file="$tmp/shell89-cli-$$.empty"
script="$tmp/shell89-cli-$$.script"
big="$tmp/shell89-cli-$$.big"
nul="$tmp/shell89-cli-$$.nul"
longline="$tmp/shell89-cli-$$.longline"
manylines="$tmp/shell89-cli-$$.manylines"
input="$tmp/shell89-cli-$$.in"
out="$tmp/shell89-cli-$$.out"
err="$tmp/shell89-cli-$$.err"
expected="$tmp/shell89-cli-$$.expected"
rm -f "$file" "$append_file" "$empty_file" "$script" "$big" "$nul" \
    "$longline" "$manylines" "$input" "$out" "$err" "$expected"

fail()
{
    echo "cli-check: FAIL: $1" >&2
    exit 1
}

expect_exit()
{
    want=$1
    shift
    rc=0
    "$@" >"$out" 2>"$err" || rc=$?
    [ "$rc" = "$want" ] || fail "expected exit $want, got $rc: $*"
}

expect_output()
{
    printf '%s' "$1" >"$expected"
    cmp "$out" "$expected" || fail "unexpected stdout: $2"
    [ ! -s "$err" ] || fail "unexpected stderr: $2"
}

# Inline scripts, pipelines, and native commands.
expect_exit 0 "$bin" -c 'text echo --text="hello"'
printf 'hello\n' >"$expected"
cmp "$out" "$expected" || fail "inline echo output"

expect_exit 0 "$bin" -c 'text echo --text="abc" | text upper'
printf 'ABC\n' >"$expected"
cmp "$out" "$expected" || fail "pipeline output"

# Byte and line counts, including a final unterminated line.
: | "$bin" -c 'text count' >"$out" 2>"$err" || fail "empty byte count"
expect_output '0
' "empty byte count"

printf 'abc' | "$bin" -c 'text count' >"$out" 2>"$err" || fail "byte count"
expect_output '3
' "byte count"

: | "$bin" -c 'text count --lines' >"$out" 2>"$err" ||
    fail "empty line count"
expect_output '0
' "empty line count"

printf 'a\n' | "$bin" -c 'text count --lines' >"$out" 2>"$err" ||
    fail "terminated line count"
expect_output '1
' "terminated line count"

printf 'a\nb' | "$bin" -c 'text count --lines' >"$out" 2>"$err" ||
    fail "final line count"
expect_output '2
' "final line count"

# Line commands: lower, grep, head, and skip.
printf 'Hello CAF\303\211\n' | "$bin" -c 'text lower' >"$out" 2>"$err" ||
    fail "lower"
printf 'hello caf\303\211\n' >"$expected"
cmp "$out" "$expected" || fail "lower output"
[ ! -s "$err" ] || fail "lower wrote stderr"

printf 'alpha\nERROR one\nbeta\nERROR two\n' |
    "$bin" -c 'text grep --pattern="ERROR"' >"$out" 2>"$err" ||
    fail "grep match"
expect_output 'ERROR one
ERROR two
' "grep match"

printf 'ERROR\nerror\nInfo\n' |
    "$bin" -c 'text grep --pattern="error" --ignore-case' >"$out" 2>"$err" ||
    fail "grep ignore case"
expect_output 'ERROR
error
' "grep ignore case"

printf 'a\nb\n' >"$input"
rc=0
"$bin" -c 'text grep --pattern="FATAL"' <"$input" >"$out" 2>"$err" || rc=$?
[ "$rc" = 1 ] || fail "grep no-match status: $rc"
[ ! -s "$out" ] || fail "grep no-match wrote stdout"
[ ! -s "$err" ] || fail "grep no-match wrote stderr"

printf 'a\nb' | "$bin" -c 'text grep --pattern=""' >"$out" 2>"$err" ||
    fail "grep empty pattern"
expect_output 'a
b' "grep empty pattern"

printf 'a\nb\nc\nd' | "$bin" -c 'text head --lines="2"' >"$out" 2>"$err" ||
    fail "head"
expect_output 'a
b
' "head"

printf 'a\nb\nc\nd' | "$bin" -c 'text skip --lines="2"' >"$out" 2>"$err" ||
    fail "skip"
expect_output 'c
d' "skip"

printf 'a\nb\nc\nd' | "$bin" -c 'text head --lines="0"' >"$out" 2>"$err" ||
    fail "head zero"
[ ! -s "$out" ] || fail "head zero wrote stdout"
[ ! -s "$err" ] || fail "head zero wrote stderr"

expect_exit 2 "$bin" -c 'text head --lines="x"'
[ ! -s "$out" ] || fail "invalid head count wrote stdout"
[ -s "$err" ] || fail "invalid head count wrote no diagnostic"
expect_exit 2 "$bin" -c 'text skip --lines=""'
[ ! -s "$out" ] || fail "invalid skip count wrote stdout"
[ -s "$err" ] || fail "invalid skip count wrote no diagnostic"

# A line over 64 KiB is rejected by line-oriented commands.
dd if=/dev/zero bs=1024 count=65 2>/dev/null | tr '\0' 'a' >"$longline"
rc=0
"$bin" -c 'text grep --pattern="a"' <"$longline" >"$out" 2>"$err" || rc=$?
[ "$rc" = 1 ] || fail "overlong line status: $rc"
[ ! -s "$out" ] || fail "overlong line wrote stdout"
[ -s "$err" ] || fail "overlong line wrote no diagnostic"

# head drains its input so an upstream files read sees no broken pipe.
i=0
while [ "$i" -lt 1000 ]; do
    printf 'line\n'
    i=$((i + 1))
done >"$manylines"
"$bin" -c "files read --path=\"$manylines\" | text head --lines=\"1\"" \
    >"$out" 2>"$err" || fail "head drain pipeline"
expect_output 'line
' "head drain pipeline"

# Binary write/read round-trip.
printf 'a\000b\377\n' >"$input"
"$bin" -c "files write --path=\"$file\"" <"$input" >"$out" 2>"$err" ||
    fail "files write"
[ ! -s "$out" ] || fail "files write wrote stdout"
[ ! -s "$err" ] || fail "files write wrote stderr"
"$bin" -c "files read --path=\"$file\"" >"$out" 2>"$err" ||
    fail "files read"
cmp "$input" "$out" || fail "binary round-trip"
[ ! -s "$err" ] || fail "files read wrote stderr"

# Append preserves earlier bytes.
printf 'start' >"$append_file"
printf 'end' | "$bin" -c "files write --path=\"$append_file\" --append" \
    >"$out" 2>"$err" || fail "files append"
[ ! -s "$out" ] || fail "files append wrote stdout"
"$bin" -c "files read --path=\"$append_file\"" >"$out" 2>"$err" ||
    fail "files append read"
printf 'startend' >"$expected"
cmp "$out" "$expected" || fail "append content"

# A script file and a script supplied on stdin.
printf 'text echo --text="file"\n' >"$script"
expect_exit 0 "$bin" "$script"
printf 'file\n' >"$expected"
cmp "$out" "$expected" || fail "script file output"

printf 'text echo --text="stdin"' | "$bin" >"$out" 2>"$err" ||
    fail "stdin script"
printf 'stdin\n' >"$expected"
cmp "$out" "$expected" || fail "stdin script output"

printf 'text echo --text="dash"' | "$bin" - >"$out" 2>"$err" ||
    fail "dash script"
printf 'dash\n' >"$expected"
cmp "$out" "$expected" || fail "dash script output"

: | "$bin" >"$out" 2>"$err" || fail "empty stdin script"
[ ! -s "$out" ] || fail "empty script wrote stdout"
[ ! -s "$err" ] || fail "empty script wrote stderr"

# Script text on stdin leaves the first stage with empty stdin.
printf 'files write --path="%s"' "$empty_file" | "$bin" >"$out" 2>"$err" ||
    fail "stdin script with file write"
[ -f "$empty_file" ] || fail "stdin script did not create the file"
[ ! -s "$empty_file" ] || fail "first stage did not receive empty stdin"

# Help exits 0 and prints usage to stdout.
"$bin" -h >"$out" 2>"$err" || fail "-h exited nonzero"
grep -q 'usage:' "$out" || fail "-h did not print usage"
[ ! -s "$err" ] || fail "-h wrote stderr"
"$bin" --help >"$out" 2>"$err" || fail "--help exited nonzero"
grep -q 'usage:' "$out" || fail "--help did not print usage"

# Validation is atomic: the valid prefix must not run.
expect_exit 127 "$bin" -c 'text echo --text="leak"; unknown op'
[ ! -s "$out" ] || fail "valid prefix ran before an unknown command"
[ -s "$err" ] || fail "expected an unknown-command diagnostic"
expect_exit 2 "$bin" -c 'text echo --text="leak"; text echo --nope'
[ ! -s "$out" ] || fail "valid prefix ran before an option error"
[ -s "$err" ] || fail "expected an option diagnostic"

# Shell-level statuses and option validation.
expect_exit 127 "$bin" -c 'unknown op'
expect_exit 2 "$bin" -c 'text echo'
expect_exit 2 "$bin" -c 'text count --nope'
expect_exit 1 "$bin" -c "files read --path=\"$tmp/missing-$$\""
expect_exit 1 "$bin" -c "files write --path=\"$tmp\""

# CLI usage errors exit 2 and execute nothing.
expect_exit 2 "$bin" --bogus
expect_exit 2 "$bin" -c
expect_exit 2 "$bin" "$script" extra
[ ! -s "$out" ] || fail "usage error wrote stdout"
expect_exit 1 "$bin" "$tmp/missing-script-$$"
[ ! -s "$out" ] || fail "missing script wrote stdout"
[ -s "$err" ] || fail "expected a missing-script diagnostic"

# A script over the source limit is rejected before execution.
dd if=/dev/zero bs=1024 count=1025 2>/dev/null | tr '\0' ' ' >"$big"
expect_exit 2 "$bin" "$big"

# A NUL byte would truncate the C string source, so it is rejected.
printf 'text echo --text="x"\000bad' >"$nul"
expect_exit 2 "$bin" "$nul"

# The archive contains no CLI entry point or helpers.
if [ -n "$lib" ]; then
    if nm -g "$lib" 2>/dev/null | grep -E ' T (main|shell89_cli_)' >/dev/null
    then
        fail "library archive exported a CLI symbol"
    fi
fi

rm -f "$file" "$append_file" "$empty_file" "$script" "$big" "$nul" \
    "$longline" "$manylines" "$input" "$out" "$err" "$expected"
echo "cli-check: ok"
