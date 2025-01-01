#ifndef TYPE_H
#define TYPE_H

#include "ast.h"
#include "target.h"

typedef enum TypeKind
{
    TYPE_ERROR,

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

typedef struct Type
{
    TypeKind kind;

    char *name;

    union
    {
        struct err
        {
            char *message;
        } err;

        struct ptr
        {
            struct Type *base_type;
        } ptr;

        struct arr
        {
            struct Type *element_type;
            int length;
        } arr;

        struct fun
        {
            struct Type **parameters;
            struct Type *return_type;
        } fun;

        struct str
        {
            struct Type **fields;
        } str;

        struct uni
        {
            struct Type **fields;
        } uni;
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

bool type_equals(Target target, Type *a, Type *b);
int type_sizeof(Target target, Type *type);
int type_alignof(Target target, Type *type);
int type_offsetof(Target target, Type *type, char *name);

#endif
