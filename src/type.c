#include "type.h"
#include "ast.h"
#include "symbol.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// builtin types storage
static Type *g_builtin_types[TYPE_PTR + 1] = {0};

// type info table for builtin types
static struct
{
    TypeKind    kind;
    size_t      size;
    size_t      alignment;
    const char *name;
} type_info_table[] = {
    {TYPE_U8, 1, 1, "u8"},
    {TYPE_U16, 2, 2, "u16"},
    {TYPE_U32, 4, 4, "u32"},
    {TYPE_U64, 8, 8, "u64"},
    {TYPE_I8, 1, 1, "i8"},
    {TYPE_I16, 2, 2, "i16"},
    {TYPE_I32, 4, 4, "i32"},
    {TYPE_I64, 8, 8, "i64"},
    {TYPE_F16, 2, 2, "f16"},
    {TYPE_F32, 4, 4, "f32"},
    {TYPE_F64, 8, 8, "f64"},
    {TYPE_PTR, 8, 8, "ptr"},
};

void type_system_init(void)
{
    // create builtin types
    for (size_t i = 0; i < sizeof(type_info_table) / sizeof(type_info_table[0]); i++)
    {
        Type *type      = malloc(sizeof(Type));
        type->kind      = type_info_table[i].kind;
        type->size      = type_info_table[i].size;
        type->alignment = type_info_table[i].alignment;
        type->name      = strdup(type_info_table[i].name);

        g_builtin_types[type->kind] = type;
    }
}

void type_system_dnit(void)
{
    for (size_t i = 0; i < sizeof(g_builtin_types) / sizeof(g_builtin_types[0]); i++)
    {
        if (g_builtin_types[i])
        {
            free(g_builtin_types[i]->name);
            free(g_builtin_types[i]);
            g_builtin_types[i] = NULL;
        }
    }
}

// builtin type accessors
Type *type_u8(void)
{
    return g_builtin_types[TYPE_U8];
}
Type *type_u16(void)
{
    return g_builtin_types[TYPE_U16];
}
Type *type_u32(void)
{
    return g_builtin_types[TYPE_U32];
}
Type *type_u64(void)
{
    return g_builtin_types[TYPE_U64];
}
Type *type_i8(void)
{
    return g_builtin_types[TYPE_I8];
}
Type *type_i16(void)
{
    return g_builtin_types[TYPE_I16];
}
Type *type_i32(void)
{
    return g_builtin_types[TYPE_I32];
}
Type *type_i64(void)
{
    return g_builtin_types[TYPE_I64];
}
Type *type_f16(void)
{
    return g_builtin_types[TYPE_F16];
}
Type *type_f32(void)
{
    return g_builtin_types[TYPE_F32];
}
Type *type_f64(void)
{
    return g_builtin_types[TYPE_F64];
}
Type *type_ptr(void)
{
    return g_builtin_types[TYPE_PTR];
}

Type *type_pointer_create(Type *base)
{
    Type *type         = malloc(sizeof(Type));
    type->kind         = TYPE_POINTER;
    type->size         = 8; // 64-bit pointers
    type->alignment    = 8;
    type->name         = NULL;
    type->pointer.base = base;
    return type;
}

Type *type_array_create(Type *elem_type, size_t length, bool is_unbound)
{
    Type *type             = malloc(sizeof(Type));
    type->kind             = TYPE_ARRAY;
    type->size             = is_unbound ? 8 : (elem_type->size * length); // unbound arrays are pointers
    type->alignment        = is_unbound ? 8 : elem_type->alignment;
    type->name             = NULL;
    type->array.elem_type  = elem_type;
    type->array.length     = length;
    type->array.is_unbound = is_unbound;
    return type;
}

Type *type_struct_create(const char *name)
{
    Type *type                  = malloc(sizeof(Type));
    type->kind                  = TYPE_STRUCT;
    type->size                  = 0; // calculated later
    type->alignment             = 1; // calculated later
    type->name                  = name ? strdup(name) : NULL;
    type->composite.fields      = NULL;
    type->composite.field_count = 0;
    return type;
}

Type *type_union_create(const char *name)
{
    Type *type                  = malloc(sizeof(Type));
    type->kind                  = TYPE_UNION;
    type->size                  = 0; // calculated later
    type->alignment             = 1; // calculated later
    type->name                  = name ? strdup(name) : NULL;
    type->composite.fields      = NULL;
    type->composite.field_count = 0;
    return type;
}

Type *type_function_create(Type *return_type, Type **param_types, size_t param_count)
{
    Type *type                 = malloc(sizeof(Type));
    type->kind                 = TYPE_FUNCTION;
    type->size                 = 8; // function pointers are 8 bytes
    type->alignment            = 8;
    type->name                 = NULL;
    type->function.return_type = return_type;
    type->function.param_count = param_count;

    if (param_count > 0)
    {
        type->function.param_types = malloc(sizeof(Type *) * param_count);
        memcpy(type->function.param_types, param_types, sizeof(Type *) * param_count);
    }
    else
    {
        type->function.param_types = NULL;
    }

    return type;
}

Type *type_alias_create(const char *name, Type *target)
{
    Type *type         = malloc(sizeof(Type));
    type->kind         = TYPE_ALIAS;
    type->size         = target->size;
    type->alignment    = target->alignment;
    type->name         = strdup(name);
    type->alias.target = target;
    return type;
}

bool type_equals(Type *a, Type *b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;

    // resolve aliases
    while (a->kind == TYPE_ALIAS)
        a = a->alias.target;
    while (b->kind == TYPE_ALIAS)
        b = b->alias.target;

    if (a->kind != b->kind)
        return false;

    switch (a->kind)
    {
    case TYPE_POINTER:
        return type_equals(a->pointer.base, b->pointer.base);

    case TYPE_ARRAY:
        return a->array.length == b->array.length && a->array.is_unbound == b->array.is_unbound && type_equals(a->array.elem_type, b->array.elem_type);

    case TYPE_FUNCTION:
        if (a->function.param_count != b->function.param_count)
            return false;
        if (!type_equals(a->function.return_type, b->function.return_type))
            return false;

        for (size_t i = 0; i < a->function.param_count; i++)
        {
            if (!type_equals(a->function.param_types[i], b->function.param_types[i]))
            {
                return false;
            }
        }
        return true;

    case TYPE_STRUCT:
    case TYPE_UNION:
        // for named types, compare by name
        if (a->name && b->name)
        {
            return strcmp(a->name, b->name) == 0;
        }
        // for anonymous types, they're only equal if they're the same object
        return a == b;

    default:
        return true; // builtin types with same kind are equal
    }
}

bool type_is_numeric(Type *type)
{
    while (type->kind == TYPE_ALIAS)
        type = type->alias.target;

    return type_is_integer(type) || type_is_float(type);
}

bool type_is_integer(Type *type)
{
    while (type->kind == TYPE_ALIAS)
        type = type->alias.target;

    return type->kind >= TYPE_U8 && type->kind <= TYPE_I64;
}

bool type_is_float(Type *type)
{
    while (type->kind == TYPE_ALIAS)
        type = type->alias.target;

    return type->kind >= TYPE_F16 && type->kind <= TYPE_F64;
}

bool type_is_signed(Type *type)
{
    while (type->kind == TYPE_ALIAS)
        type = type->alias.target;

    return (type->kind >= TYPE_I8 && type->kind <= TYPE_I64) || (type->kind >= TYPE_F16 && type->kind <= TYPE_F64);
}

bool type_is_pointer_like(Type *type)
{
    while (type->kind == TYPE_ALIAS)
        type = type->alias.target;

    return type->kind == TYPE_PTR || type->kind == TYPE_POINTER || type->kind == TYPE_FUNCTION || (type->kind == TYPE_ARRAY && type->array.is_unbound);
}

bool type_is_truthy(Type *type)
{
    while (type->kind == TYPE_ALIAS)
        type = type->alias.target;

    // in Mach, any numeric type or pointer-like type can be used in boolean context
    return type_is_numeric(type) || type_is_pointer_like(type);
}

bool type_can_cast_to(Type *from, Type *to)
{
    while (from->kind == TYPE_ALIAS)
        from = from->alias.target;
    while (to->kind == TYPE_ALIAS)
        to = to->alias.target;

    // same type
    if (type_equals(from, to))
        return true;

    // same size casting (reinterpretation)
    if (from->size == to->size)
        return true;

    // numeric conversions
    if (type_is_numeric(from) && type_is_numeric(to))
        return true;

    // pointer conversions
    if (type_is_pointer_like(from) && type_is_pointer_like(to))
        return true;

    // integer to pointer conversions (common in C)
    if (type_is_integer(from) && type_is_pointer_like(to))
        return true;

    // pointer to integer conversions
    if (type_is_pointer_like(from) && type_is_integer(to))
        return true;

    // array to pointer decay
    if (from->kind == TYPE_ARRAY && to->kind == TYPE_POINTER)
    {
        return type_equals(from->array.elem_type, to->pointer.base);
    }

    return false;
}

bool type_can_assign_to(Type *from, Type *to)
{
    while (from->kind == TYPE_ALIAS)
        from = from->alias.target;
    while (to->kind == TYPE_ALIAS)
        to = to->alias.target;

    // same type
    if (type_equals(from, to))
        return true;

    // numeric conversions (more restrictive than casting)
    if (type_is_numeric(from) && type_is_numeric(to))
        return true;

    // pointer conversions (same pointer types)
    if (type_is_pointer_like(from) && type_is_pointer_like(to))
        return true;

    // array to pointer decay
    if (from->kind == TYPE_ARRAY && to->kind == TYPE_POINTER)
    {
        return type_equals(from->array.elem_type, to->pointer.base);
    }

    // NO implicit pointer ↔ integer conversions for assignments
    return false;
}

size_t type_sizeof(Type *type)
{
    return type->size;
}

size_t type_alignof(Type *type)
{
    return type->alignment;
}

// simple constant expression evaluation for array sizes
static long long evaluate_const_expr(AstNode *expr)
{
    if (!expr)
        return -1;

    switch (expr->kind)
    {
    case AST_EXPR_LIT:
        if (expr->lit_expr.kind == TOKEN_LIT_INT)
        {
            return (long long)expr->lit_expr.int_val;
        }
        break;

    case AST_EXPR_BINARY:
    {
        long long left  = evaluate_const_expr(expr->binary_expr.left);
        long long right = evaluate_const_expr(expr->binary_expr.right);
        if (left < 0 || right < 0)
            return -1;

        switch (expr->binary_expr.op)
        {
        case TOKEN_PLUS:
            return left + right;
        case TOKEN_MINUS:
            return left - right;
        case TOKEN_STAR:
            return left * right;
        case TOKEN_SLASH:
            return right != 0 ? left / right : -1;
        default:
            return -1;
        }
    }

    case AST_EXPR_UNARY:
    {
        long long operand = evaluate_const_expr(expr->unary_expr.expr);
        if (operand < 0)
            return -1;

        switch (expr->unary_expr.op)
        {
        case TOKEN_PLUS:
            return operand;
        case TOKEN_MINUS:
            return -operand;
        default:
            return -1;
        }
    }

    default:
        return -1; // unsupported expression for constant evaluation
    }

    return -1;
}

Type *type_resolve(AstNode *type_node, SymbolTable *symbol_table)
{
    if (!type_node)
        return NULL;

    switch (type_node->kind)
    {
    case AST_TYPE_NAME:
    {
        const char *name = type_node->type_name.name;

        // check builtin types
        for (size_t i = 0; i < sizeof(type_info_table) / sizeof(type_info_table[0]); i++)
        {
            if (strcmp(name, type_info_table[i].name) == 0)
            {
                return g_builtin_types[type_info_table[i].kind];
            }
        }

        // look up user-defined types in symbol table
        if (symbol_table)
        {
            Symbol *symbol = symbol_lookup(symbol_table, name);
            if (symbol && symbol->kind == SYMBOL_TYPE)
            {
                return symbol->type;
            }
        }

        return NULL;
    }

    case AST_TYPE_PTR:
    {
        Type *base = type_resolve(type_node->type_ptr.base, symbol_table);
        if (!base)
            return NULL;
        return type_pointer_create(base);
    }

    case AST_TYPE_ARRAY:
    {
        Type *elem_type = type_resolve(type_node->type_array.elem_type, symbol_table);
        if (!elem_type)
            return NULL;

        if (!type_node->type_array.size)
        {
            // unbound array
            return type_array_create(elem_type, 0, true);
        }
        else
        {
            // evaluate array size expression
            long long size = evaluate_const_expr(type_node->type_array.size);
            if (size <= 0)
            {
                return NULL; // invalid array size
            }
            return type_array_create(elem_type, (size_t)size, false);
        }
    }

    case AST_TYPE_FUN:
    {
        Type *return_type = type_node->type_fun.return_type ? type_resolve(type_node->type_fun.return_type, symbol_table) : NULL;

        size_t param_count = type_node->type_fun.params ? type_node->type_fun.params->count : 0;
        Type **param_types = NULL;

        if (param_count > 0)
        {
            param_types = malloc(sizeof(Type *) * param_count);
            for (size_t i = 0; i < param_count; i++)
            {
                AstNode *param = type_node->type_fun.params->items[i];
                // for function types, params are direct type nodes, not param statements
                param_types[i] = type_resolve(param, symbol_table);
                if (!param_types[i])
                {
                    free(param_types);
                    return NULL;
                }
            }
        }

        Type *func_type = type_function_create(return_type, param_types, param_count);
        free(param_types); // function_create makes its own copy
        return func_type;
    }

    default:
        return NULL;
    }
}

Type *type_resolve_alias(Type *type)
{
    if (!type)
        return NULL;

    // resolve alias chain to final type
    while (type->kind == TYPE_ALIAS)
    {
        type = type->alias.target;
        if (!type)
            return NULL;
    }

    return type;
}

void type_print(Type *type)
{
    if (!type)
    {
        printf("(null)");
        return;
    }

    switch (type->kind)
    {
    case TYPE_U8:
    case TYPE_U16:
    case TYPE_U32:
    case TYPE_U64:
    case TYPE_I8:
    case TYPE_I16:
    case TYPE_I32:
    case TYPE_I64:
    case TYPE_F16:
    case TYPE_F32:
    case TYPE_F64:
    case TYPE_PTR:
        printf("%s", type->name);
        break;

    case TYPE_POINTER:
        printf("*");
        type_print(type->pointer.base);
        break;

    case TYPE_ARRAY:
        if (type->array.is_unbound)
        {
            printf("[]");
        }
        else
        {
            printf("[%zu]", type->array.length);
        }
        type_print(type->array.elem_type);
        break;

    case TYPE_STRUCT:
        printf("str %s", type->name ? type->name : "(anonymous)");
        break;

    case TYPE_UNION:
        printf("uni %s", type->name ? type->name : "(anonymous)");
        break;

    case TYPE_FUNCTION:
        printf("fun(");
        for (size_t i = 0; i < type->function.param_count; i++)
        {
            if (i > 0)
                printf(", ");
            type_print(type->function.param_types[i]);
        }
        printf(") ");
        if (type->function.return_type)
        {
            type_print(type->function.return_type);
        }
        break;

    case TYPE_ALIAS:
        printf("%s", type->name);
        break;
    }
}
