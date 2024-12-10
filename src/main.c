#include "context.h"
#include "ioutil.h"
#include "build.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int cmd_build(Context *context, int argc, char **argv)
{
    char *path_target = NULL;
    for (int i = 0; i < argc; i++)
    {
        if (argv[i][0] != '-')
        {
            path_target = argv[i];
            break;
        }
    }

    if (!file_exists(path_target))
    {
        fprintf(stderr, "error: target path does not exist: `%s`\n", path_target);
        return 1;
    }

    if (is_directory(path_target))
    {
        fprintf(stderr, "error: target path cannot be a directory\n");
        return 1;
    }

    // determine whether the target is a `.mach` source file, or a `.json`
    //   project configuration file
    char *ext = path_get_extension(path_target);
    if (strcmp(ext, "mach") == 0)
    {
        printf("building target file: %s\n", path_target);
        return build_target_file(context, path_target);
    }

    if (strcmp(ext, "json") == 0)
    {
        printf("building target project: %s\n", path_target);
        return build_target_project(context, path_target);
    }

    printf("error: target path must be a source or project file\n");
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [options] <command>\n", argv[0]);
        return 1;
    }

    Context *context = context_new();
    context->verbosity = VERBOSITY_LOW;

    // set from the environment variable `MACH_PATH`
    context->path_mach_std = getenv("MACH_PATH");

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'v') {
                context->verbosity = VERBOSITY_NONE;
                for (int j = 1; argv[i][j] == 'v'; j++) {
                    if (context->verbosity < VERBOSITY_HIGH) {
                        context->verbosity++;
                    }
                }
            } else {
                fprintf(stderr, "Unknown option: %s\n", argv[i]);
                context_free(context);
                return 1;
            }
        } else {
            break;
        }
    }

    if (argc > 1) {
        char *first_arg = argv[1];
        if (strcmp(first_arg, "build") == 0) {
            return cmd_build(context, argc - 2, argv + 2);
        } else {
            fprintf(stderr, "Unknown command: %s\n", first_arg);
            context_free(context);
            return 1;
        }
    }

    context_free(context);
    return 0;
}
