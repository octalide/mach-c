#include "op.h"

bool op_is_binary(TokenType type)
{
    switch (type)
    {
    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_STAR:
    case TOKEN_SLASH:
    case TOKEN_PERCENT:
    case TOKEN_LESS:
    case TOKEN_GREATER:
    case TOKEN_LESS_EQUAL:
    case TOKEN_GREATER_EQUAL:
    case TOKEN_EQUAL_EQUAL:
    case TOKEN_BANG_EQUAL:
    case TOKEN_AMPERSAND:
    case TOKEN_CARET:
    case TOKEN_PIPE:
    case TOKEN_AMPERSAND_AMPERSAND:
    case TOKEN_PIPE_PIPE:
    case TOKEN_EQUAL:
    case TOKEN_LESS_LESS:
    case TOKEN_GREATER_GREATER:
        return true;
    default:
        return false;
    }
}

bool op_is_unary(TokenType type)
{
    switch (type)
    {
    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_TILDE:
    case TOKEN_BANG:
    case TOKEN_QUESTION:
    case TOKEN_AT:
        return true;
    default:
        return false;
    }
}

bool op_is_right_associative(TokenType type)
{
    switch (type)
    {
    case TOKEN_CARET:
    case TOKEN_EQUAL:
    case TOKEN_LESS_LESS:
    case TOKEN_GREATER_GREATER:
    case TOKEN_AMPERSAND_AMPERSAND:
    case TOKEN_PIPE_PIPE:
        return true;
    default:
        return false;
    }
}

int op_get_precedence(TokenType op)
{
    switch (op)
    {
    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_TILDE:
    case TOKEN_BANG:
    case TOKEN_QUESTION:
    case TOKEN_AT:
        return 1;
    case TOKEN_STAR:
    case TOKEN_SLASH:
    case TOKEN_PERCENT:
        return 2;
    case TOKEN_LESS_LESS:
    case TOKEN_GREATER_GREATER:
        return 3;
    case TOKEN_LESS:
    case TOKEN_GREATER:
    case TOKEN_LESS_EQUAL:
    case TOKEN_GREATER_EQUAL:
        return 4;
    case TOKEN_EQUAL_EQUAL:
    case TOKEN_BANG_EQUAL:
        return 5;
    case TOKEN_AMPERSAND:
        return 6;
    case TOKEN_CARET:
        return 7;
    case TOKEN_PIPE:
        return 8;
    case TOKEN_AMPERSAND_AMPERSAND:
        return 9;
    case TOKEN_PIPE_PIPE:
        return 10;
    case TOKEN_EQUAL:
        return 11;
    case TOKEN_DOT:
        return 12;
    default:
        return -1;
    }
}
