#include "type.h"
#include "symbol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Type *type_create_builtin(TypeKind kind, unsigned size, unsigned alignment, bool is_signed)
{
    Type *type      = malloc(sizeof(Type));
    type->kind      = kind;
    type->size      = size;
    type->alignment = alignment;
    type->is_signed = is_signed;
    return type;
}

Type *type_create_ptr(Type *base)
{
    Type *type      = malloc(sizeof(Type));
    type->kind      = TYPE_PTR;
    type->size      = 8;
    type->alignment = 8;
    type->is_signed = false;
    type->ptr.base  = base;
    return type;
}

Type *type_create_array(Type *elem_type, int size)
{
    Type *type            = malloc(sizeof(Type));
    type->kind            = TYPE_ARRAY;
    type->size            = (size > 0) ? size * elem_type->size : 0;
    type->alignment       = elem_type->alignment;
    type->is_signed       = false;
    type->array.elem_type = elem_type;
    type->array.size      = size;
    return type;
}

Type *type_create_function(Type **param_types, int param_count, Type *return_type)
{
    Type *type                 = malloc(sizeof(Type));
    type->kind                 = TYPE_FUNCTION;
    type->size                 = 8;
    type->alignment            = 8;
    type->is_signed            = false;
    type->function.param_types = param_types;
    type->function.param_count = param_count;
    type->function.return_type = return_type;
    return type;
}

Type *type_create_struct(const char *name)
{
    Type *type                  = malloc(sizeof(Type));
    type->kind                  = TYPE_STRUCT;
    type->size                  = 0;
    type->alignment             = 1;
    type->is_signed             = false;
    type->composite.fields      = NULL;
    type->composite.field_count = 0;
    type->composite.name        = name ? strdup(name) : NULL;
    return type;
}

Type *type_create_union(const char *name)
{
    Type *type                  = malloc(sizeof(Type));
    type->kind                  = TYPE_UNION;
    type->size                  = 0;
    type->alignment             = 1;
    type->is_signed             = false;
    type->composite.fields      = NULL;
    type->composite.field_count = 0;
    type->composite.name        = name ? strdup(name) : NULL;
    return type;
}

Type *type_create_alias(const char *name, Type *target)
{
    Type *type         = malloc(sizeof(Type));
    type->kind         = TYPE_ALIAS;
    type->size         = target->size;
    type->alignment    = target->alignment;
    type->is_signed    = target->is_signed;
    type->alias.name   = strdup(name);
    type->alias.target = target;
    return type;
}

void type_dnit(Type *type)
{
    if (!type)
        return;
    switch (type->kind)
    {
    case TYPE_ARRAY:
        break;
    case TYPE_FUNCTION:
        free(type->function.param_types);
        break;
    case TYPE_STRUCT:
    case TYPE_UNION:
        free(type->composite.fields);
        free(type->composite.name);
        break;
    case TYPE_ALIAS:
        free(type->alias.name);
        break;
    default:
        break;
    }
}

bool type_equals(Type *a, Type *b)
{
    if (!a || !b)
    {
        return false;
    }

    while (a && a->kind == TYPE_ALIAS)
    {
        a = a->alias.target;
    }
    while (b && b->kind == TYPE_ALIAS)
    {
        b = b->alias.target;
    }

    if (a->kind != b->kind)
    {
        return false;
    }

    switch (a->kind)
    {
    case TYPE_PTR:
        return type_equals(a->ptr.base, b->ptr.base);
    case TYPE_ARRAY:
        return a->array.size == b->array.size && type_equals(a->array.elem_type, b->array.elem_type);
    case TYPE_FUNCTION:
        return true;
    case TYPE_ALIAS:
        return type_equals(a->alias.target, b->alias.target);
    default:
        return true;
    }
}

bool type_is_assignable(Type *target, Type *source)
{
    if (type_equals(target, source))
        return true;
    if (target->kind == TYPE_INT && source->kind == TYPE_INT)
        return true;
    if (target->kind == TYPE_FLOAT && source->kind == TYPE_FLOAT)
        return true;
    if (target->kind == TYPE_PTR && source->kind == TYPE_PTR)
        return true;

    // array compatibility: unbound arrays can accept sized arrays of same element type
    if (target->kind == TYPE_ARRAY && source->kind == TYPE_ARRAY)
    {
        // check if element types are compatible
        if (type_is_assignable(target->array.elem_type, source->array.elem_type))
        {
            // unbound array (size -1) can accept any sized array
            if (target->array.size == -1)
                return true;
            // sized arrays must have exact same size
            if (target->array.size == source->array.size)
                return true;
        }
        return false;
    }

    if (target->kind == TYPE_ALIAS)
        return type_is_assignable(target->alias.target, source);
    if (source->kind == TYPE_ALIAS)
        return type_is_assignable(target, source->alias.target);
    return false;
}

Type *type_get_common_type(Type *a, Type *b)
{
    if (type_equals(a, b))
        return a;
    if (a->kind == TYPE_INT && b->kind == TYPE_INT)
        return a;
    if (a->kind == TYPE_FLOAT && b->kind == TYPE_FLOAT)
        return a;
    if (a->kind == TYPE_INT && b->kind == TYPE_FLOAT)
        return b;
    if (a->kind == TYPE_FLOAT && b->kind == TYPE_INT)
        return a;
    if (a->kind == TYPE_PTR && b->kind == TYPE_PTR)
        return a;
    return NULL;
}

bool type_is_numeric(Type *type)
{
    if (!type)
        return false;
    while (type->kind == TYPE_ALIAS)
    {
        type = type->alias.target;
    }
    return type->kind == TYPE_INT || type->kind == TYPE_FLOAT;
}

bool type_is_integer(Type *type)
{
    if (!type)
        return false;
    while (type->kind == TYPE_ALIAS)
    {
        type = type->alias.target;
    }
    return type->kind == TYPE_INT;
}

bool type_is_pointer_like(Type *type)
{
    if (!type)
        return false;
    while (type->kind == TYPE_ALIAS)
    {
        type = type->alias.target;
    }
    return type->kind == TYPE_PTR || type->kind == TYPE_ARRAY;
}

bool type_can_cast(Type *from, Type *to)
{
    if (!from || !to)
        return false;

    // same-size reinterpretation with ::
    if (from->size == to->size)
        return true;

    // numeric conversions
    if (type_is_numeric(from) && type_is_numeric(to))
        return true;

    // integer to pointer casts (for address literals)
    if (type_is_integer(from) && to->kind == TYPE_PTR)
        return true;

    // pointer to integer casts
    if (from->kind == TYPE_PTR && type_is_integer(to))
        return true;

    // pointer conversions
    if (from->kind == TYPE_PTR && to->kind == TYPE_PTR)
        return true;

    // array to pointer decay (explicit in Mach)
    if (from->kind == TYPE_ARRAY && to->kind == TYPE_PTR)
        return true;

    return false;
}

Type *type_get_element_type(Type *type)
{
    if (!type)
        return NULL;
    while (type->kind == TYPE_ALIAS)
    {
        type = type->alias.target;
    }
    if (type->kind == TYPE_ARRAY)
        return type->array.elem_type;
    if (type->kind == TYPE_PTR)
        return type->ptr.base;
    return NULL;
}

const char *type_to_string(Type *type)
{
    if (!type)
        return "<no type>"; // for void functions

    static char buffer[256];

    switch (type->kind)
    {
    case TYPE_INT:
        if (type->size == 1)
            return type->is_signed ? "i8" : "u8";
        if (type->size == 2)
            return type->is_signed ? "i16" : "u16";
        if (type->size == 4)
            return type->is_signed ? "i32" : "u32";
        if (type->size == 8)
            return type->is_signed ? "i64" : "u64";
        return "integer";
    case TYPE_FLOAT:
        if (type->size == 2)
            return "f16";
        if (type->size == 4)
            return "f32";
        if (type->size == 8)
            return "f64";
        return "float";
    case TYPE_PTR:
        if (!type->ptr.base)
            return "ptr";
        snprintf(buffer, sizeof(buffer), "*%s", type_to_string(type->ptr.base));
        return buffer;
    case TYPE_ARRAY:
        if (type->array.size < 0)
        {
            snprintf(buffer, sizeof(buffer), "[]%s", type_to_string(type->array.elem_type));
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "[%d]%s", type->array.size, type_to_string(type->array.elem_type));
        }
        return buffer;
    case TYPE_FUNCTION:
    {
        int offset = snprintf(buffer, sizeof(buffer), "fun(");

        for (int i = 0; i < type->function.param_count; i++)
        {
            if (i > 0)
                offset += snprintf(buffer + offset, sizeof(buffer) - offset, ", ");
            offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%s", type_to_string(type->function.param_types[i]));
        }

        offset += snprintf(buffer + offset, sizeof(buffer) - offset, ")");

        if (type->function.return_type)
        {
            snprintf(buffer + offset, sizeof(buffer) - offset, " %s", type_to_string(type->function.return_type));
        }
        return buffer;
    }
    case TYPE_STRUCT:
        if (type->composite.name)
            return type->composite.name;
        {
            int offset = snprintf(buffer, sizeof(buffer), "str{");
            for (int i = 0; i < type->composite.field_count; i++)
            {
                if (i > 0)
                    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "; ");
                Symbol *field = type->composite.fields[i];
                offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%s: %s", field->name, type_to_string(field->type));
            }
            snprintf(buffer + offset, sizeof(buffer) - offset, "}");
        }
        return buffer;
    case TYPE_UNION:
        if (type->composite.name)
            return type->composite.name;
        {
            int offset = snprintf(buffer, sizeof(buffer), "uni{");
            for (int i = 0; i < type->composite.field_count; i++)
            {
                if (i > 0)
                    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "; ");
                Symbol *field = type->composite.fields[i];
                offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%s: %s", field->name, type_to_string(field->type));
            }
            snprintf(buffer + offset, sizeof(buffer) - offset, "}");
        }
        return buffer;
    case TYPE_ALIAS:
        return type->alias.name;
    default:
        return "<unknown>";
    }
}

bool type_can_accept_literal(Type *target, AstNode *literal_node)
{
    if (!target || !literal_node || literal_node->kind != AST_LIT_EXPR)
        return false;

    // follow type aliases
    while (target->kind == TYPE_ALIAS)
        target = target->alias.target;

    switch (literal_node->lit_expr.kind)
    {
    case TOKEN_LIT_INT:
    {
        if (target->kind != TYPE_INT)
            return false;

        unsigned long long value = literal_node->lit_expr.int_val;

        // check bounds based on target type size and signedness
        switch (target->size)
        {
        case 1: // u8/i8
            if (target->is_signed)
            {
                // i8: -128 to 127
                if (value > 127)
                    return false;
            }
            else
            {
                // u8: 0 to 255
                if (value > 255)
                    return false;
            }
            break;
        case 2: // u16/i16
            if (target->is_signed)
            {
                // i16: -32768 to 32767
                if (value > 32767)
                    return false;
            }
            else
            {
                // u16: 0 to 65535
                if (value > 65535)
                    return false;
            }
            break;
        case 4: // u32/i32
            if (target->is_signed)
            {
                // i32: -2147483648 to 2147483647
                if (value > 2147483647)
                    return false;
            }
            else
            {
                // u32: 0 to 4294967295
                if (value > 4294967295ULL)
                    return false;
            }
            break;
        case 8: // u64/i64
            if (target->is_signed)
            {
                // i64: -9223372036854775808 to 9223372036854775807
                if (value > 9223372036854775807ULL)
                    return false;
            }
            else
            {
                // u64: 0 to 18446744073709551615 (full range)
                // any unsigned long long value fits
            }
            break;
        default:
            return false;
        }
        return true;
    }

    case TOKEN_LIT_FLOAT:
    {
        if (target->kind != TYPE_FLOAT)
            return false;

        // for simplicity, allow any float literal to any float type
        // in a real implementation, you'd check float ranges too
        return true;
    }

    case TOKEN_LIT_CHAR:
    {
        // chars are always u8, so check if target can accept u8 range
        if (target->kind == TYPE_INT)
        {
            unsigned long long value = literal_node->lit_expr.int_val;
            if (target->size >= 1 && (!target->is_signed || target->size > 1))
            {
                return value <= 255;
            }
        }
        return false;
    }

    default:
        return false;
    }
}
