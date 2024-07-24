#include <string.h>

#include "platform.h"

Platform platform_from_string(const char *str)
{
    for (int i = 0; i < PLATFORM_COUNT; ++i)
    {
        if (strcmp(str, PLATFORM_NAMES[i].name) == 0)
        {
            return PLATFORM_NAMES[i].platform;
        }
    }

    return PLATFORM_COUNT;
}

Platform platform_current()
{
#if defined(_WIN32) || defined(_WIN64)
    const char *platform_str = "windows";
#elif defined(__linux__)
    const char *platform_str = "linux";
#elif defined(__APPLE__) && defined(__MACH__)
    const char *platform_str = "macos";
#else
    const char *platform_str = "unknown";
#endif

    return platform_from_string(platform_str);
}
