#!/bin/sh -eu
set -eu
root="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
out="$root/build/valgrind"
cc="${CC:-cc}"
if ! command -v valgrind >/dev/null 2>&1; then
    echo "valgrind: SKIPPED: not installed"
    exit 0
fi
mkdir -p "$out/obj"
set -- -std=c89 -pedantic-errors -Wall -Wextra -Werror -Wconversion
set -- "$@" -Wsign-conversion -Wstrict-prototypes -Wmissing-prototypes
set -- "$@" -Wold-style-definition -Wundef -Wshadow -Wformat=2 -Wno-long-long
set -- "$@" -D_POSIX_C_SOURCE=200809L -pthread -g -O0
for f in "$root"/src/*.c; do
    name=$(basename "$f" .c)
    "$cc" "$@" -I"$root/include" -I"$root/src" \
        -I"$root/../libstring89/include" -I"$root/../libunicode89/include" \
        -c "$f" -o "$out/obj/$name.o"
done
ar rcs "$out/libshell89.a" "$out"/obj/*.o
"$cc" "$@" -I"$root/tests/fixtures" -o "$out/fixture_proc" \
    "$root/tests/fixtures/proc_fixture.c"
for t in "$root"/tests/smoke.c "$root"/tests/unit/test_*.c \
    "$root"/tests/integration/test_native.c \
    "$root"/tests/integration/test_embedding.c \
    "$root"/tests/integration/test_equivalence.c \
    "$root"/tests/integration/test_pipelines.c \
    "$root"/tests/integration/test_sequence.c \
    "$root"/tests/property/test_property.c; do
    [ -e "$t" ] || continue
    name=$(basename "$t" .c)
    "$cc" "$@" -Wno-unused-function -Wno-unused-result -I"$root/include" \
        -I"$root/src" -I"$root/../libstring89/include" \
        -I"$root/../libunicode89/include" -I"$root/tests/support" \
        -o "$out/$name" "$t" "$root/tests/support/shell89_test.c" \
        "$root/tests/support/shell89_capture.c" \
        "$root/tests/support/shell89_fixtures.c" "$out/libshell89.a" \
        "$root/../libstring89/build/libstring89.a" \
        "$root/../libunicode89/build/libunicode89.a"
    SHELL89_PROC_FIXTURE="$out/fixture_proc" valgrind --error-exitcode=1 \
        --leak-check=full --errors-for-leak-kinds=definite,indirect \
        --child-silent-after-fork=yes "$out/$name" >/dev/null
done
echo "valgrind: ok"
