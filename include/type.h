#ifndef TYPE_H
#define TYPE_H

#include "target.h"

#include <stdlib.h>

typedef enum TypeKind
{
    TYPE_ERROR,

    TYPE_LAZY,

    TYPE_META,
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

typedef struct TypeError
{
    char *message;
} TypeError;

typedef struct TypeMeta
{
    struct Type *base_type;
} TypeMeta;

typedef struct TypePointer
{
    struct Type *base_type;
} TypePointer;

typedef struct TypeArray
{
    struct Type *element_type;
    int length;
} TypeArray;

typedef struct TypeFunction
{
    struct Type **parameters;
    struct Type *return_type;
} TypeFunction;

typedef struct TypeStruct
{
    struct Type **fields;
} TypeStruct;

typedef struct TypeUnion
{
    struct Type **fields;
} TypeUnion;

typedef struct Type
{
    TypeKind kind;

    char *name;

    union
    {
        struct TypeError *type_error;
        struct TypeLazy *type_lazy;
        struct TypeMeta *type_meta;
        struct TypePointer *type_pointer;
        struct TypeArray *type_array;
        struct TypeFunction *type_function;
        struct TypeStruct *type_struct;
        struct TypeUnion *type_union;
    } data;
} Type;

size_t align_up(size_t value, size_t alignment);
size_t max(size_t a, size_t b);

Type *type_new(TypeKind kind);
void type_free(Type *type);

char *type_to_string(Type *type);

void type_fun_add_param(Type *type, Type *param);
int type_fun_param_count(Type *type);
Type *type_fun_param_find(Type *type, char *name);

void type_str_add_field(Type *type, Type *field);
int type_str_field_count(Type *type);
Type *type_str_field_find(Type *type, char *name);

void type_uni_add_field(Type *type, Type *field);
int type_uni_field_count(Type *type);
Type *type_uni_field_find(Type *type, char *name);

bool type_array_is_unbound(Type *type);

bool type_equals(Target target, Type *a, Type *b);
int type_sizeof(Target target, Type *type);
int type_alignof(Target target, Type *type);
int type_offsetof(Target target, Type *type, char *name);

#endif
