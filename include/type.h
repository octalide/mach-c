#ifndef TYPE_H
#define TYPE_H

#include <stdbool.h>

// forward declarations
typedef struct Type    Type;
typedef struct AstNode AstNode;

typedef enum TypeKind
{
    TYPE_UNKNOWN,
    TYPE_PTR,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_ARRAY,
    TYPE_FUNCTION,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ALIAS,
} TypeKind;

struct Type
{
    TypeKind kind;
    unsigned size;
    unsigned alignment;
    bool     is_signed;

    union
    {
        struct
        {
            Type *base;
        } ptr;

        struct
        {
            Type *elem_type;
            int   size; // -1 for dynamic arrays
        } array;

        struct
        {
            Type **param_types;
            int    param_count;
            Type  *return_type; // NULL for void return
        } function;

        struct
        {
            struct Symbol **fields;
            int             field_count;
            char           *name;
        } composite;

        struct
        {
            Type *target;
            char *name;
        } alias;
    };
};

Type *type_create_builtin(TypeKind kind, unsigned size, unsigned alignment, bool is_signed);
Type *type_create_ptr(Type *base);
Type *type_create_array(Type *elem_type, int size);
Type *type_create_function(Type **param_types, int param_count, Type *return_type);
Type *type_create_struct(const char *name);
Type *type_create_union(const char *name);
Type *type_create_alias(const char *name, Type *target);
void  type_dnit(Type *type);
bool  type_equals(Type *a, Type *b);
bool  type_is_assignable(Type *target, Type *source);
Type *type_get_common_type(Type *a, Type *b);
bool  type_is_numeric(Type *type);
bool  type_is_integer(Type *type);
bool  type_is_pointer_like(Type *type);
bool  type_can_cast(Type *from, Type *to);
Type *type_get_element_type(Type *type);
bool  type_can_accept_literal(Type *target, AstNode *literal_node);

const char *type_to_string(Type *type);

#endif
