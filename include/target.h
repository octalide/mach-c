#ifndef TARGET_H
#define TARGET_H

#include <stdbool.h>

#include "platform.h"
#include "architecture.h"

typedef struct Target
{
    Platform platform;
    Architecture architecture;
} Target;

Target target_from_string(const char *str);
Target target_current();

bool valid_target(Target target);

#endif // TARGET_H
