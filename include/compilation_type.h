#ifndef COMPILATION_TYPE_H
#define COMPILATION_TYPE_H

typedef enum CompilationType
{
    COMPILATION_TYPE_EXE,
    COMPILATION_TYPE_LIB,
} CompilationType;

typedef struct compilation_type_name
{
    CompilationType type;
    char *name;
} compilation_type_name;

static const compilation_type_name COMPILATION_TYPE_NAMES[] = {
    {COMPILATION_TYPE_EXE, "executable"},
    {COMPILATION_TYPE_LIB, "library"},
};

#endif // COMPILATION_TYPE_H
