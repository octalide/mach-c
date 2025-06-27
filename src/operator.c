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

const char *op_to_string(Operator op)
{
    switch (op)
    {
    case OP_ADD:
        return "+";
    case OP_SUB:
        return "-";
    case OP_MUL:
        return "*";
    case OP_DIV:
        return "/";
    case OP_MOD:
        return "%";
    case OP_BITWISE_AND:
        return "&";
    case OP_BITWISE_OR:
        return "|";
    case OP_BITWISE_XOR:
        return "^";
    case OP_BITWISE_NOT:
        return "~";
    case OP_BITWISE_SHL:
        return "<<";
    case OP_BITWISE_SHR:
        return ">>";
    case OP_LOGICAL_AND:
        return "&&";
    case OP_LOGICAL_OR:
        return "||";
    case OP_LOGICAL_NOT:
        return "!";
    case OP_EQUAL:
        return "==";
    case OP_NOT_EQUAL:
        return "!=";
    case OP_LESS:
        return "<";
    case OP_GREATER:
        return ">";
    case OP_LESS_EQUAL:
        return "<=";
    case OP_GREATER_EQUAL:
        return ">=";
    case OP_ASSIGN:
        return "=";
    case OP_REFERENCE:
        return "?";
    case OP_DEREFERENCE:
        return "@";
    default:
        return "UNK";
    }
}
