/* proc_fixture.c - standalone process twin of the native test vocabulary. */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static unsigned long number(int argc, char **argv, const char *name,
                            unsigned long fallback)
{
    size_t n;
    int i;
    char *end;
    unsigned long v;
    const char *s;

    n = strlen(name);
    for (i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "--", 2) != 0)
        {
            continue;
        }
        s = argv[i] + 2;
        if (strncmp(s, name, n) != 0)
        {
            continue;
        }
        if (s[n] != '=')
        {
            continue;
        }
        errno = 0;
        end = NULL;
        v = strtoul(s + n + 1, &end, 10);
        if (errno != 0 || end == s + n + 1 || *end != '\0')
        {
            return fallback;
        }
        return v;
    }
    return fallback;
}

static const char *string_opt(int argc, char **argv, const char *name)
{
    size_t n;
    int i;

    n = strlen(name);
    for (i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "--", 2) != 0)
        {
            continue;
        }
        if (strncmp(argv[i] + 2, name, n) == 0 && argv[i][2 + n] == '=')
        {
            return argv[i] + 2 + n + 1;
        }
    }
    return NULL;
}

static int flag_opt(int argc, char **argv, const char *name)
{
    int i;

    for (i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "--", 2) == 0 && strcmp(argv[i] + 2, name) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static unsigned char pattern(unsigned long offset, unsigned long seed)
{
    unsigned long v;

    v = offset * 131UL + seed * 17UL + (offset >> 8);
    return (unsigned char)(v & 0xffUL);
}

static int write_all(int fd, const void *buffer, size_t length)
{
    const unsigned char *p;
    size_t off;
    ssize_t n;

    p = (const unsigned char *)buffer;
    off = 0;
    while (off < length)
    {
        n = write(fd, p + off, length - off);
        if (n < 0 && errno == EINTR)
        {
            continue;
        }
        if (n <= 0)
        {
            return 1;
        }
        off += (size_t)n;
    }
    return 0;
}

static ssize_t read_retry(int fd, void *buffer, size_t length)
{
    ssize_t n;

    do
    {
        n = read(fd, buffer, length);
    } while (n < 0 && errno == EINTR);
    return n;
}

static int do_emit(int argc, char **argv)
{
    unsigned char buf[8192];
    unsigned long size;
    unsigned long seed;
    unsigned long off;
    size_t n;
    size_t i;

    size = number(argc, argv, "size", 0);
    seed = number(argc, argv, "seed", 0);
    off = 0;
    while (off < size)
    {
        n = sizeof buf;
        if ((unsigned long)n > size - off)
        {
            n = (size_t)(size - off);
        }
        for (i = 0; i < n; i++)
        {
            buf[i] = pattern(off + (unsigned long)i, seed);
        }
        if (write_all(1, buf, n) != 0)
        {
            return 1;
        }
        off += (unsigned long)n;
    }
    return 0;
}

static int do_copy(void)
{
    unsigned char buf[8192];
    ssize_t n;

    while ((n = read_retry(0, buf, sizeof buf)) > 0)
    {
        if (write_all(1, buf, (size_t)n) != 0)
        {
            return 1;
        }
    }
    return n < 0;
}

static int do_upper(void)
{
    unsigned char buf[8192];
    ssize_t n;
    size_t i;

    while ((n = read_retry(0, buf, sizeof buf)) > 0)
    {
        for (i = 0; i < (size_t)n; i++)
        {
            if (buf[i] >= 'a' && buf[i] <= 'z')
            {
                buf[i] = (unsigned char)(buf[i] - 'a' + 'A');
            }
        }
        if (write_all(1, buf, (size_t)n) != 0)
        {
            return 1;
        }
    }
    return n < 0;
}

static int do_count(int argc, char **argv)
{
    unsigned char buf[8192];
    ssize_t n;
    size_t i;
    unsigned long total;
    unsigned long lines;
    char text[64];
    int len;
    int want_lines;

    total = 0;
    lines = 0;
    want_lines = flag_opt(argc, argv, "lines");
    while ((n = read_retry(0, buf, sizeof buf)) > 0)
    {
        total += (unsigned long)n;
        if (want_lines != 0)
        {
            for (i = 0; i < (size_t)n; i++)
            {
                if (buf[i] == '\n')
                {
                    lines++;
                }
            }
        }
    }
    if (n < 0)
    {
        return 1;
    }
    if (want_lines != 0)
    {
        total = lines;
    }
    len = sprintf(text, "%lu\n", total);
    return write_all(1, text, (size_t)len);
}

static int do_error(int argc, char **argv)
{
    const char *text;

    text = string_opt(argc, argv, "text");
    if (text == NULL)
    {
        return 1;
    }
    return write_all(2, text, strlen(text));
}

static int do_partial(int argc, char **argv)
{
    unsigned char buf[1024];
    unsigned long left;
    ssize_t n;
    size_t want;

    left = number(argc, argv, "size", 0);
    while (left != 0)
    {
        want = sizeof buf;
        if ((unsigned long)want > left)
        {
            want = (size_t)left;
        }
        n = read_retry(0, buf, want);
        if (n <= 0)
        {
            break;
        }
        left -= (unsigned long)n;
    }
    return 0;
}

static int do_args(int argc, char **argv)
{
    const char *eq;
    char text[4096];
    int len;
    int i;

    for (i = 2; i < argc; i++)
    {
        if (strncmp(argv[i], "--", 2) != 0)
        {
            continue;
        }
        eq = strchr(argv[i] + 2, '=');
        if (eq != NULL)
        {
            len = sprintf(text, "%.*s=%s\n", (int)(eq - (argv[i] + 2)),
                          argv[i] + 2, eq + 1);
        }
        else
        {
            len = sprintf(text, "%s=1\n", argv[i] + 2);
        }
        if (write_all(1, text, (size_t)len) != 0)
        {
            return 1;
        }
    }
    return 0;
}

static int do_env(void)
{
    const char *lc;
    const char *path;
    char text[512];
    int len;

    lc = getenv("LC_ALL");
    path = getenv("PATH");
    len = sprintf(text, "LC_ALL=%s\nPATH=%s\n", lc != NULL ? lc : "",
                  path != NULL ? path : "");
    return write_all(1, text, (size_t)len);
}

static int do_signal(int argc, char **argv)
{
    unsigned long sig;

    sig = number(argc, argv, "sig", 15);
    if (sig == 0 || sig > 128)
    {
        return 1;
    }
    raise((int)sig);
    return 1;
}

static int do_slow(int argc, char **argv)
{
    unsigned char buf[65536];
    unsigned long chunk;
    unsigned long delay;
    struct timespec req;
    ssize_t n;

    chunk = number(argc, argv, "chunk", 4096);
    delay = number(argc, argv, "delay", 0);
    if (chunk == 0 || chunk > sizeof buf)
    {
        chunk = sizeof buf;
    }
    req.tv_sec = (time_t)(delay / 1000000UL);
    req.tv_nsec = (long)((delay % 1000000UL) * 1000UL);
    while ((n = read_retry(0, buf, chunk)) > 0)
    {
        if (write_all(1, buf, (size_t)n) != 0)
        {
            return 1;
        }
        if (delay != 0)
        {
            nanosleep(&req, NULL);
        }
    }
    return n < 0;
}

int main(int argc, char **argv)
{
    const char *op;

    if (argc < 2)
    {
        return 2;
    }
    op = argv[1];
    if (strcmp(op, "emit") == 0)
    {
        return do_emit(argc, argv);
    }
    if (strcmp(op, "copy") == 0)
    {
        return do_copy();
    }
    if (strcmp(op, "upper") == 0)
    {
        return do_upper();
    }
    if (strcmp(op, "count") == 0)
    {
        return do_count(argc, argv);
    }
    if (strcmp(op, "fail") == 0)
    {
        return (int)(number(argc, argv, "status", 1) & 0xffUL);
    }
    if (strcmp(op, "error") == 0)
    {
        return do_error(argc, argv);
    }
    if (strcmp(op, "partial") == 0)
    {
        return do_partial(argc, argv);
    }
    if (strcmp(op, "args") == 0 || strcmp(op, "record") == 0)
    {
        return do_args(argc, argv);
    }
    if (strcmp(op, "slow") == 0)
    {
        return do_slow(argc, argv);
    }
    if (strcmp(op, "env") == 0)
    {
        return do_env();
    }
    if (strcmp(op, "signal") == 0)
    {
        return do_signal(argc, argv);
    }
    return 127;
}
