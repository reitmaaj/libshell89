#!/bin/sh -eu
set -eu
root="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
out="$root/build/matrix"
mkdir -p "$out"
set -- -std=c89 -pedantic-errors -Wall -Wextra -Werror
set -- "$@" -Wconversion -Wsign-conversion -Wstrict-prototypes
set -- "$@" -Wmissing-prototypes -Wold-style-definition -Wundef -Wshadow
set -- "$@" -Wformat=2 -Wno-long-long -D_POSIX_C_SOURCE=200809L -pthread
run_cell() {
    compiler="$1"
    std="$2"
    opt="$3"
    shift 3
    echo "matrix: $compiler $std $opt"
    rm -rf "$out/cell"
    mkdir -p "$out/cell"
    for f in "$root"/src/*.c; do
        name=$(basename "$f" .c)
        "$compiler" -std="$std" "$@" "$opt" -I"$root/include" -I"$root/src" \
            -I"$root/../libstring89/include" -I"$root/../libunicode89/include" \
            -c "$f" -o "$out/cell/$name.o"
    done
    ar rcs "$out/cell/libshell89.a" "$out/cell"/*.o
    "$compiler" -std="$std" "$@" "$opt" -I"$root/tests/fixtures" \
        -o "$out/cell/fixture_proc" "$root/tests/fixtures/proc_fixture.c"
    for t in "$root"/tests/smoke.c "$root"/tests/unit/test_*.c \
        "$root"/tests/integration/test_*.c "$root"/tests/property/test_property.c; do
        [ -e "$t" ] || continue
        name=$(basename "$t" .c)
        "$compiler" -std="$std" "$@" "$opt" -Wno-unused-function \
            -Wno-unused-result -I"$root/include" -I"$root/src" \
            -I"$root/../libstring89/include" -I"$root/../libunicode89/include" \
            -I"$root/tests/support" -o "$out/cell/$name" "$t" \
            "$root/tests/support/shell89_test.c" \
            "$root/tests/support/shell89_capture.c" \
            "$root/tests/support/shell89_fixtures.c" \
            "$out/cell/libshell89.a" \
            "$root/../libstring89/build/libstring89.a" \
            "$root/../libunicode89/build/libunicode89.a"
        SHELL89_PROC_FIXTURE="$out/cell/fixture_proc" "$out/cell/$name" >/dev/null
    done
}
for compiler in gcc clang; do
    for std in c89 c23; do
        run_cell "$compiler" "$std" -O0 "$@"
    done
done
for opt in -O0 -O2 -O3; do
    run_cell gcc c89 "$opt" "$@"
done
echo "matrix: ok"
