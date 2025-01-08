#include "type.h"
#include "target.h"

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

    switch (type->kind)
    {
        case TYPE_ERROR:
            type->data.type_error = calloc(sizeof(TypeError), 1);
            break;
        case TYPE_META:
            type->data.type_meta = calloc(sizeof(TypeMeta), 1);
            break;
        case TYPE_POINTER:
            type->data.type_pointer = calloc(sizeof(TypePointer), 1);
            break;
        case TYPE_ARRAY:
            type->data.type_array = calloc(sizeof(TypeArray), 1);
            break;
        case TYPE_FUNCTION:
            type->data.type_function = calloc(sizeof(TypeFunction), 1);
            type->data.type_function->parameters = calloc(sizeof(Type *), 2);
            type->data.type_function->parameters[0] = NULL;
            break;
        case TYPE_STRUCT:
            type->data.type_struct = calloc(sizeof(TypeStruct), 1);
            type->data.type_struct->fields = calloc(sizeof(Type *), 2);
            type->data.type_struct->fields[0] = NULL;
            break;
        case TYPE_UNION:
            type->data.type_union = calloc(sizeof(TypeUnion), 1);
            type->data.type_union->fields = calloc(sizeof(Type *), 2);
            type->data.type_union->fields[0] = NULL;
            break;
        default:
            break;
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
        type_free(type->data.type_pointer->type_base);
        type->data.type_pointer->type_base = NULL;
        break;
    case TYPE_ARRAY:
        type_free(type->data.type_array->type_element);
        type->data.type_array->type_element = NULL;
        break;
    case TYPE_FUNCTION:
        for (int i = 0; type->data.type_function->parameters[i] != NULL; i++)
        {
            type_free(type->data.type_function->parameters[i]);
            type->data.type_function->parameters[i] = NULL;
        }
        type->data.type_function->parameters = NULL;

        type_free(type->data.type_function->type_return);
        type->data.type_function->type_return = NULL;
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
        return "FUNCTION";
    case TYPE_STRUCT:
        return "STRUCT";
    case TYPE_UNION:
        return "UNION";
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
        return type_equals(target, a->data.type_array->type_element, b->data.type_array->type_element);
    case TYPE_FUNCTION:
        if (!type_equals(target, a->data.type_function->type_return, b->data.type_function->type_return))
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
        return type_equals(target, a->data.type_pointer->type_base, b->data.type_pointer->type_base);
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

        return type_sizeof(target, type->data.type_array->type_element) * type->data.type_array->length;
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
        return type_alignof(target, type->data.type_array->type_element);
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

// describe each type including things like data layout
char *type_describe(Type *type)
{
    char *result = malloc(1024);

    switch (type->kind)
    {
    case TYPE_ERROR:
        sprintf(result, "{ kind: ERROR, msg: %s }", type->data.type_error->message);
        break;
    case TYPE_META:
        sprintf(result, "{ kind: META }");
        break;
    case TYPE_VOID:
        sprintf(result, "{ kind: VOID }");
        break;
    case TYPE_U8:
        sprintf(result, "{ kind: U8, size: %d }", type_sizeof(target_current(), type));
        break;
    case TYPE_U16:
        sprintf(result, "{ kind: U16, size: %d }", type_sizeof(target_current(), type));
        break;
    case TYPE_U32:
        sprintf(result, "{ kind: U32, size: %d }", type_sizeof(target_current(), type));
        break;
    case TYPE_U64:
        sprintf(result, "{ kind: U64, size: %d }", type_sizeof(target_current(), type));
        break;
    case TYPE_I8:
        sprintf(result, "{ kind: I8, size: %d }", type_sizeof(target_current(), type));
        break;
    case TYPE_I16:
        sprintf(result, "{ kind: I16, size: %d }", type_sizeof(target_current(), type));
        break;
    case TYPE_I32:
        sprintf(result, "{ kind: I32, size: %d }", type_sizeof(target_current(), type));
        break;
    case TYPE_I64:
        sprintf(result, "{ kind: I64, size: %d }", type_sizeof(target_current(), type));
        break;
    case TYPE_F32:
        sprintf(result, "{ kind: F32, size: %d }", type_sizeof(target_current(), type));
        break;
    case TYPE_F64:
        sprintf(result, "{ kind: F64, size: %d }", type_sizeof(target_current(), type));
        break;
    case TYPE_ARRAY:
        sprintf(result, "{ kind: ARRAY, size: %d, element: %s }", type_sizeof(target_current(), type), type_describe(type->data.type_array->type_element));
        break;
    case TYPE_FUNCTION:
        sprintf(result, "{ kind: FUNCTION, return: %s, params: [", type_describe(type->data.type_function->type_return));
        for (int i = 0; type->data.type_function->parameters[i] != NULL; i++)
        {
            char *param_desc = type_describe(type->data.type_function->parameters[i]);
            strcat(result, param_desc);
            if (type->data.type_function->parameters[i + 1] != NULL)
            {
                strcat(result, ", ");
            }
            free(param_desc);
        }
        strcat(result, "] }");
        break;
    case TYPE_STRUCT:
        sprintf(result, "{ kind: STRUCT, fields: [");
        for (int i = 0; type->data.type_struct->fields[i] != NULL; i++)
        {
            char *field_desc = type_describe(type->data.type_struct->fields[i]);
            strcat(result, field_desc);
            if (type->data.type_struct->fields[i + 1] != NULL)
            {
                strcat(result, ", ");
            }
            free(field_desc);
        }
        strcat(result, "] }");
        break;
    case TYPE_UNION:
        sprintf(result, "{ kind: UNION, fields: [");
        for (int i = 0; type->data.type_union->fields[i] != NULL; i++)
        {
            char *field_desc = type_describe(type->data.type_union->fields[i]);
            strcat(result, field_desc);
            if (type->data.type_union->fields[i + 1] != NULL)
            {
                strcat(result, ", ");
            }
            free(field_desc);
        }
        strcat(result, "] }");
        break;
    default:
        sprintf(result, "{ kind: UNKNOWN }");
        break;
    }

    return result;
}
