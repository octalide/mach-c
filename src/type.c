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
        type_free(type->data.ptr.base_type);
        type->data.ptr.base_type = NULL;
        break;
    case TYPE_ARRAY:
        type_free(type->data.arr.element_type);
        type->data.arr.element_type = NULL;
        break;
    case TYPE_FUNCTION:
        for (int i = 0; type->data.fun.parameters[i] != NULL; i++)
        {
            type_free(type->data.fun.parameters[i]);
            type->data.fun.parameters[i] = NULL;
        }
        type->data.fun.parameters = NULL;

        type_free(type->data.fun.return_type);
        type->data.fun.return_type = NULL;
        break;
    case TYPE_STRUCT:
        for (int i = 0; type->data.str.fields[i] != NULL; i++)
        {
            type_free(type->data.str.fields[i]);
            type->data.str.fields[i] = NULL;
        }
        type->data.str.fields = NULL;
        break;
    case TYPE_UNION:
        for (int i = 0; type->data.uni.fields[i] != NULL; i++)
        {
            type_free(type->data.uni.fields[i]);
            type->data.uni.fields[i] = NULL;
        }
        type->data.uni.fields = NULL;
        break;
    default:
        break;
    }

    free(type);
}

char *type_to_string(Type *type)
{
    switch (type->kind)
    {
    case TYPE_ERROR:
        return "error";
    case TYPE_META:
        return "meta";
    case TYPE_VOID:
        return "void";
    case TYPE_U8:
        return "u8";
    case TYPE_U16:
        return "u16";
    case TYPE_U32:
        return "u32";
    case TYPE_U64:
        return "u64";
    case TYPE_I8:
        return "i8";
    case TYPE_I16:
        return "i16";
    case TYPE_I32:
        return "i32";
    case TYPE_I64:
        return "i64";
    case TYPE_F32:
        return "f32";
    case TYPE_F64:
        return "f64";
    case TYPE_FUNCTION:
        size_t params_len = 1; // for the initial '('
        for (int i = 0; type->data.fun.parameters[i] != NULL; i++)
        {
            char *param_str = type_to_string(type->data.fun.parameters[i]);
            params_len += strlen(param_str) + 1; // +1 for ',' or '\0'
            free(param_str);
        }

        char *params = malloc(params_len);
        params[0] = '\0';
        for (int i = 0; type->data.fun.parameters[i] != NULL; i++)
        {
            char *param_str = type_to_string(type->data.fun.parameters[i]);
            strcat(params, param_str);
            if (type->data.fun.parameters[i + 1] != NULL)
            {
                strcat(params, ",");
            }
            free(param_str);
        }

        char *return_str = type_to_string(type->data.fun.return_type);
        char *result_fun = malloc(strlen(params) + strlen(return_str) + 3);
        sprintf(result_fun, "(%s)%s", params, return_str);
        free(params);
        free(return_str);
        return result_fun;
    case TYPE_STRUCT:
        size_t fields_len = 1; // for the initial '{'
        for (int i = 0; type->data.str.fields[i] != NULL; i++)
        {
            char *field_str = type_to_string(type->data.str.fields[i]);
            fields_len += strlen(field_str) + 1; // +1 for ',' or '\0'
            free(field_str);
        }

        char *fields = malloc(fields_len);
        fields[0] = '\0';
        for (int i = 0; type->data.str.fields[i] != NULL; i++)
        {
            char *field_str = type_to_string(type->data.str.fields[i]);
            strcat(fields, field_str);
            if (type->data.str.fields[i + 1] != NULL)
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
        for (int i = 0; type->data.uni.fields[i] != NULL; i++)
        {
            char *field_str = type_to_string(type->data.uni.fields[i]);
            u_fields_len += strlen(field_str) + 1; // +1 for ',' or '\0'
            free(field_str);
        }

        char *u_fields = malloc(u_fields_len);
        u_fields[0] = '\0';
        for (int i = 0; type->data.uni.fields[i] != NULL; i++)
        {
            char *field_str = type_to_string(type->data.uni.fields[i]);
            strcat(u_fields, field_str);
            if (type->data.uni.fields[i + 1] != NULL)
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
        return "unknown";
    }
    return NULL;
}

void type_fun_add_param(Type *type, Type *param)
{
    int count = 0;
    while (type->data.fun.parameters[count] != NULL)
    {
        count++;
    }

    type->data.fun.parameters = realloc(type->data.fun.parameters, (count + 2) * sizeof(Type *));
    type->data.fun.parameters[count] = param;
    type->data.fun.parameters[count + 1] = NULL;
}

int type_fun_param_count(Type *type)
{
    int count = 0;
    while (type->data.fun.parameters[count] != NULL)
    {
        count++;
    }

    return count;
}

Type *type_fun_param_find(Type *type, char *name)
{
    for (int i = 0; type->data.fun.parameters[i] != NULL; i++)
    {
        if (strcmp(type->data.fun.parameters[i]->name, name) == 0)
        {
            return type->data.fun.parameters[i];
        }
    }

    return NULL;
}

void type_str_add_field(Type *type, Type *field)
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

Type *type_str_field_find(Type *type, char *name)
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

void type_uni_add_field(Type *type, Type *field)
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

Type *type_uni_field_find(Type *type, char *name)
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
        return type_equals(target, a->data.arr.element_type, b->data.arr.element_type);
    case TYPE_FUNCTION:
        if (!type_equals(target, a->data.fun.return_type, b->data.fun.return_type))
        {
            return false;
        }

        if (type_fun_param_count(a) != type_fun_param_count(b))
        {
            return false;
        }

        for (int i = 0; a->data.fun.parameters[i] != NULL && b->data.fun.parameters[i] != NULL; i++)
        {
            if (!type_equals(target, a->data.fun.parameters[i], b->data.fun.parameters[i]))
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
            if (!type_equals(target, a->data.str.fields[i], b->data.str.fields[i]))
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
            if (!type_equals(target, a->data.uni.fields[i], b->data.uni.fields[i]))
            {
                return false;
            }
        }

        return true;
    case TYPE_POINTER:
        return type_equals(target, a->data.ptr.base_type, b->data.ptr.base_type);
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
        return type_sizeof(target, type->data.arr.element_type) * type->data.arr.length;
    case TYPE_FUNCTION:
        return 0;
    case TYPE_STRUCT:
        int size = 0;
        for (int i = 0; type->data.str.fields[i] != NULL; i++)
        {
            size += type_sizeof(target, type->data.str.fields[i]);
        }
        return size;
    case TYPE_UNION:
        int max_size = 0;
        for (int i = 0; type->data.uni.fields[i] != NULL; i++)
        {
            max_size = max(max_size, type_sizeof(target, type->data.uni.fields[i]));
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
        return type_alignof(target, type->data.arr.element_type);
    case TYPE_FUNCTION:
        return 0;
    case TYPE_STRUCT:
        int align = 0;
        for (int i = 0; type->data.str.fields[i] != NULL; i++)
        {
            align = max(align, type_alignof(target, type->data.str.fields[i]));
        }
        return align;
    case TYPE_UNION:
        int max_align = 0;
        for (int i = 0; type->data.uni.fields[i] != NULL; i++)
        {
            max_align = max(max_align, type_alignof(target, type->data.uni.fields[i]));
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
    for (int i = 0; type->data.str.fields[i] != NULL; i++)
    {
        if (strcmp(type->data.str.fields[i]->name, name) == 0)
        {
            return offset;
        }

        offset += type_sizeof(target, type->data.str.fields[i]);
    }

    return -1;
}
