#define DEBUG_PARSER_PRINT

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

#include "compiler.h"

// define default mach installation path by platform:
// - windows: C:\Program Files\mach
// - linux: /usr/local/mach
// - macos: /usr/local/mach
// - other: /usr/local/mach
#if defined(_WIN32)
#define DEFAULT_MACH_PATH "C:\\Program Files\\mach"
#elif defined(__linux__) || defined(__APPLE__)
#define DEFAULT_MACH_PATH "/usr/local/mach"
#else
#define DEFAULT_MACH_PATH "/usr/local/mach"
#endif

int main(int argc, char *argv[])
{
    const char *env_mach_path = getenv("MACH_PATH");

    Options *options = malloc(sizeof(Options));
    options->mach_installation = env_mach_path ? env_mach_path : DEFAULT_MACH_PATH;

    options_default(options);
    options_from_args(options, argc, argv);

    Compiler *compiler = malloc(sizeof(Compiler));
    compiler_init(compiler, options);

    compiler_compile(compiler);
    if (compiler_has_error(compiler))
    {
        printf("error: %s\n", compiler->error);
    }
    
    return 0;
}
