#include "context.h"

#include <stdlib.h>

Context *context_new()
{
    Context *context = calloc(1, sizeof(Context));
    context->verbosity = VERBOSITY_LOW;

    return context;
}

void context_free(Context *context)
{
    free(context);
}
