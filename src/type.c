#include "type.h"

#include <string.h>
#include <stdio.h>

size_t align_up(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

size_t max(size_t a, size_t b)
{
    return (a > b) ? a : b;
}

Type *type_new(TypeKind kind)
{
    Type *type = calloc(sizeof(Type), 1);
    type->kind = kind;
    type->name = NULL;

    if (kind == TYPE_FUNCTION)
    {
        type->data.type_function->parameters = calloc(sizeof(Type *), 2);
        type->data.type_function->parameters[0] = NULL;
    }
    else if (kind == TYPE_STRUCT)
    {
        type->data.type_struct->fields = calloc(sizeof(Type *), 2);
        type->data.type_struct->fields[0] = NULL;
    }
    else if (kind == TYPE_UNION)
    {
        type->data.type_union->fields = calloc(sizeof(Type *), 2);
        type->data.type_union->fields[0] = NULL;
    }

    return type;
}

void type_free(Type *type)
{
    if (type == NULL)
    {
        return;
    }

    switch (type->kind)
    {
    case TYPE_POINTER:
        type_free(type->data.type_pointer->base_type);
        type->data.type_pointer->base_type = NULL;
        break;
    case TYPE_ARRAY:
        type_free(type->data.type_array->element_type);
        type->data.type_array->element_type = NULL;
        break;
    case TYPE_FUNCTION:
        for (int i = 0; type->data.type_function->parameters[i] != NULL; i++)
        {
            type_free(type->data.type_function->parameters[i]);
            type->data.type_function->parameters[i] = NULL;
        }
        type->data.type_function->parameters = NULL;

        type_free(type->data.type_function->return_type);
        type->data.type_function->return_type = NULL;
        break;
    case TYPE_STRUCT:
        for (int i = 0; type->data.type_struct->fields[i] != NULL; i++)
        {
            type_free(type->data.type_struct->fields[i]);
            type->data.type_struct->fields[i] = NULL;
        }
        type->data.type_struct->fields = NULL;
        break;
    case TYPE_UNION:
        for (int i = 0; type->data.type_union->fields[i] != NULL; i++)
        {
            type_free(type->data.type_union->fields[i]);
            type->data.type_union->fields[i] = NULL;
        }
        type->data.type_union->fields = NULL;
        break;
    default:
        break;
    }

    free(type);
}

char *type_to_string(Type *type)
{
    if (type == NULL)
    {
        return "null";
    }

    switch (type->kind)
    {
    case TYPE_ERROR:
        return "ERROR";
    case TYPE_LAZY:
        return "LAZY";
    case TYPE_META:
        return "META";
    case TYPE_VOID:
        return "VOID";
    case TYPE_U8:
        return "U8";
    case TYPE_U16:
        return "U16";
    case TYPE_U32:
        return "U32";
    case TYPE_U64:
        return "U64";
    case TYPE_I8:
        return "I8";
    case TYPE_I16:
        return "I16";
    case TYPE_I32:
        return "I32";
    case TYPE_I64:
        return "I64";
    case TYPE_F32:
        return "F32";
    case TYPE_F64:
        return "F64";
    case TYPE_FUNCTION:
        size_t params_len = 1; // for the initial '('
        for (int i = 0; type->data.type_function->parameters[i] != NULL; i++)
        {
            char *param_str = type_to_string(type->data.type_function->parameters[i]);
            params_len += strlen(param_str) + 1; // +1 for ',' or '\0'
            free(param_str);
        }

        char *params = malloc(params_len);
        params[0] = '\0';
        for (int i = 0; type->data.type_function->parameters[i] != NULL; i++)
        {
            char *param_str = type_to_string(type->data.type_function->parameters[i]);
            strcat(params, param_str);
            if (type->data.type_function->parameters[i + 1] != NULL)
            {
                strcat(params, ",");
            }
            free(param_str);
        }

        char *return_str = type_to_string(type->data.type_function->return_type);
        char *result_fun = malloc(strlen(params) + strlen(return_str) + 3);
        sprintf(result_fun, "(%s)%s", params, return_str);
        free(params);
        free(return_str);
        return result_fun;
    case TYPE_STRUCT:
        size_t fields_len = 1; // for the initial '{'
        for (int i = 0; type->data.type_struct->fields[i] != NULL; i++)
        {
            char *field_str = type_to_string(type->data.type_struct->fields[i]);
            fields_len += strlen(field_str) + 1; // +1 for ',' or '\0'
            free(field_str);
        }

        char *fields = malloc(fields_len);
        fields[0] = '\0';
        for (int i = 0; type->data.type_struct->fields[i] != NULL; i++)
        {
            char *field_str = type_to_string(type->data.type_struct->fields[i]);
            strcat(fields, field_str);
            if (type->data.type_struct->fields[i + 1] != NULL)
            {
                strcat(fields, ",");
            }
            free(field_str);
        }

        char *result_str = malloc(strlen(fields) + 3);
        sprintf(result_str, "{%s}", fields);
        free(fields);
        return result_str;
    case TYPE_UNION:
        size_t u_fields_len = 1; // for the initial '{'
        for (int i = 0; type->data.type_union->fields[i] != NULL; i++)
        {
            char *field_str = type_to_string(type->data.type_union->fields[i]);
            u_fields_len += strlen(field_str) + 1; // +1 for ',' or '\0'
            free(field_str);
        }

        char *u_fields = malloc(u_fields_len);
        u_fields[0] = '\0';
        for (int i = 0; type->data.type_union->fields[i] != NULL; i++)
        {
            char *field_str = type_to_string(type->data.type_union->fields[i]);
            strcat(u_fields, field_str);
            if (type->data.type_union->fields[i + 1] != NULL)
            {
                strcat(u_fields, ",");
            }
            free(field_str);
        }

        char *result_uni = malloc(strlen(u_fields) + 3);
        sprintf(result_uni, "<%s>", u_fields);
        free(u_fields);
        return result_uni;
    default:
        return "UNKNOWN";
    }
    return NULL;
}

void type_fun_add_param(Type *type, Type *param)
{
    int count = 0;
    while (type->data.type_function->parameters[count] != NULL)
    {
        count++;
    }

    type->data.type_function->parameters = realloc(type->data.type_function->parameters, (count + 2) * sizeof(Type *));
    type->data.type_function->parameters[count] = param;
    type->data.type_function->parameters[count + 1] = NULL;
}

int type_fun_param_count(Type *type)
{
    int count = 0;
    while (type->data.type_function->parameters[count] != NULL)
    {
        count++;
    }

    return count;
}

Type *type_fun_param_find(Type *type, char *name)
{
    for (int i = 0; type->data.type_function->parameters[i] != NULL; i++)
    {
        if (strcmp(type->data.type_function->parameters[i]->name, name) == 0)
        {
            return type->data.type_function->parameters[i];
        }
    }

    return NULL;
}

void type_str_add_field(Type *type, Type *field)
{
    int count = 0;
    while (type->data.type_struct->fields[count] != NULL)
    {
        count++;
    }

    type->data.type_struct->fields = realloc(type->data.type_struct->fields, (count + 2) * sizeof(Type *));
    type->data.type_struct->fields[count] = field;
    type->data.type_struct->fields[count + 1] = NULL;
}

int type_str_field_count(Type *type)
{
    int count = 0;
    while (type->data.type_struct->fields[count] != NULL)
    {
        count++;
    }

    return count;
}

Type *type_str_field_find(Type *type, char *name)
{
    for (int i = 0; type->data.type_struct->fields[i] != NULL; i++)
    {
        if (strcmp(type->data.type_struct->fields[i]->name, name) == 0)
        {
            return type->data.type_struct->fields[i];
        }
    }

    return NULL;
}

void type_uni_add_field(Type *type, Type *field)
{
    int count = 0;
    while (type->data.type_union->fields[count] != NULL)
    {
        count++;
    }

    type->data.type_union->fields = realloc(type->data.type_union->fields, (count + 2) * sizeof(Type *));
    type->data.type_union->fields[count] = field;
    type->data.type_union->fields[count + 1] = NULL;
}

int type_uni_field_count(Type *type)
{
    int count = 0;
    while (type->data.type_union->fields[count] != NULL)
    {
        count++;
    }

    return count;
}

Type *type_uni_field_find(Type *type, char *name)
{
    for (int i = 0; type->data.type_union->fields[i] != NULL; i++)
    {
        if (strcmp(type->data.type_union->fields[i]->name, name) == 0)
        {
            return type->data.type_union->fields[i];
        }
    }

    return NULL;
}

bool type_array_is_unbound(Type *type)
{
    return type->data.type_array->length == -1;
}

bool type_equals(Target target, Type *a, Type *b)
{
    if (a->kind != b->kind)
    {
        return false;
    }

    int size_a = type_sizeof(target, a);
    int size_b = type_sizeof(target, b);
    int align_a = type_alignof(target, a);
    int align_b = type_alignof(target, b);

    switch (a->kind)
    {
    case TYPE_ERROR:
    case TYPE_META:
    case TYPE_VOID:
        return true;
    case TYPE_U8:
    case TYPE_U16:
    case TYPE_U32:
    case TYPE_U64:
    case TYPE_I8:
    case TYPE_I16:
    case TYPE_I32:
    case TYPE_I64:
    case TYPE_F32:
    case TYPE_F64:
        return size_a == size_b && align_a == align_b;
    case TYPE_ARRAY:
        return type_equals(target, a->data.type_array->element_type, b->data.type_array->element_type);
    case TYPE_FUNCTION:
        if (!type_equals(target, a->data.type_function->return_type, b->data.type_function->return_type))
        {
            return false;
        }

        if (type_fun_param_count(a) != type_fun_param_count(b))
        {
            return false;
        }

        for (int i = 0; a->data.type_function->parameters[i] != NULL && b->data.type_function->parameters[i] != NULL; i++)
        {
            if (!type_equals(target, a->data.type_function->parameters[i], b->data.type_function->parameters[i]))
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

        for (int i = 0; a->data.type_struct->fields[i] != NULL && b->data.type_struct->fields[i] != NULL; i++)
        {
            if (!type_equals(target, a->data.type_struct->fields[i], b->data.type_struct->fields[i]))
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

        for (int i = 0; a->data.type_union->fields[i] != NULL && b->data.type_union->fields[i] != NULL; i++)
        {
            if (!type_equals(target, a->data.type_union->fields[i], b->data.type_union->fields[i]))
            {
                return false;
            }
        }

        return true;
    case TYPE_POINTER:
        return type_equals(target, a->data.type_pointer->base_type, b->data.type_pointer->base_type);
    default:
        return false;
    }
}

int type_sizeof(Target target, Type *type)
{
    switch (type->kind)
    {
    case TYPE_ERROR:
    case TYPE_META:
    case TYPE_VOID:
        return 0;
    case TYPE_U8:
    case TYPE_I8:
        return 1;
    case TYPE_U16:
    case TYPE_I16:
        return 2;
    case TYPE_U32:
    case TYPE_I32:
    case TYPE_F32:
        return 4;
    case TYPE_U64:
    case TYPE_I64:
    case TYPE_F64:
        return 8;
    case TYPE_ARRAY:
        if (type_array_is_unbound(type))
        {
            return 0;
        }

        return type_sizeof(target, type->data.type_array->element_type) * type->data.type_array->length;
    case TYPE_FUNCTION:
        return 0;
    case TYPE_STRUCT:
        int size = 0;
        for (int i = 0; type->data.type_struct->fields[i] != NULL; i++)
        {
            size += type_sizeof(target, type->data.type_struct->fields[i]);
        }
        return size;
    case TYPE_UNION:
        int max_size = 0;
        for (int i = 0; type->data.type_union->fields[i] != NULL; i++)
        {
            max_size = max(max_size, type_sizeof(target, type->data.type_union->fields[i]));
        }
        return max_size;
    case TYPE_POINTER:
        return target_info(target).register_size;
    default:
        return 0;
    }
}

int type_alignof(Target target, Type *type)
{
    switch (type->kind)
    {
    case TYPE_ERROR:
    case TYPE_META:
    case TYPE_VOID:
        return 0;
    case TYPE_U8:
    case TYPE_I8:
        return 1;
    case TYPE_U16:
    case TYPE_I16:
        return 2;
    case TYPE_U32:
    case TYPE_I32:
    case TYPE_F32:
        return 4;
    case TYPE_U64:
    case TYPE_I64:
    case TYPE_F64:
        return 8;
    case TYPE_ARRAY:
        return type_alignof(target, type->data.type_array->element_type);
    case TYPE_FUNCTION:
        return 0;
    case TYPE_STRUCT:
        int align = 0;
        for (int i = 0; type->data.type_struct->fields[i] != NULL; i++)
        {
            align = max(align, type_alignof(target, type->data.type_struct->fields[i]));
        }
        return align;
    case TYPE_UNION:
        int max_align = 0;
        for (int i = 0; type->data.type_union->fields[i] != NULL; i++)
        {
            max_align = max(max_align, type_alignof(target, type->data.type_union->fields[i]));
        }
        return max_align;
    case TYPE_POINTER:
        return target_info(target).register_size;
    default:
        return 0;
    }
}

int type_offsetof(Target target, Type *type, char *name)
{
    if (type->kind != TYPE_STRUCT)
    {
        return -1;
    }

    int offset = 0;
    for (int i = 0; type->data.type_struct->fields[i] != NULL; i++)
    {
        if (strcmp(type->data.type_struct->fields[i]->name, name) == 0)
        {
            return offset;
        }

        offset += type_sizeof(target, type->data.type_struct->fields[i]);
    }

    return -1;
}
