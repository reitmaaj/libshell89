#ifndef SHELL89_TEST_SUPPORT_H
#define SHELL89_TEST_SUPPORT_H

#include <stddef.h>
#include <stdio.h>

#include <shell89.h>

extern int shell89_test_failures;

void shell89_test_fail(const char *file, int line, const char *message);
int shell89_test_report(const char *name);

#define CHECK(cond)                                                            \
    do                                                                         \
    {                                                                          \
        if (!(cond))                                                           \
        {                                                                      \
            shell89_test_fail(__FILE__, __LINE__, #cond);                      \
        }                                                                      \
    } while (0)

#define CHECK_EQ(got, want)                                                    \
    do                                                                         \
    {                                                                          \
        long g_ = (long)(got);                                                 \
        long w_ = (long)(want);                                                \
        if (g_ != w_)                                                          \
        {                                                                      \
            char msg_[128];                                                    \
            sprintf(msg_, "expected %ld, got %ld", w_, g_);                    \
            shell89_test_fail(__FILE__, __LINE__, msg_);                       \
        }                                                                      \
    } while (0)

#define CHECK_STR(got, want)                                                   \
    do                                                                         \
    {                                                                          \
        const char *g_ = (got);                                                \
        const char *w_ = (want);                                               \
        if (g_ == NULL || w_ == NULL || strcmp(g_, w_) != 0)                   \
        {                                                                      \
            shell89_test_fail(__FILE__, __LINE__,                              \
                              (g_ == NULL) ? "got NULL string"                 \
                                           : "strings differ");                \
        }                                                                      \
    } while (0)

#endif
