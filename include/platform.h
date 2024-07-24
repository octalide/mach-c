#ifndef PLATFORM_H
#define PLATFORM_H

typedef enum Platform
{
    PLATFORM_WINDOWS,
    PLATFORM_LINUX,
    PLATFORM_MACOS,

    PLATFORM_COUNT
} Platform;

typedef struct platform_name
{
    Platform platform;
    const char *name;
} platform_name;

static const platform_name PLATFORM_NAMES[] = {
    {PLATFORM_WINDOWS, "windows"},
    {PLATFORM_LINUX, "linux"},
    {PLATFORM_MACOS, "macos"},
};

Platform platform_from_string(const char *str);
Platform platform_current();

#endif // PLATFORM_H
