#include <stdio.h>
#include <string.h>

#include "architecture.h"

#if defined(__linux__) || defined(__APPLE__)
#include <sys/utsname.h>
#endif

Architecture architecture_from_string(const char *str)
{
    for (int i = 0; i < ARCH_COUNT; ++i)
    {
        if (strcmp(str, ARCHITECTURE_NAMES[i].name) == 0)
        {
            return ARCHITECTURE_NAMES[i].arch;
        }
    }

    return ARCH_COUNT;
}

Architecture architecture_current()
{
    char *arch_str = NULL;

#if defined(__linux__) || defined(__APPLE__)
    struct utsname unameData;
    uname(&unameData);
    arch_str = unameData.machine;
#elif defined(_WIN32)
#if defined(_M_X64) || defined(_M_AMD64)
    arch_str = "x64";
#elif defined(_M_IX86)
    arch_str = "x86";
#elif defined(_M_ARM64)
    arch_str = "ARM64";
#elif defined(_M_ARM)
    arch_str = "ARM";
#else
    arch_str = "unknown";
#endif
#endif

    if (arch_str)
    {
        return architecture_from_string(arch_str);
    }

    return ARCH_COUNT;
}
