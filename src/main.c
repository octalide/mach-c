#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

#include "compiler.h"

int main(int argc, char *argv[])
{

    Options *options = options_new();
    options_from_args(options, argc, argv);

    Compiler *compiler = compiler_new(options);
    compiler_compile(compiler);
    compiler_free(compiler);

    return 0;
}
