#ifndef CONTEXT_H
#define CONTEXT_H

struct Context;

typedef enum Verbosity
{
    VERBOSITY_NONE,
    VERBOSITY_LOW,
    VERBOSITY_MEDIUM,
    VERBOSITY_HIGH
} Verbosity;

typedef struct Context
{
    Verbosity verbosity;
} Context;

Context *context_new();
void context_free(Context *context);

#endif
