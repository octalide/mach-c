#include "type.h"

size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

size_t max(size_t a, size_t b) {
    return (a > b) ? a : b;
}

Type *type_primitive(TypeKind kind)
{
    Type *type = malloc(sizeof(Type));
    type->kind = kind;

    switch (kind)
    {
    case TYPE_VOID:
        type->size = 0;
        type->alignment = 0;
        break;
    case TYPE_U8:
    case TYPE_I8:
        type->size = 1;
        type->alignment = 1;
        break;
    case TYPE_U16:
    case TYPE_I16:
        type->size = 2;
        type->alignment = 2;
        break;
    case TYPE_U32:
    case TYPE_I32:
    case TYPE_F32:
        type->size = 4;
        type->alignment = 4;
        break;
    case TYPE_U64:
    case TYPE_I64:
    case TYPE_F64:
        type->size = 8;
        type->alignment = 8;
        break;
    default:
        break;
    }

    return type;
}

Type *type_pointer(Target target)
{
    Type *type = malloc(sizeof(Type));
    type->kind = TYPE_POINTER;

    TargetInfo info = target_info(target);
    type->size = info.register_size;
    type->alignment = info.register_size;

    return type;
}

Type *type_array(Type *element_type, size_t count)
{
    Type *type = malloc(sizeof(Type));
    type->kind = TYPE_ARRAY;
    type->size = count * element_type->size;
    type->alignment = element_type->alignment;

    type->data.arr.element_type = element_type;

    return type;
}

Type *type_function(Type *return_type, FunctionParam *params)
{
    Type *type = malloc(sizeof(Type));
    type->kind = TYPE_FUNCTION;
    type->size = 0;
    type->alignment = 0;

    type->data.fun.return_type = return_type;
    type->data.fun.params = params;

    return type;
}

Type *type_struct(StructField *fields)
{
    Type *type = malloc(sizeof(Type));
    type->kind = TYPE_STRUCT;

    size_t size = 0;
    size_t alignment = 0;

    for (StructField *field = fields; field; field = field->next)
    {
        size_t field_size = field->type->size;
        size_t field_alignment = field->type->alignment;

        size = align_up(size, field_alignment);
        alignment = max(alignment, field_alignment);

        field->offset = size;
        size += field_size;
    }

    type->size = align_up(size, alignment);
    type->alignment = alignment;
    type->data.str.fields = fields;

    return type;
}

Type *type_reference(Target target, Type *base_type)
{
    Type *type = malloc(sizeof(Type));
    type->kind = TYPE_REFERENCE;

    TargetInfo info = target_info(target);
    type->size = info.register_size;
    type->alignment = info.register_size;

    type->data.ref.base_type = base_type;

    return type;
}
