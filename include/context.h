#ifndef CONTEXT_H
#define CONTEXT_H

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

    char *path_mach_std;
} Context;

Context *context_new();
void context_free(Context *context);

#endif
