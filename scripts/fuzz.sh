#!/bin/sh -eu
set -eu

root="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$root"
mkdir -p build/fuzz
set -- -std=c89 -pedantic-errors -Wall -Wextra -Werror -Wno-long-long
set -- "$@" -D_POSIX_C_SOURCE=200809L -Iinclude -Isrc
set -- "$@" -I../libstring89/include -I../libunicode89/include
clang "$@" -fsanitize=fuzzer,address,undefined -g -O1 \
    -o build/fuzz/fuzz_parser tests/property/fuzz_parser.c \
    src/shell89.c src/shell89_parse.c src/shell89_validate.c \
    src/shell89_stream.c src/shell89_posix.c \
    ../libstring89/build/libstring89.a ../libunicode89/build/libunicode89.a \
    -pthread
ASAN_OPTIONS=detect_leaks=1 ./build/fuzz/fuzz_parser -runs=100000
