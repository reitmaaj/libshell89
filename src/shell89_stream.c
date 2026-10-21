/* shell89_stream.c - callback stream operations and POSIX error capture. */

#include <errno.h>
#include <unistd.h>

#include "shell89_priv.h"

#include "shell89_test.h"

ssize_t shell89_priv_read_fd(int fd, void *buffer, size_t length)
{
#ifdef SHELL89_TESTING
    if (shell89_test_fault_should_fail(SHELL89_TEST_READ))
    {
        errno = EIO;
        return -1;
    }
#endif
    return read(fd, buffer, length);
}

ssize_t shell89_priv_write_fd(int fd, const void *buffer, size_t length)
{
#ifdef SHELL89_TESTING
    if (shell89_test_fault_should_fail(SHELL89_TEST_WRITE))
    {
        errno = EIO;
        return -1;
    }
#endif
    return write(fd, buffer, length);
}

int shell89_read(shell89_io *io, void *buffer, size_t capacity,
                 size_t *received)
{
    ssize_t n;

    if (received == NULL)
    {
        return 1;
    }
    *received = 0;
    if (io == NULL)
    {
        return 1;
    }
    if (capacity == 0)
    {
        return 0;
    }
    if (buffer == NULL)
    {
        io->error = EINVAL;
        return 1;
    }
    do
    {
        n = shell89_priv_read_fd(io->in_fd, buffer, capacity);
    } while (n < 0 && errno == EINTR);
    if (n < 0)
    {
        io->error = errno;
        return 1;
    }
    *received = (size_t)n;
    return 0;
}

static int write_all(shell89_io *io, int fd, const void *buffer, size_t length)
{
    const unsigned char *p;
    size_t offset;
    ssize_t n;

    if (io == NULL)
    {
        return 1;
    }
    if (length == 0)
    {
        return 0;
    }
    if (buffer == NULL)
    {
        io->error = EINVAL;
        return 1;
    }
    p = (const unsigned char *)buffer;
    offset = 0;
    while (offset < length)
    {
        do
        {
            n = shell89_priv_write_fd(fd, p + offset, length - offset);
        } while (n < 0 && errno == EINTR);
        if (n < 0)
        {
            io->error = errno;
            return 1;
        }
        if (n == 0)
        {
            io->error = EIO;
            return 1;
        }
        offset += (size_t)n;
    }
    return 0;
}

int shell89_write_stdout(shell89_io *io, const void *buffer, size_t length)
{
    if (io == NULL)
    {
        return 1;
    }
    return write_all(io, io->out_fd, buffer, length);
}

int shell89_write_stderr(shell89_io *io, const void *buffer, size_t length)
{
    if (io == NULL)
    {
        return 1;
    }
    return write_all(io, io->err_fd, buffer, length);
}

int shell89_stream_error(const shell89_io *io)
{
    if (io == NULL)
    {
        return 0;
    }
    return io->error;
}
