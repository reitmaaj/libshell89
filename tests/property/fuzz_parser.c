#include <stdlib.h>
#include <string.h>

#include "shell89_priv.h"

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size)
{
    struct shell89_priv_script script;
    struct shell89_priv_diag diag;
    char *source;

    source = (char *)malloc(size + 1);
    if (source == NULL)
    {
        return 0;
    }
    if (size != 0)
    {
        memcpy(source, data, size);
    }
    source[size] = '\0';
    (void)shell89_priv_parse(source, &script, &diag);
    shell89_priv_script_free(&script);
    free(source);
    return 0;
}
