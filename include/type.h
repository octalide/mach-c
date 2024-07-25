#ifndef TYPE_H
#define TYPE_H

#include <stdbool.h>
#include <stdlib.h>

#include "target.h"

typedef enum TypeKind
{
    TYPE_VOID,
    TYPE_POINTER,

    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64,
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
    TYPE_I64,
    TYPE_F32,
    TYPE_F64,

    TYPE_ARRAY,
    TYPE_FUNCTION,
    TYPE_STRUCT,

    TYPE_REFERENCE,

    TYPE_COUNT
} TypeKind;

typedef struct StructField
{
    struct Type *type;
    char *name;
    int offset;
    struct StructField *next;
} StructField;

typedef struct FunctionParam
{
    struct Type *type;
    char *name;
    struct FunctionParam *next;
} FunctionParam;

typedef struct Type
{
    TypeKind kind;
    size_t size;
    size_t alignment;

    union
    {
        struct
        {
            struct Type *element_type;
        } arr;

        struct
        {
            struct Type *return_type;
            FunctionParam *params;
        } fun;

        struct
        {
            StructField *fields;
        } str;

        struct
        {
            struct Type *base_type;
        } ref;
    } data;
} Type;

size_t align_up(size_t value, size_t alignment);
size_t max(size_t a, size_t b);

Type *type_primitive(TypeKind kind);
Type *type_pointer(Target target);
Type *type_array(Type *element_type, size_t count);
Type *type_function(Type *return_type, FunctionParam *params);
Type *type_struct(StructField *fields);
Type *type_reference(Target target, Type *type);

#endif // TYPE_H
