#include "operator.h"

Operator op_from_token_kind(TokenKind kind)
{
    for (int i = 0; i < OP_COUNT; i++)
    {
        if (op_info[i].token_kind == kind)
        {
            return op_info[i].op;
        }
    }

    return OP_UNKNOWN;
}

TokenKind op_to_token_kind(Operator op)
{
    for (int i = 0; i < OP_COUNT; i++)
    {
        if (op_info[i].op == op)
        {
            return op_info[i].token_kind;
        }
    }

    return TOKEN_ERROR;
}

bool op_is_unary(Operator op)
{
    if (op == OP_UNKNOWN)
    {
        return false;
    }

    for (int i = 0; i < OP_COUNT; i++)
    {
        if (op_info[i].op == op)
        {
            return op_info[i].unary;
        }
    }

    return false;
}

bool op_is_binary(Operator op)
{
    if (op == OP_UNKNOWN)
    {
        return false;
    }

    for (int i = 0; i < OP_COUNT; i++)
    {
        if (op_info[i].op == op)
        {
            return op_info[i].binary;
        }
    }

    return false;
}

int op_precedence(Operator op)
{
    for (int i = 0; i < OP_COUNT; i++)
    {
        if (op_info[i].op == op)
        {
            return op_info[i].precedence;
        }
    }

    return -1;
}

bool op_is_right_associative(Operator op)
{
    for (int i = 0; i < OP_COUNT; i++)
    {
        if (op_info[i].op == op)
        {
            return op_info[i].right_associative;
        }
    }

    return false;
}
