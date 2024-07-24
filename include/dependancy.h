#ifndef DEPENDANCY_H
#define DEPENDANCY_H

typedef enum DependancyType
{
    DEPENDANCY_TYPE_LOCAL,

    DEPENDANCY_COUNT
} DependancyType;

typedef struct dependancy_name
{
    DependancyType type;
    const char *name;
} dependancy_name;

static const dependancy_name DEPENDANCY_NAMES[] = {
    {DEPENDANCY_TYPE_LOCAL, "local"},
};

typedef struct Dependancy
{
    DependancyType type;
    char *name;
    char *path;
} Dependancy;

DependancyType dependancy_type_from_string(const char *str);
bool dependancy_type_valid(DependancyType type);

#endif // DEPENDANCY_H
