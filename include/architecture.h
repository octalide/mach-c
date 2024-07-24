#ifndef ARCHITECTURE_H
#define ARCHITECTURE_H

typedef enum Architecture
{
    ARCH_AMD_X86,
    ARCH_AMD_X64,
    ARCH_ARM,
    ARCH_ARM_64,

    ARCH_COUNT
} Architecture;

typedef struct architecture_name
{
    Architecture arch;
    const char *name;
} architecture_name;

static const architecture_name ARCHITECTURE_NAMES[] = {
    {ARCH_AMD_X86, "x86"},
    {ARCH_AMD_X64, "x64"},
    {ARCH_ARM, "ARM"},
    {ARCH_ARM_64, "ARM64"},
};

Architecture architecture_from_string(const char *str);
Architecture architecture_current();

#endif // ARCHITECTURE_H
