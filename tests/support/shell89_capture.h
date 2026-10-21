#ifndef SHELL89_CAPTURE_H
#define SHELL89_CAPTURE_H

#include <stddef.h>

#include <shell89.h>

int shell89_capture_run(shell89 *sh, const char *source, const void *input,
                        size_t input_len, unsigned char **out, size_t *out_len,
                        unsigned char **err, size_t *err_len,
                        shell89_result **result);
void shell89_capture_free(void *p);

#endif
