#ifndef TYPE_H
#define TYPE_H

#include <stdbool.h>
#include <stdlib.h>

#include "target.h"

struct Type;
struct Symbol;

typedef struct Symbol
{
    char *name;
    struct Type *type;
} Symbol;

typedef enum TypeKind
{
    TYPE_VOID,
    TYPE_META,
    
    TYPE_INT_U,
    TYPE_INT_S,
    TYPE_FLOAT,

    TYPE_ARRAY,
    TYPE_FUNCTION,
    TYPE_STRUCT,
    TYPE_UNION,

    TYPE_POINTER,

    TYPE_COUNT
} TypeKind;

typedef struct Type
{
    TypeKind kind;

    size_t size;
    size_t alignment;
    size_t offset;

    union data
    {
        struct ptr
        {
            struct Type *base_type;
        } ptr;

        struct arr
        {
            struct Type *element_type;
            int count;
        } arr;

        struct fun
        {
            Symbol **params;
            struct Type *return_type;
        } fun;

        struct str
        {
            Symbol **fields;
        } str;

        struct uni
        {
            Symbol **fields;
        } uni;
    } data;
} Type;

size_t align_up(size_t value, size_t alignment);
size_t max(size_t a, size_t b);

void symbol_init(Symbol *symbol);
void symbol_free(Symbol *symbol);

void type_init(Type *type, Target target);
void type_free(Type *type);

bool type_equals(Type *a, Type *b);

char *type_to_string(Type *type);

void type_fun_add_param(Type *type, Symbol *param);
int type_fun_param_count(Type *type);
Symbol *type_fun_param_find(Type *type, char *name);

void type_str_add_field(Type *type, Symbol *field);
int type_str_field_count(Type *type);
Symbol *type_str_field_find(Type *type, char *name);

void type_uni_add_field(Type *type, Symbol *field);
int type_uni_field_count(Type *type);
Symbol *type_uni_field_find(Type *type, char *name);

#endif // TYPE_H
