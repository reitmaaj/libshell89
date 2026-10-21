#ifndef SHELL89_FIXTURES_H
#define SHELL89_FIXTURES_H

#include <shell89.h>

struct shell89_fixture_context
{
    int counter;
};

void shell89_fixture_register_native(shell89 *sh,
                                     struct shell89_fixture_context *ctx);
void shell89_fixture_register_process(shell89 *sh, const char *path);

#endif
