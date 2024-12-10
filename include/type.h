#ifndef TYPE_H
#define TYPE_H

#include "ast.h"

typedef enum TypeKind
{
    TYPE_ERROR,

    TYPE_VOID,

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

    TYPE_POINTER,

    TYPE_ARRAY,
    TYPE_FUNCTION,
    TYPE_STRUCT,
    TYPE_UNION,
} TypeKind;

typedef struct Type
{
    TypeKind kind;
    struct Type *base;
} Type;

#endif
