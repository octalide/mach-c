#include <string.h>
#include <stdio.h>

#include "type.h"

size_t align_up(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

size_t max(size_t a, size_t b)
{
    return (a > b) ? a : b;
}

void symbol_init(Symbol *symbol)
{
    symbol->name = NULL;
    symbol->type = NULL;
}

void symbol_free(Symbol *symbol)
{
    type_free(symbol->type);
    free(symbol->type);
    free(symbol->name);
}

void type_init(Type *type, Target target)
{
    type->size = 0;
    type->alignment = 0;
    type->offset = 0;

    switch (type->kind)
    {
    case TYPE_ARRAY:
        type->size = type->data.arr.element_type->size * type->data.arr.count;
        type->alignment = type->data.arr.element_type->alignment;
        break;
    case TYPE_STRUCT:
        for (int i = 0; type->data.str.fields[i] != NULL; i++)
        {
            size_t field_size = type->data.str.fields[i]->type->size;
            size_t field_alignment = type->data.str.fields[i]->type->alignment;

            type->size = align_up(type->size, field_alignment) + field_size;
            type->alignment = max(type->alignment, field_alignment);
        }
        break;
    case TYPE_UNION:
        for (int i = 0; type->data.uni.fields[i] != NULL; i++)
        {
            size_t field_size = type->data.uni.fields[i]->type->size;
            size_t field_alignment = type->data.uni.fields[i]->type->alignment;

            type->size = max(type->size, field_size);
            type->alignment = max(type->alignment, field_alignment);
        }
        break;
    case TYPE_POINTER:
        type->size = target_info(target).register_size;
        type->alignment = target_info(target).register_size;
        break;
    default:
        break;
    }
}

void type_free(Type *type)
{
    if (type == NULL)
    {
        return;
    }

    switch (type->kind)
    {
    case TYPE_ARRAY:
        type_free(type->data.arr.element_type);
        break;
    case TYPE_FUNCTION:
        for (int i = 0; type->data.fun.params[i] != NULL; i++)
        {
            symbol_free(type->data.fun.params[i]);
            free(type->data.fun.params[i]);
        }
        type_free(type->data.fun.return_type);
        break;
    case TYPE_STRUCT:
        for (int i = 0; type->data.str.fields[i] != NULL; i++)
        {
            symbol_free(type->data.str.fields[i]);
            free(type->data.str.fields[i]);
        }
        break;
    case TYPE_UNION:
        for (int i = 0; type->data.uni.fields[i] != NULL; i++)
        {
            symbol_free(type->data.uni.fields[i]);
            free(type->data.uni.fields[i]);
        }
        break;
    default:
        break;
    }

    free(type);
}

bool type_equals(Type *a, Type *b)
{
    if (a->kind != b->kind)
    {
        return false;
    }

    switch (a->kind)
    {
    case TYPE_META:
    case TYPE_VOID:
        return true;
    case TYPE_INT_S:
    case TYPE_INT_U:
    case TYPE_FLOAT:
        return a->size == b->size && a->alignment == b->alignment;
    case TYPE_ARRAY:
        return a->data.arr.count == b->data.arr.count && type_equals(a->data.arr.element_type, b->data.arr.element_type);
    case TYPE_FUNCTION:
        if (!type_equals(a->data.fun.return_type, b->data.fun.return_type))
        {
            return false;
        }

        if (type_fun_param_count(a) != type_fun_param_count(b))
        {
            return false;
        }

        for (int i = 0; a->data.fun.params[i] != NULL && b->data.fun.params[i] != NULL; i++)
        {
            if (!type_equals(a->data.fun.params[i]->type, b->data.fun.params[i]->type))
            {
                return false;
            }
        }

        return true;
    case TYPE_STRUCT:
        if (type_str_field_count(a) != type_str_field_count(b))
        {
            return false;
        }

        for (int i = 0; a->data.str.fields[i] != NULL && b->data.str.fields[i] != NULL; i++)
        {
            if (!type_equals(a->data.str.fields[i]->type, b->data.str.fields[i]->type))
            {
                return false;
            }
        }

        return true;
    case TYPE_UNION:
        if (type_str_field_count(a) != type_str_field_count(b))
        {
            return false;
        }

        for (int i = 0; a->data.uni.fields[i] != NULL && b->data.uni.fields[i] != NULL; i++)
        {
            if (!type_equals(a->data.uni.fields[i]->type, b->data.uni.fields[i]->type))
            {
                return false;
            }
        }

        return true;
    case TYPE_POINTER:
        return type_equals(a->data.ptr.base_type, b->data.ptr.base_type);
    default:
        return false;
    }
}

char *type_to_string(Type *type)
{
    switch (type->kind)
    {
    case TYPE_META:
        return "meta";
    case TYPE_VOID:
        return "void";
    case TYPE_INT_U:
        return "u" + type->size;
    case TYPE_INT_S:
        return "s" + type->size;
    case TYPE_FLOAT:
        return "f" + type->size;
    case TYPE_ARRAY:
        char *element = type_to_string(type->data.arr.element_type);
        char *count = malloc(32);
        sprintf(count, "[%d]", type->data.arr.count);

        char *result_arr = malloc(strlen(element) + strlen(count) + 1);
        strcpy(result_arr, element);
        strcat(result_arr, count);

        return result_arr;
    case TYPE_FUNCTION:
        char *params = "";
        for (int i = 0; type->data.fun.params[i] != NULL; i++)
        {
            char *param_str = type_to_string(type->data.fun.params[i]->type);
            char *params_new = malloc(strlen(params) + strlen(param_str) + 2);
            strcpy(params_new, params);
            strcat(params_new, param_str);
            strcat(params_new, ",");
            free(params);
            params = params_new;
        }

        char *return_str = type_to_string(type->data.fun.return_type);
        char *result_fun = malloc(strlen(params) + strlen(return_str) + 3);
        strcpy(result_fun, "(");
        strcat(result_fun, params);
        strcat(result_fun, ")");
        strcat(result_fun, return_str);

        return result_fun;
    case TYPE_STRUCT:
        char *fields_str_all = "";
        for (int i = 0; type->data.str.fields[i] != NULL; i++)
        {
            char *field_str = type_to_string(type->data.str.fields[i]->type);
            char *fields_new = malloc(strlen(fields_str_all) + strlen(field_str) + 2);
            strcpy(fields_new, fields_str_all);
            strcat(fields_new, field_str);
            strcat(fields_new, ",");
            free(fields_str_all);
            fields_str_all = fields_new;
        }

        char *result_str = malloc(strlen(fields_str_all) + 3);
        strcpy(result_str, "{");
        strcat(result_str, fields_str_all);
        strcat(result_str, "}");

        return result_str;
    case TYPE_UNION:
        char *fields_uni_all = "";
        for (int i = 0; type->data.uni.fields[i] != NULL; i++)
        {
            char *field_str = type_to_string(type->data.uni.fields[i]->type);
            char *fields_new = malloc(strlen(fields_uni_all) + strlen(field_str) + 2);
            strcpy(fields_new, fields_uni_all);
            strcat(fields_new, field_str);
            strcat(fields_new, ",");
            free(fields_uni_all);
            fields_uni_all = fields_new;
        }

        char *result_uni = malloc(strlen(fields_uni_all) + 3);
        strcpy(result_uni, "(");
        strcat(result_uni, fields_uni_all);
        strcat(result_uni, ")");

        return result_uni;
    case TYPE_POINTER:
        char *base = type_to_string(type->data.ptr.base_type);
        char *result_ptr = malloc(strlen(base) + 2);
        strcpy(result_ptr, "*");
        strcat(result_ptr, base);

        return result_ptr;
    default:
        return "unknown";
    }
}

void type_fun_add_param(Type *type, Symbol *param)
{
    int count = 0;
    while (type->data.fun.params[count] != NULL)
    {
        count++;
    }

    type->data.fun.params = realloc(type->data.fun.params, (count + 2) * sizeof(Type *));
    type->data.fun.params[count] = param;
    type->data.fun.params[count + 1] = NULL;
}

int type_fun_param_count(Type *type)
{
    int count = 0;
    while (type->data.fun.params[count] != NULL)
    {
        count++;
    }

    return count;
}

Symbol *type_fun_param_find(Type *type, char *name)
{
    for (int i = 0; type->data.fun.params[i] != NULL; i++)
    {
        if (strcmp(type->data.fun.params[i]->name, name) == 0)
        {
            return type->data.fun.params[i];
        }
    }

    return NULL;
}

void type_str_add_field(Type *type, Symbol *field)
{
    int count = 0;
    while (type->data.str.fields[count] != NULL)
    {
        count++;
    }

    type->data.str.fields = realloc(type->data.str.fields, (count + 2) * sizeof(Type *));
    type->data.str.fields[count] = field;
    type->data.str.fields[count + 1] = NULL;
}

int type_str_field_count(Type *type)
{
    int count = 0;
    while (type->data.str.fields[count] != NULL)
    {
        count++;
    }

    return count;
}

Symbol *type_str_field_find(Type *type, char *name)
{
    for (int i = 0; type->data.str.fields[i] != NULL; i++)
    {
        if (strcmp(type->data.str.fields[i]->name, name) == 0)
        {
            return type->data.str.fields[i];
        }
    }

    return NULL;
}

void type_uni_add_field(Type *type, Symbol *field)
{
    int count = 0;
    while (type->data.uni.fields[count] != NULL)
    {
        count++;
    }

    type->data.uni.fields = realloc(type->data.uni.fields, (count + 2) * sizeof(Type *));
    type->data.uni.fields[count] = field;
    type->data.uni.fields[count + 1] = NULL;
}

int type_uni_field_count(Type *type)
{
    int count = 0;
    while (type->data.uni.fields[count] != NULL)
    {
        count++;
    }

    return count;
}

Symbol *type_uni_field_find(Type *type, char *name)
{
    for (int i = 0; type->data.uni.fields[i] != NULL; i++)
    {
        if (strcmp(type->data.uni.fields[i]->name, name) == 0)
        {
            return type->data.uni.fields[i];
        }
    }

    return NULL;
}
