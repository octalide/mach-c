#include <stdlib.h>
#include <string.h>

#include "token.h"

Token *token_new(TokenKind kind, int pos, char *raw)
{
    Token *token = calloc(1, sizeof(Token));
    token->kind = kind;
    token->pos = pos;
    token->raw = strdup(raw);
    if (token->raw == NULL)
    {
        token->kind = TOKEN_ERROR;
        token->value.error = strdup("failed to allocate token raw");
    }

    return token;
}

void token_free(Token *token)
{
    if (token == NULL)
    {
        return;
    }

    free(token->raw);
    token->raw = NULL;

    if (token->kind == TOKEN_ERROR)
    {
        free(token->value.error);
        token->value.error = NULL;
    }

    free(token);
}

char *token_kind_string(TokenKind kind)
{
    switch (kind)
    {
    case TOKEN_ERROR:
        return "ERROR";
    case TOKEN_EOF:
        return "EOF";
    case TOKEN_COMMENT:
        return "COMMENT";

    case TOKEN_IDENTIFIER:
        return "IDENTIFIER";

    case TOKEN_INT:
        return "INT";
    case TOKEN_FLOAT:
        return "FLOAT";
    case TOKEN_CHARACTER:
        return "CHARACTER";
    case TOKEN_STRING:
        return "STRING";

    case TOKEN_L_PAREN:
        return "L_PAREN";
    case TOKEN_R_PAREN:
        return "R_PAREN";
    case TOKEN_L_BRACKET:
        return "L_BRACKET";
    case TOKEN_R_BRACKET:
        return "R_BRACKET";
    case TOKEN_L_BRACE:
        return "L_BRACE";
    case TOKEN_R_BRACE:
        return "R_BRACE";
    case TOKEN_COLON:
        return "COLON";
    case TOKEN_SEMICOLON:
        return "SEMICOLON";
    case TOKEN_QUESTION:
        return "QUESTION";
    case TOKEN_AT:
        return "AT";
    case TOKEN_DOT:
        return "DOT";
    case TOKEN_COMMA:
        return "COMMA";
    case TOKEN_PLUS:
        return "PLUS";
    case TOKEN_MINUS:
        return "MINUS";
    case TOKEN_STAR:
        return "STAR";
    case TOKEN_PERCENT:
        return "PERCENT";
    case TOKEN_CARET:
        return "CARET";
    case TOKEN_AMPERSAND:
        return "AMPERSAND";
    case TOKEN_PIPE:
        return "PIPE";
    case TOKEN_TILDE:
        return "TILDE";
    case TOKEN_LESS:
        return "LESS";
    case TOKEN_GREATER:
        return "GREATER";
    case TOKEN_EQUAL:
        return "EQUAL";
    case TOKEN_BANG:
        return "BANG";
    case TOKEN_SLASH:
        return "SLASH";
    case TOKEN_BACKSLASH:
        return "BACKSLASH";
    case TOKEN_EQUAL_EQUAL:
        return "EQUAL_EQUAL";
    case TOKEN_BANG_EQUAL:
        return "BANG_EQUAL";
    case TOKEN_LESS_EQUAL:
        return "LESS_EQUAL";
    case TOKEN_GREATER_EQUAL:
        return "GREATER_EQUAL";
    case TOKEN_LESS_LESS:
        return "LESS_LESS";
    case TOKEN_GREATER_GREATER:
        return "GREATER_GREATER";
    case TOKEN_AMPERSAND_AMPERSAND:
        return "AMPERSAND_AMPERSAND";
    case TOKEN_PIPE_PIPE:
        return "PIPE_PIPE";
    case TOKEN_COLON_COLON:
        return "COLON_COLON";

    case TOKEN_KW_VAL:
        return "KW_VAL";
    case TOKEN_KW_VAR:
        return "KW_VAR";
    case TOKEN_KW_DEF:
        return "KW_DEF";
    case TOKEN_KW_USE:
        return "KW_USE";
    case TOKEN_KW_FUN:
        return "KW_FUN";
    case TOKEN_KW_NEW:
        return "KW_NEW";
    case TOKEN_KW_STR:
        return "KW_STR";
    case TOKEN_KW_UNI:
        return "KW_UNI";
    case TOKEN_KW_IF:
        return "KW_IF";
    case TOKEN_KW_OR:
        return "KW_OR";
    case TOKEN_KW_FOR:
        return "KW_FOR";
    case TOKEN_KW_BRK:
        return "KW_BRK";
    case TOKEN_KW_CNT:
        return "KW_CNT";
    case TOKEN_KW_RET:
        return "KW_RET";
    case TOKEN_KW_EXT:
        return "KW_EXT";
    case TOKEN_KW_ASM:
        return "KW_ASM";
    default:
        return "UNKNOWN";
    }
}
