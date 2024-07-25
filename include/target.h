#ifndef TARGET_H
#define TARGET_H

#include <stdbool.h>

typedef enum Platform
{
    PLATFORM_WINDOWS,
    PLATFORM_LINUX,
    PLATFORM_MACOS,

    PLATFORM_UNKNOWN
} Platform;

typedef enum Architecture
{
    ARCH_AMD_X86,
    ARCH_AMD_X64,
    ARCH_ARM,
    ARCH_ARM_64,

    ARCH_UNKNOWN
} Architecture;

typedef enum Endian
{
    ENDIAN_LITTLE,
    ENDIAN_BIG,

    ENDIAN_UNKNOWN
} Endian;

typedef struct Target
{
    Platform platform;
    Architecture architecture;
} Target;

typedef struct TargetInfo
{
    Endian endian;
    int size;
    int alignment;
    int register_size;
} TargetInfo;

typedef struct platform_name_map
{
    Platform platform;
    const char *name;
} platform_name_map;

typedef struct architecture_name_map
{
    Architecture arch;
    const char *name;
} architecture_name_map;

typedef struct endian_name_map
{
    Endian endian;
    const char *name;
} endian_name_map;

const platform_name_map PLATFORM_NAMES[] = {
    {PLATFORM_WINDOWS, "windows"},
    {PLATFORM_LINUX, "linux"},
    {PLATFORM_MACOS, "macos"},
};

const architecture_name_map ARCHITECTURE_NAMES[] = {
    {ARCH_AMD_X86, "x86"},
    {ARCH_AMD_X64, "x64"},
    {ARCH_ARM, "arm"},
    {ARCH_ARM_64, "arm64"},
};

const endian_name_map ENDIAN_NAMES[] = {
    {ENDIAN_LITTLE, "little"},
    {ENDIAN_BIG, "big"},
};

bool valid_platform(Platform platform);
bool valid_architecture(Architecture arch);
bool valid_endian(Endian endian);
bool valid_target(Target target);

Platform platform_from_string(const char *str);
Architecture architecture_from_string(const char *str);
Endian endian_from_string(const char *str);
Target target_from_string(const char *str);

char *platform_to_string(Platform platform);
char *architecture_to_string(Architecture arch);
char *endian_to_string(Endian endian);
char *target_to_string(Target target);

Platform platform_current();
Architecture architecture_current();
Target target_current();

TargetInfo target_info(Target target);

#endif // TARGET_H
