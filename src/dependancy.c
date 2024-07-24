#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "dependancy.h"

DependancyType dependancy_type_from_string(const char *str)
{
    for (size_t i = 0; i < DEPENDANCY_COUNT; i++)
    {
        if (strcmp(DEPENDANCY_NAMES[i].name, str) == 0)
        {
            return DEPENDANCY_NAMES[i].type;
        }
    }

    return DEPENDANCY_COUNT;
}

bool dependancy_type_valid(DependancyType type)
{
    return type < DEPENDANCY_COUNT;
}
