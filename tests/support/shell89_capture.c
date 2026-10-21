#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "shell89_capture.h"

static int read_file(FILE *f, unsigned char **out, size_t *out_len)
{
    struct stat st;
    unsigned char *p;
    size_t n;

    *out = NULL;
    *out_len = 0;
    fflush(f);
    if (fstat(fileno(f), &st) != 0 || st.st_size < 0)
    {
        return -1;
    }
    if (st.st_size == 0)
    {
        p = (unsigned char *)malloc(1);
        if (p == NULL)
        {
            return -1;
        }
        p[0] = 0;
        *out = p;
        return 0;
    }
    p = (unsigned char *)malloc((size_t)st.st_size);
    if (p == NULL)
    {
        return -1;
    }
    rewind(f);
    n = fread(p, 1, (size_t)st.st_size, f);
    if (n != (size_t)st.st_size)
    {
        free(p);
        return -1;
    }
    *out = p;
    *out_len = n;
    return 0;
}

int shell89_capture_run(shell89 *sh, const char *source, const void *input,
                        size_t input_len, unsigned char **out, size_t *out_len,
                        unsigned char **err, size_t *err_len,
                        shell89_result **result)
{
    FILE *in;
    FILE *output;
    FILE *errors;
    struct shell89_run_io io;
    int rc;
    int a;
    int b;

    *out = NULL;
    *err = NULL;
    *out_len = 0;
    *err_len = 0;
    in = tmpfile();
    output = tmpfile();
    errors = tmpfile();
    if (in == NULL || output == NULL || errors == NULL)
    {
        if (in != NULL)
        {
            fclose(in);
        }
        if (output != NULL)
        {
            fclose(output);
        }
        if (errors != NULL)
        {
            fclose(errors);
        }
        return -1;
    }
    if (input_len != 0)
    {
        fwrite(input, 1, input_len, in);
    }
    fflush(in);
    rewind(in);
    io.stdin_fd = fileno(in);
    io.stdout_fd = fileno(output);
    io.stderr_fd = fileno(errors);
    rc = shell89_run(sh, source, &io, result);
    a = read_file(output, out, out_len);
    b = read_file(errors, err, err_len);
    fclose(in);
    fclose(output);
    fclose(errors);
    if (a != 0 || b != 0)
    {
        free(*out);
        free(*err);
        *out = NULL;
        *err = NULL;
        return -1;
    }
    return rc;
}

void shell89_capture_free(void *p)
{
    free(p);
}
