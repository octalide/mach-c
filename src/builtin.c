#include "builtin.h"

Builtin builtin_from_string(const char *name)
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
    switch (builtin)
    {
    case BI_VOID:
        return type_primitive(TYPE_VOID);
    case BI_PTR:
        return type_pointer(target);
    
    case BI_U8:
        return type_primitive(TYPE_U8);
    case BI_U16:
        return type_primitive(TYPE_U16);
    case BI_U32:
        return type_primitive(TYPE_U32);
    case BI_U64:
        return type_primitive(TYPE_U64);
    case BI_I8:
        return type_primitive(TYPE_I8);
    case BI_I16:
        return type_primitive(TYPE_I16);
    case BI_I32:
        return type_primitive(TYPE_I32);
    case BI_I64:
        return type_primitive(TYPE_I64);
    case BI_F32:
        return type_primitive(TYPE_F32);
    case BI_F64:
        return type_primitive(TYPE_F64);
    
    case BI_SIZE_OF:
        return type_function(type_primitive(TYPE_U64), NULL);
    case BI_ALIGN_OF:
        return type_function(type_primitive(TYPE_U64), NULL);
    
    case BI_SYS_ARCH:
        return type_primitive(TYPE_U8);
    case BI_SYS_PLAT:
        return type_primitive(TYPE_U8);

    default:
        return NULL;
    }
}
