set shell := ["sh", "-eu", "-c"]

CC := env_var_or_default("CC", "cc")
CXX := env_var_or_default("CXX", "c++")
GREEN := env_var_or_default("GREEN", "../green/.agent/tmp/build/green")
STRING89 := env_var_or_default("STRING89", "../libstring89")
UNICODE89 := env_var_or_default("UNICODE89", "../libunicode89")
STRING89INC := STRING89 + "/include"
UNICODE89INC := UNICODE89 + "/include"
STRING89LIB := STRING89 + "/build/libstring89.a"
UNICODE89LIB := UNICODE89 + "/build/libunicode89.a"

STRICT := "-std=c89 -pedantic-errors -Wall -Wextra -Werror -Wconversion -Wsign-conversion -Wstrict-prototypes -Wmissing-prototypes -Wold-style-definition -Wundef -Wshadow -Wformat=2 -Wno-long-long"
POSIX := "-D_POSIX_C_SOURCE=200809L"
TESTFLAGS := "-Wno-unused-function -Wno-unused-result"
INC := "-Iinclude -Isrc -I" + STRING89INC + " -I" + UNICODE89INC + " -Itests/support"
LIBS := "build/libshell89.a " + STRING89LIB + " " + UNICODE89LIB
PTHREAD := "-pthread"
LIB := "build/libshell89.a"

default: build

deps:
	cd {{UNICODE89}} && just build
	cd {{STRING89}} && just build

build: deps
	rm -rf build/obj
	mkdir -p build/obj
	@for f in src/*.c; do \
	    name=$(basename "$f" .c); \
	    {{CC}} {{STRICT}} {{POSIX}} {{PTHREAD}} -Iinclude -Isrc -I{{STRING89INC}} -I{{UNICODE89INC}} \
	        -c "$f" -o "build/obj/$name.o" || exit 1; \
	done
	@objs=""; \
	for f in build/obj/*.o; do [ -e "$f" ] || continue; objs="$objs $f"; done; \
	rm -f {{LIB}}; \
	ar rcs {{LIB}} $objs

clean:
	rm -rf build

format:
	clang-format -i src/*.c src/*.h include/*.h tests/support/*.c tests/support/*.h tests/unit/*.c tests/integration/*.c tests/fault/*.c tests/property/*.c tests/stress/*.c tests/fixtures/*.c tests/cli/*.c examples/*.c tool/*.c tool/*.h

lint:
	@for f in scripts/*; do \
	    [ -e "$f" ] || continue; \
	    case "$f" in *.conf) continue ;; esac; \
	    shellcheck -s sh "$f" || exit 1; \
	    shellcheck -s bash "$f" || exit 1; \
	done
	@for f in src/*.c src/*.h include/*.h tests/support/*.c tests/support/*.h tests/unit/*.c tests/integration/*.c tests/fault/*.c tests/property/*.c tests/stress/*.c tests/fixtures/*.c tests/cli/*.c examples/*.c tool/*.c tool/*.h; do \
	    [ -e "$f" ] || continue; \
	    clang-format --dry-run --Werror "$f" || { echo "needs formatting: $f"; exit 1; }; \
	done

green:
	sh scripts/gen_compile_db gcc build/gcc/compile_commands.json
	sh scripts/gen_compile_db clang build/clang/compile_commands.json
	{{GREEN}} check

examples: build
	@mkdir -p build
	@for f in examples/*.c; do \
	    name=$(basename "$f" .c); \
	    {{CC}} {{STRICT}} {{POSIX}} {{PTHREAD}} -Iinclude \
	        -o "build/$name" "$f" build/libshell89.a {{STRING89LIB}} {{UNICODE89LIB}} || exit 1; \
	done
	./build/embed

tool: build
	@mkdir -p build
	{{CC}} {{STRICT}} {{POSIX}} {{PTHREAD}} -Iinclude -Itool \
	    -o build/shell89 tool/shell89.c tool/shell89_cli.c \
	    tool/shell89_lines.c build/libshell89.a {{STRING89LIB}} \
	    {{UNICODE89LIB}}

cli: tool
	@mkdir -p build
	{{CC}} {{STRICT}} {{POSIX}} {{PTHREAD}} {{TESTFLAGS}} -Iinclude -Itool \
	    -Itests/support -o build/test_cli tests/cli/test_cli.c \
	    tool/shell89_cli.c tool/shell89_lines.c \
	    tests/support/shell89_test.c build/libshell89.a {{STRING89LIB}} \
	    {{UNICODE89LIB}}
	./build/test_cli
	{{CC}} {{STRICT}} {{POSIX}} {{PTHREAD}} {{TESTFLAGS}} -Iinclude -Itool \
	    -Itests/support -o build/test_lines tests/cli/test_lines.c \
	    tool/shell89_cli.c tool/shell89_lines.c \
	    tests/support/shell89_test.c tests/support/shell89_capture.c \
	    build/libshell89.a {{STRING89LIB}} {{UNICODE89LIB}}
	./build/test_lines
	sh scripts/cli-check.sh ./build/shell89 build/libshell89.a

example-scripts: tool
	sh scripts/example-scripts-check.sh ./build/shell89 examples/scripts

header:
	@mkdir -p build
	@printf '#include <shell89.h>\nint main(void){return 0;}\n' > build/header.c
	{{CC}} -std=c89 -pedantic-errors -Wall -Wextra -Werror {{POSIX}} -Iinclude \
	    -c build/header.c -o build/header.o

cpp-check:
	@mkdir -p build
	@printf '#include <shell89.h>\nint main(){return 0;}\n' > build/header.cpp
	{{CXX}} -std=c++23 -pedantic-errors -Wall -Wextra -Werror -Iinclude \
	    -c build/header.cpp -o build/header-cpp.o

check: build lint test examples header cpp-check cli example-scripts
release: check matrix sanitize tsan valgrind fuzz test32

fixture: build
	@mkdir -p build
	{{CC}} {{STRICT}} {{POSIX}} -Itests/fixtures -o build/fixture_proc tests/fixtures/proc_fixture.c

test: fixture smoke unit integration fault property stress

smoke: build fixture
	@mkdir -p build
	{{CC}} {{STRICT}} {{POSIX}} {{PTHREAD}} {{TESTFLAGS}} {{INC}} \
	    -o build/test_smoke tests/smoke.c tests/support/shell89_test.c \
	    tests/support/shell89_capture.c tests/support/shell89_fixtures.c \
	    build/libshell89.a {{STRING89LIB}} {{UNICODE89LIB}}
	./build/test_smoke

_run dir:
	@mkdir -p build
	@for t in tests/{{dir}}/test_*.c; do \
	    [ -e "$t" ] || continue; \
	    name=$(basename "$t" .c); \
	    {{CC}} {{STRICT}} {{POSIX}} {{PTHREAD}} {{TESTFLAGS}} {{INC}} \
	        -o "build/$name" "$t" tests/support/shell89_test.c \
	        tests/support/shell89_capture.c tests/support/shell89_fixtures.c \
	        build/libshell89.a {{STRING89LIB}} {{UNICODE89LIB}} || exit 1; \
	    ./build/$name || exit 1; \
	done

unit: build
	just _run unit
integration: build fixture
	SHELL89_PROC_FIXTURE=./build/fixture_proc just _run integration
faultlib: deps
	rm -rf build/fault-obj
	mkdir -p build/fault-obj
	@for f in src/*.c; do \
	    name=$(basename "$f" .c); \
	    {{CC}} {{STRICT}} {{POSIX}} {{PTHREAD}} -DSHELL89_TESTING -Iinclude -Isrc \
	        -I{{STRING89INC}} -I{{UNICODE89INC}} -c "$f" -o "build/fault-obj/$name.o" || exit 1; \
	done
	@objs=""; for f in build/fault-obj/*.o; do objs="$objs $f"; done; \
	rm -f build/libshell89_test.a; ar rcs build/libshell89_test.a $objs

fault: faultlib fixture
	@mkdir -p build
	@for t in tests/fault/test_*.c; do \
	    [ -e "$t" ] || continue; \
	    name=$(basename "$t" .c); \
	    {{CC}} {{STRICT}} {{POSIX}} {{PTHREAD}} -DSHELL89_TESTING {{TESTFLAGS}} {{INC}} \
	        -o "build/$name" "$t" tests/support/shell89_test.c \
	        tests/support/shell89_capture.c tests/support/shell89_fixtures.c \
	        tests/support/shell89_fault.c build/libshell89_test.a \
	        {{STRING89LIB}} {{UNICODE89LIB}} || exit 1; \
	    SHELL89_PROC_FIXTURE=./build/fixture_proc ./build/$name || exit 1; \
	done
property: build
	just _run property
stress: build
	just _run stress

build32:
	@mkdir -p build/32/obj
	@if ! printf '#include <stdio.h>\nint main(void){return 0;}\n' | {{CC}} -m32 -x c - -o build/32/probe 2>/dev/null; then \
	    echo "build32: SKIPPED: no 32-bit toolchain"; \
	    exit 0; \
	fi
	cd {{UNICODE89}} && just build32
	cd {{STRING89}} && just build32
	@for f in src/*.c; do \
	    name=$(basename "$f" .c); \
	    {{CC}} {{STRICT}} {{POSIX}} {{PTHREAD}} -m32 -Iinclude -Isrc \
	        -I{{STRING89INC}} -I{{UNICODE89INC}} -c "$f" -o "build/32/obj/$name.o" || exit 1; \
	done
	@rm -f build/32/libshell89.a; \
	ar rcs build/32/libshell89.a build/32/obj/*.o

test32: build32
	@if [ ! -f build/32/libshell89.a ]; then exit 0; fi; \
	{{CC}} {{STRICT}} {{POSIX}} {{PTHREAD}} {{TESTFLAGS}} {{INC}} -m32 \
	    -o build/32/test_smoke tests/smoke.c tests/support/shell89_test.c \
	    tests/support/shell89_capture.c tests/support/shell89_fixtures.c \
	    build/32/libshell89.a {{STRING89}}/build/32/libstring89.a \
	    {{UNICODE89}}/build/32/libunicode89.a || exit 1; \
	./build/32/test_smoke || exit 1; \
	for t in tests/unit/test_*.c; do \
	    [ -e "$t" ] || continue; \
	    name=$(basename "$t" .c); \
	    {{CC}} {{STRICT}} {{POSIX}} {{PTHREAD}} {{TESTFLAGS}} {{INC}} -m32 \
	        -o "build/32/$name" "$t" tests/support/shell89_test.c \
	        tests/support/shell89_capture.c tests/support/shell89_fixtures.c \
	        build/32/libshell89.a {{STRING89}}/build/32/libstring89.a \
	        {{UNICODE89}}/build/32/libunicode89.a || exit 1; \
	    ./build/32/$name || exit 1; \
	done
	echo "test32: ok"

matrix:
	sh scripts/matrix.sh
sanitize:
	sh scripts/sanitize.sh asan
tsan:
	sh scripts/sanitize.sh tsan
valgrind:
	sh scripts/valgrind.sh
fuzz:
	sh scripts/fuzz.sh
