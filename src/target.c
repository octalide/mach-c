#include <stdlib.h>

#include "target.h"

#define TARGET_DELIMITER "/"

#if defined(__linux__) || defined(__APPLE__)
#include <sys/utsname.h>
#endif

bool valid_platform(Platform platform)
{
    return platform < PLATFORM_UNKNOWN;
}

bool valid_architecture(Architecture arch)
{
    return arch < ARCH_UNKNOWN;
}

bool valid_endian(Endian endian)
{
    return endian < ENDIAN_UNKNOWN;
}

bool valid_target(Target target)
{
    return valid_platform(target.platform) && valid_architecture(target.architecture);
}

Platform platform_from_string(const char *str)
{
    for (int i = 0; i < PLATFORM_UNKNOWN; i++)
    {
        if (strcmp(str, PLATFORM_NAMES[i].name) == 0)
        {
            return PLATFORM_NAMES[i].platform;
        }
    }

    return PLATFORM_UNKNOWN;
}

Architecture architecture_from_string(const char *str)
{
    for (int i = 0; i < ARCH_UNKNOWN; i++)
    {
        if (strcmp(str, ARCHITECTURE_NAMES[i].name) == 0)
        {
            return ARCHITECTURE_NAMES[i].arch;
        }
    }

    return ARCH_UNKNOWN;
}

Endian endian_from_string(const char *str)
{
    for (int i = 0; i < ENDIAN_UNKNOWN; i++)
    {
        if (strcmp(str, ENDIAN_NAMES[i].name) == 0)
        {
            return ENDIAN_NAMES[i].endian;
        }
    }

    return ENDIAN_UNKNOWN;
}

Target target_from_string(const char *str)
{
    char *str_copy = strdup(str);
    char *platform_str = strtok(str_copy, TARGET_DELIMITER);
    char *arch_str = strtok(NULL, TARGET_DELIMITER);

    Target target = {
        .platform = platform_from_string(platform_str),
        .architecture = architecture_from_string(arch_str),
    };

    free(str_copy);

    return target;
}

char *platform_to_string(Platform platform)
{
    for (int i = 0; i < PLATFORM_UNKNOWN; i++)
    {
        if (platform == PLATFORM_NAMES[i].platform)
        {
            return PLATFORM_NAMES[i].name;
        }
    }

    return "unknown";
}

char *architecture_to_string(Architecture arch)
{
    for (int i = 0; i < ARCH_UNKNOWN; i++)
    {
        if (arch == ARCHITECTURE_NAMES[i].arch)
        {
            return ARCHITECTURE_NAMES[i].name;
        }
    }

    return "unknown";
}

char *endian_to_string(Endian endian)
{
    for (int i = 0; i < ENDIAN_UNKNOWN; i++)
    {
        if (endian == ENDIAN_NAMES[i].endian)
        {
            return ENDIAN_NAMES[i].name;
        }
    }

    return "unknown";
}

char *target_to_string(Target target)
{
    char *platform_str = platform_to_string(target.platform);
    char *arch_str = architecture_to_string(target.architecture);

    char *str = malloc(strlen(platform_str) + strlen(arch_str) + 2);
    sprintf(str, "%s/%s", platform_str, arch_str);

    return str;
}

TargetInfo target_info(Target target)
{
    TargetInfo info = {0};

    switch (target.platform)
    {
    case PLATFORM_WINDOWS:
        info.endian = ENDIAN_LITTLE;
        break;
    case PLATFORM_LINUX:
        info.endian = ENDIAN_LITTLE;
        break;
    case PLATFORM_MACOS:
        info.endian = ENDIAN_BIG;
        break;
    default:
        info.endian = ENDIAN_UNKNOWN;
        break;
    }

    switch (target.architecture)
    {
    case ARCH_AMD_X86:
        info.size = 4;
        info.alignment = 4;
        info.register_size = 4;
        break;
    case ARCH_AMD_X64:
        info.size = 8;
        info.alignment = 8;
        info.register_size = 8;
        break;
    case ARCH_ARM:
        info.size = 4;
        info.alignment = 4;
        info.register_size = 4;
        break;
    case ARCH_ARM_64:
        info.size = 8;
        info.alignment = 8;
        info.register_size = 8;
        break;
    default:
        info.size = 0;
        info.alignment = 0;
        info.register_size = 0;
        break;
    }

    return info;
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
    arch_str = "arm64";
#elif defined(_M_ARM)
    arch_str = "arm";
#else
    arch_str = "unknown";
#endif
#endif

    if (arch_str)
    {
        return architecture_from_string(arch_str);
    }

    return ARCH_UNKNOWN;
}

Target target_current()
{
    Target target = {
        .platform = platform_current(),
        .architecture = architecture_current(),
    };

    return target;
}
