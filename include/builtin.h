#ifndef BUILTIN_H
#define BUILTIN_H

#include "type.h"
#include "target.h"

typedef enum Builtin
{
    BI_VOID,
    BI_PTR,
    BI_NIL,

    BI_U8,
    BI_U16,
    BI_U32,
    BI_U64,
    BI_I8,
    BI_I16,
    BI_I32,
    BI_I64,
    BI_F32,
    BI_F64,

    BI_SIZE_OF,
    BI_ALIGN_OF,
    BI_OFFSET_OF,

    BI_SYS_ARCH,
    BI_SYS_PLAT,

    BI_UNKNOWN
} Builtin;

typedef struct builtin_info_map
{
    Builtin type;
    char *name;
} builtin_info_map;

static const builtin_info_map BUILTIN_INFO_MAP[] = {
    {BI_VOID, "void"},
    {BI_PTR, "ptr"},
    {BI_NIL, "nil"},

    {BI_U8, "u8"},
    {BI_U16, "u16"},
    {BI_U32, "u32"},
    {BI_U64, "u64"},
    {BI_I8, "i8"},
    {BI_I16, "i16"},
    {BI_I32, "i32"},
    {BI_I64, "i64"},
    {BI_F32, "f32"},
    {BI_F64, "f64"},
    
    {BI_SIZE_OF, "size_of"},
    {BI_ALIGN_OF, "align_of"},
    {BI_OFFSET_OF, "offset_of"},

    {BI_SYS_ARCH, "__SYS_ARCH__"},
    {BI_SYS_PLAT, "__SYS_PLAT__"},
};

Builtin builtin_from_string(char *name);
Type *builtin_type(Builtin builtin, Target target);

// NOTE: the only builtins besides types that will be added to Mach will be
//   constant values as well as functions that take a type as an arg and return
//   an integer literal. These are used as the building blocks of the
//   reflection system.
// functions:
// - `size_of(type)` which returns the size of the type in bytes
// - `align_of(type)` which returns the alignment of the type in bytes
// - `offset_of(type)` which returns the offset of the field in bytes
// constants:
// - `__SYS_ARCH__` which returns the target architecture id
// - `__SYS_PLAT__` which returns the target platform id
int builtin_eval(Builtin builtin, Target target, Type *arg);

#endif // BUILTIN_H
