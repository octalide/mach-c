#include <stdlib.h>

#include "target.h"

#define TARGET_DELIMITER "/"

Target target_from_string(const char *str)
{
    Target target = {.platform = PLATFORM_COUNT, .architecture = ARCH_COUNT};

    if (strcmp(str, "current") == 0)
    {
        return target_current();
    }

    char *str_copy = strdup(str);
    char *saveptr = NULL;
    char *token = strtok_r(str_copy, TARGET_DELIMITER, &saveptr);

    if (token)
    {
        target.platform = platform_from_string(token);
        token = strtok_r(NULL, TARGET_DELIMITER, &saveptr);
    }

    if (token)
    {
        target.architecture = architecture_from_string(token);
    }

    free(str_copy);

    return target;
}

Target target_current()
{
    Target target = {
        .platform = platform_current(),
        .architecture = architecture_current(),
    };

    return target;
}

bool valid_target(Target target)
{
    return target.platform < PLATFORM_COUNT && target.architecture < ARCH_COUNT;
}
