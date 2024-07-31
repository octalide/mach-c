#include <string.h>

#include "builtin.h"

Builtin builtin_from_string(char *name)
{
    for (int i = 0; i < BI_UNKNOWN; i++)
    {
        if (strcmp(name, BUILTIN_INFO_MAP[i].name) == 0)
        {
            return BUILTIN_INFO_MAP[i].type;
        }
    }

    return BI_UNKNOWN;
}

Type *builtin_type(Builtin builtin, Target target)
{
    Type *type = calloc(1, sizeof(Type));

    switch (builtin)
    {
    case BI_VOID:
        type->kind = TYPE_VOID;
        break;
    case BI_PTR:
        type->kind = TYPE_POINTER;
        type_init(type, target);
        type->data.ptr.base_type = builtin_type(BI_VOID, target);
        break;
    case BI_NIL:
        type = builtin_type(BI_PTR, target);
        type->data.ptr.base_type = NULL;
        break;

    case BI_U8:
        type->kind = TYPE_INT_U;
        type_init(type, target);
        type->size = 1;
        type->alignment = 1;
        break;
    case BI_U16:
        type->kind = TYPE_INT_U;
        type_init(type, target);
        type->size = 2;
        type->alignment = 2;
        break;
    case BI_U32:
        type->kind = TYPE_INT_U;
        type_init(type, target);
        type->size = 4;
        type->alignment = 4;
        break;
    case BI_U64:
        type->kind = TYPE_INT_U;
        type_init(type, target);
        type->size = 8;
        type->alignment = 8;
        break;
    case BI_I8:
        type->kind = TYPE_INT_S;
        type_init(type, target);
        type->size = 1;
        type->alignment = 1;
        break;
    case BI_I16:
        type->kind = TYPE_INT_S;
        type_init(type, target);
        type->size = 2;
        type->alignment = 2;
        break;
    case BI_I32:
        type->kind = TYPE_INT_S;
        type_init(type, target);
        type->size = 4;
        type->alignment = 4;
        break;
    case BI_I64:
        type->kind = TYPE_INT_S;
        type_init(type, target);
        type->size = 8;
        type->alignment = 8;
        break;
    case BI_F32:
        type->kind = TYPE_FLOAT;
        type_init(type, target);
        type->size = 4;
        type->alignment = 4;
        break;
    case BI_F64:
        type->kind = TYPE_FLOAT;
        type_init(type, target);
        type->size = 8;
        type->alignment = 8;
        break;

    case BI_SIZE_OF:
    case BI_ALIGN_OF:
    case BI_OFFSET_OF:
        Type *param = calloc(1, sizeof(Type));
        param->kind = TYPE_META;
        type_init(param, target);

        type->kind = TYPE_FUNCTION;
        type->data.fun.return_type = builtin_type(BI_U32, target);
        type_init(type, target);

        Symbol *symbol_param = calloc(1, sizeof(Symbol));
        symbol_param->name = "type";
        symbol_param->type = param;
        symbol_init(symbol_param);
        type_fun_add_param(type, symbol_param);

        break;

    case BI_SYS_ARCH:
    case BI_SYS_PLAT:
        // NOTE: __SYS_ARCH__ and __SYS_PLAT__ are enum values representing
        //   the target architecture and platform. They are compile-time
        //   constants. They can be used directly if the programmer knows what
        //   each of their values mean, however, the standard library provides
        //   a set of utility functions and variables to make these easier to
        //   work with.
        return builtin_type(BI_U32, target);
    default:
        return NULL;
    }

    return type;
}

int builtin_eval(Builtin builtin, Target target, Type *arg)
{
    switch (builtin)
    {
    case BI_SIZE_OF:
        return (int)arg->size;
    case BI_ALIGN_OF:
        return (int)arg->alignment;
    case BI_OFFSET_OF:
        return (int)arg->offset;
    case BI_SYS_ARCH:
        return (int)target.architecture;
    case BI_SYS_PLAT:
        return (int)target.platform;
    default:
        return 0;
    }
}
