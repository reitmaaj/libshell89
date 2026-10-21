#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "shell89_priv.h"
#include "shell89_support.h"

static void test_read_eof(void)
{
    int fds[2];
    shell89_io io;
    char buf[4];
    size_t n;
    ssize_t w;

    CHECK_EQ(pipe(fds), 0);
    w = write(fds[1], "abc", 3);
    CHECK_EQ(w, 3);
    close(fds[1]);
    io.in_fd = fds[0];
    io.out_fd = -1;
    io.err_fd = -1;
    io.error = 0;
    CHECK_EQ(shell89_read(&io, buf, sizeof buf, &n), 0);
    CHECK_EQ(n, 3);
    CHECK_EQ(memcmp(buf, "abc", 3), 0);
    CHECK_EQ(shell89_read(&io, buf, sizeof buf, &n), 0);
    CHECK_EQ(n, 0);
    CHECK_EQ(shell89_read(&io, buf, sizeof buf, &n), 0);
    CHECK_EQ(n, 0);
    CHECK_EQ(shell89_read(&io, buf, 0, &n), 0);
    CHECK_EQ(n, 0);
    close(fds[0]);
}

static void test_write(void)
{
    FILE *f;
    shell89_io io;
    char data[4096];
    char check[4096];
    struct shell89_run_io unused;
    size_t i;

    (void)unused;
    for (i = 0; i < sizeof data; i++)
    {
        data[i] = (char)(i & 0xff);
    }
    f = tmpfile();
    CHECK(f != NULL);
    io.in_fd = -1;
    io.out_fd = fileno(f);
    io.err_fd = fileno(f);
    io.error = 0;
    CHECK_EQ(shell89_write_stdout(&io, data, sizeof data), 0);
    CHECK_EQ(shell89_write_stderr(&io, "xyz", 3), 0);
    CHECK_EQ(shell89_stream_error(&io), 0);
    fflush(f);
    rewind(f);
    CHECK_EQ(fread(check, 1, sizeof check, f), sizeof check);
    CHECK_EQ(memcmp(check, data, sizeof data), 0);
    CHECK_EQ(fread(check, 1, 3, f), 3);
    CHECK_EQ(memcmp(check, "xyz", 3), 0);
    fclose(f);
}

static void test_epipe(void)
{
    int fds[2];
    shell89_io io;
    struct sigaction ignore;
    struct sigaction old;
    char c;

    memset(&ignore, 0, sizeof ignore);
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    CHECK_EQ(sigaction(SIGPIPE, &ignore, &old), 0);
    CHECK_EQ(pipe(fds), 0);
    close(fds[0]);
    io.in_fd = -1;
    io.out_fd = fds[1];
    io.err_fd = fds[1];
    io.error = 0;
    CHECK(shell89_write_stdout(&io, "x", 1) != 0);
    CHECK_EQ(shell89_stream_error(&io), EPIPE);
    CHECK_EQ(sigaction(SIGPIPE, &old, NULL), 0);
    close(fds[1]);
    (void)c;
}

static void test_nulls(void)
{
    size_t n;
    shell89_io io;

    CHECK(shell89_read(NULL, NULL, 0, &n) != 0);
    CHECK(shell89_read(&io, NULL, 1, &n) != 0);
    CHECK(shell89_read(&io, NULL, 0, NULL) != 0);
    CHECK(shell89_write_stdout(NULL, "x", 1) != 0);
    CHECK(shell89_write_stderr(NULL, "x", 1) != 0);
    CHECK_EQ(shell89_stream_error(NULL), 0);
}

int main(void)
{
    test_read_eof();
    test_write();
    test_epipe();
    test_nulls();
    return shell89_test_report("unit_stream");
}
