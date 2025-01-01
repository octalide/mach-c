#include "ioutil.h"
#include "build.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int cmd_build(int argc, char **argv)
{
    char *path_target = NULL;
    for (int i = 0; i < argc; i++)
    {
        if (argv[i][0] != '-')
        {
            path_target = argv[i];

            // remove this arg after selection
            for (int j = i; j < argc - 1; j++)
            {
                argv[j] = argv[j + 1];
            }

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
        return build_target_file(path_target, argc, argv);
    }

    if (strcmp(ext, "json") == 0)
    {
        printf("building target project: %s\n", path_target);
        return build_target_project(path_target, argc, argv);
    }

    printf("error: target path must be a source or project file\n");

    return 1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [options]\n", argv[0]);
        return 1;
    }

    if (argc > 1) {
        char *first_arg = argv[1];
        if (strcmp(first_arg, "build") == 0) {
            return cmd_build(argc - 2, argv + 2);
        } else {
            fprintf(stderr, "Unknown command: %s\n", first_arg);
            return 1;
        }
    }

    return 0;
}
