#!/bin/sh -eu
set -eu
root="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
mode="${1:-asan}"
cc="${SAN_CC:-clang}"
out="$root/build/san-$mode"
mkdir -p "$out"
set -- -std=c89 -pedantic-errors -Wall -Wextra -Werror
case "$mode" in
    asan) set -- "$@" -fsanitize=address,undefined -fno-sanitize-recover=all ;;
    tsan) set -- "$@" -fsanitize=thread ;;
    *) echo "sanitize: unknown mode $mode" >&2; exit 1 ;;
esac
set -- "$@" -fno-omit-frame-pointer -g -O1
set -- "$@" -D_POSIX_C_SOURCE=200809L -pthread
rm -rf "$out"/obj
mkdir -p "$out/obj"
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
    "$root"/tests/integration/test_*.c "$root"/tests/property/test_property.c; do
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
    if [ "$mode" = asan ]; then
        ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
            SHELL89_PROC_FIXTURE="$out/fixture_proc" "$out/$name" >/dev/null
    else
        TSAN_OPTIONS=halt_on_error=1 SHELL89_PROC_FIXTURE="$out/fixture_proc" \
            "$out/$name" >/dev/null
    fi
done
echo "sanitize: $mode ok"
