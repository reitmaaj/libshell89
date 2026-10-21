#!/bin/sh -eu
# example-scripts-check.sh <binary> <suite> - run the example .shell scripts.
#
# Each script runs in an isolated directory containing a copy of fixtures/.
# Expected stdout, stderr, status, and created files live under expected/.

bin=${1:?usage: example-scripts-check.sh <binary> <suite>}
suite=${2:?usage: example-scripts-check.sh <binary> <suite>}

case "$bin" in
    /*) ;;
    *) bin=$(cd "$(dirname "$bin")" && pwd)/$(basename "$bin") ;;
esac
[ -x "$bin" ] || {
    echo "example-scripts: not executable: $bin" >&2
    exit 1
}

suite=$(cd "$suite" && pwd)
tmp=build/tmp/example-scripts
rm -rf "$tmp"
mkdir -p "$tmp"

failures=0
LC_ALL=C
export LC_ALL

fail()
{
    echo "example-scripts: FAIL: $1" >&2
    failures=$((failures + 1))
}

check_output()
{
    name=$1
    stream=$2
    actual=$3
    expected=$4
    if [ -f "$expected" ]; then
        if ! cmp "$expected" "$actual"; then
            fail "$name: $stream differs"
        fi
    else
        if [ -s "$actual" ]; then
            fail "$name: unexpected $stream"
        fi
    fi
}

check_files()
{
    name=$1
    work=$2
    actual_list=$3
    expected_dir=$4
    wanted_list=$5

    find "$work" -type f ! -path "$work/fixtures/*" -print |
        sed "s|^$work/||" | sort >"$actual_list"
    if [ ! -d "$expected_dir" ]; then
        if [ -s "$actual_list" ]; then
            fail "$name: unexpected created files"
        fi
        return
    fi
    find "$expected_dir" -type f -print | sed "s|^$expected_dir/||" |
        sort >"$wanted_list"
    if ! cmp -s "$actual_list" "$wanted_list"; then
        fail "$name: created file set differs"
        return
    fi
    while IFS= read -r relative; do
        if ! cmp "$expected_dir/$relative" "$work/$relative"; then
            fail "$name: created file differs: $relative"
        fi
    done <"$wanted_list"
}

for script in "$suite"/*.shell; do
    [ -e "$script" ] || continue
    name=$(basename "$script" .shell)
    work="$tmp/$name"
    out="$tmp/$name.stdout"
    err="$tmp/$name.stderr"
    status_file="$suite/expected/$name.status"
    mkdir -p "$work"
    cp -R "$suite/fixtures" "$work/fixtures"

    rc=0
    (cd "$work" && "$bin" "$script") >"$out" 2>"$err" || rc=$?

    if [ -f "$status_file" ]; then
        wanted_status=$(cat "$status_file")
    else
        wanted_status=0
    fi
    if [ "$rc" != "$wanted_status" ]; then
        fail "$name: expected status $wanted_status, got $rc"
    fi
    check_output "$name" stdout "$out" "$suite/expected/$name.out"
    check_output "$name" stderr "$err" "$suite/expected/$name.err"
    check_files "$name" "$work" "$tmp/$name.actual-files" \
        "$suite/expected/$name.files" "$tmp/$name.wanted-files"
done

if [ "$failures" -ne 0 ]; then
    echo "example-scripts: $failures failure(s)" >&2
    exit 1
fi
rm -rf "$tmp"
echo "example-scripts: ok"
